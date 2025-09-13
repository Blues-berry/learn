#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "vulkanexamplebase.h"  // 引入Vulkan基础示例类
#include "VulkanglTFModel.h"    // 引入glTF模型加载类
#include <fstream>
// SH 系数 (2阶 SH, 9 个 vec3 系数，对应 RGB 通道)
struct SHCoefficients {
    glm::vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
};

// 探针结构体
struct LightProbe {
    glm::vec3 position;      // 探针位置
    SHCoefficients shCoeffs;  // 探针的球谐系数
    vks::TextureCubeMap lowResCubeMap;  // 低分辨率CubeMap (16x16或32x32)
    float radius;            // 探针影响半径
    glm::vec3 debugColor;    // 新增：调试颜色
};

// 全局SH系数 (用于单个探针模式)
SHCoefficients shCoeffs;

// 探针相关变量
std::vector<LightProbe> lightProbes;  // 存储所有探针
int currentProbeIndex = 0;             // 当前选中的探针索引
bool useMultipleProbes = false;         // 是否使用多个探针
const int LOW_RES_CUBEMAP_SIZE = 16;   // 低分辨率CubeMap的大小
const int PROBE_GRID_SIZE = 3;         // 探针网格大小 (3x3x3 = 27个探针)

// UI 新增：对比模式开关
bool compareMode = false;  // 默认关闭

// 材质定义结构体
struct Material {
    // 材质参数块
    struct PushBlock {
        float roughness = 0.0f;  // 粗糙度
        float metallic = 0.0f;   // 金属度
        float specular = 0.0f;   // 镜面反射强度
        float r, g, b;           // RGB颜色分量
    } params;
    
    std::string name;  // 材质名称
    
    Material() {};  // 默认构造函数
    
    // 带参数的构造函数
    Material(std::string n, glm::vec3 c) : name(n) {
        params.r = c.r;  // 设置红色分量
        params.g = c.g;  // 设置绿色分量
        params.b = c.b;  // 设置蓝色分量
    };
};

/**
 * 基于图像的物理渲染(PBR)示例类
 * 实现了基于图像的照明(IBL)和球谐函数渲染
 */
class VulkanExample : public VulkanExampleBase
{
public:

    // 天空盒相关成员
	std::vector<std::string> skyboxNames;  // 天空盒名称列表
	int32_t skyboxIndex = 3;               // 当前选中的天空盒索引
	
	// 渲染模式：0=IBL, 1=球谐函数
	int32_t renderMode = 1;
	std::vector<std::string> renderModeNames = {"IBL", "harmonics"};

    // 纹理资源结构体
	struct Textures {
		vks::TextureCubeMap environmentCube;      // 环境贴图
		vks::TextureCubeMap environmentCube2;     // 第二环境贴图
        vks::TextureCubeMap environmentCube3;     // 第三环境贴图

		
		vks::Texture2D lutBrdf;                  // BRDF查找表
		vks::TextureCubeMap irradianceCube;      // 辐射度贴图
		vks::TextureCubeMap prefilteredCube;     // 预过滤贴图
	} textures;

    // 模型资源结构体
	struct Meshes {
		vkglTF::Model skybox;                   // 天空盒模型
		std::vector<vkglTF::Model> objects;     // 物体模型列表
		int32_t objectIndex = 0;                // 当前选中的物体索引
	} models;									// 声明 models 成员，存储所有模型资源。

    // Uniform缓冲区
	struct {

		vks::Buffer object;      // 物体uniform缓冲区
		vks::Buffer skybox;      // 天空盒uniform缓冲区
		vks::Buffer params;      // 参数uniform缓冲区
		vks::Buffer sh;			// SH系数Uniform缓冲区，存储球谐光照系数。
	} uniformBuffers;			// 声明 uniformBuffers 成员，存储所有Uniform缓冲区

    // 矩阵uniform结构体
	struct UBOMatrices {
		glm::mat4 projection;    // 投影矩阵
		glm::mat4 model;        // 模型矩阵
		glm::mat4 view;         // 视图矩阵
		glm::vec3 camPos;       // 相机位置
	} uboMatrices;				// 声明 uboMatrices 成员，存储矩阵数据。

    // 参数uniform结构体 定义 UBOParams 结构体，存储四个光源位置、曝光度和伽马值。
	// 结构体作为一个独立对象时，末尾不需要额外的填充，除非它在数组或嵌套上下文中需要确保下一个元素的对齐。
	//72 字节被认为是“满足 16 字节对齐”的，因为：所有成员的偏移量都符合 std140 的对齐规则。
	struct UBOParams {
		glm::vec4 lights[4];    // 光源参数
		float exposure = 4.5f;   // 曝光度
		float gamma = 2.2f;     // 伽马值
	} uboParams;
	
    // 管道对象 定义渲染管线结构体，包含天空盒和 PBR 对象的管线。
	struct {
		VkPipeline skybox{ VK_NULL_HANDLE };    // 天空盒渲染管道
		VkPipeline pbr{ VK_NULL_HANDLE };      // PBR渲染管道
		VkPipeline sh{ VK_NULL_HANDLE };      // 球谐函数（SH）渲染管道
	} pipelines;

