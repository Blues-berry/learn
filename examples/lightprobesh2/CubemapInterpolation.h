#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "VulkanTexture.h"

namespace vks {
    struct VulkanDevice;
    class TextureCubeMap;
}

class ProbeInterpolationPass;
class ProbeWeightVisualizationPass;

/**
 * @brief 立方体贴图插值类
 *
 * 用于在多个探针捕获的立方体贴图之间进行插值，
 * 生成完整的场景立方体贴图。
 *
 * 现在支持GPU加速的像素级插值。
 */
class CubemapInterpolation {
public:
    /**
     * @brief 探针数据结构
     */
    struct ProbeData {
        glm::vec3 position;                                    // 探针位置
        std::shared_ptr<vks::TextureCubeMap> cubemap;         // 探针的立方体贴图
    };

    /**
     * @brief 插值算法枚举
     */
    enum class InterpolationMode {
        IDW = 0,        // 反距离加权
        LINEAR = 1,     // 线性插值
        CUBIC = 2       // 三次样条插值
    };

    /**
     * @brief 构造函数
     * @param device Vulkan设备指针
     * @param example 示例接口指针
     */
    explicit CubemapInterpolation(vks::VulkanDevice* device, class IExampleInterfasce* example = nullptr);

    /**
     * @brief 析构函数
     */
    ~CubemapInterpolation();

    /**
     * @brief 添加探针数据
     * @param position 探针位置
     * @param cubemap 探针的立方体贴图
     */
    void AddProbe(const glm::vec3& position, const std::shared_ptr<vks::TextureCubeMap>& cubemap);

    /**
     * @brief 清空所有探针数据
     */
    void ClearProbes();

    /**
     * @brief 在指定位置进行立方体贴图插值（GPU加速）
     * @param position 查询位置
     * @param maxDistance 最大搜索距离
     * @param outputResolution 输出立方体贴图的分辨率
     * @param queue Vulkan队列
     * @return 插值后的立方体贴图
     */
    std::shared_ptr<vks::TextureCubeMap> InterpolateAt(
        const glm::vec3& position,
        float maxDistance = 50.0f,
        uint32_t outputResolution = 256,
        VkQueue queue = VK_NULL_HANDLE
    );

    /**
     * @brief 设置插值算法
     */
    void SetInterpolationMode(InterpolationMode mode);

    /**
     * @brief 可视化权重分布
     * @param outputResolution 输出立方体贴图的分辨率
     * @param queue Vulkan队列
     * @param visualizationMode 可视化模式 (0=单个探针权重, 1=权重热力图, 2=最近探针ID)
     * @return 权重可视化立方体贴图
     */
    std::shared_ptr<vks::TextureCubeMap> VisualizeWeights(
        uint32_t outputResolution = 256,
        VkQueue queue = VK_NULL_HANDLE,
        uint32_t visualizationMode = 1
    );

    /**
     * @brief 获取探针数量
     */
    size_t GetProbeCount() const { return probes.size(); }

    /**
     * @brief 获取指定索引的探针数据
     */
    const ProbeData& GetProbe(size_t index) const { return probes[index]; }

private:
    /**
     * @brief 计算两个位置之间的距离
     */
    float Distance(const glm::vec3& a, const glm::vec3& b) const;

    /**
     * @brief 计算插值权重（基于距离的反距离加权）
     */
    std::vector<float> ComputeWeights(
        const glm::vec3& position,
        float maxDistance
    ) const;

    /**
     * @brief 执行立方体贴图的插值（CPU版本，用于回退）
     */
    std::shared_ptr<vks::TextureCubeMap> PerformInterpolation(
        const std::vector<float>& weights
    );

    vks::VulkanDevice* device;
    class IExampleInterfasce* example;
    std::vector<ProbeData> probes;
    std::unique_ptr<ProbeInterpolationPass> interpolationPass;
    std::unique_ptr<ProbeWeightVisualizationPass> weightVisualizationPass;
    InterpolationMode interpolationMode = InterpolationMode::IDW;
    
    // 保持最后一个 StorageCubeMap 的生命周期，防止底层 Vulkan 资源被过早释放
    std::shared_ptr<class StorageCubeMap> lastStorageCube;
};

