#include "gltfload.h"
#include "VulkanDevice.h"
#include <glm/gtc/matrix_transform.hpp>

GltfModel::GltfModel(vks::VulkanDevice* dev, IExampleInterfasce* example) : device(dev), iLoader(example)
{
	PreparePerBatchResource();
	UpdateSet();
}

void GltfModel::UpdateModel(const std::shared_ptr<vkglTF::Model>& model_)
{
	model = model_;
}

void GltfModel::Destroy()
{
	localBuffer.destroy();
	materialBuffer.destroy();
	model = nullptr;
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

void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
	if (!model)
	{
		return;
	}

	uint32_t techIdx = (uint32_t)tech;
	std::vector<VkDescriptorSet> sets = {
		globalSet, descriptorSet
	};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pso);

	struct PushConstantBlock {
		glm::mat4 modelOffset;
		glm::vec4 tint;
	} pc;

	const glm::vec3 offsets[4] = {
		glm::vec3(-20.0f, 0.0f, 0.0f),  // 左
		glm::vec3(20.0f,  0.0f, 0.0f),  // 右
		glm::vec3(0.0f,   0.0f, -20.0f), // 后
		glm::vec3(0.0f,   0.0f, 20.0f)  // 前
	};
	const float scale = 50.0f;
	const glm::vec3 colors[3] = {
		glm::vec3(1.0f, 0.3f, 0.3f),
		glm::vec3(0.3f, 1.0f, 0.3f),
		glm::vec3(0.3f, 0.5f, 1.0f)
	};

	// ✅ 修复: 绑定顶点和索引缓冲
	VkDeviceSize vertexOffsets[1] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &model->vertices.buffer, vertexOffsets);
	vkCmdBindIndexBuffer(cmd, model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);

	// ✅ 修复2: 对于MAIN技术，使用localData.transform而不是push constant中的大偏移
	// ✅ 修复3: CAPTURE_SCENE中不应用偏移，直接在原点绘制
	if (tech == ETechnique::CAPTURE_SCENE) {
		// 捕获场景时，不应用偏移和缩放，直接在原点绘制
		pc.modelOffset = glm::mat4(1.0f);
		pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlock), &pc);
		// ✅ 修复: 传递pipelineLayout和bindImageSet参数
		model->draw(cmd, vkglTF::RenderFlags::BindImages, techniques[techIdx].pipelineLayout, 1);
	}
	else {
		// MAIN技术：使用SetTransform()设置的localData.transform，不应用push constant偏移
		// 这样模型就不会跟随视角移动
		pc.modelOffset = glm::mat4(1.0f);  // ✅ 不应用push constant偏移
		pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlock), &pc);
		// ✅ 修复: 传递pipelineLayout和bindImageSet参数
		model->draw(cmd, vkglTF::RenderFlags::BindImages, techniques[techIdx].pipelineLayout, 1);
	}
}

void GltfModel::PreparePerBatchResource()
{
	// 创建独立的描述符池，用于模型自己的资源
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 15 },
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

	// 创建独立的描述符集布局，用于模型自己的资源
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// 绑定 0: 局部变换矩阵（顶点和片段着色器）
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		// 绑定 1: 材质参数（片段着色器）
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		// 绑定 2: 模型纹理（片段着色器）
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
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
		sizeof(MaterialBuffer));
	materialBuffer.map();
;
	localData.transform = glm::mat4();
	memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
	memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
}

void GltfModel::UpdateSet()
{
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
	};

    // // 添加纹理绑定（假设model有textures数组）
    // std::vector<VkDescriptorImageInfo> imageInfos(15); // 默认填充dummy纹理
    // for (size_t i = 0; i < model->textures.size() && i < 15; ++i) {
    //     imageInfos[i] = { model->textures[i].sampler, model->textures[i].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    // }
    // VkWriteDescriptorSet textureWrite = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, nullptr, 15);
    // textureWrite.pImageInfo = imageInfos.data();
    // writeDescriptorSets.push_back(textureWrite);
	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GltfModel::SetTransform(const glm::mat4& transform)
{
	localData.transform = transform;
	memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
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
		descriptorSetLayout  // 绑定点 1: 模型自己的数据
	};

	// 创建管线布局（加入 Push Constant 范围：mat4 + vec4）
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);

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

	// ✅ 修复4: 根据技术类型选择不同的着色器
	// MAIN技术使用普通着色器，CAPTURE_SCENE使用multiview着色器
	if (technique == ETechnique::MAIN) {
		shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	else {
		// CAPTURE_SCENE使用multiview着色器，支持同时渲染到6个cubemap面
		shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &techniques[(uint32_t)technique].pso));
}

void GltfModel::SetUseSHAndReflection(bool useSH, bool useReflection) {
    materialData.useSH = useSH ? 1 : 0;
    materialData.useReflection = useReflection ? 1 : 0;
    memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
}

void GltfModel::ShowUI(vks::UIOverlay* overlay)
{
	if (overlay->header("Material")) {
		materialDirty |= overlay->inputFloat("roughness", &materialData.roughness, 0.1f, 2);
		materialDirty |= overlay->inputFloat("metallic", &materialData.metallic, 0.1f, 2);
		materialDirty |= overlay->inputFloat("specular", &materialData.specular, 0.1f, 2);
		materialDirty |= overlay->colorPicker("elbedo", &materialData.elbedo.r);
		materialDirty |= overlay->checkBox("UseSH", &materialData.useSH);
		materialDirty |= overlay->checkBox("UseReflection", &materialData.useReflection);
	}

	if (materialDirty)
	{
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
		materialDirty = false;
	}
}