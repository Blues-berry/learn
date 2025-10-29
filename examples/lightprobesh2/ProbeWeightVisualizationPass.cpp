#include "ProbeWeightVisualizationPass.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include <iostream>
#include <cstring>

ProbeWeightVisualizationPass::ProbeWeightVisualizationPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example)
{
    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0: 探针缓冲区
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0),
        // Binding 1: 输出立方体贴图
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            1)
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayout =
        vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

    // 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1)
    };
    VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolCI, nullptr, &descriptorPool));

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    // 创建探针缓冲区
    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &probeBuffer,
        sizeof(ProbeBuffer));
    
    // 映射缓冲区内存
    VK_CHECK_RESULT(probeBuffer.map());

    // 创建计算管线
    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/probe_weight_visualization.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));

    std::cout << "[ProbeWeightVisualizationPass] Initialized successfully" << std::endl;
}

ProbeWeightVisualizationPass::~ProbeWeightVisualizationPass()
{
    probeBuffer.destroy();
    std::cout << "[ProbeWeightVisualizationPass] Destroyed" << std::endl;
}

void ProbeWeightVisualizationPass::AddProbe(const glm::vec3& position)
{
    if (probes.size() >= 256) {
        std::cerr << "[ProbeWeightVisualizationPass::AddProbe] Error: Maximum probe count (256) reached!" << std::endl;
        return;
    }

    probes.push_back(position);
    std::cout << "[ProbeWeightVisualizationPass::AddProbe] Added probe at (" 
              << position.x << ", " << position.y << ", " << position.z << ")"
              << " Total probes: " << probes.size() << std::endl;
}

void ProbeWeightVisualizationPass::ClearProbes()
{
    probes.clear();
    std::cout << "[ProbeWeightVisualizationPass::ClearProbes] All probes cleared" << std::endl;
}

void ProbeWeightVisualizationPass::SetOutputCubemap(const std::shared_ptr<vks::TextureCubeMap>& outputCubemap_)
{
    outputCubemap = outputCubemap_;
    if (outputCubemap) {
        outputWidth = outputCubemap->width;
        outputHeight = outputCubemap->height;
        std::cout << "[ProbeWeightVisualizationPass::SetOutputCubemap] Output cubemap set: " 
                  << outputWidth << "x" << outputHeight << std::endl;
    }
}

void ProbeWeightVisualizationPass::SetVisualizationMode(VisualizationMode mode)
{
    visualizationMode = mode;
    std::cout << "[ProbeWeightVisualizationPass::SetVisualizationMode] Mode set to " << static_cast<uint32_t>(mode) << std::endl;
}

void ProbeWeightVisualizationPass::SetMaxDistance(float distance)
{
    maxDistance = distance;
    std::cout << "[ProbeWeightVisualizationPass::SetMaxDistance] Max distance set to " << distance << std::endl;
}

void ProbeWeightVisualizationPass::UpdateProbeBuffer()
{
    if (!probeBuffer.buffer) {
        std::cerr << "[ProbeWeightVisualizationPass::UpdateProbeBuffer] Probe buffer not initialized!" << std::endl;
        return;
    }

    if (!probeBuffer.mapped) {
        std::cerr << "[ProbeWeightVisualizationPass::UpdateProbeBuffer] Probe buffer not mapped!" << std::endl;
        return;
    }

    ProbeBuffer* bufferData = static_cast<ProbeBuffer*>(probeBuffer.mapped);
    bufferData->probeCount = static_cast<uint32_t>(probes.size());
    bufferData->maxDistance = static_cast<uint32_t>(maxDistance);
    bufferData->visualizationMode = static_cast<uint32_t>(visualizationMode);
    bufferData->padding = 0;

    for (size_t i = 0; i < probes.size(); ++i) {
        bufferData->probes[i].position = glm::vec4(probes[i], 0.0f);
        bufferData->probes[i].reserved = glm::vec4(0.0f);
    }

    std::cout << "[ProbeWeightVisualizationPass::UpdateProbeBuffer] Updated with " << probes.size() << " probes" << std::endl;
}

void ProbeWeightVisualizationPass::UpdateDescriptorSet()
{
    if (probes.empty() || !outputCubemap) {
        std::cerr << "[ProbeWeightVisualizationPass::UpdateDescriptorSet] Missing probes or output cubemap!" << std::endl;
        return;
    }

    // 检查输出立方体贴图是否有效
    if (!outputCubemap->view || outputCubemap->view == VK_NULL_HANDLE) {
        std::cerr << "[ProbeWeightVisualizationPass::UpdateDescriptorSet] Output cubemap view is invalid!" << std::endl;
        return;
    }

    // 更新探针缓冲区描述符
    VkDescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = probeBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ProbeBuffer);

    // 更新输出立方体贴图描述符
    VkDescriptorImageInfo outputImageInfo = {};
    outputImageInfo.imageView = outputCubemap->view;
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &outputImageInfo)
    };

    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), 
                          writeDescriptorSets.data(), 0, nullptr);

    std::cout << "[ProbeWeightVisualizationPass::UpdateDescriptorSet] Descriptor set updated" << std::endl;
}

void ProbeWeightVisualizationPass::Generate(VkQueue queue)
{
    if (probes.empty()) {
        std::cerr << "[ProbeWeightVisualizationPass::Generate] No probes available!" << std::endl;
        return;
    }

    if (!outputCubemap) {
        std::cerr << "[ProbeWeightVisualizationPass::Generate] Output cubemap not set!" << std::endl;
        return;
    }

    UpdateProbeBuffer();
    UpdateDescriptorSet();

    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // 转换图像布局为 GENERAL，用于计算着色器写入
    VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
    barrier.image = outputCubemap->image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    
    vkCmdPipelineBarrier(
        cmdBuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
    
    Draw(cmdBuf);
    device->flushCommandBuffer(cmdBuf, queue);

    std::cout << "[ProbeWeightVisualizationPass::Generate] Weight visualization completed" << std::endl;
}

void ProbeWeightVisualizationPass::Dispatch(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // 计算工作组数量
    uint32_t groupCountX = (outputWidth + 15) / 16;
    uint32_t groupCountY = (outputHeight + 15) / 16;

    vkCmdDispatch(cmd, groupCountX, groupCountY, 6);  // 6个立方体面
}

