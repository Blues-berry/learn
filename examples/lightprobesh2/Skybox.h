#pragma once

#include "VulkanglTFModel.h"
#include "VulkanTexture.h"
#include "ILoader.h"

namespace vls
{
    struct VulkanDevice;
}

class Skybox
{
public:
    explicit Skybox(vks::VulkanDevice* dev, IExampleInterfasce* example);
    ~Skybox();

    struct LocalBuffer {
        glm::mat4 transform;
    };

    void LoadFromPath(const std::string& mesh, VkQueue queue);
    void UpdateCubemap(const std::shared_ptr<vks::TextureCubeMap>& tex);
    void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout);
    void Destroy();
    void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet);
    void Update(const glm::mat4& view);

private:
    void PreparePerBatchResource();
    void UpdateSet();

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    std::unique_ptr<vkglTF::Model> model;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    LocalBuffer localData;
    vks::Buffer localBuffer;
    std::shared_ptr<vks::TextureCubeMap> cubemap;

    VkPipeline pso = VK_NULL_HANDLE;
};