#include "PRTComputeShader.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <array>
#include <string>
#include "../base/VulkanTools.h"

namespace PRT {

PRTComputeShader::PRTComputeShader(vks::VulkanDevice* device, VkQueue queue)
    : vulkanDevice(device), computeQueue(queue)
{
}

PRTComputeShader::~PRTComputeShader()
{
    Cleanup();
}

bool PRTComputeShader::Initialize()
{
    std::cout << "[PRTComputeShader] Initializing GPU compute shader..." << std::endl;

    // 创建descriptor set layout
    if (!CreateDescriptorSetLayout()) {
        std::cerr << "[PRTComputeShader] Failed to create descriptor set layout" << std::endl;
        return false;
    }

    // 创建descriptor pool
    if (!CreateDescriptorPool()) {
        std::cerr << "[PRTComputeShader] Failed to create descriptor pool" << std::endl;
        return false;
    }

    // 加载compute shader
    if (!LoadComputeShader()) {
        std::cerr << "[PRTComputeShader] Failed to load compute shader" << std::endl;
        return false;
    }

    // 创建compute pipeline
    if (!CreateComputePipeline()) {
        std::cerr << "[PRTComputeShader] Failed to create compute pipeline" << std::endl;
        return false;
    }

    // 创建descriptor set
    if (!CreateDescriptorSet()) {
        std::cerr << "[PRTComputeShader] Failed to create descriptor set" << std::endl;
        return false;
    }

    // 创建工作缓冲区
    if (!CreateBuffers()) {
        std::cerr << "[PRTComputeShader] Failed to create buffers" << std::endl;
        return false;
    }

    std::cout << "[PRTComputeShader] Initialization completed successfully" << std::endl;
    return true;
}

void PRTComputeShader::Cleanup()
{
    if (!vulkanDevice) return;

    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    // 清理缓冲区
    samplesBuffer.destroy();
    inputCoefficientsBuffer.destroy();
    outputCoefficientsBuffer.destroy();
    ltInputBuffer.destroy();
    rotationParamBuffer.destroy();

    // 清理pipeline
    if (computePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(vulkanDevice->logicalDevice, computePipeline, nullptr);
        computePipeline = VK_NULL_HANDLE;
    }

    // 清理pipeline layout
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    // 清理descriptor set layout
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    // 清理descriptor pool
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }

    // 清理shader module
    if (computeShaderModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vulkanDevice->logicalDevice, computeShaderModule, nullptr);
        computeShaderModule = VK_NULL_HANDLE;
    }

    std::cout << "[PRTComputeShader] Cleanup completed" << std::endl;
}

bool PRTComputeShader::CreateDescriptorSetLayout()
{
    std::cout << "[PRTComputeShader] Creating descriptor set layout..." << std::endl;

    // Unified layout for lighting and LT:
    // binding 0: Samples SSBO
    // binding 1: LT Input SSBO (optional)
    // binding 2: Output Coefficients SSBO
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    VkDescriptorSetLayoutBinding b0{};
    b0.binding = 0;
    b0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(b0);

    VkDescriptorSetLayoutBinding b1{};
    b1.binding = 1;
    b1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b1.descriptorCount = 1;
    b1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(b1);

    VkDescriptorSetLayoutBinding b2{};
    b2.binding = 2;
    b2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b2.descriptorCount = 1;
    b2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings.push_back(b2);

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout));
    return true;
}

bool PRTComputeShader::CreateDescriptorPool()
{
    std::cout << "[PRTComputeShader] Creating descriptor pool..." << std::endl;

    std::array<VkDescriptorPoolSize, 1> poolSizes = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4)
    };

    VkDescriptorPoolCreateInfo poolInfo = vks::initializers::descriptorPoolCreateInfo(static_cast<uint32_t>(poolSizes.size()), poolSizes.data(), 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &poolInfo, nullptr, &descriptorPool));
    return true;
}

