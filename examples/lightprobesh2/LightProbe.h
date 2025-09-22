#pragma once
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "Pass.h"
#include "UpsampleCubeMapPass.h"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>
#include <string>
#include "VulkanTexture.h"

class LightProbe {
public:
    struct UBO {
    glm::mat4 view[6];
    glm::mat4 projection;
    };
    struct SHCoefficients {
        glm::vec4 shCoeffs[9];
    };
    VkDescriptorBufferInfo shCoeffs;
    LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width_ = 1024, uint32_t height_ = 1024);
    ~LightProbe();
    void SetPosition(const glm::vec3& position_) { position = position_; }
     // 获取内部的立方体贴图
    std::shared_ptr<vks::TextureCubeMap> GetCubemap() const { return cubemap; }
    void SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_);
    void prepare();
    void CaptureCubeMap(VkFormat format, VkQueue queue);
    void GenSH(VkCommandBuffer cmdBuffer, VkQueue queue);
    void UpdateBindings();
    void updateUBO(const struct UBO& ubo);
private:
   
    void drawScene(VkCommandBuffer cmdBuf);

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;
    glm::vec3 position;
    uint32_t width, height;
    uint32_t lowReswidth = 128, lowResheight = 128;
    std::shared_ptr<vks::TextureCubeMap> cubemap;
    std::shared_ptr<vkglTF::Model> model; // 假设场景模型

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    vks::Buffer uboBuffer;
    SHCoefficients shCoefficients;
    // 深度缓冲相关
    VkImage depthImage = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
};

