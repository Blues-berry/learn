#include "Pass.h"
#include "glm/gtc/matrix_transform.hpp"
#include <array>

ComputePass::ComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example) : device(device_), iLoader(example)
{
}

ComputePass::~ComputePass()
{
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
    }
}

void ComputePass::Draw(VkCommandBuffer cmd)
{
    Dispatch(cmd);
}

FullScreenPass::FullScreenPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkFormat format_) : device(device_), iLoader(example), format(format_)
{
    clearValue.color.float32[0] = 0.f;
    clearValue.color.float32[1] = 0.f;
    clearValue.color.float32[2] = 0.f;
    clearValue.color.float32[3] = 0.f;

    beginInfo = vks::initializers::renderPassBeginInfo();
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;

    GenerateSampler();
}

FullScreenPass::~FullScreenPass()
{
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device->logicalDevice, sampler, nullptr);
    }

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    }

    if (image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, image, nullptr);
    }

    if (view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, view, nullptr);
    }

    if (deviceMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
    }

    if (renderpass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device->logicalDevice, renderpass, nullptr);
    }

    if (fbo != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device->logicalDevice, fbo, nullptr);
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
    }
}

void FullScreenPass::GenerateSampler()
{
    VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCI, nullptr, &sampler));
}

void FullScreenPass::Prepare()
{
    PrepareRenderPass();
    PreparePipeline();
    PrepareFrameBuffer();
    PrepareData();
}

void FullScreenPass::FeedDescriptor(VkDescriptorImageInfo& descriptor)
{
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void FullScreenPass::Draw(VkCommandBuffer cmd)
{
    if (pipeline == VK_NULL_HANDLE)
    {
        return;
    }

    beginInfo.renderPass = renderpass;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.framebuffer = fbo;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

void FullScreenPass::PrepareRenderPass()
{
    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderpass));
}

GenBRDFLutPass::GenBRDFLutPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : FullScreenPass(device_, example, VK_FORMAT_R16G16_SFLOAT)
{
    width = 512;
    height = 512;
}

GenBRDFLutPass::~GenBRDFLutPass()
{

}

void GenBRDFLutPass::PreparePipeline()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(nullptr, 0);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    // Look-up-table (from BRDF) pipeline
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
}

void GenBRDFLutPass::PrepareFrameBuffer()
{
    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = width;
    imageCI.extent.height = height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &image));


    VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs = {};
    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));


    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.layerCount = 1;
    viewCI.image = image;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &view));


    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &view;
    framebufferCI.width = width;
    framebufferCI.height = height;
    framebufferCI.layers = 1;

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &fbo));
}


GenSHComputePass::GenSHComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example)
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    VkPipelineLayoutCreateInfo pplInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pplInfo, nullptr, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/sh_compute.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &shCoeffBuffer,
        sizeof(SHCoefficients));
}
   
GenSHComputePass::~GenSHComputePass()
{
    shCoeffBuffer.destroy();
}

void GenSHComputePass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    cubemap = cube;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &cubemap->descriptor),
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &shCoeffBuffer.descriptor),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GenSHComputePass::FeedSH(VkDescriptorBufferInfo& descriptor)
{
    descriptor = shCoeffBuffer.descriptor;
}

void GenSHComputePass::Generate(VkQueue queue)
{
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Draw(cmdBuf);

    device->flushCommandBuffer(cmdBuf, queue);
}

void GenSHComputePass::Dispatch(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, 1, 1, 1);
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
    clearValue[1].depthStencil.stencil = 0;

    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValue.data();

    PreparePerPassResource();
}

MainPass::~MainPass()
{
    globalBuffer.unmap();
    globalBuffer.destroy();

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
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

void MainPass::UpdateBinngs()
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &environmemts.shCoeffs),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &environmemts.brdfView),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &environmemts.irradianceCube),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &environmemts.prefilteredCube),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void MainPass::PreparePerPassResource()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
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
}


GenIBLCubeMipPass::GenIBLCubeMipPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap_, VkFormat format, uint32_t mip_, uint32_t width_, uint32_t height_)
    : FullScreenPass(device_, example, format)
    , cubemap(cubemap_)
    , mipmap(mip_)
{
    width = width_;
    height = height_;

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &ubo,
        sizeof(IBLGenUBO));
    ubo.map();

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.baseMipLevel = mipmap;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = cubemap;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &view));
}

GenIBLCubeMipPass::~GenIBLCubeMipPass()
{
    ubo.destroy();
}

void GenIBLCubeMipPass::Draw(VkCommandBuffer cmd, vkglTF::Model& model)
{
    if (pipeline == VK_NULL_HANDLE)
    {
        return;
    }

    beginInfo.renderPass = renderpass;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.framebuffer = fbo;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
    
    model.draw(cmd);

    vkCmdEndRenderPass(cmd);
}

