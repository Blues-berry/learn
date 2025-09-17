#include "LightProbe.h"
#include "Pass.h"
// 使用现有的 CubeMap
void LightProbe::SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_)
{
    cubemap = cubemap_;
}
void LightProbe::prepare()
{

    
}

// 使用探针位置抓取
/*
过程涉及两个主要阶段：捕获低res cubemap（类似于之前的解释），然后使用图像blit操作或计算着色器进行上采样插值。
推荐使用vkCmdBlitImage进行线性插值，因为它内置支持缩放和滤波
*/
void LightProbe::CaptureCubeMap(VkFormat format, VkQueue queue)
{

    // --- 创建低分辨率立方体贴图（128x128） ---
    std::shared_ptr<vks::TextureCubeMap> lowResCubemap = std::make_shared<vks::TextureCubeMap>();

    VkImageCreateInfo lowResImageInfo = vks::initializers::imageCreateInfo();
    lowResImageInfo.imageType = VK_IMAGE_TYPE_2D;
    lowResImageInfo.format = format;
    lowResImageInfo.extent.width = lowReswidth;
    lowResImageInfo.extent.height = lowResheight;
    lowResImageInfo.extent.depth = 1;
    lowResImageInfo.mipLevels = 1;
    lowResImageInfo.arrayLayers = 6;
    lowResImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    lowResImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    lowResImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    lowResImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VkResult result = vkCreateImage(device->logicalDevice, &lowResImageInfo, nullptr, &lowResCubemap->image);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create low-res cubemap image");
    }

    // 分配低分辨率cubemap内存
    VkMemoryRequirements lowmemReqs;
    vkGetImageMemoryRequirements(device->logicalDevice, lowResCubemap->image, &lowmemReqs);
    VkMemoryAllocateInfo lowmemAllocInfo = vks::initializers::memoryAllocateInfo();
    lowmemAllocInfo.allocationSize = lowmemReqs.size;
    lowmemAllocInfo.memoryTypeIndex = device->getMemoryType(lowmemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    result = vkAllocateMemory(device->logicalDevice, &lowmemAllocInfo, nullptr, &lowResCubemap->deviceMemory);
    if (result != VK_SUCCESS) {
        vkDestroyImage(device->logicalDevice, lowResCubemap->image, nullptr);
        throw std::runtime_error("Failed to allocate memory for low-res cubemap");
    }
    vkBindImageMemory(device->logicalDevice, lowResCubemap->image, lowResCubemap->deviceMemory, 0);

    // 创建低分辨率图像视图
    VkImageViewCreateInfo lowResViewInfo = vks::initializers::imageViewCreateInfo();
    lowResViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    lowResViewInfo.format = format;
    lowResViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    lowResViewInfo.image = lowResCubemap->image;
    result = vkCreateImageView(device->logicalDevice, &lowResViewInfo, nullptr, &lowResCubemap->view);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device->logicalDevice, lowResCubemap->deviceMemory, nullptr);
        vkDestroyImage(device->logicalDevice, lowResCubemap->image, nullptr);
        throw std::runtime_error("Failed to create low-res image view");
    }
    if (!device || !device->logicalDevice || !queue) {
        throw std::runtime_error("Invalid device or queue pointer");
    }
    
    // 创建高分辨率立方体贴图
    std::shared_ptr<vks::TextureCubeMap> highResCubemap = std::make_shared<vks::TextureCubeMap>();
    
    // 手动创建高分辨率立方体贴图资源
    VkImageCreateInfo highResImageInfo = vks::initializers::imageCreateInfo();
    highResImageInfo.imageType = VK_IMAGE_TYPE_2D;
    highResImageInfo.format = format;
    highResImageInfo.extent.width = lowReswidth;
    highResImageInfo.extent.height = lowResheight;
    highResImageInfo.extent.depth = 1;
    highResImageInfo.mipLevels = 1;
    highResImageInfo.arrayLayers = 6; // 立方体贴图有6个面
    highResImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    highResImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    highResImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    highResImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    // 分配高分辨率立方体贴图内存
    VkMemoryRequirements highmemReqs;
    vkGetImageMemoryRequirements(device->logicalDevice, highResCubemap->image, &highmemReqs);
    
    VkMemoryAllocateInfo highmemAllocInfo = vks::initializers::memoryAllocateInfo();
    highmemAllocInfo.allocationSize = highmemReqs.size;
    highmemAllocInfo.memoryTypeIndex = device->getMemoryType(highmemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    result = vkAllocateMemory(device->logicalDevice, &highmemAllocInfo, nullptr, &highResCubemap->deviceMemory);
    if (result != VK_SUCCESS) {
        vkDestroyImage(device->logicalDevice, highResCubemap->image, nullptr);
        throw std::runtime_error("Failed to allocate memory for cubemap");
    }
    
    result = vkBindImageMemory(device->logicalDevice, highResCubemap->image, highResCubemap->deviceMemory, 0);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device->logicalDevice, highResCubemap->deviceMemory, nullptr);
        vkDestroyImage(device->logicalDevice, highResCubemap->image, nullptr);
        throw std::runtime_error("Failed to bind image memory");
    }
    
    // 创建高分辨率图像视图
    VkImageViewCreateInfo highResViewInfo = vks::initializers::imageViewCreateInfo();
    highResViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    highResViewInfo.format = format;
    highResViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    highResViewInfo.image = highResCubemap->image;
    result = vkCreateImageView(device->logicalDevice, &highResViewInfo, nullptr, &highResCubemap->view);
    if (result != VK_SUCCESS) {
        vkFreeMemory(device->logicalDevice, highResCubemap->deviceMemory, nullptr);
        vkDestroyImage(device->logicalDevice, highResCubemap->image, nullptr);
        throw std::runtime_error("Failed to create image view");
    }
    
    // 设置立方体贴图的采样器
    VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    result = vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &highResCubemap->sampler);
    if (result != VK_SUCCESS) {
        vkDestroyImageView(device->logicalDevice, highResCubemap->view, nullptr);
        vkFreeMemory(device->logicalDevice, highResCubemap->deviceMemory, nullptr);
        vkDestroyImage(device->logicalDevice, highResCubemap->image, nullptr);
        throw std::runtime_error("Failed to create sampler");
    }
    // --- 创建渲染通行证用于捕获低分辨率cubemap ---
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkRenderPass renderPass;
    result = vkCreateRenderPass(device->logicalDevice, &renderPassInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) {
        // 清理已创建资源
        vkDestroyImageView(device->logicalDevice, lowResCubemap->view, nullptr);
        vkFreeMemory(device->logicalDevice, lowResCubemap->deviceMemory, nullptr);
        vkDestroyImage(device->logicalDevice, lowResCubemap->image, nullptr);
        vkDestroySampler(device->logicalDevice, cubemap->sampler, nullptr);
        vkDestroyImageView(device->logicalDevice, cubemap->view, nullptr);
        vkFreeMemory(device->logicalDevice, cubemap->deviceMemory, nullptr);
        vkDestroyImage(device->logicalDevice, cubemap->image, nullptr);
        throw std::runtime_error("Failed to create render pass");
    }
    // --- 为每个立方体面创建帧缓冲 ---
    std::array<VkFramebuffer, 6> framebuffers;
    for (uint32_t i = 0; i < 6; ++i) {
        VkImageViewCreateInfo faceViewInfo = vks::initializers::imageViewCreateInfo();
        faceViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        faceViewInfo.format = format;
        faceViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, i, 1 };
        faceViewInfo.image = lowResCubemap->image;
        VkImageView faceView;
        result = vkCreateImageView(device->logicalDevice, &faceViewInfo, nullptr, &faceView);
        if (result != VK_SUCCESS) {
            vkDestroyRenderPass(device->logicalDevice, renderPass, nullptr);
            // 清理其他资源...
            throw std::runtime_error("Failed to create face image view");
        }

        VkFramebufferCreateInfo fbInfo = vks::initializers::framebufferCreateInfo();
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &faceView;
        fbInfo.width = lowReswidth;
        fbInfo.height = lowResheight;
        fbInfo.layers = 1;
        result = vkCreateFramebuffer(device->logicalDevice, &fbInfo, nullptr, &framebuffers[i]);
        vkDestroyImageView(device->logicalDevice, faceView, nullptr); // 清理临时视图
        if (result != VK_SUCCESS) {
            // 清理已创建的帧缓冲和其他资源...
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    // --- 渲染循环：捕获低分辨率cubemap的6个面 ---
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // 设置探针位置的视图矩阵（假设probePosition是LightProbe的成员变量）
    std::array<glm::mat4, 6> viewMatrices = {
        // +X, -X, +Y, -Y, +Z, -Z
        glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
        glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))
    };
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

    // 假设已有渲染管线（pipeline）和描述符集（descriptorSet）用于场景渲染
    // 更新统一缓冲区（UBO）中的视图/投影矩阵
    //UpdateGlobal(ubo);
    for (uint32_t face = 0; face < 6; ++face) {
        // 更新UBO（假设UBO已创建，包含view和projection矩阵）
        struct UBO {
            glm::mat4 view;
            glm::mat4 projection;
        } ubo;
        ubo.view = viewMatrices[face];
        ubo.projection = projection;
        // 假设updateUBO是LightProbe的成员函数，更新管线绑定的UBO
        //updateUBO(ubo);

        VkRenderPassBeginInfo rpBeginInfo = vks::initializers::renderPassBeginInfo();
        rpBeginInfo.renderPass = renderPass;
        rpBeginInfo.framebuffer = framebuffers[face];
        rpBeginInfo.renderArea.extent.width = lowReswidth;
        rpBeginInfo.renderArea.extent.height = lowResheight;
        VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
        rpBeginInfo.clearValueCount = 1;
        rpBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        // vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); // 假设pipeline已创建
        // vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        // // 绘制场景（假设drawScene是LightProbe的成员函数）
        // drawScene(cmdBuf);
        // vkCmdEndRenderPass(cmdBuf);
    }

    // --- 插值：从低分辨率到高分辨率 ---
    VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };

    // 低分辨率cubemap：COLOR_ATTACHMENT_OPTIMAL -> TRANSFER_SRC_OPTIMAL
    vks::tools::setImageLayout(
        cmdBuf, lowResCubemap->image,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        subresourceRange);

    // 高分辨率cubemap：UNDEFINED -> TRANSFER_DST_OPTIMAL
    vks::tools::setImageLayout(
        cmdBuf, highResCubemap->image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        subresourceRange);

    // Blit逐层上采样
    for (uint32_t layer = 0; layer < 6; ++layer) {
        VkImageBlit blitRegion = {};
        blitRegion.srcOffsets[0] = { 0, 0, 0 };
        blitRegion.srcOffsets[1] = { (int32_t)lowReswidth, (int32_t)lowResheight, 1 };
        blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.srcSubresource.mipLevel = 0;
        blitRegion.srcSubresource.baseArrayLayer = layer;
        blitRegion.srcSubresource.layerCount = 1;
        blitRegion.dstOffsets[0] = { 0, 0, 0 };
        blitRegion.dstOffsets[1] = { (int32_t)highReswidth, (int32_t)highReswidth, 1 };
        blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blitRegion.dstSubresource.mipLevel = 0;
        blitRegion.dstSubresource.baseArrayLayer = layer;
        blitRegion.dstSubresource.layerCount = 1;

        vkCmdBlitImage(
            cmdBuf, lowResCubemap->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            highResCubemap->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blitRegion, VK_FILTER_LINEAR);
    }

    // 高分辨率cubemap：TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    vks::tools::setImageLayout(
        cmdBuf, highResCubemap->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        subresourceRange);

    // --- 提交命令缓冲并清理 ---
    device->flushCommandBuffer(cmdBuf, queue);

    // 清理低分辨率cubemap和帧缓冲
    for (auto& fb : framebuffers) {
        vkDestroyFramebuffer(device->logicalDevice, fb, nullptr);
    }
    vkDestroyRenderPass(device->logicalDevice, renderPass, nullptr);
    vkDestroyImageView(device->logicalDevice, lowResCubemap->view, nullptr);
    vkFreeMemory(device->logicalDevice, lowResCubemap->deviceMemory, nullptr);
    vkDestroyImage(device->logicalDevice, lowResCubemap->image, nullptr);

}

void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue)
{



}
