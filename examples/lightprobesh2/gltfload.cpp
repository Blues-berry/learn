#include "gltfload.h"
#include "VulkanDevice.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cstring>
#include <algorithm>

GltfModel::GltfModel(vks::VulkanDevice* dev, IExampleInterfasce* example, VkQueue queue) 
	: device(dev), iLoader(example), copyQueue(queue)
{
	PreparePerBatchResource();
	UpdateSet();
}

GltfModel::~GltfModel()
{
	Destroy();
}

void GltfModel::UpdateModel(const std::shared_ptr<vkglTF::Model>& model_)
{
	model = model_;

	if (!model) {
		SetTransform(glm::mat4(1.0f));
		return;
	}

	const auto& dim = model->dimensions;
	const float maxExtent = std::max({ dim.size.x, dim.size.y, dim.size.z });
	if (maxExtent > 0.0f) {
		const float targetExtent = 12.0f;
		const float scale = targetExtent / maxExtent;
		const float baseOffsetY = (dim.center.y - dim.min.y) * scale;
		glm::mat4 transform = glm::mat4(1.0f);
		transform = glm::translate(transform, glm::vec3(0.0f, baseOffsetY, 0.0f));
		transform = glm::scale(transform, glm::vec3(scale));
		transform = glm::translate(transform, -dim.center);
		SetTransform(transform);
		std::cout << "[GltfModel] UpdateModel scale=" << scale
			<< " size=" << dim.size.x << "," << dim.size.y << "," << dim.size.z
			<< " center=" << dim.center.x << "," << dim.center.y << "," << dim.center.z
			<< std::endl;
	} else {
		SetTransform(glm::mat4(1.0f));
	}
}

void GltfModel::Destroy()
{
	localBuffer.destroy();
	materialBuffer.destroy();
	model = nullptr;
	
	// 清理纹理资源
	for (auto& image : images) {
		if (image.texture.view != VK_NULL_HANDLE) {
			vkDestroyImageView(device->logicalDevice, image.texture.view, nullptr);
		}
		if (image.texture.image != VK_NULL_HANDLE) {
			vkDestroyImage(device->logicalDevice, image.texture.image, nullptr);
		}
		if (image.texture.sampler != VK_NULL_HANDLE) {
			vkDestroySampler(device->logicalDevice, image.texture.sampler, nullptr);
		}
		if (image.texture.deviceMemory != VK_NULL_HANDLE) {
			vkFreeMemory(device->logicalDevice, image.texture.deviceMemory, nullptr);
		}
	}
	images.clear();
	textures.clear();
	materials.clear();
	
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
		descriptorPool = VK_NULL_HANDLE;
	}

	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
		descriptorSetLayout = VK_NULL_HANDLE;
	}

	for (auto& tech : techniques) {
		if (tech.pipelineLayout != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(device->logicalDevice, tech.pipelineLayout, nullptr);
			tech.pipelineLayout = VK_NULL_HANDLE;
		}

		if (tech.pso != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(device->logicalDevice, tech.pso, nullptr);
			tech.pso = VK_NULL_HANDLE;
		}
	}
}

void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech, VkPipeline pipelineOverride)
{
	if (!model)
	{
		return;
	}

	uint32_t techIdx = (uint32_t)tech;

	// 安全检查：确保 PSO 已经准备好
	if (techniques[techIdx].pso == VK_NULL_HANDLE || techniques[techIdx].pipelineLayout == VK_NULL_HANDLE)
	{
		std::cerr << "GltfModel::Draw - PSO not prepared for technique " << techIdx << "\n";
		return;
	}

	std::vector<VkDescriptorSet> sets = {
		globalSet, descriptorSet
	};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
	VkPipeline pipeline = (pipelineOverride != VK_NULL_HANDLE) ? pipelineOverride : techniques[techIdx].pso;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	struct PushConstantBlock {
		glm::mat4 modelOffset;
		glm::vec4 baseColor;
	} pc;

	auto applyMaterial = [&](const vkglTF::Material& mat) {
		materialData.albedo = mat.baseColorFactor;
		materialData.roughness = mat.roughnessFactor;
		materialData.metallic = mat.metallicFactor;
		materialData.useTexture = mat.baseColorTexture != nullptr ? 1 : 0;
		static int logCount = 0;
		if (logCount < 8) {
			std::cout << "[Material] albedo="
				<< materialData.albedo.r << ", "
				<< materialData.albedo.g << ", "
				<< materialData.albedo.b
				<< " useTexture=" << materialData.useTexture << std::endl;
			logCount++;
		}
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialUniform));
		if (mat.baseColorTexture) {
			VkDescriptorSet materialSet = mat.descriptorSet;
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pipelineLayout, 2, 1, &materialSet, 0, nullptr);
		}
	};

	model->bindBuffers(cmd);

	std::function<void(vkglTF::Node*)> drawNode = [&](vkglTF::Node* node) {
		if (!node) {
			return;
		}

		glm::mat4 nodeMatrix = node->getMatrix();
		if (node->mesh) {
			for (auto* primitive : node->mesh->primitives) {
				applyMaterial(primitive->material);
				pc.modelOffset = nodeMatrix;
				pc.baseColor = materialData.albedo;
				vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout,
							  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							  0, sizeof(PushConstantBlock), &pc);
				vkCmdDrawIndexed(cmd, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
			}
		}
		for (auto* child : node->children) {
			drawNode(child);
		}
	};

	for (auto* node : model->nodes) {
		drawNode(node);
	}
}

