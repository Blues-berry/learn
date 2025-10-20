/*
 * glTF model loading implementation for Vulkan
 * Based on glTF loading example from Sascha Willems
 */

// 定义宏以启用 stb_image 的实现，用于图像加载
// 注意：不定义TINYGLTF_IMPLEMENTATION，因为它已经在base.lib中实现
#define STB_IMAGE_IMPLEMENTATION
// 定义宏以禁用 tinygltf 的图像写入功能
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "gltfload.h"
#include "vulkantools.h"
#include "tiny_gltf.h"

namespace gltf {

    Model::~Model()
    {
        // 释放节点
        for (auto node : nodes) {
            delete node;
        }
        // Release all Vulkan resources allocated for the model
        // 销毁顶点缓冲区
        vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
        vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
        // 销毁索引缓冲区
        vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
        vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);
        // 销毁所有图像资源
        for (Image image : images) {
            vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
            vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
            vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
            vkFreeMemory(vulkanDevice->logicalDevice, image.texture.deviceMemory, nullptr);
        }
    }

    bool Model::loadFromFile(const std::string& filename, vks::VulkanDevice* device, VkQueue queue, uint32_t glTFLoadingFlags)
    {
        tinygltf::Model glTFInput;
        tinygltf::TinyGLTF gltfContext;
        std::string error, warning;

        // Pass some Vulkan resources required for setup and rendering to the glTF model loading class
        vulkanDevice = device;
        copyQueue = queue;

        std::vector<uint32_t> indexBuffer;
        std::vector<Vertex> vertexBuffer;

        // 使用 tinygltf 加载文件
        bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

        // 如果加载成功
        if (fileLoaded) {
            loadImages(glTFInput);
            loadMaterials(glTFInput);
            loadTextures(glTFInput);
            const tinygltf::Scene& scene = glTFInput.scenes[0];
            for (size_t i = 0; i < scene.nodes.size(); i++) {
                const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
                loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
            }
        }
        else {
            std::cerr << "Could not open the glTF file: " << error << std::endl;
            return false;
        }

        // Create and upload vertex and index buffer
        // We will be using one single vertex buffer and one single index buffer for the whole glTF scene
        // Primitives (of the glTF model) will then index into these using index offsets

        // 计算缓冲大小
        size_t vertexBufferSize = vertexBuffer.size() * sizeof(Vertex);
        size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
        indices.count = static_cast<uint32_t>(indexBuffer.size());

        // 暂存缓冲结构
        struct StagingBuffer {
            VkBuffer buffer;
            VkDeviceMemory memory;
        } vertexStaging, indexStaging;

        // Create host visible staging buffers (source)
        // 创建顶点暂存缓冲
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vertexBufferSize,
            &vertexStaging.buffer,
            &vertexStaging.memory,
            vertexBuffer.data()));
        // Index data
        // 创建索引暂存缓冲
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            indexBufferSize,
            &indexStaging.buffer,
            &indexStaging.memory,
            indexBuffer.data()));

        // Create device local buffers (target)
        // 创建设备本地顶点缓冲
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertexBufferSize,
            &vertices.buffer,
            &vertices.memory));
        // 创建设备本地索引缓冲
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            indexBufferSize,
            &indices.buffer,
            &indices.memory));

        // Copy data from staging buffers (host) do device local buffer (gpu)
        // 创建命令缓冲进行复制
        VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy copyRegion = {};

        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(
            copyCmd,
            vertexStaging.buffer,
            vertices.buffer,
            1,
            &copyRegion);

        copyRegion.size = indexBufferSize;
        vkCmdCopyBuffer(
            copyCmd,
            indexStaging.buffer,
            indices.buffer,
            1,
            &copyRegion);

        // 刷新命令缓冲
        vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

        // Free staging resources
        // 释放暂存资源
        vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
        vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
        vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
        vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);

        return true;
    }

    void Model::loadImages(tinygltf::Model& input)
    {
        // Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
        // loading them from disk, we fetch them from the glTF loader and upload the buffers
        // 调整图像数组大小
        images.resize(input.images.size());
        // 遍历所有图像
        for (size_t i = 0; i < input.images.size(); i++) {
            // 用引用获取临时变量，减少开销
            tinygltf::Image& glTFImage = input.images[i];
            // Get the image data from the glTF loader
            // 创建临时变量以及标志
            unsigned char* buffer = nullptr;
            VkDeviceSize bufferSize = 0;
            bool deleteBuffer = false;
            // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
            // 如果是 RGB 格式，转换为 RGBA
            if (glTFImage.component == 3) {
                // 容量扩充
                bufferSize = glTFImage.width * glTFImage.height * 4;
                buffer = new unsigned char[bufferSize];
                unsigned char* rgba = buffer;
                unsigned char* rgb = &glTFImage.image[0];
                for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
                    memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                    rgba += 4;
                    rgb += 3;
                }
                deleteBuffer = true;
            }
            // 否则直接使用原缓冲
            else {
                buffer = &glTFImage.image[0];
                bufferSize = glTFImage.image.size();
            }
            // Load texture from image buffer
            // 从缓冲加载纹理到 Vulkan
            images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vulkanDevice, copyQueue);
            // 如果转换了缓冲，释放它
            if (deleteBuffer) {
                delete[] buffer;
            }
        }
    }

    void Model::loadTextures(tinygltf::Model& input)
    {
        // 调整纹理数组大小
        textures.resize(input.textures.size());
        // 遍历纹理，设置图像索引
        for (size_t i = 0; i < input.textures.size(); i++) {
            textures[i].imageIndex = input.textures[i].source;
        }
    }

    void Model::loadMaterials(tinygltf::Model& input)
    {
        // 调整材质数组大小
        materials.resize(input.materials.size());
        // 遍历材质
        for (size_t i = 0; i < input.materials.size(); i++) {
            // We only read the most basic properties required for our sample
            tinygltf::Material glTFMaterial = input.materials[i];
            // Get the base color factor
            // 获取基础颜色因子
            if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
                materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
            }
            // Get base color texture index
            // 获取基础颜色纹理索引
            if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
                materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
            }
        }
    }

    void Model::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, 
                        std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer)
    {
        // 创建新节点
        Node* node = new Node{};
        // 初始化矩阵为单位矩阵
        node->matrix = glm::mat4(1.0f);
        node->parent = parent;

        // Get the local node matrix
        // It's either made up from translation, rotation, scale or a 4x4 matrix
        // 应用平移
        if (inputNode.translation.size() == 3) {
            node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
        }
        // 应用旋转
        if (inputNode.rotation.size() == 4) {
            glm::quat q = glm::make_quat(inputNode.rotation.data());
            node->matrix *= glm::mat4(q);
        }
        // 应用缩放
        if (inputNode.scale.size() == 3) {
            node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
        }
        // 直接应用矩阵
        if (inputNode.matrix.size() == 16) {
            node->matrix = glm::make_mat4x4(inputNode.matrix.data());
        };

        // Load node's children
        // 加载子节点
        if (inputNode.children.size() > 0) {
            for (size_t i = 0; i < inputNode.children.size(); i++) {
                loadNode(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
            }
        }

        // If the node contains mesh data, we load vertices and indices from the buffers
        // In glTF this is done via accessors and buffer views
        // 如果节点有网格
        if (inputNode.mesh > -1) {
            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            // Iterate through all primitives of this node's mesh
            // 遍历网格的所有图元
            for (size_t i = 0; i < mesh.primitives.size(); i++) {
                const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
                // 计算索引偏移和顶点起始
                uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
                uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
                uint32_t indexCount = 0;
                // Vertices
                // 处理顶点数据
                {
                    const float* positionBuffer = nullptr;
                    const float* normalsBuffer = nullptr;
                    const float* texCoordsBuffer = nullptr;
                    size_t vertexCount = 0;

                    // Get buffer data for vertex positions
                    // 获取位置数据
                    if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;
                    }
                    // Get buffer data for vertex normals
                    // 获取法线数据
                    if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }
                    // Get buffer data for vertex texture coordinates
                    // glTF supports multiple sets, we only load the first one
                    // 获取纹理坐标（仅第一个集）
                    if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    // Append data to model's vertex buffer
                    // 将顶点数据追加到缓冲
                    for (size_t v = 0; v < vertexCount; v++) {
                        Vertex vert{};
                        vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                        vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        vert.color = glm::vec3(1.0f);
                        vertexBuffer.push_back(vert);
                    }
                }
                // Indices
                // 处理索引数据
                {
                    const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
                    const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                    indexCount += static_cast<uint32_t>(accessor.count);

                    // glTF supports different component types of indices
                    // 根据组件类型处理索引
                    switch (accessor.componentType) {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                        const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                        const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                        const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++) {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    default:
                        std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                        return;
                    }
                }
                // 创建图元并添加到网格
                Primitive primitive{};
                primitive.firstIndex = firstIndex;
                primitive.indexCount = indexCount;
                primitive.materialIndex = glTFPrimitive.material;
                node->mesh.primitives.push_back(primitive);
            }
        }

        // 如果有父节点，添加到子列表，否则添加到顶级节点
        if (parent) {
            parent->children.push_back(node);
        }
        else {
            nodes.push_back(node);
        }
    }

    void Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, glm::mat4 offsetMatrix)
    {
        // All vertices and indices are stored in single buffers, so we only need to bind once
        // 绑定顶点和索引缓冲
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
        // Render all nodes at top-level
        // 绘制所有顶级节点
        for (auto& node : nodes) {
            // 应用偏移矩阵
            node->matrix = offsetMatrix * node->matrix;
            drawNode(commandBuffer, pipelineLayout, node);
            // 恢复原始矩阵，不影响后续绘制
            node->matrix = glm::inverse(offsetMatrix) * node->matrix;
        }
    }

    void Model::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node)
    {
        // 如果节点有图元
        if (node->mesh.primitives.size() > 0) {
            // Pass the node's matrix via push constants
            // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
            // 计算最终变换矩阵
            glm::mat4 nodeMatrix = node->matrix;
            Node* currentParent = node->parent;
            while (currentParent) {
                nodeMatrix = currentParent->matrix * nodeMatrix;
                currentParent = currentParent->parent;
            }
            // Pass the final matrix to the vertex shader using push constants
            // 使用 push constant 传递矩阵
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
            // 遍历图元
            for (Primitive& primitive : node->mesh.primitives) {
                if (primitive.indexCount > 0) {
                    // Get the texture index for this primitive
                    // 获取纹理
                    Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
                    // Bind the descriptor for the current primitive's texture
                    // 绑定纹理描述符集
                    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[texture.imageIndex].descriptorSet, 0, nullptr);
                    // 绘制索引
                    vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
                }
            }
        }
        // 递归绘制子节点
        for (auto& child : node->children) {
            drawNode(commandBuffer, pipelineLayout, child);
        }
    }

} // namespace gltf
