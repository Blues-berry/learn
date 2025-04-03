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
		vks::Buffer object;//�洢�任����������λ�õĻ�����
		vks::Buffer params;//�洢��Դλ�õĻ�����
	} uniformBuffers;

	struct UBOMatrices {
		glm::mat4 projection;//ͶӰ���� 64bt
		glm::mat4 model;	 //ģ�;��� 64bt
		glm::mat4 view;		 //��ͼ���� 64bt
		glm::vec3 camPos;	 //�����λ�� 12bt������140���룬��Ҫ���뵽16�ֽڣ�
	} uboMatrices;

	/*
	ÿ����Ա������ƫ�������������С�ı�����
	���磬glm::vec3�Ĵ�С��12�ֽڣ������Ķ���Ҫ��ͨ����16�ֽڣ���Ϊ����Vulkan����С����Ҫ��
	��VkPhysicalDeviceLimits::minUniformBufferOffsetAlignmentͨ����16�ֽڣ���
	����ʵ��
	struct UBOMatrices {
	glm::mat4 projection; // 64�ֽ�
	glm::mat4 model;      // 64�ֽ�
	glm::mat4 view;       // 64�ֽ�
	glm::vec3 camPos;     // 12�ֽ�
	float padding;        // 4�ֽ���䣬ȷ�����뵽16�ֽ�
	���ߣ�ʹ��C++��alignas�ؼ���ǿ�ƶ��룺
	 alignas(16) glm::vec3 camPos; // 16�ֽڶ���
} uboMatrices;
	*/
	struct Light {
		glm::vec4 position;      //��Դλ�� 16bt
		glm::vec4 colorAndRadius;//��Դ���ԣ�ǰ������ʾ��ɫ�����һ����ʾradiance
		glm::vec4 direction;
		glm::vec4 cutOff;		//outercutoff cutoff minimum pow 
	
	};

	struct UBOParams {
		Light lights[4];
	} uboParams;
	//��ʼ����Ϊ�� 
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };  // ���߲��־��
	VkPipeline pipeline{ VK_NULL_HANDLE };              // ͼ�ι��߾��
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };  // �����������־��
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };    // �����������

	// Default materials to select from
	std::vector<Material> materials;  // �����б�
	int32_t materialIndex = 0;        // ��ǰѡ�еĲ�������

	std::vector<std::string> materialNames;  // ���������б�
	std::vector<std::string> objectNames;    // ģ�������б�
	VulkanExample() : VulkanExampleBase()
	{
		title = "Physical based shading basics";  // ���ô��ڱ���
		camera.type = Camera::CameraType::firstperson;  // ���������Ϊ��һ�˳��ӽ�
		camera.setPosition(glm::vec3(10.0f, 13.0f, 1.8f));  // �����������ʼλ��
		camera.setRotation(glm::vec3(-62.5f, 90.0f, 0.0f));  // �����������ʼ��ת
		camera.movementSpeed = 4.0f;  // ����������ƶ��ٶ�
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);  // ����͸��ͶӰ����
		camera.rotationSpeed = 0.25f;  // �����������ת�ٶ�
		timerSpeed *= 0.25f;  // ������ʱ���ٶ�

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
			materialNames.push_back(material.name);  // ������������ӵ��б�
		}
		objectNames = { "Sphere", "Teapot", "Torusknot", "Venus","plane","plane_circle",""};

		materialIndex = 0;// ����Ĭ�ϲ�������Ϊ 0
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
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();  // ��ʼ�����������ʼ��Ϣ

		VkClearValue clearValues[2];  // �������ֵ����
		clearValues[0].color = defaultClearColor;  // ������ɫ���ֵ
		clearValues[1].depthStencil = { 1.0f, 0 };  // ������Ⱥ�ģ�����ֵ

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();  // ��ʼ����Ⱦͨ����ʼ��Ϣ
		renderPassBeginInfo.renderPass = renderPass;  // ������Ⱦͨ��
		renderPassBeginInfo.renderArea.offset.x = 0;  // ������Ⱦ����ƫ��
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;  // ������Ⱦ������
		renderPassBeginInfo.renderArea.extent.height = height;  // ������Ⱦ����߶�
		renderPassBeginInfo.clearValueCount = 2;  // �������ֵ����
		renderPassBeginInfo.pClearValues = clearValues;  // ָ�����ֵ����

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)  // ���������������
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];  // ����Ŀ��֡������

			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));  // ��ʼ��¼�������

			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  // ��ʼ��Ⱦͨ��

			VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);  // �����ӿ�
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);  // Ӧ���ӿ�����

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);  // ���òü�����
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);  // Ӧ�òü�����

			// Draw a grid of spheres using varying material parameters
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);  // ��ͼ�ι���
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);  // ����������

			Material mat = materials[materialIndex];  // ��ȡ��ǰѡ�еĲ���

			const uint32_t gridSize = 7;  // ���������СΪ 7x7

			// Render a 2D grid of objects with varying PBR parameters
			for (uint32_t y = 0; y < gridSize; y++) {  // ���� Y ��
				for (uint32_t x = 0; x < gridSize; x++) {  // ���� X ��
					glm::vec3 pos = glm::vec3(float(x - (gridSize / 2.0f)) * 2.5f, 0.0f, float(y - (gridSize / 2.0f)) * 2.5f);  // ��������λ��
					vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);  // ����λ�ó�����������ɫ��
					// Vary metallic and roughness, two important PBR parameters
					//mat.params.metallic = glm::clamp((float)x / (float)(gridSize - 1), 0.1f, 1.0f);  // ���� X �������������
					//mat.params.roughness = glm::clamp((float)y / (float)(gridSize - 1), 0.05f, 1.0f);  // ���� Y ��������ֲڶ�
					vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &mat);  // ���Ͳ��ʲ�����Ƭ����ɫ��
					models.objects[models.objectIndex].draw(drawCmdBuffers[i]);  // ���Ƶ�ǰģ��
				}
			}

			drawUI(drawCmdBuffers[i]);  // ���� UI

			vkCmdEndRenderPass(drawCmdBuffers[i]);  // ������Ⱦͨ��

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));  // �������������¼
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
		// 1 Pool �������ش���
		/*����˵��
		VkDescriptorPoolSize������������������ÿ�����͵�������������
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER��ָ������������ΪUniform Buffer��
		4����ʾ���������ؿ��Է���4��Uniform Buffer���͵���������
		*/
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
		};
		/*
		 VkDescriptorPoolCreateInfo���������صĴ�����Ϣ��
		poolSizes���������صĴ�С�����͡�
			2���������ؿ��Է������������������
		 */
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		//������������ ��Descriptor Pool��
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		//2 ���������������֣�Descriptor Set Layout��
		//		VkDescriptorSetLayoutBinding�������������ֵİ󶨵���Ϣ��

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		};
		/*
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER������������ΪUniform Buffer��
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT���������ڶ����Ƭ����ɫ���п��á�
		nullptr����������immutable samplers��ͨ��Ϊnullptr����
			0���󶨵�������
			1��ÿ���󶨵������һ����������
		*/
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// 3 Set // ������������
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
		//device ��gpu
		/*
		VkDescriptorSetAllocateInfo����������������Ϣ��
		descriptorPool���������ء�
		&descriptorSetLayout�������������֡�
		1������һ������������
		vkAllocateDescriptorSets�������������з�����������
		*/
		//4 Update  ������������
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.object.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		/*
		VkWriteDescriptorSet����������������Ϣ��
		descriptorSet��Ҫ���µ�����������
		VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER�����������͡�
		0 �� 1���󶨵�������
		&uniformBuffers.object.descriptor �� &uniformBuffers.params.descriptor���������Ļ�������Ϣ��
		vkUpdateDescriptorSets����������������
				
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
����Ŀ�ģ���ʼ������Uniform Buffers��

vulkanDevice->createBuffer��Vulkan���߿��еĸ�����������װ��vkCreateBuffer����������������vkAllocateMemory�������ڴ棩�ĵ��á�
����1��VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT��ָ����������;ΪUniform Buffer����ʾ�����ݽ����ݸ���ɫ����
����2��VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT��
HOST_VISIBLE������CPUֱ��ӳ��ͷ��ʻ������ڴ档
HOST_COHERENT��ȷ��CPUд���GPU�Զ��ɼ��������ֶ�ͬ����
����3��&uniformBuffers.object��&uniformBuffers.params��ָ�򻺳������󣬴�����洢�����Ԫ���ݡ�
����4��sizeof(uboMatrices)��Լ208�ֽڣ���sizeof(uboParams)��256�ֽڣ����������ݽṹ��С���仺������
VK_CHECK_RESULT���꣬���ڼ��Vulkan���������Ƿ�ɹ�����ʧ�����׳��쳣��
���������������Uniform Buffers���ֱ����ھ������ݺ͹�Դ������
	*/
	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Object vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.object, sizeof(uboMatrices)));

		/*
		&uniformBuffers.object��
		���ﴫ�ݵ���uniformBuffers.object�ĵ�ַ������һ��vks::Buffer���͵Ķ���
		
		��Vulkan API�У����������ʶ���ֶ��󣬱��磺
		
		VkBuffer����ʶһ������������
		VkImage����ʶһ��ͼ�����
		VkDevice����ʶһ���߼��豸��
		
		createBuffer��������������������������Ϣ��
		����������Ļ����������VkBuffer�����洢��uniformBuffers.object.buffer�У�����vks::Buffer��buffer��Ա����
		Ԫ���ݣ�������������С��sizeof(uboMatrices)����������ڴ�����VkDeviceMemory����ӳ��ָ�루����У��ȡ�
		
		*/

				// Shared parameter uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &uniformBuffers.params, sizeof(uboParams)));

		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.object.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());

		/*
		uniformBuffers.object.map()������vks::Buffer::map()���ײ�ͨ��vkMapMemory�����������ڴ�ӳ�䵽CPU�ɷ��ʵĵ�ַ��
	Ŀ�ģ���ȡһ���־õ��ڴ�ָ�루�洢��uniformBuffers.object.mapped�У�����CPUֱ��д�����ݡ�
    	�־�ӳ�䣺ӳ���ڳ��������ڼ䱣����Ч������ÿ�θ���ʱ�ظ�ӳ��/��ӳ��Ŀ�����
    �����uniformBuffers.object.mapped��uniformBuffers.params.mapped��ΪCPU��д���ڴ�ָ�롣

	���� ���� ������������prepareUniformBuffers 

    //1 ʹ�� map �������������ڴ�ӳ�䵽 CPU �ɷ��ʵĵ�ַ��
    VK_CHECK_RESULT(uniformBuffers.object.map());
    map�����������ڴ�ӳ�䵽 CPU �ɷ��ʵĵ�ַ��
    //2 ����ӳ��Ļ������򣬽����ݸ��Ƶ�ӳ����ڴ���
    memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
    mapped������ӳ�����ڴ�ָ�롣
	//3 ������ɺ󣬿���ȡ��ӳ�����ͷ���Դ��
	uniformBuffers.object.unmap();
	unmap��ȡ��ӳ�䣬�ͷ� CPU �Ի������ڴ�ķ��ʡ�
		*/

	}

	void updateUniformBuffers()
	{
		// 3D object
		uboMatrices.projection = camera.matrices.perspective;
		uboMatrices.view = camera.matrices.view;
		uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));

		/*
		glm::mat4(1.0f)����λ������Ϊ��㡣
		glm::radians(-90.0f + ...)�����Ƕ�ת��Ϊ���ȣ�������ת-90�ȣ��ض����壨������������ת45�ȡ�
		glm::vec3(0.0f, 1.0f, 0.0f)����Y����ת��
		*/
		uboMatrices.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
	}

	void updateLights()
	{
		const float p = 15.0f;
		uboParams.lights[0].position = glm::vec4(-p * 0.5f, -p*0.5f, -p, 1.0f);  // ���ù�Դ 0 λ��
		uboParams.lights[1].position = glm::vec4(-p * 2.5f, -p*0.5f,  p, 1.0f);  // ���ù�Դ 1 λ��
		uboParams.lights[2].position = glm::vec4( p*0.5f, -p*0.5f,  p*0.5f, 1.0f);  // ���ù�Դ 2 λ��
		uboParams.lights[3].position = glm::vec4(0.f, -p*0.5f, 0.f, 1.0f);  // ���ù�Դ 3 λ��

		uboParams.lights[0].colorAndRadius = glm::vec4(1.f, 0.f, 0.f, 15.1f);  // ��Դ 0����ɫ���뾶 30.1
		uboParams.lights[1].colorAndRadius = glm::vec4(0.f, 1.f, 0.f, 15.1f);  // ��Դ 1����ɫ
		uboParams.lights[2].colorAndRadius = glm::vec4(0.f, 0.f, 1.f, 15.1f);  // ��Դ 2����ɫ
		uboParams.lights[3].colorAndRadius = glm::vec4(1.f, 1.f, 0.f, 15.1f);  // ��Դ 3����ɫ

		uboParams.lights[0].direction = glm::vec4(1.f, 0.f, 0.f, 1.f);
		uboParams.lights[1].direction = glm::vec4(0.f, 1.f, 0.f, 1.f);
		uboParams.lights[2].direction = glm::vec4(0.f, 0.f, 1.f, 1.f);
		uboParams.lights[3].direction = glm::vec4(0.f, 1.f, 0.f, 1.f);//��Դָ�� 0.f, 0.f, 0.f, 0.f p, -p * 0.5f, -p, 1.0f

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
		VulkanExampleBase::prepareFrame();  // ׼��֡
		submitInfo.commandBufferCount = 1;  // �����ύ�������������
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];  // ָ����ǰ�������
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));  // �ύ�������������
		VulkanExampleBase::submitFrame();  // �ύ֡
	}

	void prepare()
	{
		VulkanExampleBase::prepare();  // ��ʼ�� Vulkan ������ ���û���׼������
		loadAssets();  // ����ģ����Դ
		prepareUniformBuffers();  // ׼�� uniform ������
		setupDescriptors();  // ����������
		preparePipelines();  // ׼������
		buildCommandBuffers();  // �����������
		prepared = true;  // ���׼�����
	}

	virtual void render()
	{
		if (!prepared) return;  // ���δ׼���ã�ֱ�ӷ���
		updateUniformBuffers();  // ���¾��󻺳���
		if (!paused) { updateLights(); }  // ���δ��ͣ�����¹�Դ
		draw();  // ����֡
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
	{
		if (overlay->header("Settings")) {  // ��ʾ���ñ���
			if (overlay->comboBox("Material", &materialIndex, materialNames)) {  // ����ѡ��������
				buildCommandBuffers();  // ���²��ʺ��ؽ��������
			}
			if (overlay->comboBox("Object type", &models.objectIndex, objectNames)) {  // ģ��ѡ��������
				updateUniformBuffers();  // ���¾���
				buildCommandBuffers();  // �ؽ��������
			}
		}
	}
};

VULKAN_EXAMPLE_MAIN()