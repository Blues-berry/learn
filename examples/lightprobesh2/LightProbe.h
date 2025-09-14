#pragma once
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

#include "VulkanTexture.h"

namespace vks {
    struct VulkanDevice;
}

struct SHCoefficients {
    glm::vec4 shCoeffs[9];
};

class LightProbe
{
public:
    explicit LightProbe(vks::VulkanDevice * device_) : device(device_) {}
    ~LightProbe() = default;

    // 设置探针位置
    void SetPosition(const glm::vec3& position_) { position = position_; }

    // 使用现有的 CubeMap
    void SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap> &cubemap);

    // 使用探针位置抓取
    void CaptureCubeMap(VkFormat format, VkQueue queue);

    // 生成球偕
    void GenSH(VkCommandBuffer cmdBuffer, VkQueue queue);

private:
    vks::VulkanDevice* device = nullptr;
    
    uint32_t width = 256;
    uint32_t height = 256;

    glm::vec3 position;
    
    std::shared_ptr<vks::TextureCubeMap> cubemap;

    SHCoefficients shCoefficients;
};;