bool PRTComputeShader::LoadComputeShader()
{
    std::cout << "[PRTComputeShader] Loading compute shaders..." << std::endl;

    auto loadInto = [&](const std::vector<std::string>& paths, VkShaderModule& dst) -> bool {
        for (const auto& path : paths) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) { continue; }
            size_t fileSize = static_cast<size_t>(file.tellg());
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            file.close();

            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = buffer.size();
            createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());
            if (vkCreateShaderModule(vulkanDevice->logicalDevice, &createInfo, nullptr, &dst) == VK_SUCCESS) {
                std::cout << "[PRTComputeShader] Loaded: " << path << std::endl;
                return true;
            }
        }
        return false;
    };

    std::vector<std::string> candLighting = {
        "shaders/glsl/lightprobesh2/prt_lighting.comp.spv",
        "../shaders/glsl/lightprobesh2/prt_lighting.comp.spv",
        "../../shaders/glsl/lightprobesh2/prt_lighting.comp.spv",
        "../../../shaders/glsl/lightprobesh2/prt_lighting.comp.spv"
    };
    std::vector<std::string> candLT = {
        "shaders/glsl/lightprobesh2/prt_lt.comp.spv",
        "../shaders/glsl/lightprobesh2/prt_lt.comp.spv",
        "../../shaders/glsl/lightprobesh2/prt_lt.comp.spv",
        "../../../shaders/glsl/lightprobesh2/prt_lt.comp.spv"
    };

    bool ok1 = loadInto(candLighting, computeShaderModule);
    if (!ok1) {
        std::cerr << "[PRTComputeShader] ERROR: Failed to load prt_lighting.comp.spv (compile with glslc)." << std::endl;
        return false;
    }
    bool ok2 = loadInto(candLT, computeShaderModuleLT);
    if (!ok2) {
        std::cerr << "[PRTComputeShader] ERROR: Failed to load prt_lt.comp.spv (compile with glslc)." << std::endl;
        return false;
    }
    return true;
}

bool PRTComputeShader::CreateComputePipeline()
{
    std::cout << "[PRTComputeShader] Creating compute pipelines..." << std::endl;

    // Pipeline layout (shared)
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &layoutInfo, nullptr, &pipelineLayout));

    auto makePipeline = [&](VkShaderModule module, VkPipeline& outPipe) {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = module;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stageInfo;
        pipelineInfo.layout = pipelineLayout;
        VK_CHECK_RESULT(vkCreateComputePipelines(vulkanDevice->logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outPipe));
    };

    makePipeline(computeShaderModule, computePipeline);
    makePipeline(computeShaderModuleLT, computePipelineLT);
    return true;
}

bool PRTComputeShader::CreateDescriptorSet()
{
    std::cout << "[PRTComputeShader] Creating descriptor set..." << std::endl;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &descriptorSet));
    return true;
}

bool PRTComputeShader::CreateBuffers()
{
    std::cout << "[PRTComputeShader] Creating buffers..." << std::endl;

    // Minimal: allocate output coefficients buffer and a small samples buffer (will be resized on demand)
    VkDeviceSize coeffSize = sizeof(GPUSHCoefficients);
    if (outputCoefficientsBuffer.buffer == VK_NULL_HANDLE) {
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &outputCoefficientsBuffer,
            coeffSize));
        outputCoefficientsBuffer.map();
        outputCoefficientsBuffer.setupDescriptor(coeffSize);
    }

    // initial samples buffer capacity for 64 samples
    VkDeviceSize sampleStride = sizeof(GPUSample);
    VkDeviceSize initialSamples = 64;
    VkDeviceSize samplesSize = sampleStride * initialSamples;
    if (samplesBuffer.buffer == VK_NULL_HANDLE) {
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &samplesBuffer,
            samplesSize));
        samplesBuffer.map();
        samplesBuffer.setupDescriptor(samplesSize);
    }

    return true;
}

bool PRTComputeShader::ExecuteComputeShader(
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    VkCommandBuffer cmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
    // Helper will end and submit when begin=true
    vulkanDevice->flushCommandBuffer(cmd, computeQueue, true);
    return true;
}

bool PRTComputeShader::ExecuteComputeShaderWith(
    VkPipeline pipeline,
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    VkCommandBuffer cmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
    vulkanDevice->flushCommandBuffer(cmd, computeQueue, true);
    return true;
}


