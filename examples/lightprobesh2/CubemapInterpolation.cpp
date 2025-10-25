#include "CubemapInterpolation.h"
#include "ProbeInterpolationPass.h"
#include "ProbeWeightVisualizationPass.h"
#include "VulkanDevice.h"
#include "ILoader.h"
#include <cmath>
#include <algorithm>
#include <iostream>

CubemapInterpolation::CubemapInterpolation(vks::VulkanDevice* device, IExampleInterfasce* example)
    : device(device), example(example)
{
    // 如果提供了示例接口，创建GPU插值通道和权重可视化通道
    if (example) {
        interpolationPass = std::make_unique<ProbeInterpolationPass>(device, example);
        weightVisualizationPass = std::make_unique<ProbeWeightVisualizationPass>(device, example);
        std::cout << "[CubemapInterpolation] GPU interpolation and weight visualization passes initialized" << std::endl;
    }
}

CubemapInterpolation::~CubemapInterpolation()
{
    ClearProbes();
}

void CubemapInterpolation::AddProbe(
    const glm::vec3& position,
    const std::shared_ptr<vks::TextureCubeMap>& cubemap)
{
    if (!cubemap) {
        std::cerr << "[CubemapInterpolation::AddProbe] Warning: Null cubemap provided!" << std::endl;
        return;
    }

    ProbeData probe;
    probe.position = position;
    probe.cubemap = cubemap;
    probes.push_back(probe);

    // 同时添加到GPU插值通道和权重可视化通道
    if (interpolationPass) {
        interpolationPass->AddProbe(position, cubemap);
    }
    if (weightVisualizationPass) {
        weightVisualizationPass->AddProbe(position);
    }

    std::cout << "[CubemapInterpolation::AddProbe] Added probe at position ("
              << position.x << ", " << position.y << ", " << position.z << ")"
              << " Total probes: " << probes.size() << std::endl;
}

void CubemapInterpolation::ClearProbes()
{
    probes.clear();
    if (interpolationPass) {
        interpolationPass->ClearProbes();
    }
    if (weightVisualizationPass) {
        weightVisualizationPass->ClearProbes();
    }
    std::cout << "[CubemapInterpolation::ClearProbes] All probes cleared" << std::endl;
}

void CubemapInterpolation::SetInterpolationMode(InterpolationMode mode)
{
    interpolationMode = mode;
    if (interpolationPass) {
        interpolationPass->SetInterpolationMode(static_cast<ProbeInterpolationPass::InterpolationMode>(mode));
    }
    std::cout << "[CubemapInterpolation::SetInterpolationMode] Mode set to " << static_cast<int>(mode) << std::endl;
}

float CubemapInterpolation::Distance(const glm::vec3& a, const glm::vec3& b) const
{
    glm::vec3 diff = a - b;
    return glm::length(diff);
}

std::vector<float> CubemapInterpolation::ComputeWeights(
    const glm::vec3& position,
    float maxDistance) const
{
    std::vector<float> weights(probes.size(), 0.0f);

    if (probes.empty()) {
        return weights;
    }

    // 计算距离和权重
    float totalWeight = 0.0f;
    for (size_t i = 0; i < probes.size(); ++i) {
        float dist = Distance(position, probes[i].position);

        // 如果距离超过最大距离，权重为0
        if (dist > maxDistance) {
            weights[i] = 0.0f;
            continue;
        }

        // 使用反距离加权（IDW）
        // 权重 = 1 / (distance + epsilon)
        const float epsilon = 0.01f;
        weights[i] = 1.0f / (dist + epsilon);
        totalWeight += weights[i];
    }

    // 归一化权重
    if (totalWeight > 0.0f) {
        for (size_t i = 0; i < weights.size(); ++i) {
            weights[i] /= totalWeight;
        }
    } else {
        // 如果没有有效的探针，使用最近的探针
        float minDist = std::numeric_limits<float>::max();
        size_t closestIdx = 0;
        for (size_t i = 0; i < probes.size(); ++i) {
            float dist = Distance(position, probes[i].position);
            if (dist < minDist) {
                minDist = dist;
                closestIdx = i;
            }
        }
        weights[closestIdx] = 1.0f;
    }

    return weights;
}