void GltfModel::PreparePerBatchResource()
{
	// 创建独立的描述符池，用于模型自己的资源
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

	// 创建独立的描述符集布局，用于模型自己的资源
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// 绑定 0: 局部变换矩阵（顶点和片段着色器）
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		// 绑定 1: 材质参数（片段着色器）
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	// 为模型分配描述符集
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

	device->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&localBuffer,
		sizeof(LocalBuffer));
	localBuffer.map();

	device->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&materialBuffer,
		sizeof(MaterialUniform));
	materialBuffer.map();
;
	localData.transform = glm::mat4(1.0f);
	if (localBuffer.mapped) {
		memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
	}
	if (materialBuffer.mapped) {
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialUniform));
	}
}

void GltfModel::UpdateSet()
{
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
	};

	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GltfModel::SetTransform(const glm::mat4& transform)
{
	localData.transform = transform;
	if (localBuffer.mapped) {
		memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
	}
}

void GltfModel::PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique)
{
	VkDevice rawDevice = device->logicalDevice;

	// 配置输入组装状态，使用三角形列表拓扑。
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	// 配置光栅化状态，填充模式，无背面剔除，逆时针为正面。
	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	// 配置颜色混合状态，禁用混合	
	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	// 配置颜色混合状态，指定一个附件。
	VkPipelineColorBlendStateCreateInfo colorBlendState =
		vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	// 配置深度和模板状态，初始禁用深度测试和写入。
	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	// 配置视口状态，指定一个视口和裁剪矩形。
	VkPipelineViewportStateCreateInfo viewportState =
		vks::initializers::pipelineViewportStateCreateInfo(1, 1);
	// 配置多重采样状态，禁用多重采样（单采样）。
	VkPipelineMultisampleStateCreateInfo multisampleState =
		vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
	// 定义动态状态，包括视口和裁剪矩形。
	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	// 配置动态状态。
	VkPipelineDynamicStateCreateInfo dynamicState =
		vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

	// 创建管线布局，包含两个描述符集布局：
	// 1. 来自 mainpass 的全局描述符集布局 (passLayout)
	// 2. 模型自己的描述符集布局 (descriptorSetLayout)
	// 这样可以确保两者不会产生绑定冲突
	std::vector<VkDescriptorSetLayout> setLayouts = {
		passLayout,        // 绑定点 0: 来自 mainpass 的全局数据
		descriptorSetLayout,  // 绑定点 1: 模型自己的数据
		vkglTF::descriptorSetLayoutImage // 绑定点 2: glTF 材质纹理
	};

	// 创建管线布局（Push Constant 仅携带额外模型变换矩阵）
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(glm::mat4);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(rawDevice, &pipelineLayoutCreateInfo, nullptr, &techniques[(uint32_t)technique].pipelineLayout));

	// 定义两个着色器阶段（顶点和片段）
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	// 创建图形管线
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(techniques[(uint32_t)technique].pipelineLayout, renderPass);
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

	// 加载着色器 - 根据技术类型选择不同的着色器
	if (technique == ETechnique::CAPTURE_SCENE) {
		// ✅ CAPTURE_SCENE: 使用 multiview 着色器
		shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	} else {
		// ✅ MAIN: 使用标准着色器
		shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &techniques[(uint32_t)technique].pso));
}

void GltfModel::ShowUI(vks::UIOverlay* overlay)
{
	if (overlay->header("Material")) {
		overlay->text("roughness: %.2f", materialData.roughness);
		overlay->text("metallic : %.2f", materialData.metallic);
		overlay->text("specular : %.2f", materialData.specular);
		overlay->text("albedo   : %.2f %.2f %.2f %.2f",
			materialData.albedo.r,
			materialData.albedo.g,
			materialData.albedo.b,
			materialData.albedo.a);
		overlay->text("useSH    : %s", materialData.useSH ? "Yes" : "No");
		overlay->text("useReflection: %s", materialData.useReflection ? "Yes" : "No");
		overlay->text("useTexture: %s", materialData.useTexture ? "Yes" : "No");
	}
}

// =============================================================================
// 纹理加载方法（参考gltfloading.cpp）
// =============================================================================

