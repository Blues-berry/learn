/*
 * glTF model loading class for Vulkan
 * Based on glTF loading example from Sascha Willems
 */

#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "tiny_gltf.h"
#include "vulkanexamplebase.h"
#include "ILoader.h"

namespace vks {
    class Texture2D;
}

namespace gltf {

    // 定义顶点结构，包括位置、法线、UV和颜色
    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec3 color;
    };

    // 图元结构，包含绘制调用所需的数据
    struct Primitive {
        uint32_t firstIndex;  // 第一个索引偏移
        uint32_t indexCount;  // 索引数量
        int32_t materialIndex;  // 材质索引
    };

    // 网格结构，包含多个图元
    struct Mesh {
        std::vector<Primitive> primitives;
    };

    // 前向声明 Node 结构
    struct Node;

    // A node represents an object in the glTF scene graph
    // 节点结构，表示 glTF 场景图中的对象
    struct Node {
        Node* parent;  // 父节点
        std::vector<Node*> children;  // 子节点列表
        Mesh mesh;  // 网格数据
        glm::mat4 matrix;  // 变换矩阵
        // 析构函数，递归删除子节点
        ~Node() {
            for (auto& child : children) {
                delete child;
            }
        }
    };

    // 材质结构，存储基础颜色因子和纹理索引
    struct Material {
        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        uint32_t baseColorTextureIndex;
    };

    // Contains the texture for a single glTF image
    // 图像结构，包含纹理和描述符集
    struct Image {
        vks::Texture2D texture;
        // We also store (and create) a descriptor set that's used to access this texture from the fragment shader
        VkDescriptorSet descriptorSet;
    };

    // A glTF texture stores a reference to the image and a sampler
    // 纹理结构，仅引用图像索引
    struct Texture {
        int32_t imageIndex;
    };

    // glTF模型类，用于加载和渲染glTF模型
    class Model {
    public:
    	struct ShaderData {
		vks::Buffer buffer;
		struct Values {
			glm::mat4 projection;  // 投影矩阵
			glm::mat4 model;  // 模型视图矩阵
			glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);  // 光源位置
			glm::vec4 viewPos;  // 视图位置
		} values;
	} shaderData;
        // 指向Vulkan设备的指针，用于资源创建
        vks::VulkanDevice* vulkanDevice;
        // 管道布局创建信息
        VkPipelineLayoutCreateInfo pipelineLayoutCI;
        // 复制队列，用于数据上传到GPU
        VkQueue copyQueue;

        // 所有图元的单一顶点缓冲区
        struct {
            VkBuffer buffer;
            VkDeviceMemory memory;
        } vertices;

        // 所有图元的单一索引缓冲区
        struct {
            int count;
            VkBuffer buffer;
            VkDeviceMemory memory;
        } indices;

        struct DescriptorSetLayouts {
		VkDescriptorSetLayout matrices{ VK_NULL_HANDLE };
		VkDescriptorSetLayout textures{ VK_NULL_HANDLE };
	        } descriptorSetLayouts;

    // 管道布局
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	// 描述符集（用于矩阵）
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	// 描述符集（用于纹理）
	VkDescriptorSet textureDescriptorSet{ VK_NULL_HANDLE };

	struct Pipelines {
		VkPipeline solid{ VK_NULL_HANDLE };
		VkPipeline wireframe{ VK_NULL_HANDLE };
	} pipelines;
        // 模型数据
        std::vector<Image> images;  // 图像列表
        std::vector<Texture> textures;  // 纹理列表
        std::vector<Material> materials;  // 材质列表
        std::vector<Node*> nodes;  // 节点列表（顶级节点）

        // 析构函数，释放所有资源
        ~Model();
        Model(vks::VulkanDevice* dev, IExampleInterfasce* example);
        // 加载glTF文件
        bool loadFromFile(const std::string& filename, vks::VulkanDevice* device, VkQueue queue, uint32_t glTFLoadingFlags);

        // 绘制整个场景
        void draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
        void preparePipelines(VkPipelineCache pipelineCache,VkRenderPass renderPass);
        
        // 创建描述符集布局和描述符集
        void createDescriptorSetLayouts();
        void createDescriptorSets(VkDescriptorPool descriptorPool);
        void setupDescriptors(VkDescriptorPool descriptorPool);
    private:
    	IExampleInterfasce* iLoader;
        // 加载图像
        void loadImages(tinygltf::Model& input);
        // 加载纹理引用
        void loadTextures(tinygltf::Model& input);
        // 加载材质
        void loadMaterials(tinygltf::Model& input);
        // 递归加载节点
        void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, 
                     std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);
        // 绘制单个节点，包括子节点
        void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node);
    };

} // namespace gltf
