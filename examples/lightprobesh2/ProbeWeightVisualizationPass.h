#pragma once

#include "Pass.h"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace vks {
    struct VulkanDevice;
    class TextureCubeMap;
}

/**
 * @brief 探针权重可视化计算通道
 * 
 * 使用GPU计算着色器可视化多探针插值的权重分布
 */
class ProbeWeightVisualizationPass : public ComputePass {
public:
    /**
     * @brief 可视化模式枚举
     */
    enum class VisualizationMode : uint32_t {
        SINGLE_PROBE_WEIGHT = 0,    // 显示第一个探针的权重
        WEIGHT_HEATMAP = 1,         // 权重热力图
        CLOSEST_PROBE_ID = 2        // 显示最近探针的ID
    };

    /**
     * @brief 探针数据结构（GPU端）
     */
    struct ProbeData {
        glm::vec4 position;     // 探针位置 (xyz, padding)
        glm::vec4 reserved;     // 保留用于对齐
    };

    /**
     * @brief 探针缓冲区结构（GPU端）
     */
    struct ProbeBuffer {
        uint32_t probeCount;
        uint32_t maxDistance;
        uint32_t visualizationMode;
        uint32_t padding;
        ProbeData probes[256];  // 最多支持256个探针
    };

    /**
     * @brief 构造函数
     */
    explicit ProbeWeightVisualizationPass(vks::VulkanDevice* device_, IExampleInterfasce* example);

    /**
     * @brief 析构函数
     */
    ~ProbeWeightVisualizationPass() override;

    /**
     * @brief 添加探针
     * @param position 探针位置
     */
    void AddProbe(const glm::vec3& position);

    /**
     * @brief 清空所有探针
     */
    void ClearProbes();

    /**
     * @brief 设置输出立方体贴图
     * @param outputCubemap 输出的立方体贴图
     */
    void SetOutputCubemap(const std::shared_ptr<vks::TextureCubeMap>& outputCubemap);

    /**
     * @brief 设置可视化模式
     */
    void SetVisualizationMode(VisualizationMode mode);

    /**
     * @brief 设置最大搜索距离
     */
    void SetMaxDistance(float distance);

    /**
     * @brief 执行可视化计算
     * @param queue Vulkan队列
     */
    void Generate(VkQueue queue);

    /**
     * @brief 获取探针数量
     */
    size_t GetProbeCount() const { return probes.size(); }

private:
    /**
     * @brief 更新探针缓冲区
     */
    void UpdateProbeBuffer();

    /**
     * @brief 更新描述符集
     */
    void UpdateDescriptorSet();

    /**
     * @brief 执行计算着色器分发
     */
    void Dispatch(VkCommandBuffer cmd) override;

    // 探针位置数据
    std::vector<glm::vec3> probes;
    std::shared_ptr<vks::TextureCubeMap> outputCubemap;

    // GPU缓冲区
    vks::Buffer probeBuffer;

    // 参数
    VisualizationMode visualizationMode = VisualizationMode::WEIGHT_HEATMAP;
    float maxDistance = 50.0f;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
};

