#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "VulkanTexture.h"

namespace vks {
    struct VulkanDevice;
    class TextureCubeMap;
}

/**
 * @brief 立方体贴图插值类
 * 
 * 用于在多个探针捕获的立方体贴图之间进行插值，
 * 生成完整的场景立方体贴图。
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
     * @brief 构造函数
     * @param device Vulkan设备指针
     */
    explicit CubemapInterpolation(vks::VulkanDevice* device);
    
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
     * @brief 在指定位置进行立方体贴图插值
     * @param position 查询位置
     * @param maxDistance 最大搜索距离（用于限制参与插值的探针数量）
     * @return 插值后的立方体贴图
     */
    std::shared_ptr<vks::TextureCubeMap> InterpolateAt(
        const glm::vec3& position, 
        float maxDistance = 50.0f
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
     * @brief 执行立方体贴图的插值
     */
    std::shared_ptr<vks::TextureCubeMap> PerformInterpolation(
        const std::vector<float>& weights
    );

    vks::VulkanDevice* device;
    std::vector<ProbeData> probes;
};