    // 描述符集 定义描述符集结构体，包含对象和天空盒的描述符集
	struct {
		VkDescriptorSet object{ VK_NULL_HANDLE };     // 物体描述符集
		VkDescriptorSet skybox{ VK_NULL_HANDLE };    // 天空盒描述符集
	
	} descriptorSets;
	// 定义管线布局和描述符集布局，用于管理着色器资源绑定
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };         // 管道布局
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE }; // 描述符集布局
	VkShaderModule shaderModule{ VK_NULL_HANDLE };             // 着色器模块

	


    // 材质相关成员
	std::vector<Material> materials;           // 材质列表
	int32_t materialIndex = 0;                 // 当前选中的材质索引

	// 定义材质名称和对象名称列表，用于 UI 显示。
	std::vector<std::string> materialNames;     // 材质名称列表
	std::vector<std::string> objectNames;      // 物体名称列表

    /**
     * 构造函数
     * 初始化示例的基本设置
     */
	VulkanExample() : VulkanExampleBase()
	{
		title = "IBL and SH lighting";  // 设置窗口标题

        // 设置相机参数
		camera.type = Camera::CameraType::firstperson;//设置相机为第一人称模式。
		camera.movementSpeed = 4.0f;//设置相机移动速度为4.0单位/秒。
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);//设置透视投影，视场角60度，宽高比基于窗口尺寸，近裁剪面0.1，远裁剪面256.0。
		camera.rotationSpeed = 0.25f;//设置相机旋转速度为0.25。

        // 设置相机初始位置和朝向
		camera.setRotation({ -3.75f, 180.0f, 0.0f });//设置相机初始旋转角度（欧拉角：偏航-3.75度，
		camera.setPosition({ 0.55f, 0.85f, 12.0f });//设置相机初始位置为(0.55, 0.85, 12.0)。

		
        // 添加预定义的金属材质 "push_back" 是一个编程术语，在 C++ 中特指向容器末尾添加元素的操作。
		materials.push_back(Material("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f)));
		materials.push_back(Material("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f)));
		materials.push_back(Material("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f)));
		materials.push_back(Material("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f)));
		materials.push_back(Material("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f)));
		materials.push_back(Material("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f)));
		materials.push_back(Material("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f)));
		materials.push_back(Material("White", glm::vec3(1.0f)));
		materials.push_back(Material("Dark", glm::vec3(0.1f)));
		materials.push_back(Material("Black", glm::vec3(0.0f)));
		materials.push_back(Material("Red", glm::vec3(1.0f, 0.0f, 0.0f)));
		materials.push_back(Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f)));

		for (auto material : materials) {
			materialNames.push_back(material.name);
		}
		objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };
		// 设置默认材质索引为 8（对应“Black”材质）。
		materialIndex = 8;
		// 初始化天空盒名称列表（无天空盒、Pisa、Grand Canyon、Uffizi），默认选择索引 1（Pisa）。
		skyboxNames = {"NO Skybox", "Pisa", "Grand Canyon","uffizi_cube"};
		
	}
		// 析构函数，释放 Vulkan 资源，包括管线、管线布局、描述符集布局、统一缓冲区和纹理。
	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.skybox, nullptr);
			vkDestroyPipeline(device, pipelines.pbr, nullptr);
			vkDestroyPipeline(device, pipelines.sh, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			if (shaderModule != VK_NULL_HANDLE) {
				vkDestroyShaderModule(device, shaderModule, nullptr);
			}
			uniformBuffers.object.destroy();
			uniformBuffers.skybox.destroy();
			uniformBuffers.params.destroy();
			uniformBuffers.sh.destroy();
			textures.environmentCube.destroy();
			textures.environmentCube2.destroy();
            textures.environmentCube3.destroy();
            
            
			textures.irradianceCube.destroy();
			textures.prefilteredCube.destroy();
			textures.lutBrdf.destroy();

			// 释放探针相关的资源
			for (auto& probe : lightProbes) {
				probe.lowResCubeMap.destroy();
			}
			lightProbes.clear();
		}
	}
	// 获取启用的功能，检查设备是否支持采样器各向异性。
	virtual void getEnabledFeatures()
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	}
	// 创建探针网格
	void createProbeGrid()
	{
		lightProbes.clear();

		// 计算探针间距
		float spacing = 5.0f; // 探针之间的间距
		glm::vec3 startOffset = glm::vec3(-(PROBE_GRID_SIZE - 1) * spacing * 0.5f);

		// 创建3x3x3的探针网格
		for (int x = 0; x < PROBE_GRID_SIZE; x++) {
			for (int y = 0; y < PROBE_GRID_SIZE; y++) {
				for (int z = 0; z < PROBE_GRID_SIZE; z++) {
					LightProbe probe;
					// 设置探针位置
					probe.position = startOffset + glm::vec3(x * spacing, y * spacing, z * spacing);
					// 设置探针影响半径
					probe.radius = spacing * 1.5f;
					// 设置调试颜色
					probe.debugColor = glm::vec3(
						(float)((x + y * PROBE_GRID_SIZE + z * PROBE_GRID_SIZE * PROBE_GRID_SIZE) % 3) * 0.5f,
						(float)((x + y * PROBE_GRID_SIZE + z * PROBE_GRID_SIZE * PROBE_GRID_SIZE) % 5) * 0.2f,
						(float)((x + y * PROBE_GRID_SIZE + z * PROBE_GRID_SIZE * PROBE_GRID_SIZE) % 7) * 0.14f
					);

					lightProbes.push_back(probe);
				}
			}
		}

		std::cout << "Created " << lightProbes.size() << " light probes in a " 
		          << PROBE_GRID_SIZE << "x" << PROBE_GRID_SIZE << "x" << PROBE_GRID_SIZE << " grid." << std::endl;
	}

	// 为所有探针生成低分辨率CubeMap
	void generateLowResCubeMaps()
	{
		// 为每个探针创建低分辨率CubeMap
		for (auto& probe : lightProbes) {
			// 创建低分辨率CubeMap
			// 从探针位置渲染场景到CubeMap

			// 选择源环境贴图作为基础
			vks::TextureCubeMap* sourceCube = nullptr;
			switch (skyboxIndex) {
				case 1: sourceCube = &textures.environmentCube; break;
				case 2: sourceCube = &textures.environmentCube2; break;
				case 3: sourceCube = &textures.environmentCube3; break;
				default: continue;
			}

			// 创建低分辨率CubeMap图像
			VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
			imageCI.imageType = VK_IMAGE_TYPE_2D;
			imageCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			imageCI.extent.width = LOW_RES_CUBEMAP_SIZE;
			imageCI.extent.height = LOW_RES_CUBEMAP_SIZE;
			imageCI.extent.depth = 1;
			imageCI.mipLevels = 1;
			imageCI.arrayLayers = 6;
			imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &probe.lowResCubeMap.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, probe.lowResCubeMap.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &probe.lowResCubeMap.deviceMemory));
			VK_CHECK_RESULT(vkBindImageMemory(device, probe.lowResCubeMap.image, probe.lowResCubeMap.deviceMemory, 0));

			// 创建图像视图
			VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
			viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			viewCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			viewCI.subresourceRange = {};
			viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewCI.subresourceRange.levelCount = 1;
			viewCI.subresourceRange.layerCount = 6;
			viewCI.image = probe.lowResCubeMap.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &probe.lowResCubeMap.view));

			// 创建采样器
			VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
			samplerCI.magFilter = VK_FILTER_LINEAR;
			samplerCI.minFilter = VK_FILTER_LINEAR;
			samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCI.minLod = 0.0f;
			samplerCI.maxLod = 1.0f;
			samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &probe.lowResCubeMap.sampler));

			// 设置描述符
			probe.lowResCubeMap.descriptor.imageView = probe.lowResCubeMap.view;
			probe.lowResCubeMap.descriptor.sampler = probe.lowResCubeMap.sampler;
			probe.lowResCubeMap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			probe.lowResCubeMap.device = vulkanDevice;
			probe.lowResCubeMap.width = LOW_RES_CUBEMAP_SIZE;
			probe.lowResCubeMap.height = LOW_RES_CUBEMAP_SIZE;

			// 将源环境贴图复制到低分辨率CubeMap
			// 注意：这里简化处理，实际应该实现从探针位置渲染场景到CubeMap
			VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

			// 转换源图像布局
			VkImageSubresourceRange subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 6;

			vks::tools::setImageLayout(
				cmdBuf,
				sourceCube->image,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				subresourceRange);

			vks::tools::setImageLayout(
				cmdBuf,
				probe.lowResCubeMap.image,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				subresourceRange);

			// 复制每个面
			for (uint32_t face = 0; face < 6; face++) {
				VkImageCopy copyRegion = {};
				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = face;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = face;
				copyRegion.dstSubresource.mipLevel = 0;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				// 计算缩放比例
				float scale = (float)LOW_RES_CUBEMAP_SIZE / (float)sourceCube->width;
				copyRegion.extent.width = LOW_RES_CUBEMAP_SIZE;
				copyRegion.extent.height = LOW_RES_CUBEMAP_SIZE;
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					sourceCube->image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					probe.lowResCubeMap.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &copyRegion);
			}

			// 恢复图像布局
			vks::tools::setImageLayout(
				cmdBuf,
				sourceCube->image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				subresourceRange);

			vks::tools::setImageLayout(
				cmdBuf,
				probe.lowResCubeMap.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				subresourceRange);

			vulkanDevice->flushCommandBuffer(cmdBuf, queue);
		}
	}

	// 创建计算管线的辅助函数
	VkPipeline createComputePipeline(VkPipelineLayout pipelineLayout)
	{
		// 加载计算着色器
		VkPipelineShaderStageCreateInfo shaderStage = loadShader(getShadersPath() + "lightprobesh/sh_compute.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VkShaderModule shaderModule = shaderStage.module;

		shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shaderStage.module = shaderModule;
		shaderStage.pName = "main";

		VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
		computePipelineCI.stage = shaderStage;

		VkPipeline computePipeline;
		VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &computePipeline));

		// 注意：这里不销毁shaderModule，因为它被管线使用
		return computePipeline;
	}

	// 为所有探针计算球谐系数
	void generateAllSHCoefficients()
	{
		// 为每个探针计算球谐系数
		// 使用计算着色器基于探针的低分辨率CubeMap计算

		// 创建描述符池
		std::vector<VkDescriptorPoolSize> poolSizes = {
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(lightProbes.size()) },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(lightProbes.size()) }
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, static_cast<uint32_t>(lightProbes.size()));
		VkDescriptorPool descriptorPool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// 创建描述符集布局
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
		};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VkDescriptorSetLayout descriptorSetLayout;
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

		// 创建计算管线布局
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		VkPipelineLayout pipelineLayout;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// 使用辅助函数创建计算管线
		VkPipeline computePipeline = createComputePipeline(pipelineLayout);

		// 为每个探针创建存储缓冲区和描述符集
		std::vector<vks::Buffer> shStorageBuffers(lightProbes.size());
		std::vector<VkDescriptorSet> descriptorSets(lightProbes.size());

		// 分配描述符集
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		for (size_t i = 0; i < lightProbes.size(); i++) {
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[i]));

			// 创建存储缓冲区
			VkDeviceSize bufferSize = sizeof(SHCoefficients);
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				&shStorageBuffers[i],
				bufferSize));

			// 更新描述符集
			VkDescriptorImageInfo imageInfo = lightProbes[i].lowResCubeMap.descriptor;
			VkDescriptorBufferInfo bufferInfo = shStorageBuffers[i].descriptor;
			std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo),
				vks::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &bufferInfo)
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}

		// 创建命令缓冲区
		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		// 绑定计算管线
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

		// 为每个探针分派计算任务
		for (size_t i = 0; i < lightProbes.size(); i++) {
			// 绑定描述符集
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

			// 分派计算任务
			vkCmdDispatch(cmdBuf, 1, 1, 1);

			// 添加内存屏障，确保计算完成后数据可读
			VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
			barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

			// 确保存储缓冲区数据可见
			VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
			bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			bufferBarrier.buffer = shStorageBuffers[i].buffer;
			bufferBarrier.size = sizeof(SHCoefficients);
			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

			// 将存储缓冲区的数据复制到探针的SH系数
			VkBufferCopy copyRegion = {};
			copyRegion.size = sizeof(SHCoefficients);

			// 创建临时缓冲区用于读取
			vks::Buffer tempBuffer;
			VK_CHECK_RESULT(vulkanDevice->createBuffer(
				VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&tempBuffer,
				sizeof(SHCoefficients)));

			// 复制到临时缓冲区
			vkCmdCopyBuffer(cmdBuf, shStorageBuffers[i].buffer, tempBuffer.buffer, 1, &copyRegion);

			// 提交命令缓冲区并等待完成
			vulkanDevice->flushCommandBuffer(cmdBuf, queue);
			vkQueueWaitIdle(queue);

			// 从临时缓冲区读取SH系数
			SHCoefficients tempCoeffs;
			if (tempBuffer.buffer && tempBuffer.memory) {
				void* mappedData = nullptr;
				VkResult mapResult = vkMapMemory(device, tempBuffer.memory, 0, sizeof(SHCoefficients), 0, &mappedData);
				if (mapResult == VK_SUCCESS && mappedData) {
					memcpy(&tempCoeffs, mappedData, sizeof(SHCoefficients));
					vkUnmapMemory(device, tempBuffer.memory);

					// 更新探针的SH系数（确保探针有效）
					if (i < lightProbes.size()) {
						lightProbes[i].shCoeffs = tempCoeffs;
					} else {
						std::cerr << "Error: Invalid probe index " << i << std::endl;
					}
				} else {
					std::cerr << "Error: Failed to map tempBuffer memory" << std::endl;
				}
			} else {
				std::cerr << "Error: tempBuffer is not properly initialized" << std::endl;
			}

			// 释放临时缓冲区
			if (tempBuffer.buffer) {
				tempBuffer.destroy();
			}

			// 重新开始命令缓冲区记录以处理下一个探针
			cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		}

		// 提交最后的命令缓冲区
		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		// 清理资源
		vkDestroyPipeline(device, computePipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyShaderModule(device, shaderModule, nullptr);

		for (auto& buffer : shStorageBuffers) {
			buffer.destroy();
		}
	}

	// 根据位置插值获取球谐系数
	SHCoefficients interpolateSHCoefficients(const glm::vec3& position)
	{
		if (!useMultipleProbes || lightProbes.empty()) {
			return shCoeffs; // 返回全局球谐系数
		}

		// 找到影响当前位置的探针
		std::vector<std::pair<float, int>> weightedProbes;

		for (int i = 0; i < lightProbes.size(); i++) {
			float distance = glm::distance(position, lightProbes[i].position);
			if (distance < lightProbes[i].radius) {
				// 计算权重 (距离越近权重越大)
				float weight = 1.0f - (distance / lightProbes[i].radius);
				weightedProbes.push_back(std::make_pair(weight, i));
			}
		}

		// 如果没有找到影响的探针，返回全局球谐系数
		if (weightedProbes.empty()) {
			return shCoeffs;
		}

		// 按权重排序
		std::sort(weightedProbes.begin(), weightedProbes.end(), 
			[](const std::pair<float, int>& a, const std::pair<float, int>& b) {
				return a.first > b.first;
			});

		// 归一化权重
		float totalWeight = 0.0f;
		for (const auto& wp : weightedProbes) {
			totalWeight += wp.first;
		}

		// 插值球谐系数
		SHCoefficients result{};
		for (const auto& wp : weightedProbes) {
			float weight = wp.first / totalWeight;
			const SHCoefficients& probeCoeffs = lightProbes[wp.second].shCoeffs;

			// 对每个系数进行加权插值
			result.l00 += probeCoeffs.l00 * weight;
			result.l1m1 += probeCoeffs.l1m1 * weight;
			result.l10 += probeCoeffs.l10 * weight;
			result.l1p1 += probeCoeffs.l1p1 * weight;
			result.l2m2 += probeCoeffs.l2m2 * weight;
			result.l2m1 += probeCoeffs.l2m1 * weight;
			result.l20 += probeCoeffs.l20 * weight;
			result.l2p1 += probeCoeffs.l2p1 * weight;
			result.l2p2 += probeCoeffs.l2p2 * weight;
		}

		return result;
	}

	// 定义命令缓冲区开始信息，用于初始化命令缓冲区录制。
	void buildCommandBuffers()
	{
		// 初始化命令缓冲区开始信息。
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };  // 颜色缓冲区清除为深灰色（0.1, 0.1, 0.1, 1.0）
		clearValues[1].depthStencil = { 1.0f, 0 };             	// 深度缓冲区清除为 1.0，模板缓冲区清除为 0

		// 配置渲染通道开始信息，指定渲染通道、渲染区域（全屏）、清除值等。
		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;              
		renderPassBeginInfo.renderArea.offset.x = 0;            
		renderPassBeginInfo.renderArea.offset.y = 0;            
		renderPassBeginInfo.renderArea.extent.width = width;    
		renderPassBeginInfo.renderArea.extent.height = height;  
		renderPassBeginInfo.clearValueCount = 2;                
		renderPassBeginInfo.pClearValues = clearValues;         

		
		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// 遍历所有绘制命令缓冲区，设置当前帧缓冲区。
			// 将当前帧的渲染目标绑定到渲染通道 drawCmdBuffers[i] 是录制渲染指令的容器，与 frameBuffers[i] 一一绑定
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			// 开始录制命令缓冲区，检查 Vulkan API 调用是否成功。
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			// 开始渲染通道，指定渲染命令内联执行
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			// 设置设置视口，覆盖整个窗口，指定宽度、高度和深度范围（0.0 到 1.0）
			VkViewport viewport = vks::initializers::viewport((float)width,	(float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			// 设置裁剪矩形，覆盖整个窗口 
			VkRect2D scissor = vks::initializers::rect2D(width,	height,	0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// 如果选择了天空盒（skyboxIndex > 0），绑定天空盒描述符集和管线，绘制天空盒模型
			if (skyboxIndex > 0) {
				// 绑定对象的描述符集，用于渲染 3D 对象
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				models.skybox.draw(drawCmdBuffers[i]);
			}

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.object, 0, NULL);
			// 根据渲染模式（IBL 或球谐函数）绑定 PBR 管线（当前代码中两种模式都使用相同的 PBR 管线，是待实现的逻辑）。
			if (renderMode == 0) {
				// IBL渲染模式
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
			} else {
				// 球谐函数渲染模式
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sh); 
			}
			// vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);

			// 循环渲染 10 个对象，沿 X 轴排列，动态调整粗糙度和金属度（从左到右逐渐变化）。
			Material mat = materials[materialIndex];
			const uint32_t objcount = 10;
			for (uint32_t x = 0; x < objcount; x++) {
				glm::vec3 pos = glm::vec3(float(x - (objcount / 2.0f)) * 2.15f, 0.0f, 0.0f);
				mat.params.roughness = 1.0f-glm::clamp((float)x / (float)objcount, 0.005f, 1.0f);
				mat.params.metallic = glm::clamp((float)x / (float)objcount, 0.005f, 1.0f);
				// 通过推送常量将对象位置传递给顶点着色器，材质参数传递给片段着色器。
				// 推送常量（Push Constants）机制
				// 作用：高效传递小块数据到着色器，无需描述符集（Descriptor Sets）或 Uniform 缓冲区。
				// 优势：低开销，适合每帧频繁更新的数据（如模型位置、材质参数）。
				vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
				vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &mat);
				// 绘制当前选中的对象模型。
				models.objects[models.objectIndex].draw(drawCmdBuffers[i]);

			}
			// 绘制用户界面（UI），包含材质选择、对象选择等控件。
			drawUI(drawCmdBuffers[i]);
			
			// 渲染探针调试信息
			if (useMultipleProbes && !lightProbes.empty()) {
				for (auto& probe : lightProbes) {
					// 创建简单的球体模型矩阵
					glm::mat4 model = glm::translate(glm::mat4(1.0f), probe.position);
					model = glm::scale(model, glm::vec3(0.05f));
					
					// 推送常量数据
					vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
					vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4), sizeof(glm::vec3), &probe.debugColor);
					
					// 绘制探针标记
					models.objects[0].draw(drawCmdBuffers[i]); // 使用第一个模型(球体)
				}
			}
			
			// 结束渲染通道
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		// 定义 glTF 模型加载标志，预变换顶点并翻转 Y 轴（适配 Vulkan 坐标系）。
		uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
		// 加载天空盒模型（立方体 glTF 文件）。
		models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
		// 加载对象模型（球体、茶壶、环面结、金星雕像）。
		std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
		models.objects.resize(filenames.size());
		for (size_t i = 0; i < filenames.size(); i++) {
			models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue, glTFLoadingFlags);
		}
		// HDR cubemap 加载环境立方体贴图（Pisa、Grand Canyon、Uffizi），使用 16 位浮点格式
		textures.environmentCube.loadFromFile(getAssetPath() + "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		textures.environmentCube2.loadFromFile(getAssetPath() + "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
        textures.environmentCube3.loadFromFile(getAssetPath() + "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		// textures.environmentCube4.loadFromFile(getAssetPath() + "textures/hdr/grace_cross.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		// textures.environmentCube5.loadFromFile(getAssetPath() + "textures/hdr/rnl_cross.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);

		// 使用已有的 loadShader 函数加载计算着色器
		VkPipelineShaderStageCreateInfo shaderStageInfo = loadShader(getShadersPath() + "lightprobesh/sh_compute.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		shaderModule = shaderStageInfo.module;
	}

	void setupDescriptors()
	{
		// Descriptor Pool 定义描述符池大小，分配 4 个统一缓冲区和 6 个组合图像采样器。
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)
		};
		// 创建描述符池，分配 3 个描述符集。
		// 4 个统一缓冲区和 6 个组合图像采样器被分配给下面两个符集
		// 物体渲染描述符集 ( descriptorSets.object )
		// 绑定天空盒纹理和采样器。
		VkDescriptorPoolCreateInfo descriptorPoolInfo =	vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Descriptor set layout 
		// 定义描述符集布局绑定，包括 3 个统一缓冲区（矩阵和参数）和 3 个图像采样器（辐照度、BRDF、预过滤贴图）。
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),  // SH coefficients
		};
		// 创建描述符集布局
		VkDescriptorSetLayoutCreateInfo descriptorLayout = 	vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Descriptor sets 为对象分配描述符集。初始化描述符集分配信息，分配1个描述符集。
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		// Objects 配置对象描述符集，绑定统一缓冲区和纹理资源，并更新描述符集
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.object));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.object.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.irradianceCube.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &textures.lutBrdf.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &textures.prefilteredCube.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &uniformBuffers.sh.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Sky box 为天空盒分配描述符集，绑定天空盒统一缓冲区和环境贴图，并更新描述符集。
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.skybox));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.skybox, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.skybox.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.skybox, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.skybox, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.environmentCube.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
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
			vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
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

		// Pipeline layout 初始化管线布局创建信息，指定描述符集布局。
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		// Push constant ranges 定义推送常量范围：
		std::vector<VkPushConstantRange> pushConstantRanges = {
			// 第一个范围：顶点着色器，传输 glm::vec3（物体位置），偏移0。
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0),
			// 第二个范围：片段着色器，传输 Material::PushBlock（材质参数），偏移 sizeof(glm::vec3)。
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),

		};
		// 指定两个推送常量范围。
		pipelineLayoutCreateInfo.pushConstantRangeCount = 2; 
		// 设置推送常量范围。
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		// 创建管线布局。
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		// 定义两个着色器阶段（顶点和片段）。
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Pipelines
		// 初始化图形管线创建信息，指定管线布局和渲染通道。
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
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
		// 设置顶点输入状态，包括位置、法线和UV坐标。
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		// Skybox pipeline (background cube)

		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));

		// PBR pipeline using IBL
		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/lightprobesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbr));
		// SH pipeline (used for the light probe)
		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/lightprobesh_sh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.sh));
			
			
	}

	// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
	// 生成BRDF查找表（LUT），用于PBR镜面反射计算。
	void generateBRDFLUT()
	{
		// 记录开始时间，用于计算生成时间。
		auto tStart = std::chrono::high_resolution_clock::now();
		// 定义BRDF LUT的格式为16位浮点RG（红绿通道）。
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
		// 定义BRDF LUT的尺寸为512x512像素
		const int32_t dim = 512;
		// Image 创建BRDF LUT纹理。 
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();// 初始化图像创建信息。
		imageCI.imageType = VK_IMAGE_TYPE_2D;// 设置图像类型为2D。
		imageCI.format = format;	// 设置图像格式为R16G16_SFLOAT。
		imageCI.extent.width = dim;	// 设置图像宽度为512。
		imageCI.extent.height = dim;// 设置图像高度为512。
		imageCI.extent.depth = 1;	// 设置图像深度为1。
		imageCI.mipLevels = 1;		// 设置图像Mipmap级别为1。无MIP映射
		imageCI.arrayLayers = 1;	// 设置图像数组层为1。
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样（无多重采样）。
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;// 设置图像平铺方式为最佳（GPU优化）。
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;	// 设置图像用途为颜色附件和采样。
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.lutBrdf.image)); // 创建BRDF LUT图像。
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo(); // 初始化内存分配信息
		VkMemoryRequirements memReqs;											 // 定义变量存储图像内存需求。
		vkGetImageMemoryRequirements(device, textures.lutBrdf.image, &memReqs);	 // 获取图像内存需求。
		memAlloc.allocationSize = memReqs.size;									 // 设置内存分配大小为图像内存需求大小。
		// 选择适合的内存类型（设备本地）。
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		// 分配内存。 
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.lutBrdf.deviceMemory));
		// 将内存绑定到BRDF LUT图像。
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.lutBrdf.image, textures.lutBrdf.deviceMemory, 0));
		// Image view 初始化图像视图创建信息。
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;// 设置视图类型为2D。
		viewCI.format = format;					// 设置视图格式为R16G16_SFLOAT。
		viewCI.subresourceRange = {};			// 初始化子资源范围。
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;// 设置子资源为颜色附件。
		viewCI.subresourceRange.levelCount = 1;//  设置MIP级别数为1。
		viewCI.subresourceRange.layerCount = 1;//  设置层数为1。
		viewCI.image = textures.lutBrdf.image;//   关联BRDF LUT图像。
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.lutBrdf.view));// 创建BRDF LUT图像视图。
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();// 初始化采样器创建信息。
		samplerCI.magFilter = VK_FILTER_LINEAR;// 设置放大过滤为线性。
		samplerCI.minFilter = VK_FILTER_LINEAR;// 设置缩小过滤为线性。
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;// 设置MIP映射模式为线性。
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 设置U方向纹理寻址为边缘夹紧。
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 设置V方向纹理寻址为边缘夹紧。
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 设置W方向纹理寻址为边缘夹紧。
		samplerCI.minLod = 0.0f;// 设置最小LOD为0。
		samplerCI.maxLod = 1.0f;// 设置最大LOD为1。
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;// 设置边界颜色为不透明白色。。
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.lutBrdf.sampler));// 创建BRDF LUT采样器。

		textures.lutBrdf.descriptor.imageView = textures.lutBrdf.view;// 设置BRDF LUT描述符的图像视图。
		textures.lutBrdf.descriptor.sampler = textures.lutBrdf.sampler;// 设置BRDF LUT描述符的采样器。
		textures.lutBrdf.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// 设置图像布局为着色器只读。
		textures.lutBrdf.device = vulkanDevice;// 关联Vulkan设备。

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};// 初始化附件描述。
		// Color attachment
		attDesc.format = format; // 设置附件格式为R16G16_SFLOAT。
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;// 设置单采样。
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;// 加载时清除附件。
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;// 存储附件数据。
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;// 忽略模板加载。
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;// 忽略模板存储。
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// 初始布局为未定义。
		attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// 最终布局为着色器只读。
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };// 定义颜色附件引用，索引0，布局为颜色附件。

		VkSubpassDescription subpassDescription = {};// 初始化子通道描述
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;// 设置子通道为图形管线。
		subpassDescription.colorAttachmentCount = 1; // 设置一个颜色附件。
		subpassDescription.pColorAttachments = &colorReference;// 关联颜色附件引用。

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;// 定义两个子通道依赖。
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();

		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
		framebufferCI.renderPass = renderpass;
		framebufferCI.attachmentCount = 1;
		framebufferCI.pAttachments = &textures.lutBrdf.view;
		framebufferCI.width = dim;
		framebufferCI.height = dim;
		framebufferCI.layers = 1;

		VkFramebuffer framebuffer;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCI, nullptr, &framebuffer));

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));

		// Pipeline layout
		VkPipelineLayout pipelinelayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = &emptyInputState;

		// Look-up-table (from BRDF) pipeline
		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = framebuffer;

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);
		vkCmdEndRenderPass(cmdBuf);
		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkQueueWaitIdle(queue);

		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
	}

	// Generate an irradiance cube map from the environment cube map
	void generateIrradianceCube()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		const int32_t dim = 64;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.irradianceCube.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.irradianceCube.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.irradianceCube.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.irradianceCube.image, textures.irradianceCube.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = textures.irradianceCube.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.irradianceCube.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.irradianceCube.sampler));

		textures.irradianceCube.descriptor.imageView = textures.irradianceCube.view;
		textures.irradianceCube.descriptor.sampler = textures.irradianceCube.sampler;
		textures.irradianceCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.irradianceCube.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();
		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offscreen framebuffer
		{
			// Color attachment
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent.width = dim;
			imageCreateInfo.extent.height = dim;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = format;
			colorImageView.flags = 0;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreen.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

			VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::tools::setImageLayout(
				layoutCmd,
				offscreen.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.environmentCube.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Pipeline layout
		struct PushBlock {
			glm::mat4 mvp;
			// Sampling deltas
			float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
			float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
		} pushBlock;

		VkPipelineLayout pipelinelayout;
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderpass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vks::tools::setImageLayout(
			cmdBuf,
			textures.irradianceCube.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

				models.skybox.draw(cmdBuf);

				vkCmdEndRenderPass(cmdBuf);

				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					textures.irradianceCube.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back
				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}

		vks::tools::setImageLayout(
			cmdBuf,
			textures.irradianceCube.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
		vkFreeMemory(device, offscreen.memory, nullptr);
		vkDestroyImageView(device, offscreen.view, nullptr);
		vkDestroyImage(device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating irradiance cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
	}

	// Prefilter environment cubemap
	// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
	void generatePrefilteredCube()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.prefilteredCube.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.prefilteredCube.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.prefilteredCube.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.prefilteredCube.image, textures.prefilteredCube.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = textures.prefilteredCube.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.prefilteredCube.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.prefilteredCube.sampler));

		textures.prefilteredCube.descriptor.imageView = textures.prefilteredCube.view;
		textures.prefilteredCube.descriptor.sampler = textures.prefilteredCube.sampler;
		textures.prefilteredCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.prefilteredCube.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();
		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offfscreen framebuffer
		{
			// Color attachment
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent.width = dim;
			imageCreateInfo.extent.height = dim;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = format;
			colorImageView.flags = 0;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreen.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

			VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::tools::setImageLayout(
				layoutCmd,
				offscreen.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo =	vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.environmentCube.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Pipeline layout
		struct PushBlock {
			glm::mat4 mvp;
			float roughness;
			uint32_t numSamples = 32u;
		} pushBlock;

		VkPipelineLayout pipelinelayout;
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderpass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		shaderStages[0] = loadShader(getShadersPath() + "pbribl/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbribl/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vks::tools::setImageLayout(
			cmdBuf,
			textures.prefilteredCube.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			pushBlock.roughness = (float)m / (float)(numMips - 1);
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

				models.skybox.draw(cmdBuf);

				vkCmdEndRenderPass(cmdBuf);

				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					textures.prefilteredCube.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back
				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}

		vks::tools::setImageLayout(
			cmdBuf,
			textures.prefilteredCube.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
		vkFreeMemory(device, offscreen.memory, nullptr);
		vkDestroyImageView(device, offscreen.view, nullptr);
		vkDestroyImage(device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating pre-filtered enivornment cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Object vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.object,
			sizeof(uboMatrices)));

		// Skybox vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.skybox,
			sizeof(uboMatrices)));

		// Shared parameter uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.params,
			sizeof(uboParams)));
		// SH uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.sh,
			sizeof(SHCoefficients))); // 显式指定 144 字节
		
		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.object.map());
		VK_CHECK_RESULT(uniformBuffers.skybox.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());
		VK_CHECK_RESULT(uniformBuffers.sh.map());
		updateUniformBuffers();
		updateParams();
	}

	void updateUniformBuffers()
	{
		// 3D object
		uboMatrices.projection = camera.matrices.perspective;
		uboMatrices.view = camera.matrices.view;
		uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
		uboMatrices.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
		// Skybox
		uboMatrices.model = glm::mat4(glm::mat3(camera.matrices.view));
		memcpy(uniformBuffers.skybox.mapped, &uboMatrices, sizeof(uboMatrices));

		// 根据相机位置更新球谐系数
		if (useMultipleProbes) {
			// 获取相机在世界空间中的位置
			glm::vec3 worldCameraPos = camera.position;
			// 插值获取球谐系数
			SHCoefficients interpolatedCoeffs = interpolateSHCoefficients(worldCameraPos);
			// 更新SH uniform buffer
			memcpy(uniformBuffers.sh.mapped, &interpolatedCoeffs, sizeof(SHCoefficients));
		}
	}

	void updateParams()
	{
		const float p = 15.0f;
		uboParams.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
		uboParams.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
		uboParams.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
		uboParams.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		generateBRDFLUT();
		generateIrradianceCube();
		generatePrefilteredCube();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		loadSkyboxTexture();

		// 初始化探针网格
		createProbeGrid();
		generateLowResCubeMaps();
		generateSHCoefficients(); // 生成全局球谐系数
		generateAllSHCoefficients(); // 为所有探针生成球谐系数

		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		updateUniformBuffers();
		draw();
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	virtual void loadSkyboxTexture() {
		// 根据 skyboxIndex 选择正确的环境贴图
		VkDescriptorImageInfo* currentSkyboxDescriptor = nullptr;
		switch(skyboxIndex) {
        case 0: currentSkyboxDescriptor = nullptr; break;
        case 1: currentSkyboxDescriptor = &textures.environmentCube.descriptor; break;
        case 2: currentSkyboxDescriptor = &textures.environmentCube2.descriptor; break;
        case 3: currentSkyboxDescriptor = &textures.environmentCube3.descriptor; break;
    }
		if (currentSkyboxDescriptor) {
			// 更新描述符集
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,  // 绑定点
				currentSkyboxDescriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

	
		if (currentSkyboxDescriptor) {
			// 更新描述符集
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,  // 绑定点
				currentSkyboxDescriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		}
		
		// 更新 uniform buffer 并重建命令缓冲区
		
		generateSHCoefficients();  // 更新 SH 系数
		updateUniformBuffers();
		buildCommandBuffers();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			if (overlay->comboBox("Material", &materialIndex, materialNames)) {
				buildCommandBuffers();
			}
			if (overlay->comboBox("Object type", &models.objectIndex, objectNames)) {
				updateUniformBuffers();
				buildCommandBuffers();
			}
			if (overlay->inputFloat("Exposure", &uboParams.exposure, 0.1f, 2)) {
				updateParams();
			}
			if (overlay->inputFloat("Gamma", &uboParams.gamma, 0.1f, 2)) {
				updateParams();
			}
			if (overlay->comboBox("Skybox", &skyboxIndex, skyboxNames)) {
				loadSkyboxTexture(); // 已包含buildCommandBuffers调用
			}
			// 添加渲染模式选择
			if (overlay->comboBox("Render Mode", &renderMode, renderModeNames)) {
				// 切换渲染模式时重建命令缓冲区
				buildCommandBuffers();
			}

			// 添加多探针模式开关
			if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
				if (useMultipleProbes) {
					generateAllSHCoefficients();
				}
				buildCommandBuffers();
			}

			// 添加当前探针选择器（仅在多探针模式下显示）
			if (useMultipleProbes && !lightProbes.empty()) {
				std::vector<std::string> probeNames;
				for (int i = 0; i < lightProbes.size(); i++) {
					probeNames.push_back("Probe " + std::to_string(i));
				}
				if (overlay->comboBox("Current Probe", &currentProbeIndex, probeNames)) {
					buildCommandBuffers();
				}
			}
		}
	}
	// 辅助函数：获取 SH basis (包含 normalization，常量匹配 LearnOpenGL)
	// 辅助函数：获取 SH basis (修正符号以匹配标准 LearnOpenGL 和论文)
	std::vector<float> getSHBasis(const glm::vec3& dir) {
		float x = dir.x, y = dir.y, z = dir.z;
		float x2 = x * x, y2 = y * y, z2 = z * z;
		return {
			0.282095f,                   // l00
			-0.488603f * y,              // l1m1 (负号修正)
			0.488603f * z,               // l10
			-0.488603f * x,              // l1p1 (负号修正)
			1.092548f * x * y,           // l2m2
			-1.092548f * y * z,          // l2m1 (负号修正)
			0.315392f * (3.0f * z2 - 1.0f), // l20
			-1.092548f * x * z,          // l2p1 (负号修正)
			0.546274f * (x2 - y2)        // l2p2
		};
	}


