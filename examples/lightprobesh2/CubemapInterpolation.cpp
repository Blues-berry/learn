#include "CubemapInterpolation.h"
#include "VulkanDevice.h"
#include <cmath>
#include <algorithm>
#include <iostream>

CubemapInterpolation::CubemapInterpolation(vks::VulkanDevice* device)
    : device(device)
{
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

    std::cout << "[CubemapInterpolation::AddProbe] Added probe at position ("
              << position.x << ", " << position.y << ", " << position.z << ")"
              << " Total probes: " << probes.size() << std::endl;
}

void CubemapInterpolation::ClearProbes()
{
    probes.clear();
    std::cout << "[CubemapInterpolation::ClearProbes] All probes cleared" << std::endl;
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
    float maxDistance)
{
    if (probes.empty()) {
        std::cerr << "[CubemapInterpolation::InterpolateAt] No probes available for interpolation!" << std::endl;
        return nullptr;
    }

    // 计算插值权重
    std::vector<float> weights = ComputeWeights(position, maxDistance);

    // 执行插值
    return PerformInterpolation(weights);
}

