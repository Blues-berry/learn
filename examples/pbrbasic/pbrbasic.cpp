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
		Light lights[4];
	} uboParams;
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

	void updateLights()
	{
		const float p = 15.0f;
		uboParams.lights[0].position = glm::vec4(-p * 0.5f, -p*0.5f, -p, 1.0f);  // 设置光源 0 位置
		uboParams.lights[1].position = glm::vec4(-p * 2.5f, -p*0.5f,  p, 1.0f);  // 设置光源 1 位置
		uboParams.lights[2].position = glm::vec4( p*0.5f, -p*0.5f,  p*0.5f, 1.0f);  // 设置光源 2 位置
		uboParams.lights[3].position = glm::vec4(0.f, -p*0.5f, 0.f, 1.0f);  // 设置光源 3 位置

		uboParams.lights[0].colorAndRadius = glm::vec4(1.f, 0.f, 0.f, 15.1f);  // 光源 0：红色，半径 30.1
		uboParams.lights[1].colorAndRadius = glm::vec4(0.f, 1.f, 0.f, 15.1f);  // 光源 1：绿色
		uboParams.lights[2].colorAndRadius = glm::vec4(0.f, 0.f, 1.f, 15.1f);  // 光源 2：蓝色
		uboParams.lights[3].colorAndRadius = glm::vec4(1.f, 1.f, 0.f, 15.1f);  // 光源 3：黄色

		uboParams.lights[0].direction = glm::vec4(1.f, 0.f, 0.f, 1.f);
		uboParams.lights[1].direction = glm::vec4(0.f, 1.f, 0.f, 1.f);
		uboParams.lights[2].direction = glm::vec4(0.f, 0.f, 1.f, 1.f);
		uboParams.lights[3].direction = glm::vec4(0.f, 1.f, 0.f, 1.f);//光源指向 0.f, 0.f, 0.f, 0.f p, -p * 0.5f, -p, 1.0f

		uboParams.lights[0].cutOff=glm::vec4(12.5f,18.5f,0.f,0.f);
		uboParams.lights[1].cutOff=glm::vec4(12.5f,18.5f,0.f,0.f);
		uboParams.lights[2].cutOff=glm::vec4(12.5f,18.5f,0.f,0.f);
		uboParams.lights[3].cutOff=glm::vec4(cos(glm::radians(12.5)), cos(glm::radians(50.5)),0.f,20.f);
		if (!paused)								
		{																		   
			uboParams.lights[0].position.x = sin(glm::radians(timer*5 * 360.0f)) * 5.0f;
			uboParams.lights[0].position.z = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
			uboParams.lights[1].position.x = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
			uboParams.lights[1].position.y = sin(glm::radians(timer*5 * 360.0f)) * 5.0f; 																		 
			//uboParams.lights[2].position.x = sin(glm::radians(timer*5 * 360.0f)) * 5.0f;
			//uboParams.lights[2].position.z = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
			uboParams.lights[3].position.x = cos(glm::radians(timer*5 * 360.0f)) * 5.0f;
			uboParams.lights[3].position.y = sin(glm::radians(timer*5 * 360.0f)) * 5.0f;

		}

		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
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
		if (!paused) { updateLights(); }  // 如果未暂停，更新光源
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