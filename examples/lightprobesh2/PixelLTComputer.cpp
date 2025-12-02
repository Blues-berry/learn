#include "PixelLTComputer.h"
#include "VulkanInitializers.hpp"
#include "VulkanTools.h"

PixelLTComputer::PixelLTComputer(vks::VulkanDevice* device, uint32_t width, uint32_t height)
{
    this->device = device;
    this->width = width;
    this->height = height;

    PrepareResources();
    PrepareDescriptors();
    PreparePipeline();
}

PixelLTComputer::~PixelLTComputer()
{
    vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
}

void PixelLTComputer::PrepareResources()
{
    ltCoeffs = std::make_shared<vks::Texture2DArray>(device, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, width, height, 9);
}

void PixelLTComputer::PrepareDescriptors()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 4),
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));
}

void PixelLTComputer::PreparePipeline()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout, 0);
        VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = vks::tools::loadShader("shaders/glsl/lightprobesh2/prt_lt_pixel.comp.spv", device->logicalDevice);
    shaderStage.pName = "main";
    computePipelineCreateInfo.stage = shaderStage;
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &pipeline));

    vkDestroyShaderModule(device->logicalDevice, computePipelineCreateInfo.stage.module, nullptr);
}

void PixelLTComputer::Compute(VkCommandBuffer cmd, GBufferPass* gBufferPass, vks::Buffer* samplesBuffer)
{
    // Update descriptors
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &gBufferPass->GetPositionAttachment()->descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &gBufferPass->GetNormalAttachment()->descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &gBufferPass->GetAlbedoAttachment()->descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &samplesBuffer->descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4, &ltCoeffs->descriptor)
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // Dispatch compute
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);
    vkCmdDispatch(cmd, width / 16, height / 16, 1);
}

