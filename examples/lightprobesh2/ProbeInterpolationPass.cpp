#include "ProbeInterpolationPass.h"
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include <iostream>
#include <cstring>

ProbeInterpolationPass::ProbeInterpolationPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example)
{
    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        // Binding 0: 探针缓冲区
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_SHADER_STAGE_COMPUTE_BIT,
            0),
        // Binding 1: 探针立方体贴图数组（最多256个）
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            VK_SHADER_STAGE_COMPUTE_BIT,
            1,
            256),  // 数组大小
        // Binding 2: 输出立方体贴图
        vks::initializers::descriptorSetLayoutBinding(
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_SHADER_STAGE_COMPUTE_BIT,
            2)
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
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256),
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

    // 创建计算管线
    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/probe_interpolation.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));

    std::cout << "[ProbeInterpolationPass] Initialized successfully" << std::endl;
}

ProbeInterpolationPass::~ProbeInterpolationPass()
{
    probeBuffer.destroy();
    std::cout << "[ProbeInterpolationPass] Destroyed" << std::endl;
}

void ProbeInterpolationPass::AddProbe(const glm::vec3& position, const std::shared_ptr<vks::TextureCubeMap>& cubemap)
{
    if (!cubemap) {
        std::cerr << "[ProbeInterpolationPass::AddProbe] Warning: Null cubemap provided!" << std::endl;
        return;
    }

    if (probes.size() >= 256) {
        std::cerr << "[ProbeInterpolationPass::AddProbe] Error: Maximum probe count (256) reached!" << std::endl;
        return;
    }

    ProbeInfo probe;
    probe.position = position;
    probe.cubemap = cubemap;
    probes.push_back(probe);

    std::cout << "[ProbeInterpolationPass::AddProbe] Added probe at (" 
              << position.x << ", " << position.y << ", " << position.z << ")"
              << " Total probes: " << probes.size() << std::endl;
}

void ProbeInterpolationPass::ClearProbes()
{
    probes.clear();
    std::cout << "[ProbeInterpolationPass::ClearProbes] All probes cleared" << std::endl;
}

void ProbeInterpolationPass::SetOutputCubemap(const std::shared_ptr<vks::TextureCubeMap>& outputCubemap_)
{
    outputCubemap = outputCubemap_;
    if (outputCubemap) {
        outputWidth = outputCubemap->width;
        outputHeight = outputCubemap->height;
        std::cout << "[ProbeInterpolationPass::SetOutputCubemap] Output cubemap set: " 
                  << outputWidth << "x" << outputHeight << std::endl;
    }
}

void ProbeInterpolationPass::SetInterpolationMode(InterpolationMode mode)
{
    interpolationMode = mode;
    std::cout << "[ProbeInterpolationPass::SetInterpolationMode] Mode set to " << static_cast<uint32_t>(mode) << std::endl;
}

void ProbeInterpolationPass::SetMaxDistance(float distance)
{
    maxDistance = distance;
    std::cout << "[ProbeInterpolationPass::SetMaxDistance] Max distance set to " << distance << std::endl;
}

void ProbeInterpolationPass::UpdateProbeBuffer()
{
    if (!probeBuffer.buffer) {
        std::cerr << "[ProbeInterpolationPass::UpdateProbeBuffer] Probe buffer not initialized!" << std::endl;
        return;
    }

    ProbeBuffer* bufferData = static_cast<ProbeBuffer*>(probeBuffer.mapped);
    bufferData->probeCount = static_cast<uint32_t>(probes.size());
    bufferData->maxDistance = static_cast<uint32_t>(maxDistance);
    bufferData->interpolationMode = static_cast<uint32_t>(interpolationMode);
    bufferData->padding = 0;

    for (size_t i = 0; i < probes.size(); ++i) {
        bufferData->probes[i].position = glm::vec4(probes[i].position, 0.0f);
        bufferData->probes[i].reserved = glm::vec4(0.0f);
    }

    std::cout << "[ProbeInterpolationPass::UpdateProbeBuffer] Updated with " << probes.size() << " probes" << std::endl;
}

void ProbeInterpolationPass::UpdateDescriptorSet()
{
    if (probes.empty() || !outputCubemap) {
        std::cerr << "[ProbeInterpolationPass::UpdateDescriptorSet] Missing probes or output cubemap!" << std::endl;
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

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

    // 写入探针缓冲区
    writeDescriptorSets.push_back(
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &bufferInfo));

    // 写入探针立方体贴图数组
    std::vector<VkDescriptorImageInfo> probeImageInfos;
    for (const auto& probe : probes) {
        VkDescriptorImageInfo imageInfo = {};
        imageInfo.sampler = probe.cubemap->sampler;
        imageInfo.imageView = probe.cubemap->view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        probeImageInfos.push_back(imageInfo);
    }

    // 填充剩余的数组元素为空
    for (size_t i = probes.size(); i < 256; ++i) {
        probeImageInfos.push_back(probeImageInfos[0]);  // 使用第一个作为占位符
    }

    VkWriteDescriptorSet writeProbeImages = vks::initializers::writeDescriptorSet(
        descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, probeImageInfos.data());
    writeProbeImages.descriptorCount = 256;
    writeDescriptorSets.push_back(writeProbeImages);

    // 写入输出立方体贴图
    writeDescriptorSets.push_back(
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &outputImageInfo));

    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), 
                          writeDescriptorSets.data(), 0, nullptr);

    std::cout << "[ProbeInterpolationPass::UpdateDescriptorSet] Descriptor set updated" << std::endl;
}

void ProbeInterpolationPass::Generate(VkQueue queue)
{
    if (probes.empty()) {
        std::cerr << "[ProbeInterpolationPass::Generate] No probes available!" << std::endl;
        return;
    }

    if (!outputCubemap) {
        std::cerr << "[ProbeInterpolationPass::Generate] Output cubemap not set!" << std::endl;
        return;
    }

    UpdateProbeBuffer();
    UpdateDescriptorSet();

    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    Draw(cmdBuf);
    device->flushCommandBuffer(cmdBuf, queue);

    std::cout << "[ProbeInterpolationPass::Generate] Interpolation completed" << std::endl;
}

void ProbeInterpolationPass::Dispatch(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // 计算工作组数量
    uint32_t groupCountX = (outputWidth + 15) / 16;
    uint32_t groupCountY = (outputHeight + 15) / 16;

    vkCmdDispatch(cmd, groupCountX, groupCountY, 6);  // 6个立方体面
}

