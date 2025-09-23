#include "UpsampleCubeMapPass.h"
#include <stdexcept>

UpsampleCubeMapPass::UpsampleCubeMapPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example), lowResWidth(0), lowResHeight(0), highResWidth(0), highResHeight(0)
{
    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // 低分辨率cubemap
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }           // 高分辨率cubemap
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1)
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    // 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 创建计算管线
    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/upsample_cubemap.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));
}

UpsampleCubeMapPass::~UpsampleCubeMapPass() {
    // 清理在基类中已处理
}

void UpsampleCubeMapPass::SetCubeMaps(const std::shared_ptr<vks::TextureCubeMap>& lowResCube, 
                                      const std::shared_ptr<vks::TextureCubeMap>& highResCube) {
    lowResCubemap = lowResCube;
    highResCubemap = highResCube;

    // 更新描述符集
    VkDescriptorImageInfo lowResInfo = lowResCubemap->descriptor;
    lowResInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo highResInfo = {};
    highResInfo.imageView = highResCubemap->view;
    highResInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &lowResInfo),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &highResInfo)
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void UpsampleCubeMapPass::Generate(VkQueue queue, uint32_t lowWidth, uint32_t lowHeight, uint32_t highWidth, uint32_t highHeight) {
    lowResWidth = lowWidth;
    lowResHeight = lowHeight;
    highResWidth = highWidth;
    highResHeight = highHeight;

    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // 布局转换
    VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
    barrier.image = lowResCubemap->image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    barrier.image = highResCubemap->image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    Draw(cmdBuf);

    // 转换高分辨率cubemap到采样布局
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 创建栅栏用于同步
    VkFence fence;
    VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();
    vkCreateFence(device->logicalDevice, &fenceInfo, nullptr, &fence);
    
    // 提交命令缓冲区（使用设备特定的flushCommandBuffer方法）
    device->flushCommandBuffer(cmdBuf, queue);
    
    // 提交一个空的提交信息到队列，附加栅栏，用于等待命令缓冲区完成
    VkSubmitInfo submitInfo = vks::initializers::submitInfo();
    submitInfo.commandBufferCount = 0;  // 不提交新的命令缓冲区
    submitInfo.pCommandBuffers = nullptr;
    
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device->logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device->logicalDevice, fence, nullptr);
}


void UpsampleCubeMapPass::Dispatch(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    // 假设工作组大小为16x16，覆盖高分辨率cubemap的6个面
    uint32_t groupCountX = (highResWidth + 15) / 16;
    uint32_t groupCountY = (highResHeight + 15) / 16;
    vkCmdDispatch(cmd, groupCountX, groupCountY, 6);
}