void GenIBLCubeMipPass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo.descriptor),
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cube->descriptor),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GenIBLCubeMipPass::PrepareRenderPass()
{
    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    const uint32_t viewMask = 0b00111111;
    const uint32_t correlationMask = 0b00111111;

    VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{};
    renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
    renderPassMultiviewCI.subpassCount = 1;
    renderPassMultiviewCI.pViewMasks = &viewMask;
    renderPassMultiviewCI.correlationMaskCount = 1;
    renderPassMultiviewCI.pCorrelationMasks = &correlationMask;

    renderPassCI.pNext = &renderPassMultiviewCI;

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderpass));
}

void GenIBLCubeMipPass::PreparePipeline()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

    FeeShader(shaderStages);

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
}

void GenIBLCubeMipPass::PrepareFrameBuffer()
{
    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &view;
    framebufferCI.width = width;
    framebufferCI.height = height;
    framebufferCI.layers = 1;

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &fbo));
}

void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);

    IBLGenUBO uboData = {};
    // POSITIVE_X
    uboData.mvp[0] = project * glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // NEGATIVE_X
    uboData.mvp[1] = project * glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // POSITIVE_Y
    uboData.mvp[2] = project * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // NEGATIVE_Y
    uboData.mvp[3] = project * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // POSITIVE_Z
    uboData.mvp[4] = project * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // NEGATIVE_Z
    uboData.mvp[5] = project * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    FeedUBO(uboData);

    memcpy(ubo.mapped, &uboData, sizeof(IBLGenUBO));
}

void GenIrradianceCubeMip::FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shaderStages)
{
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
}

void GenIrradianceCubeMip::FeedUBO(IBLGenUBO & uboData)
{
    uboData.deltaPhi = (2.0f * float(M_PI)) / 180.0f;
    uboData.deltaTheta = (0.5f * float(M_PI)) / 64.0f;
}

void GenPrefilterEnvMapMip::FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shaderStages)
{
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
}

void GenPrefilterEnvMapMip::FeedUBO(IBLGenUBO& uboData)
{
    uboData.roughness = roughness;
    uboData.numSamples = 64;
}

GenIBLPass::GenIBLPass(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width)
    : device(device_)
    , iLoader(example)
{
    const int32_t dim = static_cast<int32_t>(width);
    const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;

    {
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &irradianceImage));
        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device->logicalDevice, irradianceImage, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &irradianceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, irradianceImage, irradianceMemory, 0));

        viewCI.image = irradianceImage;
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &irradianceView));
    }

    {
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &prefilteredImage));
        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device->logicalDevice, prefilteredImage, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &prefilteredMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, prefilteredImage, prefilteredMemory, 0));

        viewCI.image = prefilteredImage;
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &prefilteredView));
    }


    irradiance.resize(numMips);
    prefiltered.resize(numMips);
    for (uint32_t i = 0; i < numMips; ++i)
    {
        auto w = static_cast<float>(dim * std::pow(0.5f, i));
        auto h = static_cast<float>(dim * std::pow(0.5f, i));

        irradiance[i].reset(new GenIrradianceCubeMip(device, example, irradianceImage, format, i, w, h));
        irradiance[i]->Prepare();

        float roughness = (float)i / (float)(numMips - 1);
        prefiltered[i].reset(new GenPrefilterEnvMapMip(device, example, prefilteredImage, format, i, w, h, roughness));
        prefiltered[i]->Prepare();
    }
}

GenIBLPass::~GenIBLPass()
{
    if (irradianceImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, irradianceImage, nullptr);
    }

    if (irradianceImage != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, irradianceView, nullptr);
    }

    if (irradianceMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, irradianceMemory, nullptr);
    }

    if (prefilteredImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, prefilteredImage, nullptr);
    }

    if (prefilteredMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, prefilteredMemory, nullptr);
    }

    if (prefilteredView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, prefilteredView, nullptr);
    }
}

void GenIBLPass::SetModel(const std::shared_ptr<vkglTF::Model>&model_)
{
    model = model_;
}

void GenIBLPass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    cubemap = cube;
    for (uint32_t i = 0; i < numMips; ++i)
    {
        irradiance[i]->SetCubeMap(cube);
        prefiltered[i]->SetCubeMap(cube);
    }
}

void GenIBLPass::Draw(VkCommandBuffer cmd)
{
    for (uint32_t i = 0; i < numMips; ++i)
    {
        irradiance[i]->Draw(cmd, *model);
        prefiltered[i]->Draw(cmd, *model);
    }
}

void GenIBLPass::Generate(VkQueue queue)
{
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Draw(cmdBuf);

    device->flushCommandBuffer(cmdBuf, queue);
}

void GenIBLPass::FeedIrradianceMap(VkDescriptorImageInfo& descriptor)
{
    descriptor.imageView = irradianceView;
    descriptor.sampler = irradiance[0]->GetDefaultSampler();
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void GenIBLPass::FeedPrefilteredMap(VkDescriptorImageInfo& descriptor)
{
    descriptor.imageView = prefilteredView;
    descriptor.sampler = irradiance[0]->GetDefaultSampler();
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}