#pragma once

#include "VulkanglTFModel.h"
#include "VulkanUIOverlay.h"
#include "ILoader.h"
#include "glm/glm.hpp"

namespace vks {
	struct VulkanDevice;
}

class PreviewModel {
public:
	explicit PreviewModel(vks::VulkanDevice* dev, IExampleInterfasce* example);
	~PreviewModel() = default;

	struct MaterialBuffer {
		float roughness = 1.f;
		float metallic = 0.5;
		float specular = 0.5;
		float padding = 0.f;
		glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);
	};

	struct LocalBuffer {
		glm::mat4 transform;
	};

	void UpdateModel(const std::shared_ptr<vkglTF::Model>& model);
	void Destroy();
	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet);
	void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout);

	void ShowUI(vks::UIOverlay* overlay);

private:
	void PreparePerBatchResource();
	void UpdateSet();

	vks::VulkanDevice* device;
	IExampleInterfasce* iLoader;
	std::shared_ptr<vkglTF::Model> model;

	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	LocalBuffer localData;
	vks::Buffer localBuffer;

	MaterialBuffer materialData;
	vks::Buffer materialBuffer;

	VkPipeline pso = VK_NULL_HANDLE;

	bool materialDirty = false;
};