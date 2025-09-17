#include "LightProbe.h"

// 使用现有的 CubeMap
void LightProbe::SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_)
{
    cubemap = cubemap_;
}

// 使用探针位置抓取
void LightProbe::CaptureCubeMap(VkFormat format, VkQueue queue)
{
    // 创建一个新的立方体贴图
    cubemap = std::make_shared<vks::TextureCubeMap>();
    
    // 手动创建立方体贴图资源
    VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format = format;
    imageCreateInfo.extent.width = width;
    imageCreateInfo.extent.height = height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = 1;
    imageCreateInfo.arrayLayers = 6; // 立方体贴图有6个面
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &cubemap->image));
    
    // 分配内存
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device->logicalDevice, cubemap->image, &memReqs);
    
    VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &cubemap->deviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, cubemap->image, cubemap->deviceMemory, 0));
    
    // 创建图像视图
    VkImageViewCreateInfo viewCreateInfo = vks::initializers::imageViewCreateInfo();
    viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCreateInfo.format = format;
    viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    viewCreateInfo.image = cubemap->image;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCreateInfo, nullptr, &cubemap->view));
    
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
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &cubemap->sampler));
    
    // 为每个立方体面创建帧缓冲和渲染通道
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // 这里应该有一个渲染循环，为立方体的六个面分别渲染场景
    // 使用不同的视图矩阵，每个视图矩阵对应立方体的一个面
    // 这需要一个特殊的渲染通道和帧缓冲设置
    
    // 设置图像布局
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = 6;
    
    vks::tools::setImageLayout(
        cmdBuf,
        cubemap->image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        subresourceRange);
    
    // 提交命令缓冲并等待完成
    device->flushCommandBuffer(cmdBuf, queue);
}

void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue)
{



}
