/*
* Vulkan Example - Physical based shading basics with GPU-based clustered lighting
* Copyright (C) 2017-2024 by Sascha Willems - www.saschawillems.de
* This code is licensed under the MIT license
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

const int maxnumLights = 64;
const uint32_t CLUSTER_SIZE_X = 8;
const uint32_t CLUSTER_SIZE_Y = 8;
const uint32_t CLUSTER_SIZE_Z = 8;
const uint32_t TOTAL_CLUSTERS = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
const uint32_t lightIndexListnum = maxnumLights * TOTAL_CLUSTERS;

struct Material {
    struct PushBlock {
        float roughness;
        float metallic;
        float r, g, b;
    } params{};
    std::string name;
    Material() {}
    Material(std::string n, glm::vec3 c, float r, float m) : name(n) {
        params.roughness = r;
        params.metallic = m;
        params.r = c.r;
        params.g = c.g;
        params.b = c.b;
    }
};

struct Light {
    glm::vec4 position;
    glm::vec4 colorAndRadius;
    glm::vec4 direction;
    glm::vec4 cutOff;
};

struct ClusterCountsandOffsets {
    struct Cluster {
        uint32_t count;
        uint32_t offset;
        float padding[2];
    };
    Cluster cluster[TOTAL_CLUSTERS];
};

struct ClusterIndexList {
    struct Indices {
        uint32_t clusterIndexList;
        float padding[3];
    };
    Indices indices[maxnumLights * TOTAL_CLUSTERS];
};

struct UBOParams {
    Light lights[maxnumLights];
};

class VulkanExample : public VulkanExampleBase {
public:
    struct Meshes {
        std::vector<vkglTF::Model> objects;
        int32_t objectIndex = 0;
    };
    Meshes models;

    struct {
        vks::Buffer object;
        vks::Buffer params;
        vks::Buffer clusterData;
        vks::Buffer clusterIndexList;
        vks::Buffer globalCounter; // Added for Compute Shader
        vks::Buffer sphereVertex;
        vks::Buffer sphereIndex;
        vks::Buffer sphereNormal;
    } uniformBuffers;

    struct UBOMatrices {
        glm::mat4 projection;
        glm::mat4 model;
        glm::mat4 view;
        glm::vec3 camPos;
        float padding;
    } uboMatrices;

    UBOParams uboParams;
    ClusterCountsandOffsets clusterData;
    ClusterIndexList clusterIndexList;

    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline pipeline{ VK_NULL_HANDLE };
    VkPipeline computePipeline{ VK_NULL_HANDLE }; // Added for Compute Shader
    VkPipelineLayout computePipelineLayout{ VK_NULL_HANDLE }; // Added for Compute Shader
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    VkCommandBuffer computeCmdBuffer{ VK_NULL_HANDLE }; // Added for Compute Shader

    std::vector<Material> materials;
    int32_t materialIndex = 0;
    std::vector<std::string> materialNames;
    std::vector<std::string> objectNames;

    uint32_t sphereIndexCount = 0;

    VulkanExample() : VulkanExampleBase() {
        title = "Physical based shading basics";
        camera.type = Camera::CameraType::firstperson;
        camera.setPosition(glm::vec3(10.0f, 13.0f, 1.8f));
        camera.setRotation(glm::vec3(-62.5f, 90.0f, 0.0f));
        camera.movementSpeed = 8.0f;
        camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
        camera.rotationSpeed = 0.25f;
        timerSpeed *= 0.25f;

        materials.push_back(Material("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f), 0.1f, 1.0f));
        materials.push_back(Material("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f), 0.1f, 1.0f));
        materials.push_back(Material("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f), 0.1f, 1.0f));
        materials.push_back(Material("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f), 0.1f, 1.0f));
        materials.push_back(Material("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f), 0.1f, 1.0f));
        materials.push_back(Material("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f), 0.1f, 1.0f));
        materials.push_back(Material("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f), 0.1f, 1.0f));
        materials.push_back(Material("planematerial", glm::vec3(0.955008f, 0.654864f, 0.336057f), 0.1f, 1.0f));
        materials.push_back(Material("White", glm::vec3(1.0f), 0.1f, 1.0f));
        materials.push_back(Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 0.1f, 1.0f));
        materials.push_back(Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.1f, 1.0f));
        materials.push_back(Material("Black", glm::vec3(0.0f), 0.1f, 1.0f));

        for (auto material : materials) {
            materialNames.push_back(material.name);
        }

        objectNames = { "Sphere", "Teapot", "Torusknot", "Venus", "plane", "plane_circle" };
        materialIndex = 0;
    }

    ~VulkanExample() {
        if (device) {
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipeline(device, computePipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            vkFreeCommandBuffers(device, cmdPool, 1, &computeCmdBuffer);
            uniformBuffers.object.destroy();
            uniformBuffers.params.destroy();
            uniformBuffers.clusterData.destroy();
            uniformBuffers.clusterIndexList.destroy();
            uniformBuffers.globalCounter.destroy();
            uniformBuffers.sphereVertex.destroy();
            uniformBuffers.sphereIndex.destroy();
            uniformBuffers.sphereNormal.destroy();
        }
    }

    void generateSphereGeometry(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<uint32_t>& indices, uint32_t sectors = 64, uint32_t stacks = 64) {
        vertices.clear();
        normals.clear();
        indices.clear();
        const float PI = 3.14159265359f;
        float sectorStep = 2 * PI / sectors;
        float stackStep = PI / stacks;

        for (uint32_t i = 0; i <= stacks; ++i) {
            float stackAngle = PI / 2 - i * stackStep;
            float xy = cosf(stackAngle);
            float z = sinf(stackAngle);

            for (uint32_t j = 0; j <= sectors; ++j) {
                float sectorAngle = j * sectorStep;
                glm::vec3 vertex;
                vertex.x = xy * cosf(sectorAngle);
                vertex.y = xy * sinf(sectorAngle);
                vertex.z = z;
                vertices.push_back(vertex);
                normals.push_back(glm::normalize(vertex));
            }
        }

        for (uint32_t i = 0; i < stacks; ++i) {
            uint32_t k1 = i * (sectors + 1);
            uint32_t k2 = k1 + sectors + 1;

            for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if (i != (stacks - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
    }

    void prepareSphereBuffers() {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<uint32_t> indices;
        generateSphereGeometry(vertices, normals, indices);
        sphereIndexCount = static_cast<uint32_t>(indices.size());

        VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
        vks::Buffer stagingVertexBuffer;
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingVertexBuffer,
            vertexBufferSize,
            vertices.data()));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uniformBuffers.sphereVertex,
            vertexBufferSize));
        vulkanDevice->copyBuffer(&stagingVertexBuffer, &uniformBuffers.sphereVertex, queue);
        stagingVertexBuffer.destroy();

        VkDeviceSize normalBufferSize = normals.size() * sizeof(glm::vec3);
        vks::Buffer stagingNormalBuffer;
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingNormalBuffer,
            normalBufferSize,
            normals.data()));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uniformBuffers.sphereNormal,
            normalBufferSize));
        vulkanDevice->copyBuffer(&stagingNormalBuffer, &uniformBuffers.sphereNormal, queue);
        stagingNormalBuffer.destroy();

        VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
        vks::Buffer stagingIndexBuffer;
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingIndexBuffer,
            indexBufferSize,
            indices.data()));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uniformBuffers.sphereIndex,
            indexBufferSize));
        vulkanDevice->copyBuffer(&stagingIndexBuffer, &uniformBuffers.sphereIndex, queue);
        stagingIndexBuffer.destroy();
    }

    void buildCommandBuffers() {
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        VkClearValue clearValues[2];
        clearValues[0].color = defaultClearColor;
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = width;
        renderPassBeginInfo.renderArea.extent.height = height;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;

        for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
            renderPassBeginInfo.framebuffer = frameBuffers[i];
            VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

            VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

            Material mat = materials[materialIndex];
            const uint32_t gridSize = 7;

            for (uint32_t y = 0; y < gridSize; y++) {
                for (uint32_t x = 0; x < gridSize; x++) {
                    glm::vec3 pos = glm::vec3(float(x - (gridSize / 2.0f)) * 2.5f, 0.0f, float(y - (gridSize / 2.0f)) * 2.5f);
                    vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
                    vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &mat.params);
                    models.objects[models.objectIndex].draw(drawCmdBuffers[i]);
                }
            }

            VkDeviceSize offsets[] = { 0, 0 };
            VkBuffer vertexBuffers[] = { uniformBuffers.sphereVertex.buffer, uniformBuffers.sphereNormal.buffer };
            vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 2, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(drawCmdBuffers[i], uniformBuffers.sphereIndex.buffer, 0, VK_INDEX_TYPE_UINT32);

            for (int j = 0; j < maxnumLights; ++j) {
                glm::vec3 pos = glm::vec3(uboParams.lights[j].position);
                vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
                Material::PushBlock dummyMat = { 0.5f, 0.1f, uboParams.lights[j].colorAndRadius.x, uboParams.lights[j].colorAndRadius.y, uboParams.lights[j].colorAndRadius.z };
                vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &dummyMat);
                vkCmdDrawIndexed(drawCmdBuffers[i], sphereIndexCount, 1, 0, 0, j);
            }

            drawUI(drawCmdBuffers[i]);
            vkCmdEndRenderPass(drawCmdBuffers[i]);
            VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
        }
    }

    void dispatchCompute() {
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        VK_CHECK_RESULT(vkBeginCommandBuffer(computeCmdBuffer, &cmdBufInfo));

        vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDispatch(computeCmdBuffer, maxnumLights, 1, 1);

        // 添加内存屏障
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(computeCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

        VK_CHECK_RESULT(vkEndCommandBuffer(computeCmdBuffer));

        VkSubmitInfo submitInfo = vks::initializers::submitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &computeCmdBuffer;
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        vkQueueWaitIdle(queue);
    }

    void loadAssets() {
        std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf", "plane.gltf", "plane_circle.gltf" };
        models.objects.resize(filenames.size());
        for (size_t i = 0; i < filenames.size(); i++) {
            models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue,
                vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY);
        }
    }

    void setupDescriptors() {
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8), // 增加数量
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4), // 增加数量
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT, 3),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4), // globalCounter
        };
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.object.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &uniformBuffers.clusterIndexList.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &uniformBuffers.clusterData.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &uniformBuffers.globalCounter.descriptor),
        };
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
    }

    VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage) {
        VkPipelineShaderStageCreateInfo shaderStage = {};
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = stage;
        shaderStage.pName = "main";
        std::string spirvFile = fileName;
        if (fileName.ends_with(".hlsl")) {
            spirvFile = fileName.substr(0, fileName.size() - 5) + ".spv";
        }
        shaderStage.module = vks::tools::loadShader(spirvFile.c_str(), device);
        return shaderStage;
    }

	void preparePipelines() {
		// 图形管线
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0),
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),
		};
		pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
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

		shaderStages[0] = loadShader(getShadersPath() + "pbrbasic/pbr.vert.hlsl", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbrbasic/pbr.frag.hlsl", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// 计算管线
		VkPipelineLayoutCreateInfo computePipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &computePipelineLayoutCI, nullptr, &computePipelineLayout));

		VkComputePipelineCreateInfo computePipelineCI = {};
		computePipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCI.layout = computePipelineLayout;
		computePipelineCI.stage = loadShader(getShadersPath() + "pbrbasic/cluster.comp.hlsl", VK_SHADER_STAGE_COMPUTE_BIT);
		computePipelineCI.basePipelineHandle = VK_NULL_HANDLE;
		computePipelineCI.basePipelineIndex = -1;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCI, nullptr, &computePipeline));
	}

    void prepareUniformBuffers() {



        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        VkDeviceSize minAlignment = properties.limits.minUniformBufferOffsetAlignment;
        VkDeviceSize alignedSizeClusterIndexList = ((sizeof(clusterIndexList) + minAlignment - 1) / minAlignment) * minAlignment;

        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.object,
            sizeof(uboMatrices)));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.params,
            sizeof(uboParams)));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.clusterData,
            sizeof(clusterData)));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.clusterIndexList,
            alignedSizeClusterIndexList));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.globalCounter,
            sizeof(uint32_t)));

        VK_CHECK_RESULT(uniformBuffers.object.map());
        VK_CHECK_RESULT(uniformBuffers.params.map());
        VK_CHECK_RESULT(uniformBuffers.clusterData.map());
        VK_CHECK_RESULT(uniformBuffers.clusterIndexList.map());
        VK_CHECK_RESULT(uniformBuffers.globalCounter.map());

        //初始化数据
        memset(uniformBuffers.clusterData.mapped, 0, sizeof(clusterData));
        memset(uniformBuffers.clusterIndexList.mapped, 0, alignedSizeClusterIndexList);

        prepareSphereBuffers();
    }

    void updateUniformBuffers() {
        uboMatrices.projection = camera.matrices.perspective;
        uboMatrices.view = camera.matrices.view;
        float rotationAngle = -90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f);
        uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        uboMatrices.camPos = camera.position * -1.0f;
        memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
        uniformBuffers.object.flush();
        //在 prepareUniformBuffers 中，所有缓冲区都被映射为 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT，但未检查是否在 GPU 内存中正确更新。在 render 函数中，globalCounter 被重置为零，但可能未及时同步到 GPU。

        //解决方案：

        //    确保 updateUniformBuffers 和 updateLights 的 memcpy 操作后，调用 vkFlushMappedBufferMemory：
    
    }

    void updateLights() {
        const float p = 15.0f;
        const int gridSize = static_cast<int>(ceil(sqrt(static_cast<float>(maxnumLights))));
        const float spacing = 2.0f * p / (gridSize - 1);

        int lightIndex = 0;
        for (int y = 0; y < gridSize && lightIndex < maxnumLights; y++) {
            for (int x = 0; x < gridSize && lightIndex < maxnumLights; x++) {
                float posX = -p + x * spacing;
                float posZ = -p + y * spacing;
                float posY = -p * 0.5f;

                uboParams.lights[lightIndex].position = glm::vec4(posX, posY, posZ, 1.0f);

                glm::vec3 color;
                switch (lightIndex % 4) {
                case 0: color = glm::vec3(1.0f, 0.0f, 0.0f); break;
                case 1: color = glm::vec3(0.0f, 1.0f, 0.0f); break;
                case 2: color = glm::vec3(0.0f, 0.0f, 1.0f); break;
                case 3: color = glm::vec3(1.0f, 1.0f, 0.0f); break;
                }
                uboParams.lights[lightIndex].colorAndRadius = glm::vec4(color, 15.0f);
                glm::vec3 direction = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(posX, posY, posZ));
                uboParams.lights[lightIndex].direction = glm::vec4(direction, 1.0f);
                uboParams.lights[lightIndex].cutOff = glm::vec4(12.5f, 18.5f, 0.0f, 0.0f);
                lightIndex++;
            }
        }

        if (!paused) {
            for (int i = 0; i < maxnumLights; i++) {
                uboParams.lights[i].position.x += sin(glm::radians(timer * 360.0f)) * 0.1f;
                uboParams.lights[i].position.z += cos(glm::radians(timer * 360.0f)) * 0.1f;
            }
        }

        memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
        uniformBuffers.params.flush();

        //在 prepareUniformBuffers 中，所有缓冲区都被映射为 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT，但未检查是否在 GPU 内存中正确更新。在 render 函数中，globalCounter 被重置为零，但可能未及时同步到 GPU。

//解决方案：

//    确保 updateUniformBuffers 和 updateLights 的 memcpy 操作后，调用 vkFlushMappedBufferMemory：
    }

    void prepare() {
        VulkanExampleBase::prepare();
        loadAssets();
        prepareUniformBuffers();

        // Allocate compute command buffer
        VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
        VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &computeCmdBuffer));

        setupDescriptors();
        preparePipelines();
        buildCommandBuffers();
        prepared = true;
    }

    void draw() {
        VulkanExampleBase::prepareFrame();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        VulkanExampleBase::submitFrame();
    }

    void render() {
        if (!prepared) return;
        updateUniformBuffers();
        // ... 调试信息..
        uint32_t* counter = (uint32_t*)uniformBuffers.globalCounter.mapped;
        printf("Global counter: %u\n", *counter);
        ClusterCountsandOffsets* clusterData = (ClusterCountsandOffsets*)uniformBuffers.clusterData.mapped;


        for (uint32_t i = 0; i < TOTAL_CLUSTERS; i++) {
            printf("Cluster %u: count=%u, offset=%u\n", i, clusterData->cluster[i].count, clusterData->cluster[i].offset);
        }

        if (!paused) {
            updateLights();
            uint32_t zero = 0;
            memcpy(uniformBuffers.globalCounter.mapped, &zero, sizeof(uint32_t)); // Reset global counter
            dispatchCompute(); // Run Compute Shader

            // 等待计算完成
            VkFence fence;
            VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();
            vkCreateFence(device, &fenceInfo, nullptr, &fence);
            vkQueueSubmit(queue, 1, &submitInfo, fence);
            vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(device, fence, nullptr);


        }
        draw();
    }

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay) {
        if (overlay->header("Settings")) {
            if (overlay->comboBox("Material", &materialIndex, materialNames)) {
                buildCommandBuffers();
            }
            if (overlay->comboBox("Type", &models.objectIndex, objectNames)) {
                updateUniformBuffers();
                buildCommandBuffers();
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
