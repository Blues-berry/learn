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
		// ✅ 修复1: 调整默认材质参数，使模型初始可见
		float roughness = 0.5f;     // 从1.0改为0.5，减少粗糙度
		float metallic = 0.5;
		float specular = 0.5;
		float padding = 0.f;
		glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);

		int32_t useSH = 0;          // 从1改为0，初始不使用SH（SH还没生成）
		int32_t useReflection = 0;  // 初始不使用反射
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