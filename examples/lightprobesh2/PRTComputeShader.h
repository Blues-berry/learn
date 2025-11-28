#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>
#include "VulkanDevice.h"
#include "VulkanBuffer.h"

// GPU端PRT计算的数据结构定义
// 这些结构体必须与GLSL shader中的结构体内存布局完全一致

namespace PRT {

// ============================================================================
// GPU端数据结构（与GLSL对应）
// ============================================================================

// 球谐系数（GPU端）- 9个系数，每个RGB三通道
struct GPUSHCoefficients {
    glm::vec4 coeffs[9];  // 使用vec4便于GPU对齐，w分量未使用
};

// 采样方向和辐射度
struct GPUSample {
    glm::vec4 direction;   // xyz为方向，w未使用
    glm::vec4 radiance;    // xyz为辐射度，w未使用
};

// Light Transport计算的输入数据
struct GPULTInput {
    glm::vec4 position;    // xyz为位置，w未使用
    glm::vec4 normal;      // xyz为法向量，w未使用
    glm::vec4 albedo;      // xyz为反射率，w未使用
};

// 旋转参数
struct GPURotationParam {
    float angleRadians;    // 旋转角度（弧度）
    float padding[3];      // 对齐到16字节
};

// ============================================================================
// GPU计算管理类
// ============================================================================

class PRTComputeShader {
public:
    explicit PRTComputeShader(vks::VulkanDevice* device);
    ~PRTComputeShader();

    // 初始化compute pipeline
    bool Initialize();

    // 清理资源
    void Cleanup();

    // ========================================================================
    // 光照投影计算 (Lighting Projection)
    // ========================================================================
    
    // 计算光照的球谐系数
    // 输入：采样方向和辐射度
    // 输出：球谐系数
    bool ComputeLightingProjection(
        const std::vector<glm::vec3>& directions,
        const std::vector<glm::vec3>& radiances,
        GPUSHCoefficients& outputCoeffs
    );

    // ========================================================================
    // Light Transport计算 (Per-Vertex)
    // ========================================================================
    
    // 计算单个顶点的Light Transport系数
    bool ComputeLightTransportSingle(
        const glm::vec3& position,
        const glm::vec3& normal,
        const glm::vec3& albedo,
        const std::vector<glm::vec3>& directions,
        GPUSHCoefficients& outputCoeffs
    );

    // 批量计算多个顶点的Light Transport系数
    bool ComputeLightTransportBatch(
        const std::vector<glm::vec3>& positions,
        const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& albedos,
        const std::vector<glm::vec3>& directions,
        std::vector<GPUSHCoefficients>& outputCoeffsBatch
    );

    // ========================================================================
    // 球谐旋转计算 (SH Rotation)
    // ========================================================================
    
    // 计算旋转后的球谐系数
    bool ComputeRotatedSHCoefficients(
        const GPUSHCoefficients& inputCoeffs,
        float angleRadians,
        GPUSHCoefficients& outputCoeffs
    );

    // 批量计算多个旋转角度的系数
    bool ComputeMultipleRotations(
        const GPUSHCoefficients& inputCoeffs,
        int numRotations,
        float maxAngleDegrees,
        std::vector<GPUSHCoefficients>& outputCoeffsBatch
    );

    // ========================================================================
    // 辅助方法
    // ========================================================================
    
    // 获取compute pipeline
    VkPipeline GetComputePipeline() const { return computePipeline; }
    
    // 获取pipeline layout
    VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }

private:
    // Vulkan设备
    vks::VulkanDevice* vulkanDevice;

    // Compute pipeline相关
    VkPipeline computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

    // Shader module
    VkShaderModule computeShaderModule = VK_NULL_HANDLE;

    // 工作缓冲区
    vks::Buffer samplesBuffer;           // 采样方向和辐射度
    vks::Buffer inputCoefficientsBuffer; // 输入球谐系数
    vks::Buffer outputCoefficientsBuffer;// 输出球谐系数
    vks::Buffer ltInputBuffer;           // Light Transport输入数据
    vks::Buffer rotationParamBuffer;     // 旋转参数

    // 私有方法
    bool CreateComputePipeline();
    bool CreateDescriptorSetLayout();
    bool CreateDescriptorPool();
    bool CreateDescriptorSet();
    bool LoadComputeShader();
    bool CreateBuffers();
    
    // 执行compute shader
    bool ExecuteComputeShader(
        uint32_t groupCountX,
        uint32_t groupCountY = 1,
        uint32_t groupCountZ = 1
    );

    // 更新descriptor set中的buffer绑定
    bool UpdateDescriptorSet(
        const vks::Buffer& samplesBuffer,
        const vks::Buffer& inputBuffer,
        const vks::Buffer& outputBuffer
    );
};

} // namespace PRT

