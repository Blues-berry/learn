#pragma once

// 定义宏以禁用 tinygltf 的图像写入功能
#define TINYGLTF_NO_STB_IMAGE_WRITE
// 如果是 Android 平台，定义宏以从资产加载 glTF

// 包含 tinygltf 头文件，用于 glTF 文件解析
#include "tiny_gltf.h"

// 包含 Vulkan 设备头文件，间接包含 VulkanTools.h
#include "VulkanDevice.h"
#include "ILoader.h"
// 包含 Vulkan 示例基类头文件，提供 Vulkan 初始化和渲染基础
#include "vulkanexamplebase.h"


// 定义 VulkanglTFModel 类，用于存储和渲染 glTF 模型
// Contains everything required to render a glTF model in Vulkan
// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
class VulkanglTFModel
{
public:


	// The vertex layout for the samples' model
	// 定义顶点结构，包括位置、法线、UV 和颜色
	struct Vertex {
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		glm::vec3 color;
	};

	// Single vertex buffer for all primitives
	// 所有图元的单一顶点缓冲区
	struct {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertices;

	// Single index buffer for all primitives
	// 所有图元的单一索引缓冲区
	struct {
		int count;
		VkBuffer buffer;
		VkDeviceMemory memory;
	} indices;

	// The following structures roughly represent the glTF scene structure
	// To keep things simple, they only contain those properties that are required for this sample
	// 前向声明 Node 结构
	struct Node;

	// A primitive contains the data for a single draw call
	// 图元结构，包含绘制调用所需的数据
	struct Primitive {
		uint32_t firstIndex;  // 第一个索引偏移
		uint32_t indexCount;  // 索引数量
		int32_t materialIndex;  // 材质索引
	};

	// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
	// 网格结构，包含多个图元
	struct Mesh {
		std::vector<Primitive> primitives;
		
 	};

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

	// A glTF material stores information in e.g. the texture that is attached to it and colors
	// 材质结构，存储基础颜色因子和纹理索引
	struct Material {
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		uint32_t baseColorTextureIndex;
	};

	// Contains the texture for a single glTF image
	// Images may be reused by texture objects and are as such separated
	// 图像结构，包含纹理和描述符集
	struct Image {
		vks::Texture2D texture;
		// We also store (and create) a descriptor set that's used to access this texture from the fragment shader
		VkDescriptorSet descriptorSet;
	};

	// A glTF texture stores a reference to the image and a sampler
	// In this sample, we are only interested in the image
	// 纹理结构，仅引用图像索引
	struct Texture {
		int32_t imageIndex;
	};

	/*
		Model data
	*/
	// 图像列表
	std::vector<Image> images;
	// 纹理列表
	std::vector<Texture> textures;
	// 材质列表
	std::vector<Material> materials;
	// 节点列表（顶级节点）
	std::vector<Node*> nodes;

    explicit VulkanglTFModel(vks::VulkanDevice* dev, IExampleInterfasce* example);
	// 析构函数，释放所有资源
	~VulkanglTFModel();
    void loadImages(tinygltf::Model& input);
	void loadTextures(tinygltf::Model& input);
	void loadMaterials(tinygltf::Model& input);
    void loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFModel::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer);
	void drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFModel::Node* node);
	void draw(VkCommandBuffer commandBuffer,VkPipelineLayout pipelineLayout);
    void buildCommandBuffers(VkCommandBuffer cmd);
	void loadglTFFile(VkQueue queue,std::string filename);
    void preparePipelines(VkRenderPass renderPass,VkPipelineCache pipelineCache);
    void setupDescriptors(VkRenderPass renderPass);
    void prepareUniformBuffers();
    void updateUniformBuffers();
    void prepare(VkRenderPass renderPass,VkPipelineCache pipelineCache,VkCommandBuffer cmd);
public:
    // Uniform buffer data
    // 统一缓冲区数据
    struct {
        vks::Buffer buffer;
        struct Values {
            glm::mat4 projection;  // 投影矩阵
            glm::mat4 view;  // 视图矩阵
            glm::mat4 model;  // 模型矩阵
            glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);  // 光源位置
            glm::vec4 viewPos;  // 视图位置
            float exposure = 4.5f;  // 曝光度
            float gamma = 2.2f;  // 伽马值
        } values;
    } shaderData;
    
public:
    // 管线布局和描述符集
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    
    // buildCommandBuffers函数所需的成员变量
    VkRenderPass pass = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<VkCommandBuffer> drawCmdBuffers;
    std::vector<VkFramebuffer> frameBuffers;
    
private:
	// The class requires some Vulkan objects so it can create it's own resources
	// 指向 Vulkan 设备的指针，用于资源创建
	vks::VulkanDevice* vulkanDevice;
    IExampleInterfasce* iLoader; // 指向 Vulkan 示例基类的指针，用于渲染循环
	// 复制队列，用于数据上传到 GPU
	VkQueue copyQueue;
    // 管道结构，包含实心和线框管道
	struct Pipelines {
		VkPipeline solid{ VK_NULL_HANDLE };
		VkPipeline wireframe{ VK_NULL_HANDLE };
	} pipelines;

	// 管道布局和描述符集已移至public部分
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

	// 描述符集布局结构
	struct DescriptorSetLayouts {
		VkDescriptorSetLayout matrices{ VK_NULL_HANDLE };
		VkDescriptorSetLayout textures{ VK_NULL_HANDLE };
	} descriptorSetLayouts;

};