float half_to_float(uint16_t half) {
    uint32_t sign = (half >> 15) & 0x1;
    uint32_t exp = (half >> 10) & 0x1F;
    uint32_t mant = half & 0x3FF;
    if (exp == 0x1F) {
        if (mant == 0) return sign ? -INFINITY : INFINITY;
        return NAN;
    }
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        float val = mant / 1024.0f * powf(2.0f, -14.0f);
        return sign ? -val : val;
    }
    exp = exp - 15 + 127;
    uint32_t result = (sign << 31) | (exp << 23) | (mant << 13);
    return *reinterpret_cast<float*>(&result);
}

void saveCubemapToPPM(const char* filename, uint16_t* data, uint32_t width, uint32_t height) {
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height * 6 << "\n255\n";
    for (uint32_t i = 0; i < width * height * 6 * 4; i += 4) {
        int r = std::min(255, int(half_to_float(data[i]) * 255.0f));
        int g = std::min(255, int(half_to_float(data[i + 1]) * 255.0f));
        int b = std::min(255, int(half_to_float(data[i + 2]) * 255.0f));
        file << r << " " << g << " " << b << "\n";
    }
    file.close();
}
// 用途：，根据给定的 3D 方向向量从立方体贴图中采样颜色。
glm::vec3 sampleCubemap(uint16_t* data, uint32_t width, uint32_t height, glm::vec3 dir) {
	// maxAxis：存储方向向量中绝对值最大的分量，用于确定采样哪个立方体贴图面。
	// u, v：纹理坐标（范围 [0, 1]），用于在选定面上采样。
	// face：表示立方体贴图的六个面（0 到 5，通常对应 +X, -X, +Y, -Y, +Z, -Z）。
    float maxAxis, u, v;
	uint32_t face;
	// 检查方向向量的 x 分量是否具有最大绝对值，以确定是否采样 X 轴相关面（+X 或 -X）。
    if (std::abs(dir.x) >= std::abs(dir.y) && std::abs(dir.x) >= std::abs(dir.z)) {
	// maxAxis = std::abs(dir.x)：将 x 分量的绝对值存储为最大轴。
    // face = dir.x > 0 ? 0 : 1：如果 x 为正，选择 +X 面（face=0）；否则选择 -X 面（face=1）。
	// u = dir.x > 0 ? -dir.z : dir.z：根据 x 的正负，计算 u 坐标（对应 z 分量，考虑立方体贴图坐标系）。
 	// v = -dir.y：v 坐标为 y 分量的负值（考虑立方体贴图的坐标系方向）。
        maxAxis = std::abs(dir.x);
        face = dir.x > 0 ? 0 : 1;
        u = dir.x > 0 ? -dir.z : dir.z;
        v = -dir.y;
    } else if (std::abs(dir.y) >= std::abs(dir.x) && std::abs(dir.y) >= std::abs(dir.z)) {
        maxAxis = std::abs(dir.y);
        face = dir.y > 0 ? 2 : 3;
        u = dir.x;
        v = dir.y > 0 ? dir.z : -dir.z;
    } else {
        maxAxis = std::abs(dir.z);
        face = dir.z > 0 ? 4 : 5;
        u = dir.z > 0 ? dir.x : -dir.x;
        v = -dir.y;
    }
	// 用途：将 u 和 v 坐标归一化到 [0, 1] 范围：
    u = (u / maxAxis + 1.0f) * 0.5f;
    v = (v / maxAxis + 1.0f) * 0.5f;
	// 将归一化的 u, v 坐标转换为像素坐标
    uint32_t x = std::min((uint32_t)(u * width), width - 1);
    uint32_t y = std::min((uint32_t)(v * height), height - 1);
	// 声明静态变量 sampleCount，用于记录采样次数（仅用于调试日志）。
    static int sampleCount = 0;
    if (sampleCount < 10) {
        std::ofstream logFile("../../examples/lightprobesh/lightprobeshsh_coefficients.log", std::ios::app);
        logFile << "SampleCubemap " << sampleCount << ": face=" << face << ", u=" << u << ", v=" << v 
                << ", x=" << x << ", y=" << y << "\n";
        logFile.close();
        sampleCount++;
    }
	// 计算单个立方体贴图面的像素总数（宽 × 高）。
    uint32_t pixelCount = width * height;
	// 计算采样像素在立方体贴图中的偏移量（faceOffset）和像素在数据数组中的偏移量（pixelOffset）。
    uint32_t faceOffset = face * pixelCount;
    uint32_t pixelOffset = faceOffset + y * width + x;
    uint32_t offset = pixelOffset * 4;
	// 检查偏移是否超出数据范围（6 面 × 每面像素数 × 4 通道）。
    if (offset + 3 >= 6 * pixelCount * 4) {
		// 如果偏移越界，返回黑色（RGB = 0, 0, 0）以避免非法访问。
        return glm::vec3(0.0f);
    }
	// 从数据中读取 RGB 通道值（半精度浮点格式，uint16_t），并转换为浮点数：
    float r = half_to_float(data[offset]);
    float g = half_to_float(data[offset + 1]);
    float b = half_to_float(data[offset + 2]);
    return glm::vec3(r, g, b);
}


