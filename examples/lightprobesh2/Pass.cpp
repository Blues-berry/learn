#include "Pass.h"

ComputePass::ComputePass(vks::VulkanDevice* device_) : device(device_)
{
}

ComputePass::~ComputePass()
{
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    }
}


GenSHComputePass::GenSHComputePass(vks::VulkanDevice* device_)
    : ComputePass(device_)
{
    //VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    //computePipelineCI.stage = shader;

    //VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline))
}
   
GenSHComputePass::~GenSHComputePass()
{
}

MainPass::MainPass(vks::VulkanDevice* device_)
    : device(device_)
    , beginInfo(vks::initializers::renderPassBeginInfo())
{
    clearValue.resize(2);

    clearValue[0].color.float32[0] = 0.2f;
    clearValue[0].color.float32[1] = 0.2f;
    clearValue[0].color.float32[2] = 0.2f;
    clearValue[0].color.float32[3] = 0.f;

    clearValue[1].depthStencil.depth = 1.0;
    clearValue[1].depthStencil.stencil = 0.0;

    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValue.data();

    PreparePerPassResource();
}

void MainPass::SetUp(VkRenderPass pass)
{
    beginInfo.renderPass = pass;
}

void MainPass::UpdateGlobal(const GlobalUbo& ubo)
{
    memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo));
}

void MainPass::Draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width, uint32_t height, std::function<void(VkCommandBuffer)> &&encoder)
{
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.framebuffer = framebuffer;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = vks::initializers::viewport((float)beginInfo.renderArea.extent.width, (float)beginInfo.renderArea.extent.height, 0.0f, 1.0f);
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = vks::initializers::rect2D(viewport.width, viewport.height, 0, 0);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    encoder(cmd);

    vkCmdEndRenderPass(cmd);
}

void MainPass::Destroy()
{
    globalBuffer.unmap();
    globalBuffer.destroy();
}

void MainPass::PreparePerPassResource()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &globalBuffer,
        sizeof(GlobalUbo));
    globalBuffer.map();

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalBuffer.descriptor),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}