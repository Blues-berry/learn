/*
* Vulkan Example - Physical based shading basics with GPU-based clustered lighting
* Copyright (C) 2017-2024 by Sascha Willems - www.saschawillems.de
* This code is licensed under the MIT license
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"

const int maxnumLights = 16;
const uint32_t CLUSTER_SIZE_X = 4;
const uint32_t CLUSTER_SIZE_Y = 4;
const uint32_t CLUSTER_SIZE_Z = 4;
const uint32_t TOTAL_CLUSTERS = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
const uint32_t lightIndexListnum = maxnumLights * TOTAL_CLUSTERS;
// 定义栅栏超时时间为5秒（以纳秒为单位）
// 使用 VulkanTools.h 中定义的 DEFAULT_FENCE_TIMEOUT

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
private:
    VkFence computeFence;
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
        uint32_t maxlightindexnum; // 替换 padding 为 maxlightindexnum，与着色器保持一致
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
    VkQueue computeQueue = VK_NULL_HANDLE;

    std::vector<Material> materials;
    int32_t materialIndex = 0;
    std::vector<std::string> materialNames;
    std::vector<std::string> objectNames;

    uint32_t sphereIndexCount = 0;

    VulkanExample() : VulkanExampleBase(), computeFence(VK_NULL_HANDLE) {
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
            // 确保所有操作完成
            vkDeviceWaitIdle(device);

            // 销毁fence
            if (computeFence != VK_NULL_HANDLE) {
                vkDestroyFence(device, computeFence, nullptr);
                computeFence = VK_NULL_HANDLE;
            }

            // 销毁管线
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, pipeline, nullptr);
                pipeline = VK_NULL_HANDLE;
            }
            if (computePipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, computePipeline, nullptr);
                computePipeline = VK_NULL_HANDLE;
            }

            // 销毁管线布局
            if (pipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
                pipelineLayout = VK_NULL_HANDLE;
            }
            if (computePipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
                computePipelineLayout = VK_NULL_HANDLE;
            }

            // 销毁描述符集布局
            if (descriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
                descriptorSetLayout = VK_NULL_HANDLE;
            }

            // 释放命令缓冲区
            if (computeCmdBuffer != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(device, cmdPool, 1, &computeCmdBuffer);
                computeCmdBuffer = VK_NULL_HANDLE;
            }

            // 销毁缓冲区
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


            // 添加内存屏障
            VkMemoryBarrier memoryBarrier = {};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(drawCmdBuffers[i], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &memoryBarrier, 0, nullptr, 0, nullptr);

            //插入管线屏障：从计算着色器阶段到片段着色器阶段
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
        // 检查计算队列是否有效
        if (computeQueue == VK_NULL_HANDLE) {
            // 尝试重新获取计算队列
            vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.compute, 0, &computeQueue);
            if (computeQueue == VK_NULL_HANDLE) {
                std::cerr << "Error: Invalid compute queue" << std::endl;
                return; // 不抛出异常，而是返回
            }
        }

        // 检查命令缓冲区是否有效
        if (computeCmdBuffer == VK_NULL_HANDLE) {
            // 重新分配命令缓冲区
            VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
            VkResult result = vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &computeCmdBuffer);
            if (result != VK_SUCCESS) {
                std::cerr << "Error: Failed to allocate compute command buffer: " << result << std::endl;
                return; // 不抛出异常，而是返回
            }
        }

        // 检查计算管线是否有效
        if (computePipeline == VK_NULL_HANDLE) {
            std::cerr << "Error: Invalid compute pipeline" << std::endl;
            return; // 不抛出异常，而是返回
        }

        // 检查描述符集是否有效
        if (descriptorSet == VK_NULL_HANDLE) {
            std::cerr << "Error: Invalid descriptor set" << std::endl;
            return; // 不抛出异常，而是返回
        }

        // 确保之前的计算操作已完成
        if (computeFence != VK_NULL_HANDLE) {
            // 使用更长的超时时间
            VkResult waitResult = vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);
            if (waitResult == VK_SUCCESS) {
                // 重置fence以备下次使用
                vkResetFences(device, 1, &computeFence);
            } else {
                std::cerr << "Warning: Compute fence wait failed with error: " << waitResult << std::endl;
                
                // 根据错误类型采取不同的恢复策略
                if (waitResult == VK_TIMEOUT) {
                    // 超时情况下，尝试等待设备空闲
                    std::cerr << "Fence wait timed out, attempting to wait for device idle..." << std::endl;
                    vkDeviceWaitIdle(device);
                    // 重置fence
                    vkResetFences(device, 1, &computeFence);
                } else {
                    // 其他错误，尝试重新创建fence
                    std::cerr << "Recreating compute fence due to error..." << std::endl;
                    vkDestroyFence(device, computeFence, nullptr);
                    computeFence = VK_NULL_HANDLE;
                }
            }
        }

        // 如果fence无效，创建一个新的
        if (computeFence == VK_NULL_HANDLE) {
            VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();
            VkResult fenceResult = vkCreateFence(device, &fenceInfo, nullptr, &computeFence);
            if (fenceResult != VK_SUCCESS) {
                std::cerr << "Critical error: Failed to create compute fence: " << fenceResult << std::endl;
                // 尝试等待设备空闲作为最后的恢复手段
                vkDeviceWaitIdle(device);
                return; // 不抛出异常，而是返回
            }
        } else {
            // 确保fence处于重置状态
            vkResetFences(device, 1, &computeFence);
        }

        // 1. 重置命令缓冲区
        VkResult resetResult = vkResetCommandBuffer(computeCmdBuffer, 0);
        if (resetResult != VK_SUCCESS) {
            // 尝试重新分配命令缓冲区
            vkFreeCommandBuffers(device, cmdPool, 1, &computeCmdBuffer);
            VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
            resetResult = vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &computeCmdBuffer);
            if (resetResult != VK_SUCCESS) {
                std::cerr << "Error: Failed to reallocate compute command buffer: " << resetResult << std::endl;
                return; // 不抛出异常，而是返回
            }
        }

        // 2. 开始录制命令
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        cmdBufInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // 确保使用一次性提交标志
        VkResult beginResult = vkBeginCommandBuffer(computeCmdBuffer, &cmdBufInfo);
        if (beginResult != VK_SUCCESS) {
            std::cerr << "Error: Failed to begin compute command buffer: " << beginResult << std::endl;
            return; // 不抛出异常，而是返回
        }

        // 3. 绑定计算管线与描述符集
        vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout,
            0, 1, &descriptorSet, 0, nullptr);

        // 4. 派发计算工作组（每个灯光一个线程）
        vkCmdDispatch(computeCmdBuffer, maxnumLights, 1, 1);

        // 添加更全面的内存屏障，确保计算着色器的写入对后续渲染可见
        VkMemoryBarrier memoryBarrier = {};
        memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        
        // 添加缓冲区内存屏障，专门针对光照索引列表和集群数据
        VkBufferMemoryBarrier bufferBarriers[2] = {};
        
        // 光照索引列表屏障
        bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bufferBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarriers[0].buffer = uniformBuffers.clusterIndexList.buffer;
        bufferBarriers[0].offset = 0;
        bufferBarriers[0].size = VK_WHOLE_SIZE;
        
        // 集群数据屏障
        bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bufferBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarriers[1].buffer = uniformBuffers.clusterData.buffer;
        bufferBarriers[1].offset = 0;
        bufferBarriers[1].size = VK_WHOLE_SIZE;
        
        // 全面的管线屏障，确保计算结果对图形管线可见
        vkCmdPipelineBarrier(
            computeCmdBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
            0,
            1, &memoryBarrier,
            2, bufferBarriers,
            0, nullptr
        );

        // 5. 结束命令缓冲区录制
        VkResult endResult = vkEndCommandBuffer(computeCmdBuffer);
        if (endResult != VK_SUCCESS) {
            std::cerr << "Error: Failed to end compute command buffer: " << endResult << std::endl;
            return; // 不抛出异常，而是返回
        }

        // 6. 提交命令缓冲区到计算队列
        VkSubmitInfo submitInfo = vks::initializers::submitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &computeCmdBuffer;

        // 添加信号量以实现更精确的同步
        VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
        VkSemaphore computeCompleteSemaphore = VK_NULL_HANDLE;
        VkResult semaphoreResult = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &computeCompleteSemaphore);
        
        if (semaphoreResult == VK_SUCCESS) {
            // 使用信号量和栅栏进行双重同步
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &computeCompleteSemaphore;
            
            // 提交计算队列
            VkResult submitResult = vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
            if (submitResult != VK_SUCCESS) {
                std::cerr << "Error: Failed to submit compute queue: " << submitResult << std::endl;
                
                // 等待设备空闲后重试一次
                vkDeviceWaitIdle(device);
                vkResetFences(device, 1, &computeFence); // 确保fence重置
                
                submitResult = vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
                if (submitResult != VK_SUCCESS) {
                    std::cerr << "Error: Failed to submit compute queue after retry: " << submitResult << std::endl;
                    vkDestroySemaphore(device, computeCompleteSemaphore, nullptr);
                    return;
                }
            }
            
            // 创建一个命令缓冲区来执行队列所有权转移
            VkCommandBuffer transferCmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            
            // 添加缓冲区内存屏障，专门针对光照索引列表和集群数据
            VkBufferMemoryBarrier queueTransferBarriers[2] = {};
            
            // 光照索引列表屏障
            queueTransferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            queueTransferBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            queueTransferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            queueTransferBarriers[0].srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
            queueTransferBarriers[0].dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
            queueTransferBarriers[0].buffer = uniformBuffers.clusterIndexList.buffer;
            queueTransferBarriers[0].offset = 0;
            queueTransferBarriers[0].size = VK_WHOLE_SIZE;
            
            // 集群数据屏障
            queueTransferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            queueTransferBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            queueTransferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            queueTransferBarriers[1].srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
            queueTransferBarriers[1].dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
            queueTransferBarriers[1].buffer = uniformBuffers.clusterData.buffer;
            queueTransferBarriers[1].offset = 0;
            queueTransferBarriers[1].size = VK_WHOLE_SIZE;
            
            vkCmdPipelineBarrier(
                transferCmdBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                2, queueTransferBarriers,
                0, nullptr
            );
            
            vulkanDevice->flushCommandBuffer(transferCmdBuffer, queue, true);
            
            // 销毁信号量
            vkDestroySemaphore(device, computeCompleteSemaphore, nullptr);
        } else {
            // 如果创建信号量失败，回退到只使用栅栏
            std::cerr << "Warning: Failed to create compute semaphore, falling back to fence-only synchronization" << std::endl;
            
            // 提交计算队列
            VkResult submitResult = vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
            if (submitResult != VK_SUCCESS) {
                std::cerr << "Error: Failed to submit compute queue: " << submitResult << std::endl;
                
                // 等待设备空闲后重试一次
                vkDeviceWaitIdle(device);
                vkResetFences(device, 1, &computeFence); // 确保fence重置
                
                submitResult = vkQueueSubmit(computeQueue, 1, &submitInfo, computeFence);
                if (submitResult != VK_SUCCESS) {
                    std::cerr << "Error: Failed to submit compute queue after retry: " << submitResult << std::endl;
                    return;
                }
            }
        }
        
        // 不在这里等待fence完成，让render函数处理同步
       











            //7.20
        //vkResetCommandBuffer(computeCmdBuffer, 0);
        //VkCommandBufferBeginInfo beginInfo = {};
        //beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        //vkBeginCommandBuffer(computeCmdBuffer, &beginInfo);
        //VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        //VK_CHECK_RESULT(vkBeginCommandBuffer(computeCmdBuffer, &cmdBufInfo));
        //vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        //vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        //vkCmdDispatch(computeCmdBuffer, maxnumLights, 1, 1);
        //VK_CHECK_RESULT(vkEndCommandBuffer(computeCmdBuffer));
        //VkSubmitInfo submitInfo = vks::initializers::submitInfo();
        //submitInfo.commandBufferCount = 1;
        //submitInfo.pCommandBuffers = &computeCmdBuffer;

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
        VkDeviceSize alignedSizeClusterIndexList = (sizeof(clusterIndexList) + minAlignment - 1) & ~(minAlignment - 1);

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
        // 设置最大光源索引数量
        uboMatrices.maxlightindexnum = lightIndexListnum;
        memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
        uniformBuffers.object.flush(); // 确保同步
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

        // 确保光照数据正确同步到GPU
        memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
        
        // 使用内存屏障确保数据对GPU可见
        VkMappedMemoryRange memoryRange = {};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = uniformBuffers.params.memory;
        memoryRange.offset = 0;
        memoryRange.size = sizeof(uboParams);
        vkFlushMappedMemoryRanges(device, 1, &memoryRange);
        
        // 添加命令缓冲区来执行内存屏障
        VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        
        VkBufferMemoryBarrier bufferBarrier = {};
        bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        bufferBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        bufferBarrier.buffer = uniformBuffers.params.buffer;
        bufferBarrier.offset = 0;
        bufferBarrier.size = VK_WHOLE_SIZE;
        
        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            1, &bufferBarrier,
            0, nullptr
        );
        
        vulkanDevice->flushCommandBuffer(cmdBuffer, queue, true);

//    确保 updateUniformBuffers 和 updateLights 的 memcpy 操作后，调用 vkFlushMappedBufferMemory：
    }

    void prepare() {
        VulkanExampleBase::prepare();
        loadAssets();
        prepareUniformBuffers();

        // Allocate compute command buffer
        VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
        VkResult cmdResult = vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &computeCmdBuffer);
        if (cmdResult != VK_SUCCESS) {
            std::cerr << "Error: Failed to allocate compute command buffer: " << cmdResult << std::endl;
            computeCmdBuffer = VK_NULL_HANDLE;
        }

        // Get compute queue
        vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.compute, 0, &computeQueue);
        if (computeQueue == VK_NULL_HANDLE) {
            std::cerr << "Error: Failed to get compute queue" << std::endl;
        }

        // Create compute fence
        VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();
        VkResult fenceResult = vkCreateFence(device, &fenceInfo, nullptr, &computeFence);
        if (fenceResult != VK_SUCCESS) {
            std::cerr << "Warning: Failed to create compute fence: " << fenceResult << std::endl;
            computeFence = VK_NULL_HANDLE;
            // 尝试重新创建
            fenceResult = vkCreateFence(device, &fenceInfo, nullptr, &computeFence);
            if (fenceResult != VK_SUCCESS) {
                std::cerr << "Error: Failed to recreate compute fence: " << fenceResult << std::endl;
                computeFence = VK_NULL_HANDLE;
            }
        }

        setupDescriptors();
        preparePipelines();
        buildCommandBuffers();
        prepared = true;
    }

    void draw() {
        // 使用基类的标准绘制流程
        VulkanExampleBase::prepareFrame();

        // 检查命令缓冲区是否有效
        if (drawCmdBuffers[currentBuffer] == VK_NULL_HANDLE) {
            std::cerr << "Error: Command buffer is null" << std::endl;
            return;
        }

        // 创建一个fence用于同步
        VkFence fence;
        VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
        VkResult fenceResult = vkCreateFence(device, &fenceInfo, nullptr, &fence);
        if (fenceResult != VK_SUCCESS) {
            std::cerr << "Error: Failed to create fence: " << fenceResult << std::endl;
            return;
        }

        // 直接使用基类的提交机制
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

        // 使用基类的信号量机制
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &semaphores.presentComplete;

        // 创建本地变量以避免类型转换问题
        VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitInfo.pWaitDstStageMask = &waitStageMask;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &semaphores.renderComplete;

        // 提交到队列
        VkResult result = vkQueueSubmit(queue, 1, &submitInfo, fence);
        if (result != VK_SUCCESS) {
            std::cerr << "Error: Failed to submit draw command buffer: " << result << std::endl;
            vkDestroyFence(device, fence, nullptr);
            
            // 等待设备空闲后重试
            vkDeviceWaitIdle(device);
            
            // 尝试使用VK_NULL_HANDLE作为fence参数重新提交
            result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
            if (result != VK_SUCCESS) {
                std::cerr << "Error: Failed to submit draw command buffer without fence: " << result << std::endl;
                return;
            }
        }

        // 等待队列完成
        if (fence != VK_NULL_HANDLE) {
            // 使用定义的超时常量
            result = vkWaitForFences(device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
            if (result != VK_SUCCESS) {
                std::cerr << "Warning: Failed to wait for fence: " << result << std::endl;
                // 记录错误但不终止程序，可能在下一帧会完成
                // 尝试重置fence以避免资源泄漏
                vkResetFences(device, 1, &fence);
            } else {
                // 成功完成，销毁fence
                vkDestroyFence(device, fence, nullptr);
            }
        }

        VulkanExampleBase::submitFrame();
    }

    // 2. 修复render函数
    void render() {
        if (!prepared) return;
        updateUniformBuffers();

        // 调试信息
        uint32_t* counter = (uint32_t*)uniformBuffers.globalCounter.mapped;
        printf("Global counter before: %u\n", *counter);

        if (!paused) {
            updateLights();

            // 重置全局计数器
            uint32_t zero = 0;
            memcpy(uniformBuffers.globalCounter.mapped, &zero, sizeof(uint32_t));
            uniformBuffers.globalCounter.flush(); // 确保数据同步到GPU

            // 执行计算着色器
            dispatchCompute(); // 不需要try-catch，函数内部已处理错误

            // 调试：检查计算结果
            counter = (uint32_t*)uniformBuffers.globalCounter.mapped;
            printf("Global counter after compute: %u\n", *counter);

            // 使用栅栏而不是队列空闲来确保计算完成
            // 这样可以更精确地控制同步，避免不必要的等待
            if (computeFence != VK_NULL_HANDLE) {
                // 使用更长的超时时间等待计算完成
                VkResult fenceResult = vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);
                
                if (fenceResult == VK_SUCCESS) {
                    // 重置fence以备下次使用
                    vkResetFences(device, 1, &computeFence);
                    
                    // 调试：验证计算结果
                    counter = (uint32_t*)uniformBuffers.globalCounter.mapped;
                    printf("Global counter after fence wait: %u\n", *counter);
                    
                    // 确保缓冲区数据对GPU可见
                    uniformBuffers.clusterData.flush();
                    uniformBuffers.clusterIndexList.flush();
                    
                    // 添加内存屏障，确保计算结果对图形管线可见
                    VkCommandBuffer cmdBuffer = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
                    
                    VkBufferMemoryBarrier bufferBarriers[2] = {};
                    
                    // 光照索引列表屏障
                    bufferBarriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    bufferBarriers[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                    bufferBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    bufferBarriers[0].srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
                    bufferBarriers[0].dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
                    bufferBarriers[0].buffer = uniformBuffers.clusterIndexList.buffer;
                    bufferBarriers[0].offset = 0;
                    bufferBarriers[0].size = VK_WHOLE_SIZE;
                    
                    // 集群数据屏障
                    bufferBarriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
                    bufferBarriers[1].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                    bufferBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    bufferBarriers[1].srcQueueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
                    bufferBarriers[1].dstQueueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;
                    bufferBarriers[1].buffer = uniformBuffers.clusterData.buffer;
                    bufferBarriers[1].offset = 0;
                    bufferBarriers[1].size = VK_WHOLE_SIZE;
                    
                    vkCmdPipelineBarrier(
                        cmdBuffer,
                        VK_PIPELINE_STAGE_HOST_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0,
                        0, nullptr,
                        2, bufferBarriers,
                        0, nullptr
                    );
                    
                    vulkanDevice->flushCommandBuffer(cmdBuffer, queue, true);
                } else {
                    std::cerr << "Warning: Compute fence wait failed with error: " << fenceResult << std::endl;
                    
                    // 如果等待失败，尝试等待设备空闲作为后备方案
                    if (computeQueue != VK_NULL_HANDLE) {
                        std::cerr << "Attempting to wait for compute queue idle as fallback..." << std::endl;
                        VkResult queueIdleResult = vkQueueWaitIdle(computeQueue);
                        if (queueIdleResult != VK_SUCCESS) {
                            std::cerr << "Critical error: Failed to wait for compute queue idle: " << queueIdleResult << std::endl;
                        } else {
                            // 队列空闲成功，重置fence
                            vkResetFences(device, 1, &computeFence);
                        }
                    }
                }
            }
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