bool PRTComputeShader::UpdateDescriptorSet(
    const vks::Buffer& samplesBuf,
    const vks::Buffer& inputBuffer,
    const vks::Buffer& outputBuf)
{
    std::vector<VkWriteDescriptorSet> writes;

    VkWriteDescriptorSet w0{};
    w0.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w0.dstSet = descriptorSet;
    w0.dstBinding = 0; // samples
    w0.dstArrayElement = 0;
    w0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w0.descriptorCount = 1;
    VkDescriptorBufferInfo info0 = samplesBuf.descriptor;
    w0.pBufferInfo = &info0;
    writes.push_back(w0);

    // Optional LT input at binding 1
    if (inputBuffer.buffer != VK_NULL_HANDLE) {
        VkWriteDescriptorSet w1{};
        w1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w1.dstSet = descriptorSet;
        w1.dstBinding = 1; // LT input
        w1.dstArrayElement = 0;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w1.descriptorCount = 1;
        VkDescriptorBufferInfo info1 = inputBuffer.descriptor;
        w1.pBufferInfo = &info1;
        writes.push_back(w1);
    }

    VkWriteDescriptorSet w2{};
    w2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w2.dstSet = descriptorSet;
    w2.dstBinding = 2; // output
    w2.dstArrayElement = 0;
    w2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w2.descriptorCount = 1;
    VkDescriptorBufferInfo info2 = outputBuf.descriptor;
    w2.pBufferInfo = &info2;
    writes.push_back(w2);

    vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    return true;
}

// ============================================================================
// 公共API实现
// ============================================================================

bool PRTComputeShader::ComputeLightingProjection(
    const std::vector<glm::vec3>& directions,
    const std::vector<glm::vec3>& radiances,
    GPUSHCoefficients& outputCoeffs)
{
    if (directions.empty() || radiances.empty() || directions.size() != radiances.size()) {
        std::cerr << "[PRTComputeShader] ComputeLightingProjection invalid input" << std::endl;
        return false;
    }

    const size_t N = directions.size();
    const VkDeviceSize stride = sizeof(GPUSample);
    const VkDeviceSize needed = static_cast<VkDeviceSize>(N) * stride;

    // (Re)allocate samples buffer if capacity insufficient
    if (samplesBuffer.buffer == VK_NULL_HANDLE || samplesBuffer.size < needed) {
        if (samplesBuffer.buffer != VK_NULL_HANDLE) {
            samplesBuffer.destroy();
        }
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &samplesBuffer,
            needed));
        samplesBuffer.map();
    }

    // Fill samples CPU-side
    std::vector<GPUSample> temp(N);
    for (size_t i = 0; i < N; ++i) {
        temp[i].direction = glm::vec4(glm::normalize(directions[i]), 0.0f);
        temp[i].radiance  = glm::vec4(radiances[i], 0.0f);
    }
    memcpy(samplesBuffer.mapped, temp.data(), needed);
    samplesBuffer.flush(needed);
    samplesBuffer.setupDescriptor(needed); // ensure .length() correct in shader

    // Ensure output buffer ready
    const VkDeviceSize coeffSize = sizeof(GPUSHCoefficients);
    if (outputCoefficientsBuffer.buffer == VK_NULL_HANDLE) {
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &outputCoefficientsBuffer,
            coeffSize));
        outputCoefficientsBuffer.map();
    }
    outputCoefficientsBuffer.setupDescriptor(coeffSize);

    // Update descriptors
    UpdateDescriptorSet(samplesBuffer, inputCoefficientsBuffer, outputCoefficientsBuffer);

    // Dispatch one workgroup (shader sums all samples)
    if (!ExecuteComputeShader(1, 1, 1)) {
        std::cerr << "[PRTComputeShader] ExecuteComputeShader failed" << std::endl;
        return false;
    }

    // Read back
    outputCoefficientsBuffer.invalidate(coeffSize);
    memcpy(&outputCoeffs, outputCoefficientsBuffer.mapped, sizeof(GPUSHCoefficients));
    return true;
}

bool PRTComputeShader::ComputeLightTransportSingle(
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& albedo,
    const std::vector<glm::vec3>& directions,
    GPUSHCoefficients& outputCoeffs)
{
    std::vector<glm::vec3> positions = { position };
    std::vector<glm::vec3> normals   = { normal };
    std::vector<glm::vec3> albedos   = { albedo };
    std::vector<GPUSHCoefficients> batch;
    if (!ComputeLightTransportBatch(positions, normals, albedos, directions, batch)) {
        return false;
    }
    if (!batch.empty()) {
        outputCoeffs = batch[0];
        return true;
    }
    return false;
}

