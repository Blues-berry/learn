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
struct UBO {
    glm::mat4 view;
    glm::mat4 projection;
};
class LightProbe {
public:
    struct SHCoefficients {
        glm::vec4 shCoeffs[9];
    };
    VkDescriptorBufferInfo shCoeffs;
    LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, glm::vec3 position_, uint32_t width_ = 1024, uint32_t height_ = 1024);
    ~LightProbe();

    void SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_);
    void prepare();
    void CaptureCubeMap(VkFormat format, VkQueue queue);
    void GenSH(VkCommandBuffer cmdBuffer, VkQueue queue);

private:
    void updateUBO(const struct UBO& ubo);
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

