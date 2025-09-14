#pragma once
#include "VulkanDevice.h"
#include "glm/glm.hpp"
#include "ILoader.h"
#include <functional>

namespace vks
{
    struct VulkanDevice;
}

class ComputePass
{
public:
    explicit ComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    ~ComputePass();

protected:
    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    VkPipeline pipeline = VK_NULL_HANDLE;
};

class GenSHComputePass : public ComputePass
{
public:
    explicit GenSHComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    ~GenSHComputePass();
};

class MainPass
{
public:
    explicit MainPass(vks::VulkanDevice* device_);
    ~MainPass();

    struct GlobalUbo {
        glm::mat4 project;
        glm::mat4 view;
        glm::vec4 light[4];
        glm::vec4 cameraPos;
        float exposure = 4.5f;
        float gamma = 2.2f;
    };

    void UpdateGlobal(const GlobalUbo& ubo);

    void SetUp(VkRenderPass renderPass);
    
    void Draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width, uint32_t height, std::function<void(VkCommandBuffer)> &&encoder);

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


class FullScreenPass
{
public:
    FullScreenPass(vks::VulkanDevice* dev, IExampleInterfasce* example, VkFormat format);
    virtual ~FullScreenPass();

    void Prepare();
    void Draw(VkCommandBuffer cmd);

    VkImageView view = VK_NULL_HANDLE;
protected:
    void PrepareRenderPass();

    virtual void PreparePipeline() = 0;
    virtual void PrepareFrameBuffer() = 0;

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    VkRenderPassBeginInfo beginInfo;
    VkClearValue clearValue = {};

    uint32_t width = 1;
    uint32_t height = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VkRenderPass renderpass = VK_NULL_HANDLE;
    VkFramebuffer fbo = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};

class GenBRDFLutPass : public FullScreenPass
{
public:
    explicit GenBRDFLutPass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    ~GenBRDFLutPass();

private:
    void PreparePipeline() override;
    void PrepareFrameBuffer() override;
};

class GenIrranceCubeSinglePass : public FullScreenPass
{
public:
    explicit GenIrranceCubeSinglePass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap, uint32_t mip, uint32_t face);
    ~GenIrranceCubeSinglePass();

private:
    uint32_t mip;
    uint32_t face;

    VkImage cubemap; // weakRef
    VkImageView subView;
};