bool PRTComputeShader::ComputeLightTransportBatch(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec3>& albedos,
    const std::vector<glm::vec3>& directions,
    std::vector<GPUSHCoefficients>& outputCoeffsBatch)
{
    size_t count = positions.size();
    if (count == 0 || normals.size() != count || albedos.size() != count || directions.empty()) {
        std::cerr << "[PRTComputeShader] ComputeLightTransportBatch invalid input" << std::endl;
        return false;
    }

    // Prepare samples buffer (directions)
    const size_t Ns = directions.size();
    const VkDeviceSize strideS = sizeof(GPUSample);
    const VkDeviceSize sizeS = static_cast<VkDeviceSize>(Ns) * strideS;
    if (samplesBuffer.buffer == VK_NULL_HANDLE || samplesBuffer.size < sizeS) {
        if (samplesBuffer.buffer != VK_NULL_HANDLE) samplesBuffer.destroy();
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &samplesBuffer,
            sizeS));
        samplesBuffer.map();
    }
    std::vector<GPUSample> sampleTemp(Ns);
    for (size_t i = 0; i < Ns; ++i) {
        sampleTemp[i].direction = glm::vec4(glm::normalize(directions[i]), 0.0f);
        sampleTemp[i].radiance  = glm::vec4(0.0f); // not used in LT
    }
    memcpy(samplesBuffer.mapped, sampleTemp.data(), sizeS);
    samplesBuffer.flush(sizeS);
    samplesBuffer.setupDescriptor(sizeS);

    // Prepare LT input buffer
    const VkDeviceSize strideLT = sizeof(GPULTInput);
    const VkDeviceSize sizeLT = static_cast<VkDeviceSize>(count) * strideLT;
    if (ltInputBuffer.buffer == VK_NULL_HANDLE || ltInputBuffer.size < sizeLT) {
        if (ltInputBuffer.buffer != VK_NULL_HANDLE) ltInputBuffer.destroy();
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &ltInputBuffer,
            sizeLT));
        ltInputBuffer.map();
    }
    struct LTStaging { glm::vec4 p,n,a; };
    std::vector<LTStaging> ltTemp(count);
    for (size_t i = 0; i < count; ++i) {
        ltTemp[i].p = glm::vec4(positions[i], 0.0f);
        ltTemp[i].n = glm::vec4(glm::normalize(normals[i]), 0.0f);
        ltTemp[i].a = glm::vec4(albedos[i], 0.0f);
    }
    memcpy(ltInputBuffer.mapped, ltTemp.data(), sizeLT);
    ltInputBuffer.flush(sizeLT);
    ltInputBuffer.setupDescriptor(sizeLT);

    // Prepare output buffer as an array for count vertices
    const VkDeviceSize coeffSize = sizeof(GPUSHCoefficients);
    const VkDeviceSize outSize = coeffSize * static_cast<VkDeviceSize>(count);
    if (outputCoefficientsBuffer.buffer == VK_NULL_HANDLE || outputCoefficientsBuffer.size < outSize) {
        if (outputCoefficientsBuffer.buffer != VK_NULL_HANDLE) outputCoefficientsBuffer.destroy();
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &outputCoefficientsBuffer,
            outSize));
        outputCoefficientsBuffer.map();
    }
    outputCoefficientsBuffer.setupDescriptor(outSize);

    // Bind descriptors: samples (0), ltInputs (1), outputs (2)
    UpdateDescriptorSet(samplesBuffer, ltInputBuffer, outputCoefficientsBuffer);

    // Dispatch: one invocation per vertex
    if (!ExecuteComputeShaderWith(computePipelineLT, static_cast<uint32_t>(count), 1, 1)) {
        std::cerr << "[PRTComputeShader] ExecuteComputeShaderWith (LT) failed" << std::endl;
        return false;
    }

    // Readback
    outputCoefficientsBuffer.invalidate(outSize);
    outputCoeffsBatch.resize(count);
    memcpy(outputCoeffsBatch.data(), outputCoefficientsBuffer.mapped, outSize);
    return true;
}

bool PRTComputeShader::ComputeRotatedSHCoefficients(
    const GPUSHCoefficients& inputCoeffs,
    float angleRadians,
    GPUSHCoefficients& outputCoeffs)
{
    // TODO: 实现单个旋转计算
    std::cout << "[PRTComputeShader] Computing rotated SH coefficients..." << std::endl;
    return true;
}

bool PRTComputeShader::ComputeMultipleRotations(
    const GPUSHCoefficients& inputCoeffs,
    int numRotations,
    float maxAngleDegrees,
    std::vector<GPUSHCoefficients>& outputCoeffsBatch)
{
    // TODO: 实现多个旋转计算
    std::cout << "[PRTComputeShader] Computing " << numRotations << " rotations..." << std::endl;
    return true;
}

} // namespace PRT

