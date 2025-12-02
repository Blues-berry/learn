/*
* Vulkan texture loader
*
* Copyright(C) by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license(MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <fstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include "vulkan/vulkan.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanTools.h"

#if defined(__ANDROID__)
#	include <android/asset_manager.h>
#endif

namespace vks
{
class Texture
{
  public:
	vks::VulkanDevice *   device;
	VkImage               image;
	VkImageLayout         imageLayout;
	VkDeviceMemory        deviceMemory;
	VkImageView           view;
		VkFormat format;
	uint32_t              width, height;
	uint32_t              mipLevels;
	uint32_t              layerCount;
	VkDescriptorImageInfo descriptor;
	VkSampler             sampler;

	void      updateDescriptor();
	void      destroy();
	ktxResult loadKTXFile(std::string filename, ktxTexture **target);
};

class Texture2D : public Texture
{
  public:
		Texture2D() = default;
		Texture2D(vks::VulkanDevice* device, VkFormat format, VkImageUsageFlags imageUsageFlags, uint32_t width, uint32_t height, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL);

	void loadFromFile(
	    std::string        filename,
	    VkFormat           format,
	    vks::VulkanDevice *device,
	    VkQueue            copyQueue,
	    VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
	    VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	    bool               forceLinear     = false);
	void fromBuffer(
	    void *             buffer,
	    VkDeviceSize       bufferSize,
	    VkFormat           format,
	    uint32_t           texWidth,
	    uint32_t           texHeight,
	    vks::VulkanDevice *device,
	    VkQueue            copyQueue,
	    VkFilter           filter          = VK_FILTER_LINEAR,
	    VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
	    VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class Texture2DArray : public Texture
	{
	public:
		Texture2DArray() = default;
		Texture2DArray(vks::VulkanDevice* device, VkFormat format, VkImageUsageFlags imageUsageFlags, uint32_t width, uint32_t height, uint32_t layers, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, VkImageLayout imageLayout = VK_IMAGE_LAYOUT_GENERAL);
	void loadFromFile(
	    std::string        filename,
	    VkFormat           format,
	    vks::VulkanDevice *device,
	    VkQueue            copyQueue,
	    VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
	    VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};

class TextureCubeMap : public Texture
{
  public:
	void loadFromFile(
	    std::string        filename,
	    VkFormat           format,
	    vks::VulkanDevice *device,
	    VkQueue            copyQueue,
	    VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
	    VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};
}        // namespace vks
