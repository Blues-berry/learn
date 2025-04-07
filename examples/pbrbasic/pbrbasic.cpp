/*
* Vulkan Example - Physical based shading basics
*
* See http://graphicrants.blogspot.de/2013/08/specular-brdf-reference.html for a good reference to the different functions that make up a specular BRDF
*
* Copyright (C) 2017-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"


const int maxnumLights = 64; // 目标光源数量

// C++ 端：集群的维度
const uint32_t CLUSTER_SIZE_X = 16;  // 屏幕宽度方向的集群数
const uint32_t CLUSTER_SIZE_Y = 16;  // 屏幕高度方向的集群数
const uint32_t CLUSTER_SIZE_Z = 16;  // 深度方向的集群数
const uint32_t TOTAL_CLUSTERS = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;

// C++ 端：集群光源数据结构
struct ClusterLightData {
	uint32_t lightCount;      // 当前集群影响的光源数量
	uint32_t lightOffset;     // 在全局光源索引列表中的起始偏移
};

struct ClusterData {
	std::vector<ClusterLightData> clusters; // TOTAL_CLUSTERS 个元素
	std::vector<uint32_t> lightIndexList;   // 全局光源索引列表
};
struct Material {
	// Parameter block used as push constant block
	struct PushBlock {
		float roughness;
		float metallic;
		float r, g, b;

	} params{};
	std::string name;
	Material() {};
	Material(std::string n, glm::vec3 c, float r, float m) : name(n) {
		params.roughness = r;
		params.metallic = m;
		params.r = c.r;
		params.g = c.g;
		params.b = c.b;
	};
};

class VulkanExample : public VulkanExampleBase
{
public:
	struct Meshes {
		std::vector<vkglTF::Model> objects;
		int32_t objectIndex = 0;
	} models;

	struct {
		vks::Buffer object;//存储变换矩阵和摄像机位置的缓冲区
		vks::Buffer params;//存储光源位置的缓冲区
	} uniformBuffers;

	struct UBOMatrices {
		glm::mat4 projection;//投影矩阵 64bt
		glm::mat4 model;	 //模型矩阵 64bt
		glm::mat4 view;		 //视图矩阵 64bt
		glm::vec3 camPos;	 //摄像机位置 12bt（根据140对齐，需要对齐到16字节）
	} uboMatrices;

	/*
	每个成员变量的偏移量必须是其大小的倍数。
	例如，glm::vec3的大小是12字节，但它的对齐要求通常是16字节，因为这是Vulkan的最小对齐要求
	（VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment通常是16字节）。
	具体实现
	struct UBOMatrices {
	glm::mat4 projection; // 64字节
	glm::mat4 model;      // 64字节
	glm::mat4 view;       // 64字节
	glm::vec3 camPos;     // 12字节
	float padding;        // 4字节填充，确保对齐到16字节
	或者，使用C++的alignas关键字强制对齐：
	 alignas(16) glm::vec3 camPos; // 16字节对齐
} uboMatrices;
	*/
	struct Light {
		glm::vec4 position;      //光源位置 16bt
		glm::vec4 colorAndRadius;//光源属性，前三个表示颜色，最后一个表示radiance
		glm::vec4 direction;
		glm::vec4 cutOff;		//outercutoff cutoff minimum pow 
	};

	struct UBOParams {
		Light lights[maxnumLights];         // 光源数组
		uint32_t clusterLightCounts[TOTAL_CLUSTERS]; // 每个集群的光源数量
		uint32_t clusterLightOffsets[TOTAL_CLUSTERS]; // 每个集群的偏移量
		uint32_t lightIndexList[maxnumLights * TOTAL_CLUSTERS]; // 全局光源索引列表（假设每个集群最多影响所有光源）
	}uboParams;

	//初始化均为空 
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };  // 管线布局句柄
	VkPipeline pipeline{ VK_NULL_HANDLE };              // 图形管线句柄
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };  // 描述符集布局句柄
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };    // 描述符集句柄

	// Default materials to select from
	std::vector<Material> materials;  // 材质列表
	int32_t materialIndex = 0;        // 当前选中的材质索引

	std::vector<std::string> materialNames;  // 材质名称列表
	std::vector<std::string> objectNames;    // 模型名称列表
	VulkanExample() : VulkanExampleBase()
	{
		title = "Physical based shading basics";  // 设置窗口标题
		camera.type = Camera::CameraType::firstperson;  // 设置摄像机为第一人称视角
		camera.setPosition(glm::vec3(10.0f, 13.0f, 1.8f));  // 设置摄像机初始位置
		camera.setRotation(glm::vec3(-62.5f, 90.0f, 0.0f));  // 设置摄像机初始旋转
		camera.movementSpeed = 4.0f;  // 设置摄像机移动速度
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);  // 设置透视投影参数
		camera.rotationSpeed = 0.25f;  // 设置摄像机旋转速度
		timerSpeed *= 0.25f;  // 减慢计时器速度

		// Setup some default materials (source: https://seblagarde.wordpress.com/2011/08/17/feeding-a-physical-based-lighting-mode/)
		materials.push_back(Material("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f), 0.1f, 1.0f));
		materials.push_back(Material("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f), 0.1f, 1.0f));
		materials.push_back(Material("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f), 0.1f, 1.0f));
		materials.push_back(Material("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f), 0.1f, 1.0f));
		materials.push_back(Material("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f), 0.1f, 1.0f));
		materials.push_back(Material("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f), 0.1f, 1.0f));
		materials.push_back(Material("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f), 0.1f, 1.0f));
		materials.push_back(Material("planematerial", glm::vec3(0.955008f, 0.654864f, 0.336057f), 0.1f, 1.0f));
		// Testing materials
		materials.push_back(Material("White", glm::vec3(1.0f), 0.1f, 1.0f));
		materials.push_back(Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 0.1f, 1.0f));
		materials.push_back(Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.1f, 1.0f));
		materials.push_back(Material("Black", glm::vec3(0.0f), 0.1f, 1.0f));

		for (auto material : materials) {
			materialNames.push_back(material.name);  // 将材质名称添加到列表
		}
		objectNames = { "Sphere", "Teapot", "Torusknot", "Venus","plane","plane_circle",""};

		materialIndex = 0;// 设置默认材质索引为 0
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			uniformBuffers.object.destroy();
			uniformBuffers.params.destroy();
		}
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();  // 初始化命令缓冲区开始信息

		VkClearValue clearValues[2];  // 定义清除值数组
		clearValues[0].color = defaultClearColor;  // 设置颜色清除值
		clearValues[1].depthStencil = { 1.0f, 0 };  // 设置深度和模板清除值

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();  // 初始化渲染通道开始信息
		renderPassBeginInfo.renderPass = renderPass;  // 设置渲染通道
		renderPassBeginInfo.renderArea.offset.x = 0;  // 设置渲染区域偏移
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;  // 设置渲染区域宽度
		renderPassBeginInfo.renderArea.extent.height = height;  // 设置渲染区域高度
		renderPassBeginInfo.clearValueCount = 2;  // 设置清除值数量
		renderPassBeginInfo.pClearValues = clearValues;  // 指定清除值数组

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)  // 遍历所有命令缓冲区
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];  // 设置目标帧缓冲区

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));  // 开始记录命令缓冲区

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  // 开始渲染通道

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);  // 设置视口
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);  // 应用视口设置

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);  // 设置裁剪矩形
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);  // 应用裁剪矩形

			// Draw a grid of spheres using varying material parameters
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);  // 绑定图形管线
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);  // 绑定描述符集

			Material mat = materials[materialIndex];  // 获取当前选中的材质

			const uint32_t gridSize = 7;  // 定义网格大小为 7x7

			// Render a 2D grid of objects with varying PBR parameters
			for (uint32_t y = 0; y < gridSize; y++) {  // 遍历 Y 轴
				for (uint32_t x = 0; x < gridSize; x++) {  // 遍历 X 轴
					glm::vec3 pos = glm::vec3(float(x - (gridSize / 2.0f)) * 2.5f, 0.0f, float(y - (gridSize / 2.0f)) * 2.5f);  // 计算物体位置
					vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);  // 推送位置常量到顶点着色器
					// Vary metallic and roughness, two important PBR parameters
					//mat.params.metallic = glm::clamp((float)x / (float)(gridSize - 1), 0.1f, 1.0f);  // 根据 X 坐标调整金属度
					//mat.params.roughness = glm::clamp((float)y / (float)(gridSize - 1), 0.05f, 1.0f);  // 根据 Y 坐标调整粗糙度
					vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &mat);  // 推送材质参数到片段着色器
					models.objects[models.objectIndex].draw(drawCmdBuffers[i]);  // 绘制当前模型
				}
			}

			drawUI(drawCmdBuffers[i]);  // 绘制 UI

			vkCmdEndRenderPass(drawCmdBuffers[i]);  // 结束渲染通道

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));  // 结束命令缓冲区记录
		}
	}

	void loadAssets()
	{
		std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf","plane.gltf","plane_circle.gltf"};
		models.objects.resize(filenames.size());
		for (size_t i = 0; i < filenames.size(); i++) {			
			models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue, vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY);
		}
	}

	void setupDescriptors()
	{
		// 1 Pool 描述符池创建
		/*参数说明
		VkDescriptorPoolSize：定义了描述符池中每种类型的描述符数量。
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER：指定描述符类型为Uniform Buffer。
		4：表示该描述符池可以分配4个Uniform Buffer类型的描述符。
		*/
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
		};
		/*
		 VkDescriptorPoolCreateInfo：描述符池的创建信息。
		poolSizes：描述符池的大小和类型。
			2：描述符池可以分配的描述符集数量。
		 */
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		//创建描述符池 （Descriptor Pool）
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		//2 创建描述符集布局（Descriptor Set Layout）
		//		VkDescriptorSetLayoutBinding：描述符集布局的绑定点信息。

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		/*
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER：描述符类型为Uniform Buffer。
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT：描述符在顶点和片段着色器中可用。
		nullptr：描述符的immutable samplers（通常为nullptr）。
			0：绑定点索引。
			1：每个绑定点可以有一个描述符。
		*/
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// 3 Set // 分配描述符集
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
		//device 是gpu
		/*
		VkDescriptorSetAllocateInfo：描述符集分配信息。
		descriptorPool：描述符池。
		&descriptorSetLayout：描述符集布局。
		1：分配一个描述符集。
		vkAllocateDescriptorSets：从描述符池中分配描述符集
		*/
		//4 Update  更新描述符集
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.object.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		/*
		VkWriteDescriptorSet：描述符集更新信息。
		descriptorSet：要更新的描述符集。
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER：描述符类型。
		0 或 1：绑定点索引。
		&uniformBuffers.object.descriptor 或 &uniformBuffers.params.descriptor：描述符的缓冲区信息。
		vkUpdateDescriptorSets：更新描述符集。
				
		*/
	}

	void preparePipelines()
	{
		// Layout
		// We use push constant to pass material information
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0),
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),
		};
		pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =  vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal });

		// PBR pipeline
		shaderStages[0] = loadShader(getShadersPath() + "pbrbasic/pbr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrbasic/pbr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
	}


	/*
函数目的：初始化两个Uniform Buffers。

vulkanDevice->createBuffer：Vulkan工具库中的辅助函数，封装了vkCreateBuffer（创建缓冲区）和vkAllocateMemory（分配内存）的调用。
参数1：VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT，指定缓冲区用途为Uniform Buffer，表示其数据将传递给着色器。
参数2：VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT：
HOST_VISIBLE：允许CPU直接映射和访问缓冲区内存。
HOST_COHERENT：确保CPU写入后，GPU自动可见，无需手动同步。
参数3：&uniformBuffers.object或&uniformBuffers.params，指向缓冲区对象，创建后存储句柄和元数据。
参数4：sizeof(uboMatrices)（约208字节）和sizeof(uboParams)（256字节），根据数据结构大小分配缓冲区。
VK_CHECK_RESULT：宏，用于检查Vulkan函数调用是否成功，若失败则抛出异常。
结果：创建了两个Uniform Buffers，分别用于矩阵数据和光源参数。
	*/
	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Object vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.object, sizeof(uboMatrices)));

		/*
		&uniformBuffers.object：
		这里传递的是uniformBuffers.object的地址，它是一个vks::Buffer类型的对象。
		
		在Vulkan API中，句柄用来标识各种对象，比如：
		
		VkBuffer：标识一个缓冲区对象。
		VkImage：标识一个图像对象。
		VkDevice：标识一个逻辑设备。
		
		createBuffer函数会在这个对象中填充两类信息：
		句柄：创建的缓冲区句柄（VkBuffer），存储在uniformBuffers.object.buffer中（假设vks::Buffer有buffer成员）。
		元数据：包括缓冲区大小（sizeof(uboMatrices)）、分配的内存句柄（VkDeviceMemory）、映射指针（如果有）等。
		
		*/

				// Shared parameter uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.params, sizeof(uboParams)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.object.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());

		/*
		uniformBuffers.object.map()：调用vks::Buffer::map()，底层通过vkMapMemory将缓冲区的内存映射到CPU可访问的地址。
	目的：获取一个持久的内存指针（存储在uniformBuffers.object.mapped中），供CPU直接写入数据。
    	持久映射：映射在程序运行期间保持有效，避免每次更新时重复映射/解映射的开销。
    结果：uniformBuffers.object.mapped和uniformBuffers.params.mapped成为CPU可写的内存指针。

	先行 操作 创建缓冲区，prepareUniformBuffers 

    //1 使用 map 方法将缓冲区内存映射到 CPU 可访问的地址：
    VK_CHECK_RESULT(uniformBuffers.object.map());
    map：将缓冲区内存映射到 CPU 可访问的地址。
    //2 更新映射的缓冲区域，将数据复制到映射的内存中
    memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
    mapped：返回映射后的内存指针。
	//3 更新完成后，可以取消映射以释放资源：
	uniformBuffers.object.unmap();
	unmap：取消映射，释放 CPU 对缓冲区内存的访问。
		*/

	}

	void updateUniformBuffers()
	{
		// 3D object
		uboMatrices.projection = camera.matrices.perspective;
		uboMatrices.view = camera.matrices.view;
		uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));

		/*
		glm::mat4(1.0f)：单位矩阵作为起点。
		glm::radians(-90.0f + ...)：将角度转换为弧度，基础旋转-90度，特定物体（如茶壶）额外旋转45度。
		glm::vec3(0.0f, 1.0f, 0.0f)：绕Y轴旋转。
		*/
		uboMatrices.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
	}

	//void updateLights()
	//{
	//	const float p = 15.0f;
	//	uboParams.lights[0].position = glm::vec4(-p * 0.5f, -p*0.5f, -p, 1.0f);  // 设置光源 0 位置
	//	uboParams.lights[1].position = glm::vec4(-p * 2.5f, -p*0.5f,  p, 1.0f);  // 设置光源 1 位置
	//	uboParams.lights[2].position = glm::vec4( p*0.5f, -p*0.5f,  p*0.5f, 1.0f);  // 设置光源 2 位置
	//	uboParams.lights[3].position = glm::vec4(0.f, -p*0.5f, 0.f, 1.0f);  // 设置光源 3 位置

	//	uboParams.lights[0].colorAndRadius = glm::vec4(1.f, 0.f, 0.f, 15.1f);  // 光源 0：红色，半径 30.1
	//	uboParams.lights[1].colorAndRadius = glm::vec4(0.f, 1.f, 0.f, 15.1f);  // 光源 1：绿色
	//	uboParams.lights[2].colorAndRadius = glm::vec4(0.f, 0.f, 1.f, 15.1f);  // 光源 2：蓝色
	//	uboParams.lights[3].colorAndRadius = glm::vec4(1.f, 1.f, 0.f, 15.1f);  // 光源 3：黄色

	//	uboParams.lights[0].direction = glm::vec4(1.f, 0.f, 0.f, 1.f);
	//	uboParams.lights[1].direction = glm::vec4(0.f, 1.f, 0.f, 1.f);
	//	uboParams.lights[2].direction = glm::vec4(0.f, 0.f, 1.f, 1.f);
	//	uboParams.lights[3].direction = glm::vec4(0.f, 1.f, 0.f, 1.f);//光源指向 0.f, 0.f, 0.f, 0.f p, -p * 0.5f, -p, 1.0f

	//	uboParams.lights[0].cutOff=glm::vec4(12.5f,18.5f,0.f,0.f);
	//	uboParams.lights[1].cutOff=glm::vec4(12.5f,18.5f,0.f,0.f);
	//	uboParams.lights[2].cutOff=glm::vec4(12.5f,18.5f,0.f,0.f);
	//	uboParams.lights[3].cutOff=glm::vec4(cos(glm::radians(12.5)), cos(glm::radians(50.5)),0.f,20.f);
	//	if (!paused)								
	//	{																		   
	//		uboParams.lights[0].position.x = sin(glm::radians(timer*5 * 360.0f)) * 5.0f;
	//		uboParams.lights[0].position.z = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
	//		uboParams.lights[1].position.x = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
	//		uboParams.lights[1].position.y = sin(glm::radians(timer*5 * 360.0f)) * 5.0f; 																		 
	//		//uboParams.lights[2].position.x = sin(glm::radians(timer*5 * 360.0f)) * 5.0f;
	//		//uboParams.lights[2].position.z = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
	//		uboParams.lights[3].position.x = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
	//		uboParams.lights[3].position.y = sin(glm::radians(timer*5 * 360.0f)) * 5.0f;

	//	}

	//	memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
	//}
	void updateLights()
	{
		const float p = 15.0f; // 空间范围参数
		const int gridSize = static_cast<int>(ceil(sqrt(static_cast<float>(maxnumLights)))); // 计算网格大小，例如 8x8
		const float spacing = 2.0f * p / (gridSize - 1); // 每个光源之间的间距

		int lightIndex = 0;
		for (int y = 0; y < gridSize && lightIndex < maxnumLights; y++) {
			for (int x = 0; x < gridSize && lightIndex < maxnumLights; x++) {
				// 计算光源位置
				float posX = -p + x * spacing; // 从 -p 到 p
				float posZ = -p + y * spacing; // 从 -p 到 p
				float posY = -p * 0.5f; // 固定高度

				uboParams.lights[lightIndex].position = glm::vec4(posX, posY, posZ, 1.0f);

				// 设置光源颜色（循环使用几种颜色）
				glm::vec3 color;
				switch (lightIndex % 4) {
				case 0: color = glm::vec3(1.0f, 0.0f, 0.0f); break; // 红
				case 1: color = glm::vec3(0.0f, 1.0f, 0.0f); break; // 绿
				case 2: color = glm::vec3(0.0f, 0.0f, 1.0f); break; // 蓝
				case 3: color = glm::vec3(1.0f, 1.0f, 0.0f); break; // 黄
				}
				uboParams.lights[lightIndex].colorAndRadius = glm::vec4(color, 15.1f);

				// 设置方向（可选，指向原点）
				glm::vec3 direction = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(posX, posY, posZ));
				uboParams.lights[lightIndex].direction = glm::vec4(direction, 1.0f);

				// 设置截止角度（保持与原代码一致）
				uboParams.lights[lightIndex].cutOff = glm::vec4(12.5f, 18.5f, 0.0f, 0.0f);
				lightIndex++;
			}
		}

		// 如果需要动态效果，可以添加动画（可选）
		if (!paused) {
			for (int i = 0; i < maxnumLights; i++) {
				uboParams.lights[i].position.x += sin(glm::radians(timer * 360.0f)) * 0.1f;
				uboParams.lights[i].position.z += cos(glm::radians(timer * 360.0f)) * 0.1f;
			}
		}

		// 更新缓冲区
		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
	}
	// 函数目的：将光源分配到三维空间的集群中，更新 uboParams 中的集群数据，并写入 Uniform Buffer。
	// 调用时机：在 render() 中，当 !paused 时与 updateLights() 一起调用。
	// 输入：依赖 uboMatrices（矩阵数据）、uboParams.lights（光源位置）、uniformBuffers.params（缓冲区）。
	// 输出：更新 uboParams.clusterLightCounts、clusterLightOffsets 和 lightIndexList，并同步到 uniformBuffers.params。
	void updateLightsCluster() {
		ClusterData clusterData;
		// ClusterData：临时结构体，定义为：
		// struct ClusterData {
		//     std::vector<ClusterLightData> clusters; // TOTAL_CLUSTERS 个元素
		//     std::vector<uint32_t> lightIndexList;   // 全局光源索引列表
		// };
		// clusterData：局部变量，用于存储集群分配的中间结果。
		// 作用：初始化一个空的 ClusterData 对象，clusters 和 lightIndexList 默认构造为空。
		clusterData.clusters.resize(TOTAL_CLUSTERS);
		// clusters：clusterData 的成员，一个 std::vector<ClusterLightData>。
		// TOTAL_CLUSTERS：4096（16*16*16），总集群数。
		// resize(TOTAL_CLUSTERS)：调整 clusters 大小为 4096，每个元素初始化为 {lightCount=0, lightOffset=0}。
		// 作用：为每个集群分配空间，确保有 4096 个 ClusterLightData 元素。
		clusterData.lightIndexList.clear();
		// lightIndexList：clusterData 的成员，一个 std::vector<uint32_t>，存储所有光源索引。
		// clear()：清空向量，确保从空状态开始。
		// 作用：重置 lightIndexList，准备填充新的光源索引。

		// 视锥体参数
		glm::mat4 invViewProj = glm::inverse(uboMatrices.projection * uboMatrices.view);
		// glm::mat4：GLM 库的 4x4 矩阵类型。
		// uboMatrices：全局变量，来自 uniformBuffers.object。
		// .projection：投影矩阵（透视投影，60° FOV，0.1f 近裁剪面，256.0f 远裁剪面）。
		// .view：视图矩阵（摄像机变换，基于 camera.position 和 rotation）。
		// projection * view：视图-投影矩阵，将世界空间变换到裁剪空间。
		// glm::inverse(...)：计算逆矩阵。
		// invViewProj：逆视图-投影矩阵（当前未使用，为扩展准备）。
		// 作用：为光源位置变换提供工具，尽管本函数仅使用正向变换。
		float zNear = 0.1f;
		// zNear：近平面距离，与 camera.setPerspective(60.0f, width/height, 0.1f, 256.0f) 一致。
		// 值 0.1f：近裁剪面距离（世界空间单位）。
		// 作用：定义深度范围起点，用于 Z 轴集群计算。
		float zFar = 256.0f;
		// zFar：远平面距离，与 camera.setPerspective 设置一致。
		// 值 256.0f：远裁剪面距离。
		// 作用：定义深度范围终点。

		for (int i = 0; i < maxnumLights; i++) {
			glm::vec4 lightPos = uboParams.lights[i].position;
			float radius = uboParams.lights[i].colorAndRadius.w;

			// 转换到 NDC 空间
			glm::vec4 clipPos = uboMatrices.projection * uboMatrices.view * lightPos;
			// projection * view：视图-投影矩阵。
			// lightPos：世界空间位置。
			// clipPos：裁剪空间坐标（x, y, z, w）。
			// 作用：将光源位置从世界空间变换到裁剪空间。
			clipPos /= clipPos.w;
			if (clipPos.z < -1.0f || clipPos.z > 1.0f) continue;//未剔除视锥体外的光源（clipPos.z < -1 或 > 1）。
			// 转换为集群坐标
			uint32_t clusterX = static_cast<uint32_t>((clipPos.x * 0.5f + 0.5f) * CLUSTER_SIZE_X);
			// clipPos.x：NDC X 坐标（[-1, 1]）。
			// * 0.5f + 0.5f：映射到 [0, 1]（屏幕空间比例）。
			// * CLUSTER_SIZE_X：映射到 [0, 16]（CLUSTER_SIZE_X = 16）。
			// static_cast<uint32_t>：转换为无符号整数，取整。
			// clusterX：X 轴集群索引（0 到 15）。
			// 作用：计算光源在 X 方向的集群位置。
			uint32_t clusterY = static_cast<uint32_t>((clipPos.y * 0.5f + 0.5f) * CLUSTER_SIZE_Y);
			// Y与X同理
			float depth = (clipPos.z * 0.5f + 0.5f) * (zFar - zNear) + zNear;
			// clip DARK POOLPos.z：NDC Z 坐标（[-1, 1]）。
			// * 0.5f + 0.5f：映射到 [0, 1]。
			// * (zFar - zNear) + zNear：映射到 [0.1, 256]（线性深度）。
			// depth：世界空间深度值。
			// 作用：将 NDC Z 转换为线性深度。
			uint32_t clusterZ = static_cast<uint32_t>((log(depth / zNear) / log(zFar / zNear)) * CLUSTER_SIZE_Z);
			// depth / zNear：深度相对于近平面的比例。
			// log(...)：对数变换，优化深度分布（近处集群更细腻）。
			// log(zFar / zNear)：深度范围的对数总和。
			// / ... * CLUSTER_SIZE_Z：映射到 [0, 16]（CLUSTER_SIZE_Z = 16）。
			// clusterZ：Z 轴集群索引（0 到 15）。
			// 作用：计算光源在 Z 方向的集群位置，使用对数分布。

			// 限制范围
			clusterX = glm::clamp(clusterX, 0u, CLUSTER_SIZE_X - 1);
			// glm::clamp：限制值在指定范围内。
			// clusterX：X 轴索引。
			// 0u：最小值（0）。
			// CLUSTER_SIZE_X - 1：最大值（15）。
			// 作用：确保 clusterX 在 [0, 15] 内，防止越界。
			clusterY = glm::clamp(clusterY, 0u, CLUSTER_SIZE_Y - 1);
			clusterZ = glm::clamp(clusterZ, 0u, CLUSTER_SIZE_Z - 1);

			// 添加到集群
			uint32_t clusterIdx = clusterZ * CLUSTER_SIZE_X * CLUSTER_SIZE_Y + clusterY * CLUSTER_SIZE_X + clusterX;
			// clusterZ * CLUSTER_SIZE_X * CLUSTER_SIZE_Y：Z 平面偏移（每层 16*16=256 个集群）。
			// clusterY * CLUSTER_SIZE_X：Y 行偏移（每行 16 个集群）。
			// + clusterX：X 列偏移。
			// clusterIdx：一维集群索引（0 到 4095）。
			// 作用：将三维坐标 (X, Y, Z) 转换为一维索引。
			clusterData.clusters[clusterIdx].lightCount++;
			// clusters[clusterIdx]：第 clusterIdx 个集群的 ClusterLightData。
			// .lightCount：该集群的光源数量。
			// ++：自增，表示该集群多一个光源。
			// 作用：记录当前集群的光源数量。
			memset(uboParams.lightIndexList, 0xFF, sizeof(uboParams.lightIndexList)); // 先填充无效值
			if (clusterData.clusters[clusterIdx].lightCount == 1) {
				clusterData.clusters[clusterIdx].lightOffset = static_cast<uint32_t>(clusterData.lightIndexList.size());
			}//当一个集群首次分配光源（lightCount 从 0 变为 1）时，记录 lightIndexList 当前的大小作为该集群的 lightOffset。
			// .lightOffset：该集群在 lightIndexList 中的起始偏移。
			// lightIndexList.size()：当前全局索引列表的长度。
			// static_cast<uint32_t>：转换为无符号整数。
			// 作用：设置偏移为当前 lightIndexList 大小（首次分配时记录起始位置）。
			// 注意：此处逻辑有误，lightOffset 应只在 lightCount 从 0 变为 1 时设置，否则会被覆盖。
			clusterData.lightIndexList.push_back(i);
			// lightIndexList：全局光源索引列表。
			// push_back(i)：添加光源索引 i（0 到 63）。
			// i：当前光源的索引。
			// 作用：将光源 i 添加到全局索引列表。
		}

		// 更新 UBOParams
		for (uint32_t i = 0; i < TOTAL_CLUSTERS; i++) {
			uboParams.clusterLightCounts[i] = clusterData.clusters[i].lightCount;
			uboParams.clusterLightOffsets[i] = clusterData.clusters[i].lightOffset;
			// uboParams.clusterLightCounts[i]：全局 uboParams 中的集群光源计数。
			// clusterData.clusters[i].lightCount：第 i 个集群的光源数量。
			// clusterLightOffsets[i] 全局 uboParams 中的集群光源偏移量
			// uboParams.clusterLightOffsets[i] = (clusterData.clusters[i].lightCount > 0) ? clusterData.clusters[i].lightOffset : UINT_MAX;//若集群为空（lightCount == 0），设置 lightOffset 为无效值（例如 UINT_MAX）：
			// 作用：更新每个集群的光源数量和偏移量。
		}
		memcpy(uboParams.lightIndexList, clusterData.lightIndexList.data(), clusterData.lightIndexList.size() * sizeof(uint32_t));
		// memcpy：内存拷贝函数。
		// uboParams.lightIndexList：目标数组，大小为 maxnumLights * TOTAL_CLUSTERS（64 * 4096 = 262144）。
		// clusterData.lightIndexList.data()：源数据，动态向量中的光源索引。
		// .size()：当前 lightIndexList 中的元素数（实际光源数，可能小于 64）。
		// * sizeof(uint32_t)：拷贝字节数（每个索引 4 字节）。
		// 作用：将动态分配的光源索引拷贝到固定大小的数组。
		// 注意：未填充剩余空间，可能导致未定义行为。
		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
		// uniformBuffers.params.mapped：映射的 Uniform Buffer 内存指针（由 prepareUniformBuffers() 设置）。
		// &uboParams：源数据地址。
		// sizeof(uboParams)：数据大小（lights: 64*64=4096 字节，counts: 4096*4=16384 字节，offsets: 16384 字节，list: 262144*4=1048576 字节，总计约 1.08 MB）。
		// 作用：将更新后的 uboParams 写入 Uniform Buffer，供着色器使用。
	}



	void draw()
	{
		VulkanExampleBase::prepareFrame();  // 准备帧
		submitInfo.commandBufferCount = 1;  // 设置提交的命令缓冲区数量
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];  // 指定当前命令缓冲区
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));  // 提交命令缓冲区到队列
		VulkanExampleBase::submitFrame();  // 提交帧
	}

	void prepare()
	{
		VulkanExampleBase::prepare();  // 初始化 Vulkan 环境。 调用基类准备函数
		loadAssets();  // 加载模型资源
		prepareUniformBuffers();  // 准备 uniform 缓冲区
		setupDescriptors();  // 设置描述符
		preparePipelines();  // 准备管线
		buildCommandBuffers();  // 构建命令缓冲区
		prepared = true;  // 标记准备完成
	}

	virtual void render()
	{
		if (!prepared) return;  // 如果未准备好，直接返回
		updateUniformBuffers();  // 更新矩阵缓冲区
		if (!paused) { updateLights(); updateLightsCluster(); }  // 如果未暂停，更新光源
		draw();  // 绘制帧
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
	{
		if (overlay->header("Settings")) {  // 显示设置标题
			if (overlay->comboBox("Material", &materialIndex, materialNames)) {  // 材质选择下拉框
				buildCommandBuffers();  // 更新材质后重建命令缓冲区
			}
			if (overlay->comboBox("Object type", &models.objectIndex, objectNames)) {  // 模型选择下拉框
				updateUniformBuffers();  // 更新矩阵
				buildCommandBuffers();  // 重建命令缓冲区
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()