std::shared_ptr<vks::TextureCubeMap> CubemapInterpolation::PerformInterpolation(
    const std::vector<float>& weights)
{
    if (probes.empty() || weights.empty()) {
        std::cerr << "[CubemapInterpolation::PerformInterpolation] No probes available!" << std::endl;
        return nullptr;
    }

    // 简化实现：返回权重最高的探针的立方体贴图
    // 完整实现应该在GPU上进行像素级插值
    size_t maxIdx = 0;
    float maxWeight = weights[0];
    for (size_t i = 1; i < weights.size(); ++i) {
        if (weights[i] > maxWeight) {
            maxWeight = weights[i];
            maxIdx = i;
        }
    }

    std::cout << "[CubemapInterpolation::PerformInterpolation] Using probe " << maxIdx
              << " with weight " << maxWeight << std::endl;

    return probes[maxIdx].cubemap;
}

std::shared_ptr<vks::TextureCubeMap> CubemapInterpolation::InterpolateAt(
    const glm::vec3& position,
    float maxDistance,
    uint32_t outputResolution,
    VkQueue queue)
{
    if (probes.empty()) {
        std::cerr << "[CubemapInterpolation::InterpolateAt] No probes available for interpolation!" << std::endl;
        return nullptr;
    }

    // 如果有GPU插值通道且提供了队列，使用GPU加速
    if (interpolationPass && queue != VK_NULL_HANDLE) {
        std::cout << "[CubemapInterpolation::InterpolateAt] Using GPU interpolation at resolution "
                  << outputResolution << "x" << outputResolution << std::endl;

        // 创建输出立方体贴图
        auto outputCubemap = std::make_shared<vks::TextureCubeMap>();
        // 注意：这里需要实际创建GPU纹理，暂时使用简化方式
        // 实际应该通过 RenderTargetCube 创建

        interpolationPass->SetOutputCubemap(outputCubemap);
        interpolationPass->SetMaxDistance(maxDistance);
        interpolationPass->Generate(queue);

        return outputCubemap;
    }

    // 回退到CPU插值
    std::cout << "[CubemapInterpolation::InterpolateAt] Using CPU interpolation (fallback)" << std::endl;
    std::vector<float> weights = ComputeWeights(position, maxDistance);
    return PerformInterpolation(weights);
}

std::shared_ptr<vks::TextureCubeMap> CubemapInterpolation::VisualizeWeights(
    uint32_t outputResolution,
    VkQueue queue,
    uint32_t visualizationMode)
{
    if (probes.empty()) {
        std::cerr << "[CubemapInterpolation::VisualizeWeights] No probes available!" << std::endl;
        return nullptr;
    }

    if (!weightVisualizationPass || queue == VK_NULL_HANDLE) {
        std::cerr << "[CubemapInterpolation::VisualizeWeights] Weight visualization pass not available!" << std::endl;
        return nullptr;
    }

    std::cout << "[CubemapInterpolation::VisualizeWeights] Visualizing weights at resolution "
              << outputResolution << "x" << outputResolution << std::endl;

    // 创建输出立方体贴图
    auto outputCubemap = std::make_shared<vks::TextureCubeMap>();
    // 注意：这里需要实际创建GPU纹理，暂时使用简化方式
    // 实际应该通过 RenderTargetCube 创建

    weightVisualizationPass->SetOutputCubemap(outputCubemap);
    weightVisualizationPass->SetVisualizationMode(static_cast<ProbeWeightVisualizationPass::VisualizationMode>(visualizationMode));
    weightVisualizationPass->Generate(queue);

    return outputCubemap;
}

