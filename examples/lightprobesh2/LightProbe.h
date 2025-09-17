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

    //
    void prepare();
    // 设置探针位置
    void SetPosition(const glm::vec3& position_) { position = position_; }

    // 使用现有的 CubeMap
    void SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap> &cubemap);

    // 使用探针位置抓取
    void CaptureCubeMap(VkFormat format, VkQueue queue);

    // 生成球偕
    void GenSH(VkCommandBuffer cmdBuffer, VkQueue queue);
    
    // 获取内部的立方体贴图
    std::shared_ptr<vks::TextureCubeMap> GetCubemap() const { return cubemap; }

private:
    vks::VulkanDevice* device = nullptr;
    uint32_t lowReswidth = 128;
    uint32_t lowResheight = 128;
    uint32_t highReswidth = 256;
    uint32_t highResheight = 256;

    glm::vec3 position;
    
    std::shared_ptr<vks::TextureCubeMap> cubemap;

    SHCoefficients shCoefficients;
};