void GltfModel::loadImages(tinygltf::Model& input)
{
	images.resize(input.images.size());
	for (size_t i = 0; i < input.images.size(); i++) {
		tinygltf::Image& glTFImage = input.images[i];
		unsigned char* buffer = nullptr;
		VkDeviceSize bufferSize = 0;
		bool deleteBuffer = false;
		
		// 将RGB转换为RGBA
		if (glTFImage.component == 3) {
			bufferSize = glTFImage.width * glTFImage.height * 4;
			buffer = new unsigned char[bufferSize];
			unsigned char* rgba = buffer;
			unsigned char* rgb = &glTFImage.image[0];
			for (size_t j = 0; j < glTFImage.width * glTFImage.height; ++j) {
				memcpy(rgba, rgb, sizeof(unsigned char) * 3);
				rgba += 4;
				rgb += 3;
			}
			deleteBuffer = true;
		}
		else {
			buffer = &glTFImage.image[0];
			bufferSize = glTFImage.image.size();
		}
		
		// 加载纹理到Vulkan
		images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, 
		                             glTFImage.width, glTFImage.height, device, copyQueue);
		
		if (deleteBuffer) {
			delete[] buffer;
		}
	}
	
	std::cout << "[GltfModel::loadImages] Loaded " << images.size() << " images" << std::endl;
}

void GltfModel::loadTextures(tinygltf::Model& input)
{
	textures.resize(input.textures.size());
	for (size_t i = 0; i < input.textures.size(); i++) {
		textures[i].imageIndex = input.textures[i].source;
	}
	
	std::cout << "[GltfModel::loadTextures] Loaded " << textures.size() << " texture references" << std::endl;
}

void GltfModel::loadMaterials(tinygltf::Model& input)
{
	materials.resize(input.materials.size());
	for (size_t i = 0; i < input.materials.size(); i++) {
		tinygltf::Material glTFMaterial = input.materials[i];
		
		// 获取基础颜色因子
		if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
			materials[i].baseColorFactor = glm::make_vec4(
				glTFMaterial.values["baseColorFactor"].ColorFactor().data());
		}
		
		// 获取基础颜色纹理索引
		if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
			materials[i].baseColorTextureIndex = 
				glTFMaterial.values["baseColorTexture"].TextureIndex();
		}
		
		// 获取粗糙度
		if (glTFMaterial.values.find("roughnessFactor") != glTFMaterial.values.end()) {
			materials[i].roughness = 
				static_cast<float>(glTFMaterial.values["roughnessFactor"].Factor());
		}
		
		// 获取金属度
		if (glTFMaterial.values.find("metallicFactor") != glTFMaterial.values.end()) {
			materials[i].metallic = 
				static_cast<float>(glTFMaterial.values["metallicFactor"].Factor());
		}
	}
	
	std::cout << "[GltfModel::loadMaterials] Loaded " << materials.size() << " materials" << std::endl;
}

void GltfModel::LoadModelWithTextures(const std::string& filename, uint32_t fileLoadingFlags)
{
	std::cout << "[GltfModel::LoadModelWithTextures] Loading: " << filename << std::endl;
	
	// 使用tinygltf加载glTF文件以获取纹理数据
	tinygltf::TinyGLTF loader;
	std::string error, warning;
	
	bool fileLoaded = loader.LoadASCIIFromFile(&gltfInput, &error, &warning, filename);
	
	if (!fileLoaded) {
		std::cerr << "[GltfModel] Failed to load glTF file: " << filename << std::endl;
		if (!error.empty()) {
			std::cerr << "  Error: " << error << std::endl;
		}
		if (!warning.empty()) {
			std::cerr << "  Warning: " << warning << std::endl;
		}
		return;
	}
	
	// 加载纹理数据
	loadImages(gltfInput);
	loadTextures(gltfInput);
	loadMaterials(gltfInput);
	
	// 使用vkglTF加载几何数据
	auto vkModel = std::make_shared<vkglTF::Model>();
	vkModel->loadFromFile(filename, device, copyQueue, fileLoadingFlags);
	UpdateModel(vkModel);
	
	// 更新材质参数（使用第一个材质）
	if (!materials.empty()) {
		materialData.roughness = materials[0].roughness;
		materialData.metallic = materials[0].metallic;
		materialData.albedo = materials[0].baseColorFactor;
		materialData.useTexture = (materials[0].baseColorTextureIndex >= 0) ? 1 : 0;
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialUniform));
		
		std::cout << "[GltfModel] Material 0: roughness=" << materials[0].roughness 
		          << ", metallic=" << materials[0].metallic 
		          << ", hasTexture=" << (materials[0].baseColorTextureIndex >= 0 ? "yes" : "no") << std::endl;
	}
	
	std::cout << "[GltfModel::LoadModelWithTextures] ✓ Completed" << std::endl;
}