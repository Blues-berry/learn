#pragma once

#include "PlyParser.h"
#include "glm/glm.hpp"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "ILoader.h"

struct GaussianSplattingVertex {
	glm::vec3 position;
};

struct GaussianSplattingVertexInfo {
	glm::vec4 scale;  // 0-2: scale 3: opacity
	glm::vec4 sh[16];
	glm::mat4 rotation;
};


class GaussianSplattingElement {
public:
	explicit GaussianSplattingElement(vks::VulkanDevice* device_, IExampleInterfasce* example) : device(device_), iLoader(example) {}
	~GaussianSplattingElement() = default;

	void SetVetexCount(uint32_t count);
	void SetPosition(const vks::Buffer& src, VkDeviceSize size, VkCommandBuffer cmd);
	void SetExtra(const vks::Buffer& src, VkDeviceSize size, VkCommandBuffer cmd);

	void Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, VkDescriptorSet globalSet);
private:
	vks::VulkanDevice* device;
	IExampleInterfasce* iLoader;

	vks::Buffer position;
	vks::Buffer gsExtra;
	uint32_t vertexCount = 0;
};

struct GaussianElementData {
	uint32_t vertexCount;
	std::vector<GaussianSplattingVertex> position;
	std::vector<GaussianSplattingVertexInfo> extra;
};

struct GaussianSplattingCacheFile {
	bool LoadFromFile(const std::string& path);

	std::vector<GaussianElementData> elementData;
};

class GaussianSplattingItem {
public:
	GaussianSplattingItem(vks::VulkanDevice* device_, IExampleInterfasce* example);
	~GaussianSplattingItem();

	void SetData(const std::shared_ptr<PlyObject>& object, VkQueue queue, const std::string& cacheFile);
	void LoadFromCache(std::shared_ptr<GaussianSplattingCacheFile>& cache, VkQueue queue);

	void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout);

	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet);
private:
	void PreparePerBatchResource();

	vks::VulkanDevice* device;
	IExampleInterfasce* iLoader;
	
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline pso = VK_NULL_HANDLE;

	std::vector<std::unique_ptr<GaussianSplattingElement>> elements;
};