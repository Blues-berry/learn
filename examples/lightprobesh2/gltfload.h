#pragma once

#include "VulkanGltfModel.h"
#include "VulkanUIOverlay.h"
#include "Pass.h"
#include "ILoader.h"
#include "VulkanTexture.h"
#include "glm/glm.hpp"
#include "tiny_gltf.h"
#include <vector>

namespace vks {
	struct VulkanDevice;
}

class GltfModel {
public:
	explicit GltfModel(vks::VulkanDevice* dev, IExampleInterfasce* example, VkQueue copyQueue);
	~GltfModel();

	// ✅ 纹理相关结构（参考gltfloading.cpp）
	struct Image {
		vks::Texture2D texture;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	};

	struct Texture {
		int32_t imageIndex = -1;
	};

	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		int32_t baseColorTextureIndex = -1;
		float roughness = 1.0f;
		float metallic = 0.5f;
		vkglTF::Texture* baseColorTexture = nullptr; // 指向 vkglTF 模型中的纹理
	};

	struct MaterialBuffer {
		float roughness = 1.f;
		float metallic = 0.5;
		float specular = 0.5;
		float padding = 0.f;
		glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);

		int32_t useSH = 1;
		int32_t useReflection = 0;
		int32_t useTexture = 0;  // 是否使用纹理
		int32_t padding2 = 0;
	};

	struct LocalBuffer {
		glm::mat4 transform;
	};

	void UpdateModel(const std::shared_ptr<vkglTF::Model>& model);
	void LoadModelWithTextures(const std::string& filename, uint32_t fileLoadingFlags);  // 加载带纹理的模型
	void Destroy();
	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech);
	void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique);

	void ShowUI(vks::UIOverlay* overlay);
	void SetTransform(const glm::mat4& transform);
    std::shared_ptr<vkglTF::Model> getModel() const { return model; }
    
    // 纹理加载相关方法
    const std::vector<Image>& GetImages() const { return images; }
    const std::vector<Material>& GetMaterials() const { return materials; }

private:
	void PreparePerBatchResource();
	void UpdateSet();
	void RefreshMaterialDataFromModel();
	
	// 纹理加载方法（参考gltfloading.cpp）
	void loadImages(tinygltf::Model& input);
	void loadTextures(tinygltf::Model& input);
	void loadMaterials(tinygltf::Model& input);

	vks::VulkanDevice* device;
	IExampleInterfasce* iLoader;
	VkQueue copyQueue;
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
	uint32_t imageSetIndex = 1; // Which descriptor set index vkglTF textures are bound to
	
	// ✅ 纹理数据
	std::vector<Image> images;
	std::vector<Texture> textures;
	std::vector<Material> materials;
	tinygltf::Model gltfInput;  // ✅ 保存原始glTF数据以访问纹理
};