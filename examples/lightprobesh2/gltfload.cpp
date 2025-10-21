#include "gltfload.h"
#include "VulkanDevice.h"

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
	model->draw(cmd);
}

void GltfModel::PreparePerBatchResource()
{
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
	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
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

	std::vector<VkDescriptorSetLayout> setLayotus = {
		passLayout,
		descriptorSetLayout
	};

	// Pipeline layout 初始化管线布局创建信息，指定描述符集布局。
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayotus.data(), static_cast<uint32_t>(setLayotus.size()));
	// 创建管线布局。
	VK_CHECK_RESULT(vkCreatePipelineLayout(rawDevice, &pipelineLayoutCreateInfo, nullptr, &techniques[(uint32_t)technique].pipelineLayout));
	// 定义两个着色器阶段（顶点和片段）。
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	// Pipelines
	// 初始化图形管线创建信息，指定管线布局和渲染通道。
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(techniques[(uint32_t)technique].pipelineLayout, renderPass);
	// 设置输入组装状态。
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	// 设置光栅化状态。
	pipelineCI.pRasterizationState = &rasterizationState;
	// 设置颜色混合状态。
	pipelineCI.pColorBlendState = &colorBlendState;
	// 设置深度和模板状态。
	pipelineCI.pMultisampleState = &multisampleState;
	// 设置视口状态。
	pipelineCI.pViewportState = &viewportState;
	// 设置深度和模板状态。
	pipelineCI.pDepthStencilState = &depthStencilState;
	// 设置动态状态。
	pipelineCI.pDynamicState = &dynamicState;
	// 设置着色器阶段数量（2）。
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	// 设置着色器阶段数组。
	pipelineCI.pStages = shaderStages.data();
	// 设置顶点输入状态，包括位置、法线和UV坐标。
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

	// Skybox pipeline (background cube)

	shaderStages[0] = iLoader->LoadShader("lightprobesh2/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = iLoader->LoadShader("lightprobesh2/lightprobesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
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