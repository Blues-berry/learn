#pragma once

#include "VulkanglTFModel.h"
#include "VulkanUIOverlay.h"
#include "Pass.h"
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

		int32_t useSH = 1;
		int32_t useReflection = 0;
	};

	struct LocalBuffer {
		glm::mat4 transform;
	};

	void UpdateModel(const std::shared_ptr<vkglTF::Model>& model);
	void Destroy();
	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech);
	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech, const glm::vec3& position);
	void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique);

	void ShowUI(vks::UIOverlay* overlay);
    std::shared_ptr<vkglTF::Model> getModel() const { return model; }
	// Allow enabling/disabling SH and reflection from external code
	void SetUseSHAndReflection(bool useSH, bool useReflection);

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