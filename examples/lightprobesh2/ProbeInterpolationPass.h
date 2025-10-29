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
 * @brief 探针插值计算通道
 * 
 * 使用GPU计算着色器进行多探针立方体贴图的像素级插值
 */
class ProbeInterpolationPass : public ComputePass {
public:
    /**
     * @brief 插值算法枚举
     */
    enum class InterpolationMode : uint32_t {
        IDW = 0,        // 反距离加权
        LINEAR = 1,     // 线性插值
        CUBIC = 2       // 三次样条插值
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
        uint32_t interpolationMode;
        uint32_t padding;
        glm::vec4 queryPosition;  // 查询位置 (xyz, padding)
        ProbeData probes[256];  // 最多支持256个探针
    };

    /**
     * @brief 构造函数
     */
    explicit ProbeInterpolationPass(vks::VulkanDevice* device_, IExampleInterfasce* example);

    /**
     * @brief 析构函数
     */
    ~ProbeInterpolationPass() override;

    /**
     * @brief 添加探针
     * @param position 探针位置
     * @param cubemap 探针的立方体贴图
     */
    void AddProbe(const glm::vec3& position, const std::shared_ptr<vks::TextureCubeMap>& cubemap);

    /**
     * @brief 清空所有探针
     */
    void ClearProbes();

    /**
     * @brief 设置输出立方体贴图
     * @param outputCubemap 输出的高分辨率立方体贴图
     */
    void SetOutputCubemap(const std::shared_ptr<vks::TextureCubeMap>& outputCubemap);

    /**
     * @brief 设置插值模式
     */
    void SetInterpolationMode(InterpolationMode mode);

    /**
     * @brief 设置最大搜索距离
     */
    void SetMaxDistance(float distance);

    /**
     * @brief 设置查询位置（相机位置或其他查询点）
     */
    void SetQueryPosition(const glm::vec3& position);

    /**
     * @brief 执行插值计算
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

    // 探针数据
    struct ProbeInfo {
        glm::vec3 position;
        std::shared_ptr<vks::TextureCubeMap> cubemap;
    };

    std::vector<ProbeInfo> probes;
    std::shared_ptr<vks::TextureCubeMap> outputCubemap;

    // GPU缓冲区
    vks::Buffer probeBuffer;

    // 参数
    InterpolationMode interpolationMode = InterpolationMode::IDW;
    float maxDistance = 50.0f;
    glm::vec3 queryPosition = glm::vec3(0.0f);
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
};

