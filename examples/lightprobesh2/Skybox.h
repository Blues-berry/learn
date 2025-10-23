#pragma once

#include "VulkanglTFModel.h"
#include "VulkanTexture.h"
#include "ILoader.h"
#include "Pass.h"

namespace vks
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

    void SetModel(const std::shared_ptr<vkglTF::Model> &model_);
    // 新增：设置 cubemap
    void SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cubemap_) {
        cubemap = cubemap_;
        // UpdateDescriptorSet();  // 如果有 descriptorSet，需要更新
    }
    void UpdateCubemap(const std::shared_ptr<vks::TextureCubeMap>& tex);
    void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique);
    void Destroy();
    void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique technique);
    void Update(const glm::mat4& view);

private:
    void PreparePerBatchResource();
    void UpdateSet();

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    std::shared_ptr<vkglTF::Model> model;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::array<Technique, (uint32_t)ETechnique::NUM> techniques;


    LocalBuffer localData;
    vks::Buffer localBuffer;
    std::shared_ptr<vks::TextureCubeMap> cubemap;
};