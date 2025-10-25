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
	// ✅ 不需要调用 UpdateSet()，因为 model 不影响 descriptor bindings
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

	// ✅ 安全检查：确保 PSO 已经准备好
	if (techniques[techIdx].pso == VK_NULL_HANDLE || techniques[techIdx].pipelineLayout == VK_NULL_HANDLE)
	{
		std::cerr << "GltfModel::Draw - PSO not prepared for technique " << techIdx << "\n";
		return;
	}

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

	// ============ 根据技术类型选择不同的绘制方式 ============
	if (tech == ETechnique::CAPTURE_SCENE) {
		// ✅ CAPTURE_SCENE 模式：绘制单个实例，位置与 MainPass 中第一个实例相同
		// 这样可以被 multiview 着色器正确处理，渲染到 6 个立方体面
		// gltfModel 应该在世界坐标系中的相同位置，而不是原点

		// 使用与 MainPass 中第一个实例相同的位置和缩放
		// const glm::vec3 captureOffset = glm::vec3(-20.0f, 0.0f, 0.0f);  // 与 MainPass 中第一个实例相同
		// const float captureScale = 50.0f;

		// pc.modelOffset = glm::translate(glm::mat4(1.0f), captureOffset) *
		// 				glm::scale(glm::mat4(1.0f), glm::vec3(captureScale));
		// pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // 白色，不着色

		// vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout,
		// 				  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		// 				  0, sizeof(PushConstantBlock), &pc);

		// // 使用 BindImages 标志以绑定纹理资源
		// model->draw(cmd, vkglTF::RenderFlags::BindImages,
		// 		   techniques[techIdx].pipelineLayout, 1);

		for (int i = 0; i < 4; ++i) {
			pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) *
							glm::scale(glm::mat4(1.0f), glm::vec3(scale));
			pc.tint = glm::vec4(colors[i % 3], 1.0f);

			vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout,
							  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							  0, sizeof(PushConstantBlock), &pc);

			model->draw(cmd);
		}
	}
	else {
		// ✅ MAIN 模式：绘制 4 个不同位置的模型实例
		for (int i = 0; i < 4; ++i) {
			pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) *
							glm::scale(glm::mat4(1.0f), glm::vec3(scale));
			pc.tint = glm::vec4(colors[i % 3], 1.0f);

			vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout,
							  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
							  0, sizeof(PushConstantBlock), &pc);

			model->draw(cmd);
		}
	}
}

void GltfModel::PreparePerBatchResource()
{
	// ✅ 修复：移除纹理绑定，着色器不使用 binding 2 的纹理数组
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
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
		sizeof(MaterialBuffer));
	materialBuffer.map();
;
	localData.transform = glm::mat4();
	memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
	memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
}

void GltfModel::UpdateSet()
{
	// ✅ 修复：只绑定 uniform buffers，着色器不使用纹理数组
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
	};
	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GltfModel::SetTransform(const glm::mat4& transform)
{
	localData.transform = transform;
	memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
}

void GltfModel::SetUseSHAndReflection(bool useSH, bool useReflection)
{
	materialData.useSH = useSH ? 1 : 0;
	materialData.useReflection = useReflection ? 1 : 0;
	// Immediately write to GPU buffer
	if (materialBuffer.mapped) {
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
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

	// 加载着色器 - 根据技术类型选择不同的着色器
	if (technique == ETechnique::CAPTURE_SCENE) {
		// ✅ CAPTURE_SCENE: 使用 multiview 着色器
		shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	} else {
		// ✅ MAIN: 使用标准着색器
		shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	}
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &techniques[(uint32_t)technique].pso));
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