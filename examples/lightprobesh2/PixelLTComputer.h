#pragma once

#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "GBufferPass.h"

class PixelLTComputer
{
public:
    PixelLTComputer(vks::VulkanDevice* device, uint32_t width, uint32_t height);
    ~PixelLTComputer();

    void Compute(VkCommandBuffer cmd, GBufferPass* gBufferPass, vks::Buffer* samplesBuffer);
    std::shared_ptr<vks::Texture2DArray> GetLTTextureArray() { return ltCoeffs; }

private:
    void PrepareResources();
    void PrepareDescriptors();
    void PreparePipeline();

    vks::VulkanDevice* device;
    uint32_t width, height;

    std::shared_ptr<vks::Texture2DArray> ltCoeffs;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

