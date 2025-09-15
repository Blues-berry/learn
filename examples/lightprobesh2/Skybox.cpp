#include "Skybox.h"

/**
 * @brief Skybox类的构造函数
 * @param dev Vulkan设备指针，用于设备相关的操作
 * @param example 示例接口指针，用于加载资源等操作
 */
Skybox::Skybox(vks::VulkanDevice* dev, IExampleInterfasce* example) : device(dev), iLoader(example) // 使用成员初始化列表初始化设备指针和接口加载器指针
{
	PreparePerBatchResource();
}

/**
 * Skybox类的析构函数
 * 用于释放Skybox对象占用的资源
 */
Skybox::~Skybox()
{
}

void Skybox::LoadFromPath(const std::string& mesh, VkQueue queue)
{
	// 定义 glTF 模型加载标志，预变换顶点并翻转 Y 轴（适配 Vulkan 坐标系）。
	uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
	// 加载天空盒模型（立方体 glTF 文件）。
	model = std::make_unique<vkglTF::Model>();
	model->loadFromFile(getAssetPath() + "models/cube.gltf", device, queue, glTFLoadingFlags);
}

/**
 * @brief 更新天空盒的立方体贴图
 * @param tex 共享指针指向的立方体贴图纹理对象
 */
void Skybox::UpdateCubemap(const std::shared_ptr<vks::TextureCubeMap>& tex)
{
    // 将传入的立方体贴图赋值给成员变量cubemap
	cubemap = tex;
    // 更新描述符集，确保使用新的纹理
	UpdateSet();
}

/**
 * @brief 更新描述符集
 * 该函数用于更新Skybox对象的描述符集，包含uniform缓冲区和立方体贴图采样器
 */
void Skybox::UpdateSet()
{
    // 创建描述符集写入操作数组，包含两个写入操作：
    // 1. 写入uniform缓冲区
    // 2. 写入立方体贴图采样器
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        // 第一个写入操作：设置uniform缓冲区，用于传递着色器uniform变量
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
        // 第二个写入操作：设置立方体贴图采样器，用于天空盒纹理采样
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cubemap->descriptor)
	};
    // 调用Vulkan API更新描述符集
    // 参数：逻辑设备、写入操作数量、写入操作数组、要删除的描述符集数量、要删除的描述符集数组
	vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

/**
 * @brief 为Skybox准备每批次所需的资源
 * 该函数用于创建描述符池、描述符集布局、分配描述符集，并创建和初始化一个本地缓冲区
 */
void Skybox::PreparePerBatchResource()
{
    // 创建描述符池大小数组，包含两种类型的描述符：uniform缓冲区和组合图像采样器
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },      // uniform缓冲区描述符
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // 组合图像采样器描述符
	};
    // 创建描述符池信息结构体，使用上述池大小和最大1个描述符集
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 创建描述符集布局绑定数组，定义两个绑定：uniform缓冲区和组合图像采样器
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0), // uniform缓冲区绑定
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1), // 组合图像采样器绑定
	};
    // 创建描述符集布局信息结构体
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    // 分配描述符集
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    // 创建本地缓冲区，用于存储变换矩阵
	device->createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,    // 用作uniform缓冲区
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // 主机可见且主机一致性内存属性，允许CPU直接访问并保证写入立即可见
		&localBuffer,                          // 缓冲区指针
		sizeof(LocalBuffer));                   // 缓冲区大小
    // 映射缓冲区内存以便访问
	localBuffer.map();

    // 初始化本地缓冲区数据为单位矩阵
	LocalBuffer empty = {};
	empty.transform = glm::mat4();
	memcpy(localBuffer.mapped, &empty, sizeof(LocalBuffer)); // 将单位矩阵复制到缓冲区
}

void Skybox::PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout)
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
	shaderStages[0] = iLoader->LoadShader("lightprobesh2/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = iLoader->LoadShader("lightprobesh2/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pso));
}

void Skybox::Update(const glm::mat4& view)
{
	LocalBuffer empty = {};
	empty.transform = glm::mat4(glm::mat3(view));
	memcpy(localBuffer.mapped, &empty, sizeof(LocalBuffer));
}

void Skybox::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet)
{
	std::vector<VkDescriptorSet> sets = {
		globalSet, descriptorSet
	};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
	model->draw(cmd);
}

void Skybox::Destroy()
{
	localBuffer.destroy();
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

	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}

	if (pso != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device->logicalDevice, pso, nullptr);
		pso = VK_NULL_HANDLE;
	}

}