// 从当前立方体贴图计算球谐（SH）系数，用于环境光照计算。
void generateSHCoefficients() {
    // 打开日志文件（追加模式），记录 SH 系数生成过程
    std::ofstream logFile("../../examples/lightprobesh/lightprobeshsh_coefficients.log", std::ios::app);
    logFile << "\n" << "begin time(UTC): " << std::chrono::system_clock::now() << "\n";
    logFile << "Starting SH coefficient generation (GPU)\n";
    logFile << "skyboxIndex: " << skyboxIndex << "\n";

    // 选择当前立方体贴图
    vks::TextureCubeMap* currentCube = nullptr;
    switch (skyboxIndex) {
        case 0:
            logFile << "Invalid skyboxIndex, setting SH coefficients to zero\n";
            shCoeffs = SHCoefficients{};
            memcpy(uniformBuffers.sh.mapped, &shCoeffs, sizeof(SHCoefficients));
            logFile.close();
            return;
        case 1: currentCube = &textures.environmentCube; break;
        case 2: currentCube = &textures.environmentCube2; break;
        case 3: currentCube = &textures.environmentCube3; break;
    }

    // 检查立方体贴图是否有效
    if (!currentCube || !currentCube->image) {
        logFile << "Error: currentCube is null or not initialized\n";
        shCoeffs = SHCoefficients{};
        memcpy(uniformBuffers.sh.mapped, &shCoeffs, sizeof(SHCoefficients));
        logFile.close();
        return;
    }

    logFile << "Cubemap width: " << currentCube->width << ", height: " << currentCube->height << "\n";

    // 创建存储缓冲区，用于存储 SH 系数
    vks::Buffer shStorageBuffer;
    VkDeviceSize bufferSize = sizeof(SHCoefficients);
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &shStorageBuffer,
        bufferSize));

    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VkDescriptorPool descriptorPool;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VkDescriptorSet descriptorSet;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    // 更新描述符集
    VkDescriptorImageInfo imageInfo = currentCube->descriptor;
    VkDescriptorBufferInfo bufferInfo = shStorageBuffer.descriptor;
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &bufferInfo)
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // 创建计算管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VkPipelineLayout pipelineLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

   // 使用辅助函数创建计算管线
    VkPipeline computePipeline = createComputePipeline(pipelineLayout);

    // 创建命令缓冲区
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // 绑定管线和描述符集
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // 分派计算任务（9 个线程，1 个工作组）
    vkCmdDispatch(cmdBuf, 1, 1, 1);

    // 添加内存屏障，确保计算完成后数据可读
    VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    // 确保存储缓冲区数据可见
    VkBufferMemoryBarrier bufferBarrier = vks::initializers::bufferMemoryBarrier();
    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    bufferBarrier.buffer = shStorageBuffer.buffer;
    bufferBarrier.size = sizeof(SHCoefficients);
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &bufferBarrier, 0, nullptr);

    // 将存储缓冲区的数据复制到 Uniform 缓冲区
    VkBufferCopy copyRegion = {};
    copyRegion.size = sizeof(SHCoefficients);
    vkCmdCopyBuffer(cmdBuf, shStorageBuffer.buffer, uniformBuffers.sh.buffer, 1, &copyRegion);

    // 提交命令缓冲区
    vulkanDevice->flushCommandBuffer(cmdBuf, queue);

    // 读取 SH 系数用于日志记录
    SHCoefficients tempCoeffs;
    memcpy(&tempCoeffs, uniformBuffers.sh.mapped, sizeof(SHCoefficients));

    // 记录 SH 系数
    logFile << "SH Coefficients for skyboxIndex " << skyboxIndex << ":\n";
    logFile << "l00: " << tempCoeffs.l00.x << ", " << tempCoeffs.l00.y << ", " << tempCoeffs.l00.z << "\n";
    logFile << "l1m1: " << tempCoeffs.l1m1.x << ", " << tempCoeffs.l1m1.y << ", " << tempCoeffs.l1m1.z << "\n";
    logFile << "l10: " << tempCoeffs.l10.x << ", " << tempCoeffs.l10.y << ", " << tempCoeffs.l10.z << "\n";
    logFile << "l1p1: " << tempCoeffs.l1p1.x << ", " << tempCoeffs.l1p1.y << ", " << tempCoeffs.l1p1.z << "\n";
    logFile << "l2m2: " << tempCoeffs.l2m2.x << ", " << tempCoeffs.l2m2.y << ", " << tempCoeffs.l2m2.z << "\n";
    logFile << "l2m1: " << tempCoeffs.l2m1.x << ", " << tempCoeffs.l2m1.y << ", " << tempCoeffs.l2m1.z << "\n";
    logFile << "l20: " << tempCoeffs.l20.x << ", " << tempCoeffs.l20.y << ", " << tempCoeffs.l20.z << "\n";
    logFile << "l2p1: " << tempCoeffs.l2p1.x << ", " << tempCoeffs.l2p1.y << ", " << tempCoeffs.l2p1.z << "\n";
    logFile << "l2p2: " << tempCoeffs.l2p2.x << ", " << tempCoeffs.l2p2.y << ", " << tempCoeffs.l2p2.z << "\n";
    logFile << "SH coefficient generation completed (GPU)\n";
    logFile << "end time(UTC): " << std::chrono::system_clock::now() << "\n";
    logFile.close();
    // 清理资源有bug
    // 先销毁存储缓冲区，然后再销毁其他资源
    // shStorageBuffer.destroy();
    // vkDestroyPipeline(device, computePipeline, nullptr);
    // vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    // vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    // vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    // vkDestroyShaderModule(device, shaderModule, nullptr);
}
// 从当前 environmentCube 计算 SH 系数。实际可参考 https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf

};

VULKAN_EXAMPLE_MAIN()
