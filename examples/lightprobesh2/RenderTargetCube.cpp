#include "Pass.h"

std::shared_ptr<vks::TextureCubeMap> RenderTargetCube::GetTextureCubeMap() {
    if (!cubeMap) {
        cubeMap = std::make_shared<vks::TextureCubeMap>();
        cubeMap->image = GetImage();
        cubeMap->device = device;
        // 初始布局是 COLOR_ATTACHMENT_OPTIMAL，因为这是 RenderTarget
        cubeMap->imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        cubeMap->layerCount = 6;  // 立方体贴图总是6层
        cubeMap->width = GetWidth();
        cubeMap->height = GetHeight();
        cubeMap->mipLevels = 1;
        
        // 创建立方体贴图的视图
        VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
        viewCI.image = cubeMap->image;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        viewCI.format = GetFormat();
        viewCI.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCI.subresourceRange.baseMipLevel = 0;
        viewCI.subresourceRange.levelCount = 1;
        viewCI.subresourceRange.baseArrayLayer = 0;
        viewCI.subresourceRange.layerCount = 6;
        
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &cubeMap->view));
        
        // 创建采样器
        VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
        samplerCI.magFilter = VK_FILTER_LINEAR;
        samplerCI.minFilter = VK_FILTER_LINEAR;
        samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCI.minLod = 0.0f;
        samplerCI.maxLod = 1.0f;
        samplerCI.maxAnisotropy = 1.0f;
        
        VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCI, nullptr, &cubeMap->sampler));
        
        // 更新描述符信息
        cubeMap->descriptor.imageLayout = cubeMap->imageLayout;
        cubeMap->descriptor.imageView = cubeMap->view;
        cubeMap->descriptor.sampler = cubeMap->sampler;
    }
    return cubeMap;
}