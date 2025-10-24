#include "ProbeVisualizer.h"
#include "VulkanDevice.h"

ProbeVisualizer::ProbeVisualizer(vks::VulkanDevice* dev, IExampleInterfasce* example) 
    : device(dev), iLoader(example)
{
    PreparePerBatchResource();
    UpdateSet();
}

void ProbeVisualizer::Initialize()
{
    // ✅ 修复：使用 PreviewModel 中已经加载的球体模型
    // 不需要重新加载，因为球体模型已经在 LoadAssets() 中加载过了
    // 我们只需要在 DrawProbe 时使用已有的模型

    // 注意：sphereModel 将在 DrawProbe 时从外部传入或使用预加载的模型
    // 这里不需要加载，避免重复加载和卡死
}

void ProbeVisualizer::Destroy()
{
    localBuffer.destroy();
    materialBuffer.destroy();
    sphereModel = nullptr;
    
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

void ProbeVisualizer::DrawProbe(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech,
                                const glm::vec3& position, const glm::vec4& color, bool bindPipeline)
{
    if (!sphereModel)
    {
        return;
    }

    uint32_t techIdx = (uint32_t)tech;

    // 应用位置和缩放变换
    glm::mat4 translate = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(probeScale));
    localData.transform = translate * scale;
    memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));

    // 更新材质颜色
    materialData.elbedo = color;
    memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));

    // ✅ 只在第一次绘制时绑定管线和描述符集
    if (bindPipeline) {
        std::vector<VkDescriptorSet> sets = {
            globalSet, descriptorSet
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pipelineLayout, 0,
                               static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pso);

        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, &sphereModel->vertices.buffer, offsets);
        vkCmdBindIndexBuffer(cmd, sphereModel->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    // ✅ 关键修复：不使用 BindImages 标志，避免覆盖我们的描述符集
    // BindImages 标志会在 drawNode 中绑定材质描述符集，覆盖我们的描述符集
    sphereModel->draw(cmd, 0, techniques[techIdx].pipelineLayout, 1);
}

void ProbeVisualizer::DrawProbes(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech,
                                 const std::vector<glm::vec3>& positions)
{
    if (!sphereModel || positions.empty()) return;

    uint32_t techIdx = (uint32_t)tech;
    std::vector<VkDescriptorSet> sets = {
        globalSet, descriptorSet
    };

    // ✅ 只在第一次绑定管线和描述符集
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pipelineLayout, 0,
                           static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, techniques[techIdx].pso);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, &sphereModel->vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmd, sphereModel->indices.buffer, 0, VK_INDEX_TYPE_UINT32);

    // 绘制多个探针，每个使用不同的颜色
    for (size_t i = 0; i < positions.size(); ++i)
    {
        // 根据索引生成不同的颜色
        float hue = static_cast<float>(i) / static_cast<float>(positions.size());
        glm::vec4 color = glm::vec4(
            0.5f + 0.5f * std::sin(hue * 6.28f),
            0.5f + 0.5f * std::sin(hue * 6.28f + 2.09f),
            0.5f + 0.5f * std::sin(hue * 6.28f + 4.18f),
            1.0f
        );

        // 应用位置和缩放变换
        glm::mat4 translate = glm::translate(glm::mat4(1.0f), positions[i]);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(probeScale));
        localData.transform = translate * scale;
        memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));

        // 更新材质颜色
        materialData.elbedo = color;
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));

        // ✅ 直接遍历节点并绘制，避免 draw() 的复杂逻辑
        for (auto& node : sphereModel->nodes) {
            DrawNodeDirect(node, cmd, techniques[techIdx].pipelineLayout);
        }
    }
}

// ✅ 新增：直接绘制节点，避免 draw() 中的 buffersBound 问题
void ProbeVisualizer::DrawNodeDirect(vkglTF::Node* node, VkCommandBuffer cmd, VkPipelineLayout pipelineLayout)
{
    if (node->mesh) {
        for (vkglTF::Primitive* primitive : node->mesh->primitives) {
            // 直接绘制，不绑定材质描述符集
            vkCmdDrawIndexed(cmd, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
        }
    }
    for (auto& child : node->children) {
        DrawNodeDirect(child, cmd, pipelineLayout);
    }
}

void ProbeVisualizer::PreparePerBatchResource()
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

    localData.transform = glm::mat4();
    memcpy(localBuffer.mapped, &localData, sizeof(LocalBuffer));
    memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
}

void ProbeVisualizer::UpdateSet()
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void ProbeVisualizer::PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique)
{
    VkDevice rawDevice = device->logicalDevice;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
        vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState =
        vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState =
        vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState =
        vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState =
        vks::initializers::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState =
        vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    
    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
        vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    std::vector<VkDescriptorSetLayout> setLayotus = {
        passLayout,
        descriptorSetLayout
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayotus.data(), static_cast<uint32_t>(setLayotus.size()));
    VK_CHECK_RESULT(vkCreatePipelineLayout(rawDevice, &pipelineLayoutCreateInfo, nullptr, &techniques[(uint32_t)technique].pipelineLayout));
    
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

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

    shaderStages[0] = iLoader->LoadShader("lightprobesh2/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/lightprobesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &techniques[(uint32_t)technique].pso));
}

