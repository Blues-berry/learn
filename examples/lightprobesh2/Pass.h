#pragma once
#include "VulkanDevice.h"
#include "glm/glm.hpp"
#include <functional>

namespace vks
{
    struct VulkanDevice;
}

class ComputePass
{
public:
    explicit ComputePass(vks::VulkanDevice* device_);
    ~ComputePass();

protected:
    vks::VulkanDevice* device;
    VkPipeline pipeline;
};

class GenSHComputePass : public ComputePass
{
public:
    explicit GenSHComputePass(vks::VulkanDevice* device_);
    ~GenSHComputePass();
};

class MainPass
{
public:
    MainPass(vks::VulkanDevice* device_);
    ~MainPass() = default;

    struct GlobalUbo {
        glm::mat4 project;
        glm::mat4 view;
        glm::vec4 light[4];
        float exposure = 4.5f;
        float gamma = 2.2f;
    };

    void UpdateGlobal(const GlobalUbo& ubo);

    void SetUp(VkRenderPass renderPass);
    
    void Draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width, uint32_t height, std::function<void(VkCommandBuffer)> &&encoder);

    void Destroy();

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;
private:
    void PreparePerPassResource();
    vks::VulkanDevice* device;

    std::vector<VkClearValue> clearValue;
    VkDescriptorPool descriptorPool;

    VkRenderPassBeginInfo beginInfo;

    vks::Buffer globalBuffer;
};