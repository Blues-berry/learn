#include "PreviewModel.h"
#include "VulkanDevice.h"

/**
 * @brief PreviewModel类的构造函数
 * @param dev 指向VulkanDevice设备的指针，用于管理Vulkan设备资源
 * @param example 指向IExampleInterface接口的指针，用于提供示例接口功能
 */
PreviewModel::PreviewModel(vks::VulkanDevice* dev, IExampleInterfasce* example) : device(dev), iLoader(example) // 使用初始化列表设置设备指针和接口指针
{
	PreparePerBatchResource();
	UpdateSet();
}

void PreviewModel::UpdateModel(const std::shared_ptr<vkglTF::Model>& model_)
{
	model = model_;
}

/**
 * @brief PreviewModel类的析构函数，用于释放和清理模型相关的资源
 * 该函数会销毁所有与模型相关的Vulkan对象，包括缓冲区、描述符池、管线等
 */
void PreviewModel::Destroy()
{
    // 销毁局部缓冲区和材质缓冲区
	localBuffer.destroy();
	materialBuffer.destroy();
    // 将模型指针置空
	model = nullptr;

    // 检查并销毁描述符池（如果存在）
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
		descriptorPool = VK_NULL_HANDLE;  // 将句柄置空，避免悬空指针
	}

    // 检查并销毁描述符集布局（如果存在）
	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
		descriptorSetLayout = VK_NULL_HANDLE;  // 将句柄置空，避免悬空指针
	}

    // 检查并销毁管线布局（如果存在）
	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;  // 将句柄置空，避免悬空指针
	}

    // 检查并销毁图形管线（如果存在）
	if (pso != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device->logicalDevice, pso, nullptr);
		pso = VK_NULL_HANDLE;  // 将句柄置空，避免悬空指针
	}
}

void PreviewModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet)
{
	if (!model)
	{
		return;
	}

	std::vector<VkDescriptorSet> sets = {
		globalSet, descriptorSet
	};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
	model->draw(cmd);
}

void PreviewModel::PreparePerBatchResource()
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

/**
 * @brief 更新描述符集
 * 该函数用于更新描述符集，将本地缓冲区和材质缓冲区的绑定信息更新到描述符集中
 */
void PreviewModel::UpdateSet()
{
    // 创建描述符集写入操作数组，包含两个写入操作
    // 第一个写入操作：绑定本地缓冲区到描述符集的0号槽位
    // 第二个写入操作：绑定材质缓冲区到描述符集的1号槽位
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
	};
    // 调用Vulkan API更新描述符集
    // 参数：逻辑设备、写入操作数量、写入操作数组指针、无需设置操作、无需设置操作参数
	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void PreviewModel::PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout)
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
		vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
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
	VK_CHECK_RESULT(vkCreatePipelineLayout(rawDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	// 定义两个着色器阶段（顶点和片段）。
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	// Pipelines
	// 初始化图形管线创建信息，指定管线布局和渲染通道。
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
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
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pso));
}

void PreviewModel::ShowUI(vks::UIOverlay* overlay)
{
	if (overlay->header("Material")) {
		materialDirty |= overlay->inputFloat("roughness", &materialData.roughness, 0.1f, 2);
		materialDirty |= overlay->inputFloat("metallic", &materialData.metallic, 0.1f, 2);
		materialDirty |= overlay->inputFloat("specular", &materialData.specular, 0.1f, 2);
		materialDirty |= overlay->colorPicker("elbedo", &materialData.elbedo.r);
	}

	if (materialDirty)
	{
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
		materialDirty = false;
	}
}