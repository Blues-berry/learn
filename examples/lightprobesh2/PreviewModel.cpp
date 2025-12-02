#include "PreviewModel.h"
#include "VulkanDevice.h"

PreviewModel::PreviewModel(vks::VulkanDevice* dev, IExampleInterfasce* example) : device(dev), iLoader(example)
{
	PreparePerBatchResource();
	UpdateSet();
}

void PreviewModel::UpdateModel(const std::shared_ptr<vkglTF::Model>& model_)
{
	model = model_;
}

void PreviewModel::Destroy()
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

void PreviewModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
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

	// ✅ 修复: 绑定顶点和索引缓冲，并传递正确的参数
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(cmd, 0, 1, &model->vertices.buffer, offsets);
	vkCmdBindIndexBuffer(cmd, model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	model->draw(cmd, vkglTF::RenderFlags::BindImages, techniques[techIdx].pipelineLayout, 1);
}

void PreviewModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech, const glm::vec3& position)
{
    if (!model)
    {
        printf("[ERROR] Model is null!\n");
        return;
    }

    uint32_t techIdx = (uint32_t)tech;
    std::vector<VkDescriptorSet> sets = {
        globalSet, descriptorSet
    };

    //printf("[DEBUG] Drawing preview model at position: (%.2f, %.2f, %.2f)\n", position.x, position.y, position.z);
    
    // Update the model's position to match the light source
    // Use a simple scale to make the light source visible
    float scale = 0.5f; // Adjust size as needed
    glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), position);
    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
    
    // Update the local buffer with the new transform
    localData.transform = modelMatrix;
    memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));

    // Make sure the material is updated with the latest light color
    if (materialDirty) {
        printf("[DEBUG] Material is dirty, updating material buffer...\n");
        printf("[DEBUG] Current material color: (%.2f, %.2f, %.2f, %.2f)\n", 
               materialData.elbedo.r, materialData.elbedo.g, materialData.elbedo.b, materialData.elbedo.a);
        
        if (materialBuffer.mapped) {
            memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
            printf("[DEBUG] Material buffer updated\n");
        } else {
            printf("[ERROR] Material buffer not mapped!\n");
        }
        materialDirty = false;
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pipelineLayout, 
                           0, static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pso);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &model->vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    model->draw(cmd, vkglTF::RenderFlags::BindImages, techniques[techIdx].pipelineLayout, 1);
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

void PreviewModel::UpdateSet()
{
    // Update material buffer if dirty
    if (materialDirty && materialBuffer.mapped) {
        printf("[DEBUG] Material is dirty, updating buffer...\n");
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
        
        // Flush the memory to make sure the GPU sees the update
        VkMappedMemoryRange memoryRange = {};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = materialBuffer.memory;
        memoryRange.offset = 0;
        memoryRange.size = sizeof(MaterialBuffer);
        vkFlushMappedMemoryRanges(device->logicalDevice, 1, &memoryRange);
        
        materialDirty = false;
        printf("[DEBUG] Material buffer updated and flushed\n");
    }
    
    // Update descriptor sets
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
    };
    
    vkUpdateDescriptorSets(device->logicalDevice, 
                          static_cast<uint32_t>(writeDescriptorSets.size()), 
                          writeDescriptorSets.data(), 
                          0, 
                          nullptr);
                          
    // Debug output
    #ifdef _DEBUG
    printf("[DEBUG] Descriptor sets updated with material:\n");
    printf("  - Albedo: (%.2f, %.2f, %.2f, %.2f)\n", 
           materialData.elbedo.r, materialData.elbedo.g, materialData.elbedo.b, materialData.elbedo.a);
    printf("  - Roughness: %.2f, Metallic: %.2f, Specular: %.2f\n", 
           materialData.roughness, materialData.metallic, materialData.specular);
    printf("  - useLighting: %d, useSH: %d, useReflection: %d\n", 
           materialData.useLighting, materialData.useSH, materialData.useReflection);
    #endif
}

void PreviewModel::PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique)
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
	// 设置顶点输入状态，只需要位置属性
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position });

	// 使用简化后的光源着色器
	shaderStages[0] = iLoader->LoadShader("lightprobesh2/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = iLoader->LoadShader("lightprobesh2/light_source.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &techniques[(uint32_t)technique].pso));
}

void PreviewModel::ShowUI(vks::UIOverlay* overlay)
{
	if (overlay->header("Material")) {
		// Lighting toggle
		if (overlay->checkBox("Enable Lighting", &materialData.useLighting)) {
			// When lighting is enabled, disable SH to avoid over-lighting
			if (materialData.useLighting) {
				materialData.useSH = 0;
			}
			materialDirty = true;
		}

		// Only show material properties when lighting is enabled
		if (materialData.useLighting) {
			overlay->text("Lighting Properties:");
			materialDirty |= overlay->inputFloat("Roughness", &materialData.roughness, 0.1f, 2);
			materialDirty |= overlay->inputFloat("Metallic", &materialData.metallic, 0.1f, 2);
			materialDirty |= overlay->inputFloat("Specular", &materialData.specular, 0.1f, 2);
		}

		// Always show color
		materialDirty |= overlay->colorPicker("Albedo", &materialData.elbedo.r);

		// SH toggle (mutually exclusive with direct lighting)
		if (overlay->checkBox("Use SH (Spherical Harmonics)", &materialData.useSH)) {
			// When SH is enabled, disable direct lighting to avoid over-brightness
			if (materialData.useSH) {
				materialData.useLighting = 0;
			}
			materialDirty = true;
		}

		// Reflection toggle
		materialDirty |= overlay->checkBox("Use Reflection", &materialData.useReflection);
	}

	if (materialDirty)
	{
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
		materialDirty = false;
	}
}

void PreviewModel::SetUseSHAndReflection(bool useSH, bool useReflection)
{
	materialData.useSH = useSH ? 1 : 0;
	materialData.useReflection = useReflection ? 1 : 0;
	// Immediately write to GPU buffer
	if (materialBuffer.mapped) {
		memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
	}
}

void PreviewModel::SetLightColor(const glm::vec3& color)
{
    // NOTE: Light color should NOT modify the material albedo!
    // Light color is a global property that affects all objects uniformly.
    // It should only be applied in the shader's lighting calculation,
    // not by changing the material properties.
    // This function is kept for API compatibility but does nothing.
    printf("[DEBUG] SetLightColor called with color: (%.2f, %.2f, %.2f) - NO-OP (light color is global)\n",
           color.r, color.g, color.b);
}