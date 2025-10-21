#pragma once

#include "VulkanGltfModel.h"
#include "VulkanUIOverlay.h"
#include "Pass.h"
#include "ILoader.h"
#include "glm/glm.hpp"

namespace vks {
	struct VulkanDevice;
}

class GltfModel {
public:
	explicit GltfModel(vks::VulkanDevice* dev, IExampleInterfasce* example);
	~GltfModel() = default;

	struct MaterialBuffer {
		float roughness = 1.f;
		float metallic = 0.5;
		float specular = 0.5;
		float padding = 0.f;
		glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);

		int32_t useSH = 1;
		int32_t useReflection = 0;
	};

	struct LocalBuffer {
		glm::mat4 transform;
	};

	void UpdateModel(const std::shared_ptr<vkglTF::Model>& model);
	void Destroy();
	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech);
	void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique);

	void ShowUI(vks::UIOverlay* overlay);
	void SetTransform(const glm::mat4& transform);
    std::shared_ptr<vkglTF::Model> getModel() const { return model; }

private:
	void PreparePerBatchResource();
	void UpdateSet();

	vks::VulkanDevice* device;
	IExampleInterfasce* iLoader;
	std::shared_ptr<vkglTF::Model> model;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	std::array<Technique, (uint32_t)ETechnique::NUM> techniques;

	LocalBuffer localData;
	vks::Buffer localBuffer;

	MaterialBuffer materialData;
	vks::Buffer materialBuffer;

	bool materialDirty = false;
};