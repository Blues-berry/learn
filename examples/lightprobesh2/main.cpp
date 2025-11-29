#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "LightProbe.h"
#include "gltfload.h"
#include "Skybox.h"
#include "Pass.h"
#include "ILoader.h"
#include "PreviewModel.h"
#include "CubemapInterpolation.h"
#include "SphericalHarmonics.h"
#include "PRTComputeShader.h"
#include "tiny_gltf.h"
#include "../base/VulkanTools.h"

#define PI 3.14159265358979323846

// 注意：不定义TINYGLTF_IMPLEMENTATION，因为它已经在base.lib中实现

// 定义宏以启用 stb_image 的实现，用于图像加载
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif

// 定义宏以禁用 tinygltf 的图像写入功能
#ifndef TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#endif

// 包含必要的头文件：
// - cstdint：提供标准整数类型（如 uint32_t）。
// - glm/glm.hpp：包含 GLM 库，用于数学运算（向量、矩阵等）。
// - glm/gtc/constants.hpp：提供数学常数（如 M_PI）。
// - vulkanexamplebase.h：Vulkan 示例基类，包含 Vulkan 初始化和渲染基础功能。
// - VulkanglTFModel.h：支持 glTF 模型加载。
// - LightProbe.h：光照探针相关功能。
// - GltfScene.h：glTF 场景管理。
// - Skybox.h：天空盒实现。
// - Pass.h：渲染通道类定义。
// - ILoader.h：着色器加载接口。
// - PreviewModel.h：预览模型类。
// - fstream：文件流操作（为后续扩展保留）。

// 对比渲染模式设置
enum class RenderCompareMode {
    NORMAL = 0,           // 正常渲染
    ORIGINAL_ONLY,        // 仅原始环境贴图
    SINGLE_PROBE,         // 单探针捕获效果
    MULTI_PROBE,          // 多探针捕获效果
    SPLIT_VIEW            // 分屏对比
};

class VulkanExample : public VulkanExampleBase, public IExampleInterfasce
{
public:

    VulkanExample() : VulkanExampleBase()
    {
        // 构造函数：初始化 Vulkan 示例，继承自 VulkanExampleBase。
        camera.type = Camera::CameraType::firstperson; // 设置相机为第一人称模式。
        camera.movementSpeed = 4.0f; // 设置相机移动速度为 4.0 单位/秒。
        camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f); // 设置透视投影：60° 视角，宽高比基于窗口尺寸，近裁剪面 0.1，远裁剪面 256.0。
        camera.rotationSpeed = 0.25f; // 设置相机旋转速度为 0.25。

        // 设置相机初始位置和朝向。
        camera.setRotation({ -90.0f, 0.0f, 0.0f }); // 初始视角俯视盒内，保持左右墙体对称.
        camera.setPosition({ 0.84f, .16f, 7.43f });  // 靠近盒口上方，贴近平面截图的取景.

        // 启用多视图扩展（VK_KHR_multiview），用于同时渲染多个视角（如立方体贴图的 6 个面）。
        enabledDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

        // 启用获取物理设备属性的扩展（VK_KHR_get_physical_device_properties2），支持多视图特性查询。
        enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

        // 配置多视图特性。
        physicalDeviceMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR; // 设置结构体类型。
        physicalDeviceMultiviewFeatures.multiview = VK_TRUE; // 启用多视图支持。
        deviceCreatepNextChain = &physicalDeviceMultiviewFeatures; // 将多视图特性链接到设备创建链。
    }

    ~VulkanExample() override
    {
        // 析构函数：清理 Vulkan 资源。
        vkDeviceWaitIdle(device); // 等待设备空闲，确保所有命令完成。

        if (skybox)
        {
            skybox->Destroy(); // 销毁天空盒对象。
            skybox = nullptr; // 清空指针。
        }

        if (previewModel)
        {
            previewModel->Destroy(); // 销毁预览模型对象。
            previewModel = nullptr; // 清空指针。
        }
        if (gltfModel)
        {
            gltfModel->Destroy(); // 销毁预览模型对象。
            gltfModel = nullptr; // 清空指针。
        }

        cubeMaps.clear(); // 清空立方体贴图列表。

        // 销毁描述符池
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            descriptorPool = VK_NULL_HANDLE;
        }

        // 清空渲染通道对象。
        mainPass = nullptr;
        shGenPass = nullptr;
        brdfPass = nullptr;
        genIBL = nullptr;
    }

    void LoadAssets();
    // 声明加载资产函数，用于加载立方体贴图和 glTF 模型。

    void LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format);
    // 声明加载立方体贴图函数，指定名称、路径和格式。

    void LoadPreviewModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags);
    // 声明加载预览模型函数，指定名称、路径和 glTF 加载标志。
    void LoadgltfModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags);
    // 声明加载预览模型函数，指定名称、路径和 glTF 加载标志。
    void PrepareScene();
    // 声明准备场景函数，初始化天空盒和预览模型。

    void UpdateSkyBox();
    // 声明更新天空盒函数，切换立方体贴图并重新生成相关资源。

    void PrepareProbes();
    // 声明准备光照探针函数，初始化光照探针。

    void PreparePasses();
    // 声明准备渲染通道函数，初始化所有渲染通道。

    void CaptureCubemap(const glm::vec3& position);
    // 声明捕获立方体贴图函数，在指定位置生成新的立方体贴图。

    void CaptureAllProbes();
    // 自动捕获所有探针的立方体贴图

    void SetCompareMode(RenderCompareMode mode);
    // 设置对比渲染模式

    void PrecomputePRT();
    // 预计算PRT球谐系数

    void UpdatePRTLighting();
    // 更新PRT光照

    void ReginPrefilterPasses();
    // 声明重新生成预过滤通道函数（未实现）。

    void prepare() override
    {
        // 重写基类的 prepare 方法，执行初始化流程。
        VulkanExampleBase::prepare(); // 调用基类初始化 Vulkan 环境。
        LoadAssets(); // 加载资产（立方体贴图和模型）。
        PreparePasses(); // 准备渲染通道。
        PrepareProbes(); // 准备光照探针。
        PrepareScene(); // 准备场景（天空盒和预览模型）。

        // 初始化GPU PRT计算器
        prtCompute = std::make_unique<PRT::PRTComputeShader>(vulkanDevice, queue);
        if (!prtCompute->Initialize()) {
            std::cerr << "[VulkanExample] Warning: PRTComputeShader Initialize failed. Will fallback to CPU for now." << std::endl;
        }
        prepared = true; // 标记初始化完成。
    }

    void render() override
    {
        // 重写基类的 render 方法，执行渲染逻辑。
        if (!prepared)
            return; // 如果未初始化，直接返回。
        draw(); // 调用 draw 方法执行渲染。
    }

    void drawFrame(VkCommandBuffer cmd);
    // 声明绘制单帧函数，记录渲染命令。

    void drawSplitView(VkCommandBuffer cmd);
    // 分屏对比渲染：同时显示原始、单探针、多探针效果

    void prepareData();
    // 声明准备数据函数，更新相机和全局数据。

    void draw()
    {
        // 绘制一帧的逻辑。
        // 自动旋转光源（降低速度）
        if (autoRotateLight) {
            lightRotationAngle += 0.005f; // 降低旋转速度
            if (lightRotationAngle > 6.28318f) {
                lightRotationAngle -= 6.28318f;
            }
        }

        prepareData(); // 准备渲染数据（如相机矩阵）。

        VulkanExampleBase::prepareFrame(); // 准备帧（基类方法，可能包括交换链准备）。

        VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer]; // 获取当前帧的命令缓冲区。

        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo(); // 初始化命令缓冲区开始信息。
        VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo)); // 开始记录命令。

        drawFrame(cmdBuffer); // 调用 drawFrame 记录渲染命令。

        VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer)); // 结束命令缓冲区记录。

        submitInfo.commandBufferCount = 1; // 设置提交的命令缓冲区数量。
        submitInfo.pCommandBuffers = &cmdBuffer; // 指定命令缓冲区。
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE)); // 提交命令到队列。
        VulkanExampleBase::submitFrame(); // 提交帧（基类方法，包括呈现交换链）。
    }

    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    // 声明 UI 覆盖更新函数，用于交互式设置。

    // GPU PRT 预计算与导出
    void ExportPRTDataGPU();
    std::unique_ptr<PRT::PRTComputeShader> prtCompute;
    bool isExportingPRT = false;
    std::string prtExportStatus;
    // Spotlight parameters (degrees)
    float spotInnerDeg = 15.0f;
    float spotOuterDeg = 25.0f;
    // Irradiance A_l is always applied (π, 2π/3, π/4)

    VkPipelineShaderStageCreateInfo LoadShader(const std::string& path, VkShaderStageFlagBits stage) override
    {
        // 实现 IExampleInterfasce 接口的 LoadShader 方法，加载着色器。
        return loadShader(getShadersPath() + path, stage); // 调用基类方法加载指定路径的着色器。
    }

private:
    VkPhysicalDeviceMultiviewFeaturesKHR physicalDeviceMultiviewFeatures{};
    // 定义多视图特性结构体（已在构造函数中初始化）。

    std::vector<std::unique_ptr<LightProbe>> lightProbes;
    int32_t lightProbesIndex = 0;
    // 探针配置
    ProbeGridConfig probeGridConfig;
    bool useMultipleProbes = false;
    // 光照探针列表，存储场景中的光照探针。
    std::unique_ptr<CaptureScenePass> capturePass;   // 捕获场景通道，用于生成立方体贴图。
    // 天空盒相关成员。
    std::vector<std::shared_ptr<vks::TextureCubeMap>> cubeMaps;
    // 立方体贴图列表，存储多个立方体贴图。
    std::vector<std::string> cubemapNames;
    // 立方体贴图名称列表。
    int32_t skyboxIndex = 0;
    // 当前使用的天空盒（立方体贴图）索引。

    // 预览模型相关成员。
    std::shared_ptr<vkglTF::Model> skyboxModel;
    tinygltf::Model glTFInput;
    // 天空盒模型（通常为立方体）。
    std::vector<std::shared_ptr<vkglTF::Model>> previewModels;
    std::vector<std::shared_ptr<vkglTF::Model>> gltfModels;
    // 预览模型列表。
    std::vector<std::string> previewModelNames;
    // 预览模型名称列表。
    int32_t modelIndex = 0;
    // 当前使用的预览模型索引。

    std::vector<std::string> gltfModelNames;
    // 预览模型名称列表。
    int32_t gltfmodelIndex = 0;
    // 当前使用的预览模型索引。

    // 渲染管线相关成员。
    bool globalDirty = true;
    // 全局数据脏标志，表示是否需要更新全局数据。
    MainPass::GlobalUbo mainPassData = {};
    // 主渲染通道的统一缓冲区对象（UBO）。
    std::unique_ptr<MainPass> mainPass;
    // 主渲染通道对象。
    std::unique_ptr<GenBRDFLutPass> brdfPass;
    // BRDF 查找表生成通道。
    std::unique_ptr<GenSHComputePass> shGenPass;
    // 球谐（SH）计算通道。
    std::unique_ptr<GenIBLPass> genIBL;

    // 场景相关成员。
    std::unique_ptr<Skybox> skybox;
    // 天空盒对象。
    std::unique_ptr<PreviewModel> previewModel;

    // 预览模型对象。
    std::unique_ptr<LightProbe> probe;

    // glTF模型对象
    std::unique_ptr<GltfModel> gltfModel;
    std::vector<std::unique_ptr<GltfModel>> gltfClones;

    // 描述符池
    VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };

    bool showProbes = false; // Toggle for visualizing probes

    // 立方体贴图插值
    std::unique_ptr<CubemapInterpolation> cubemapInterpolation;

    // 插值算法选择
    int32_t interpolationModeIndex = 0;  // 0=IDW, 1=Linear, 2=Cubic
    std::vector<std::string> interpolationModeNames = {"IDW", "Linear", "Cubic"};

    // 对比渲染
    RenderCompareMode compareMode = RenderCompareMode::NORMAL;
    std::shared_ptr<vks::TextureCubeMap> originalCubemap;      // 原始环境贴图
    std::shared_ptr<vks::TextureCubeMap> singleProbeCubemap;   // 单探针捕获
    std::shared_ptr<vks::TextureCubeMap> multiProbeCubemap;    // 多探针捕获/插值

    // 光源控制
    bool lightEnabled = true;
    float lightIntensity = 100.0f;
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float lightRotationAngle = 0.0f;
    bool autoRotateLight = false;

    // PRT系统
    bool usePRT = false;
    std::vector<glm::vec3> precomputedSHCoefficients; // 预计算的SH系数
    int shSamples = 16; // SH采样数量

    // PRT新系统
    std::vector<PRTPrecomputer::RotatedCoefficients> prtData; // 预计算的PRT数据
    SHCoefficients currentSHCoefficients; // 当前的球谐系数
    std::string prtDataFile = "prt_data.txt"; // PRT数据文件路径
};

// =============================================================================
// 资源加载相关函数
// =============================================================================

void VulkanExample::LoadAssets()
{
    // 加载资产，包括立方体贴图和 glTF 模型。
    LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT); // 加载“pisa”立方体贴图。
    LoadCubeMap("gcanyon", "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT); // 加载“gcanyon”立方体贴图。
    LoadCubeMap("uffizi", "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT); // 加载“uffizi”立方体贴图。

    uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY; // 设置 glTF 加载标志：预变换顶点，翻转 Y 轴。

    LoadPreviewModel("sphere", "models/sphere.gltf", glTFLoadingFlags); // 加载球体模型。
    // LoadPreviewModel("teapot", "models/teapot.gltf", glTFLoadingFlags); // 加载茶壶模型。
    // LoadPreviewModel("torusknot", "models/torusknot.gltf", glTFLoadingFlags); // 加载环面结模型。
    // LoadPreviewModel("venus", "models/venus.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    // LoadPreviewModel("armor", "models/armor/armor.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    // LoadPreviewModel("chinesedragon", "models/chinesedragon.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    // LoadPreviewModel("sibenik", "models/sibenik.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    // LoadPreviewModel("fireplace", "models/fireplace.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    // LoadPreviewModel("glowsphere", "models/glowsphere.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    // LoadPreviewModel("rock01", "models/rock01.gltf", glTFLoadingFlags); // 加载模型。scene


    // LoadgltfModel("CornellBox-scene", "models/scene.gltf", glTFLoadingFlags); //
    LoadgltfModel("CornellBox-Original", "models/CornellBox-Original.gltf", glTFLoadingFlags); //
    LoadgltfModel("cornell", "models/scene.gltf", glTFLoadingFlags); //
    // LoadgltfModel("FlightHelmet", "models/FlightHelmet/glTF/FlightHelmet.gltf", glTFLoadingFlags); //
    // LoadgltfModel("CesiumMan", "models/CesiumMan/glTF/CesiumMan.gltf", glTFLoadingFlags); //
    skyboxModel = std::make_shared<vkglTF::Model>(); // 创建天空盒模型对象。
    skyboxModel->loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags); // 加载立方体模型作为天空盒。
}

void VulkanExample::PrepareScene()
{
    // 准备场景，初始化天空盒和预览模型。
    skybox = std::make_unique<Skybox>(vulkanDevice, this); // 创建天空盒对象。
    skybox->SetModel(skyboxModel); // 设置天空盒模型。
    skybox->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN); // 准备天空盒的管线状态对象（PSO）。
    skybox->UpdateCubemap(cubeMaps[skyboxIndex]); // 设置初始立方体贴图。

    previewModel = std::make_unique<PreviewModel>(vulkanDevice, this); // 创建预览模型对象。
    previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN); // 准备预览模型的 PSO。
    previewModel->UpdateModel(previewModels[modelIndex]); // 设置初始预览模型。
    previewModel->SetLightColor(lightColor); // 设置初始光源颜色

    // 准备gltfModel - 为MainPass和CapturePass都准备PSO
    if (!gltfModels.empty()) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this, queue); // 创建 glTF 模型对象，传入queue用于纹理加载
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN); // 为MainPass准备PSO
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE); // 为CapturePass准备PSO
        gltfModel->UpdateModel(gltfModels[gltfmodelIndex]); // 设置第一个模型


    }
}

void VulkanExample::UpdateSkyBox()
{
    // 更新天空盒，切换立方体贴图并重新生成相关资源。
    skybox->UpdateCubemap(cubeMaps[skyboxIndex]); // 更新天空盒的立方体贴图。
    shGenPass->SetCubeMap(cubeMaps[skyboxIndex]); // 更新球谐计算通道的立方体贴图。
    genIBL->SetCubeMap(cubeMaps[skyboxIndex]); // 更新 IBL 通道的立方体贴图。

    // 生成球谐系数和环境贴图。
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true); // 创建主命令缓冲区。
    shGenPass->Draw(cmdBuf); // 执行球谐计算。
    genIBL->Draw(cmdBuf); // 执行 IBL 渲染。
    vulkanDevice->flushCommandBuffer(cmdBuf, queue); // 提交并刷新命令缓冲区。

    // 更新mainPass的描述符绑定，确保使用最新的立方体贴图
    mainPass->UpdateBindings();
}

void VulkanExample::LoadPreviewModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags)
{
    // 加载预览模型。
    auto model = std::make_shared<vkglTF::Model>(); // 创建 glTF 模型对象。
    model->loadFromFile(getAssetPath() + cubemapPath, vulkanDevice, queue, glTFLoadingFlags); // 从文件加载模型。

    previewModels.emplace_back(model); // 添加到预览模型列表。
    previewModelNames.emplace_back(name); // 添加模型名称。
}
void VulkanExample::LoadgltfModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags)
{
    // 加载预览模型。
    std::cerr << "LoadgltfModel: Loading " << name << " from " << cubemapPath << "\n";
    auto model = std::make_shared<vkglTF::Model>(); // 创建 glTF 模型对象。
    model->loadFromFile(getAssetPath() + cubemapPath, vulkanDevice, queue, glTFLoadingFlags); // 从文件加载模型。

    gltfModels.emplace_back(model); // 添加到预览模型列表。
    gltfModelNames.emplace_back(name); // 添加模型名称。
    std::cerr << "LoadgltfModel: Loaded " << name << ", total models: " << gltfModels.size() << "\n";
}
void VulkanExample::LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format)
{
    // 加载立方体贴图。
    auto cubemap = std::shared_ptr<vks::TextureCubeMap>(new vks::TextureCubeMap(), [](vks::TextureCubeMap* cubemap) {
        if (cubemap)
        {
            cubemap->destroy(); // 自定义删除器：销毁立方体贴图。
            delete cubemap;
        }
    });

    cubemap->loadFromFile(getAssetPath() + cubemapPath, VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue); // 从文件加载立方体贴图。
    cubeMaps.emplace_back(cubemap); // 添加到立方体贴图列表。
    cubemapNames.emplace_back(name); // 添加贴图名称。
}

// =============================================================================
// 渲染通道和场景准备函数
// =============================================================================

void VulkanExample::PreparePasses()
{
    // 准备描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
        vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32)
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 33);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 准备所有渲染通道。
    mainPass = std::make_unique<MainPass>(vulkanDevice); // 创建主渲染通道。
    mainPass->SetUp(renderPass); // 设置主渲染通道的渲染通道对象。
    // 创建 capturePass（1024×1024，R16G16B16A16_SFLOAT）
    capturePass = std::make_unique<CaptureScenePass>(vulkanDevice, this, VK_FORMAT_R16G16B16A16_SFLOAT, 1024, 1024);

    brdfPass = std::make_unique<GenBRDFLutPass>(vulkanDevice, this); // 创建 BRDF 查找表生成通道。
    brdfPass->Prepare(); // 准备 BRDF 通道资源。
    brdfPass->FeedDescriptor(mainPass->environmemts.brdfView); // 设置主渲染通道的 BRDF 描述符。

    shGenPass = std::make_unique<GenSHComputePass>(vulkanDevice, this); // 创建球谐计算通道。
    shGenPass->SetCubeMap(cubeMaps[skyboxIndex]); // 设置初始立方体贴图。
    shGenPass->FeedSH(mainPass->environmemts.shCoeffs); // 设置主渲染通道的球谐系数描述符。

    genIBL = std::make_unique<GenIBLPass>(vulkanDevice, this, 256); // 创建 IBL 生成通道，贴图尺寸为 256。
    genIBL->SetCubeMap(cubeMaps[skyboxIndex]); // 设置初始立方体贴图。
    genIBL->SetModel(skyboxModel); // 设置天空盒模型。
    if (gltfModel) {
        genIBL->SetModel(gltfModel->getModel()); // 设置 glTF 模型。
    }
    genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube); // 设置主渲染通道的辐照度贴图描述符。
    genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube); // 设置主渲染通道的预过滤贴图描述符。

    // 执行一次 BRDF、球谐和 IBL 的渲染。否则初始会是黑球
    {
        VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true); // 创建主命令缓冲区。
        brdfPass->Draw(cmdBuf); // 绘制 BRDF 查找表。
        shGenPass->Draw(cmdBuf); // 执行球谐计算。
        genIBL->Draw(cmdBuf); // 绘制 IBL 贴图。
        vulkanDevice->flushCommandBuffer(cmdBuf, queue); // 提交并刷新命令缓冲区。
    }
    mainPass->UpdateBindings(); // 更新主渲染通道的描述符绑定。
}

void VulkanExample::ReginPrefilterPasses()
{
    // 重新生成预过滤通道（TODO）。
}

// =============================================================================
// Light Probe 相关函数
// =============================================================================

void VulkanExample::PrepareProbes()
{
    // 清理已有探针
    for (auto &p : lightProbes) {
        // if (p) p->Destroy();
    }
    lightProbes.clear();

    // 初始化立方体贴图插值对象（支持GPU加速）
    // 在多探针模式下，插值后再次插值将销毁之前所有探针
    if (!cubemapInterpolation) {
        cubemapInterpolation = std::make_unique<CubemapInterpolation>(vulkanDevice, this);
    } else {
        cubemapInterpolation->ClearProbes();
    }

    // 自动布置光照探针：在 probeGridConfig 定义的包围盒内使用指定的维度放置探针。
    // 如果 probeGridConfig 没有被设置（dimensions 为 0），则创建一个中心探针。
    glm::vec3 minB = probeGridConfig.minBounds;
    glm::vec3 maxB = probeGridConfig.maxBounds;
    glm::ivec3 dims = probeGridConfig.dimensions;
    // 多探针模式使用 16×16 分辨率
    uint32_t res = 16;

    if (dims.x <= 0 || dims.y <= 0 || dims.z <= 0) {
        // 创建一个单一探针，放在包围盒中心
        glm::vec3 center = (minB + maxB) * 0.5f;
        auto p = std::make_unique<LightProbe>(vulkanDevice, this, res, res);
        p->SetPosition(center);
        p->setSkybox(skybox.get());
        p->setPreviewModel(previewModel.get());
        if (gltfModel) p->SetGltfModel(gltfModel.get());
        lightProbes.push_back(std::move(p));
        return;
    }

    // 计算每个单元格大小并创建网格
    glm::vec3 extent = maxB - minB;
    glm::vec3 cellSize = glm::vec3(
        extent.x / static_cast<float>(dims.x),
        extent.y / static_cast<float>(dims.y),
        extent.z / static_cast<float>(dims.z)
    );

    for (int x = 0; x < dims.x; ++x) {
        for (int y = 0; y < dims.y; ++y) {
            for (int z = 0; z < dims.z; ++z) {
                glm::vec3 pos = minB + (glm::vec3(x, y, z) + 0.5f) * cellSize; // 放在单元中心
                auto p = std::make_unique<LightProbe>(vulkanDevice, this, res, res);
                p->SetPosition(pos);
                p->setSkybox(skybox.get());
                p->setPreviewModel(previewModel.get());
                p->SetGltfModel(gltfModel.get());
                lightProbes.push_back(std::move(p));
            }
        }
    }
}

// 自动捕获所有探针的立方体贴图
void VulkanExample::CaptureAllProbes()
{
    if (lightProbes.empty()) {
        std::cerr << "[VulkanExample::CaptureAllProbes] No probes to capture!" << std::endl;
        return;
    }

    std::cout << "[VulkanExample::CaptureAllProbes] Starting capture for " << lightProbes.size() << " probes..." << std::endl;

    // 为每个探针捕获立方体贴图
    for (size_t i = 0; i < lightProbes.size(); ++i) {
        auto& p = lightProbes[i];
        std::cout << "  Capturing probe " << (i + 1) << "/" << lightProbes.size() << "..." << std::endl;
        // 执行捕获
        p->CaptureCubeMap(queue);
        // 获取捕获的立方体贴图
        auto capturedCubemap = p->GetCubemap();
        if (!capturedCubemap) {
            std::cerr << "    Error: Failed to get cubemap for probe " << i << std::endl;
            continue;
        }
        // 添加到全局 cubeMaps 列表
        cubeMaps.push_back(capturedCubemap);
        std::string probeName = "Probe_" + std::to_string(i) + "_" + std::to_string(cubeMaps.size() - 1);
        cubemapNames.push_back(probeName);
        // 添加到插值对象
        if (cubemapInterpolation) {
            cubemapInterpolation->AddProbe(p->GetPosition(), capturedCubemap);
        }
        std::cout << "    ✓ Probe " << i << " captured successfully" << std::endl;
    }
    // 等待所有操作完成
    vkDeviceWaitIdle(vulkanDevice->logicalDevice);
    // 保存多探针捕获结果用于对比（使用最后一个探针或插值结果）
    // 此处直接用最后一个可能有bug
    // (TO FIX)
    if (!lightProbes.empty()) {
        multiProbeCubemap = lightProbes.back()->GetCubemap();
    }
    // 更新天空盒为最后一个捕获的探针
    if (!cubeMaps.empty()) {
        skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
        UpdateSkyBox();
        std::cout << "[VulkanExample::CaptureAllProbes] All probes captured! Updated skybox to probe " << skyboxIndex << std::endl;
    }
}
// 渲染相关函数
// =============================================================================

void VulkanExample::prepareData()
{
    // 准备渲染数据。
    // 使用分开的 projection 和 view 矩阵，与着色器匹配
    mainPassData.projection = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f); // 设置相机位置（齐次坐标）。
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f); // 设置光源位置

    // 更新光源参数
    mainPassData.useLightSource = lightEnabled ? 1 : 0;
    mainPassData.lightIntensity = lightIntensity;
    mainPassData.lightColor = lightColor;

    // 计算光源位置（绕Y轴旋转）
    if (lightEnabled) {
        float radius = 15.0f; // 减小旋转半径
        mainPassData.lightPosition = glm::vec3(
            0.0f, // 固定X位置
            5.5f, // 固定Y位置
            -9.0f  // 固定Z位置
        );

        // 如果启用自动旋转，添加旋转偏移
        if (autoRotateLight) {
            mainPassData.lightPosition.x += radius * sin(lightRotationAngle) * 0.3f; // 降低旋转幅度
            mainPassData.lightPosition.z += radius * cos(lightRotationAngle) * 0.3f; // 降低旋转幅度
            mainPassData.lightPosition.y += radius * cos(lightRotationAngle) * 0.3f; // 降低旋转幅度
        }
    } else {
        mainPassData.lightPosition = glm::vec3(0.0f, 5.5f, -9.0f);
    }

    mainPass->UpdateGlobal(mainPassData); // 更新主渲染通道的全局 UBO 数据。

    // 更新PRT光照
    UpdatePRTLighting();

    skybox->Update(camera.matrices.view); // 更新天空盒的视图矩阵。
}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // 根据对比模式选择渲染方式
    if (compareMode == RenderCompareMode::SPLIT_VIEW) {
        drawSplitView(cmd);
        return;
    }

    // 绘制单帧。
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        // 匿名函数：记录绘制命令。
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); // 绘制天空盒。

        // 绘制预览模型，使用光源位置
        if (previewModel) {
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, mainPassData.lightPosition);
        }

        if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); } // 添加空指针检查
        for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }

        // 渲染探针为球体
        // TO FIX(探针可视化有问题)
        if (showProbes) {
            for (const auto& probe : lightProbes) {
                probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            }
        }

        drawUI(cmd); // 绘制 UI（基类方法）。
    });
}

// 分屏对比渲染实现（简化版本）
void VulkanExample::drawSplitView(VkCommandBuffer cmd)
{
    // 保存原始设置
    auto savedCubemap = cubeMaps[skyboxIndex];

    // 计算每个视口的宽度（三分屏：原始 | 单探针 | 多探针）
    // TOFIX(分屏尺寸不对)
    uint32_t viewportWidth = width / 3;
    // uint32_t viewportHeight = height / 3;
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this, viewportWidth, savedCubemap](VkCommandBuffer cmd) {
        // === 左侧：原始环境 ===
        if (originalCubemap) {
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(viewportWidth);
            viewport.height = static_cast<float>(height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {viewportWidth, height};

            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // 临时切换到原始cubemap
            skybox->UpdateCubemap(originalCubemap);
            skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            // Always draw preview model (as light source when enabled, otherwise as regular preview)
            glm::vec3 previewPosition = glm::vec3(0.0f, 5.5f, -7.0f);
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, previewPosition);
            if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
            for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
        }

        // === 中间：单探针捕获 ===
        if (singleProbeCubemap) {
            VkViewport viewport{};
            viewport.x = static_cast<float>(viewportWidth);
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(viewportWidth);
            viewport.height = static_cast<float>(height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {static_cast<int32_t>(viewportWidth), 0};
            scissor.extent = {viewportWidth, height};

            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // 临时切换到单探针cubemap
            skybox->UpdateCubemap(singleProbeCubemap);
            skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            // Always draw preview model (as light source when enabled, otherwise as regular preview)
            glm::vec3 previewPosition = glm::vec3(0.0f, 5.5f, -7.0f);
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, previewPosition);
            if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
            for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
        }

        // === 右侧：多探针捕获/插值 ===
        if (multiProbeCubemap) {
            VkViewport viewport{};
            viewport.x = static_cast<float>(viewportWidth * 2);
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(viewportWidth);
            viewport.height = static_cast<float>(height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {static_cast<int32_t>(viewportWidth * 2), 0};
            scissor.extent = {viewportWidth, height};

            vkCmdSetViewport(cmd, 0, 1, &viewport);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            // 临时切换到多探针cubemap
            skybox->UpdateCubemap(multiProbeCubemap);
            skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, glm::vec3(0.0f, -5.f, -7.0f));
            if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
            for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }

            // 在多探针视图中显示探针位置
            if (showProbes) {
                for (const auto& probe : lightProbes) {
                    probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
                }
            }
        }

        // 恢复全屏viewport和scissor用于UI
        VkViewport fullViewport{};
        fullViewport.x = 0.0f;
        fullViewport.y = 0.0f;
        fullViewport.width = static_cast<float>(width);
        fullViewport.height = static_cast<float>(height);
        fullViewport.minDepth = 0.0f;
        fullViewport.maxDepth = 1.0f;

        VkRect2D fullScissor{};
        fullScissor.offset = {0, 0};
        fullScissor.extent = {width, height};

        vkCmdSetViewport(cmd, 0, 1, &fullViewport);
        vkCmdSetScissor(cmd, 0, 1, &fullScissor);

        // 恢复原始cubemap
        skybox->UpdateCubemap(savedCubemap);

        // 绘制UI
        drawUI(cmd);
    });
}

// =============================================================================
// UI 相关函数
// =============================================================================

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    // 更新 UI 覆盖界面。
    if (overlay->header("Settings")) { // 显示“设置”标题。
        // 显示当前相机位置（只读）
        overlay->text("Camera Position:");
        overlay->text("  X: %.2f", camera.position.x);
        overlay->text("  Y: %.2f", camera.position.y);
        overlay->text("  Z: %.2f", camera.position.z);

        if (overlay->inputFloat("Exposure", &mainPassData.exposure, 0.1f, 2)) { // 曝光度调节控件。
            globalDirty = true; // 标记全局数据需要更新。
        }
        if (overlay->inputFloat("Gamma", &mainPassData.gamma, 0.1f, 2)) { // 伽马值调节控件。
            globalDirty = true; // 标记全局数据需要更新。
        }
        if (overlay->comboBox("Skybox", &skyboxIndex, cubemapNames)) { // 天空盒选择下拉框。
            UpdateSkyBox(); // 更新天空盒。
        }
        if (overlay->comboBox("PreviewModel", &modelIndex, previewModelNames)) { // 预览模型选择下拉框。
            previewModel->UpdateModel(previewModels[modelIndex]); // 更新预览模型。
        }

        // GLTF 模型切换
        if (!gltfModelNames.empty() && gltfModel) {
            if (overlay->comboBox("GLTF Model", &gltfmodelIndex, gltfModelNames)) {
                gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
                std::cout << "[VulkanExample] Switched to GLTF model: " << gltfModelNames[gltfmodelIndex] << std::endl;
            }
        }

        // 光源控制
        if (overlay->header("Light Source")) {
            if (overlay->checkBox("Enable Light", &lightEnabled)) {
                globalDirty = true;
            }

            if (overlay->inputFloat("Light Intensity", &lightIntensity, 1.0f, 2)) {
                globalDirty = true;
            }

            if (overlay->checkBox("Auto Rotate", &autoRotateLight)) {
                globalDirty = true;
            }

            if (overlay->sliderFloat("Light Rotation", &lightRotationAngle, 0.0f, 6.28318f)) {
                globalDirty = true;
            }

            // 光源颜色控制
            float color[3] = { lightColor.r, lightColor.g, lightColor.b };
            if (overlay->colorPicker("Light Color", color)) {
                lightColor = glm::vec3(color[0], color[1], color[2]);
                previewModel->SetLightColor(lightColor);
                globalDirty = true;
            }
        }

        // PRT控制
        if (overlay->header("PRT Relighting")) {
            if (overlay->checkBox("Use PRT", &usePRT)) {
                if (usePRT) {
                    PrecomputePRT();
                }
                globalDirty = true;
            }

            if (overlay->sliderInt("SH Samples", &shSamples, 1, 128)) {
                if (usePRT) {
                    PrecomputePRT();
                }
                globalDirty = true;
            }

            overlay->text("Precomputed SH Coeffs: %zu", precomputedSHCoefficients.size());

            if (usePRT && !precomputedSHCoefficients.empty()) {
                overlay->text("PRT Status: Active");
            } else if (usePRT) {
                overlay->text("PRT Status: Computing...");
            } else {
                overlay->text("PRT Status: Inactive");
            }
        }

        // === PRT GPU 导出 ===
        if (overlay->header("PRT GPU Export")) {
            // Spotlight controls
            bool changed = false;
            changed |= overlay->sliderFloat("Spot Inner (deg)", &spotInnerDeg, 1.0f, 60.0f);
            changed |= overlay->sliderFloat("Spot Outer (deg)", &spotOuterDeg, 1.0f, 90.0f);
            if (spotOuterDeg < spotInnerDeg) spotOuterDeg = spotInnerDeg;
            if (changed) {
                spotInnerDeg = glm::clamp(spotInnerDeg, 1.0f, 89.0f);
                spotOuterDeg = glm::clamp(spotOuterDeg, spotInnerDeg, 90.0f);
            }
            // Irradiance A_l is always applied by default (no toggle)
            if (overlay->button("Export PRT (GPU)")) {
                ExportPRTDataGPU();
            }
            if (isExportingPRT) {
                overlay->text("Exporting: %s", prtExportStatus.c_str());
            }
        }

        if (overlay->button("Capture Cubemap at Camera")) { // 捕获立方体贴图按钮。
            CaptureCubemap(camera.position); // 在相机位置捕获立方体贴图。
        }
        // 使用LightProbe的静态UI方法（包含探针网格配置）
        LightProbe::ShowProbeGridUI(overlay, probeGridConfig, showProbes);

        if (probeGridConfig.enabled) {
            // 点击 "Generate Probes" 会生成探针
            if (overlay->button("Generate Probes")) {
                PrepareProbes();
            }

            // 动捕获所有探针的按钮
            if (overlay->button("Capture All Probes")) {
                CaptureAllProbes();
            }

            // 插值算法选择
            if (overlay->comboBox("Interpolation Mode", &interpolationModeIndex, interpolationModeNames)) {
                if (cubemapInterpolation) {
                    auto mode = static_cast<CubemapInterpolation::InterpolationMode>(interpolationModeIndex);
                    cubemapInterpolation->SetInterpolationMode(mode);
                    std::cout << "[VulkanExample] Interpolation mode changed to " << interpolationModeNames[interpolationModeIndex] << std::endl;
                }
            }

            // 立方体贴图插值按钮（支持GPU加速和多种算法）
            if (overlay->button("Interpolate Cubemap (GPU)")) {
                if (cubemapInterpolation && cubemapInterpolation->GetProbeCount() > 0) {
                    // 在相机位置进行插值，使用GPU加速，分辨率256x256
                    auto interpolatedCubemap = cubemapInterpolation->InterpolateAt(
                        camera.position,
                        50.0f,      // maxDistance
                        256,        // outputResolution
                        queue       // Vulkan队列
                    );
                    if (interpolatedCubemap) {
                        cubeMaps.push_back(interpolatedCubemap);
                        cubemapNames.push_back("Interpolated_" + interpolationModeNames[interpolationModeIndex] + "_" + std::to_string(cubeMaps.size() - 1));
                        skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
                        UpdateSkyBox();
                        // 更新天空盒(用于multiview)
                        multiProbeCubemap = interpolatedCubemap;
                        std::cout << "[VulkanExample] GPU-accelerated interpolated cubemap created using "
                                  << interpolationModeNames[interpolationModeIndex] << " at camera position" << std::endl;

                                }
                } else {
                    std::cerr << "[VulkanExample] No probes available for interpolation!" << std::endl;
                }

            }

            // 权重可视化按钮
            if (overlay->button("Visualize Weights (Heatmap)")) {
                if (cubemapInterpolation && cubemapInterpolation->GetProbeCount() > 0) {
                    // 可视化权重热力图
                    auto weightVisualization = cubemapInterpolation->VisualizeWeights(
                        256,        // outputResolution
                        queue,      // Vulkan队列
                        1           // visualizationMode: WEIGHT_HEATMAP
                    );
                    if (weightVisualization) {
                        cubeMaps.push_back(weightVisualization);
                        cubemapNames.push_back("WeightHeatmap_" + std::to_string(cubeMaps.size() - 1));
                        skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
                        UpdateSkyBox();
                        std::cout << "[VulkanExample] Weight heatmap visualization created" << std::endl;
                    }
                } else {
                    std::cerr << "[VulkanExample] No probes available for weight visualization!" << std::endl;
                }
            }

            // 显示最近探针ID的可视化
            if (overlay->button("Visualize Closest Probe ID")) {
                if (cubemapInterpolation && cubemapInterpolation->GetProbeCount() > 0) {
                    // 可视化最近探针的ID
                    auto probeIDVisualization = cubemapInterpolation->VisualizeWeights(
                        256,        // outputResolution
                        queue,      // Vulkan队列
                        2           // visualizationMode: CLOSEST_PROBE_ID
                    );
                    if (probeIDVisualization) {
                        cubeMaps.push_back(probeIDVisualization);
                        cubemapNames.push_back("ProbeID_" + std::to_string(cubeMaps.size() - 1));
                        skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
                        UpdateSkyBox();
                        std::cout << "[VulkanExample] Probe ID visualization created" << std::endl;
                    }
                } else {
                    std::cerr << "[VulkanExample] No probes available for probe ID visualization!" << std::endl;
                }
            }
        }
    }

    // 对比渲染模式UI
    if (overlay->header("Rendering Comparison")) {
        const char* compareModeNames[] = { "Normal", "Original Only", "Single Probe", "Multi Probe", "Split View" };
        int currentMode = static_cast<int>(compareMode);
        if (overlay->comboBox("Compare Mode", &currentMode, std::vector<std::string>(compareModeNames, compareModeNames + 5))) {
            SetCompareMode(static_cast<RenderCompareMode>(currentMode));
        }

        overlay->text("Current Mode:");
        switch (compareMode) {
            case RenderCompareMode::NORMAL:
                overlay->text("  Normal rendering");
                break;
            case RenderCompareMode::ORIGINAL_ONLY:
                overlay->text("  Original environment only");
                break;
            case RenderCompareMode::SINGLE_PROBE:
                overlay->text("  Single probe capture");
                break;
            case RenderCompareMode::MULTI_PROBE:
                overlay->text("  Multi-probe/interpolated");
                break;
            case RenderCompareMode::SPLIT_VIEW:
                overlay->text("  Split view (3-way)");
                overlay->text("  Left: Original | Middle: Single | Right: Multi");

                // 显示每个cubemap的状态
                overlay->text("Status:");
                overlay->text("  Original: %s", originalCubemap ? "Ready" : "Not captured");
                overlay->text("  Single: %s", singleProbeCubemap ? "Ready" : "Not captured");
                overlay->text("  Multi: %s", multiProbeCubemap ? "Ready" : "Not captured");

                if (!originalCubemap || !singleProbeCubemap || !multiProbeCubemap) {
                    overlay->text("Tip: Capture all modes first!");
                }
                break;
        }

        // 快速捕获按钮
        if (compareMode == RenderCompareMode::SPLIT_VIEW) {
            if (overlay->button("Quick Setup Split View")) {
                // 自动捕获所需的cubemap
                if (!originalCubemap && !cubeMaps.empty()) {
                    originalCubemap = cubeMaps[0];
                }
                if (!singleProbeCubemap) {
                    CaptureCubemap(camera.position);
                }
                std::cout << "[VulkanExample] Split view setup complete!" << std::endl;
            }
        }
    }

    previewModel->ShowUI(overlay); // 显示预览模型的 UI 控件。
}

// =============================================================================
// 立方体贴图捕获函数
// =============================================================================
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    probe = std::make_unique<LightProbe>(vulkanDevice, this, 1024, 1024);
    probe->SetPosition(position);
    probe->setSkybox(skybox.get());
    probe->setPreviewModel(previewModel.get());

    // 使用已经存在的 gltfModel（在 PrepareScene 中创建）
    // 如果 gltfModel 不存在，则创建一个新的
    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this, queue); // 传入queue用于纹理加载
        gltfModel->UpdateModel(previewModel->getModel());

        // 为MAIN技术准备PSO（用于主渲染）
        gltfModel->PreparePSO(
            renderPass,
            mainPass->descriptorSetLayout,
            ETechnique::MAIN
        );

        // CAPTURE_SCENE: 使用 capturePass 的 renderPass
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    } else {
        // 如果 gltfModel 已存在，确保 CAPTURE_SCENE PSO 已准备
        // 检查 CAPTURE_SCENE PSO 是否已准备
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    }

    probe->SetGltfModel(gltfModel.get());
    probe->CaptureCubeMap(queue);
    // 新增：保存六个方向图片，文件名前缀为 "Captured_"+编号
    std::string basePath = "Captured_" + std::to_string(cubeMaps.size()) + "_";
    probe->SaveCubeMapFaces(queue, basePath);

    // ... SH/IBL 生成 ...
    auto capturedCubemap = probe->GetCubemap();
    cubeMaps.push_back(capturedCubemap);
    cubemapNames.push_back("Captured_" + std::to_string(cubeMaps.size() - 1));
    skyboxIndex = static_cast<int>(cubeMaps.size() - 1);

    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    // --- 生成 SH ---
    shGenPass->SetCubeMap(capturedCubemap);
    shGenPass->Generate(queue);

    VkDescriptorBufferInfo shBufferInfo;
    shGenPass->FeedSH(shBufferInfo);                    // 正确！
    mainPass->environmemts.shCoeffs = shBufferInfo;     // 存入 MainPass

    genIBL->SetCubeMap(capturedCubemap);
    genIBL->Generate(queue);
    genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
    genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

    mainPass->UpdateBindings();
    skybox->SetCubeMap(capturedCubemap);
    // Enable SH and reflection on models after generating SH/IBL
    if (previewModel) {
        previewModel->SetUseSHAndReflection(true, true);
    }
    if (gltfModel) {
        //gltfModel->SetUseSHAndReflection(true, true);
    }

    lightProbes.push_back(std::move(probe));

    // 保存单探针捕获结果用于对比
    singleProbeCubemap = capturedCubemap;

    // 保存第一个cubemap作为original如果还未设置
    if (!originalCubemap && !cubeMaps.empty()) {
        originalCubemap = cubeMaps[0];
    }
}

// 对比渲染模式切换
void VulkanExample::SetCompareMode(RenderCompareMode mode)
{
    compareMode = mode;

    // 根据模式切换环境贴图
    switch (mode) {
        case RenderCompareMode::ORIGINAL_ONLY:
            if (!originalCubemap && !cubeMaps.empty()) {
                originalCubemap = cubeMaps[0]; // 假设第一个是原始贴图
            }
            if (originalCubemap) {
                skybox->UpdateCubemap(originalCubemap);
                shGenPass->SetCubeMap(originalCubemap);
                genIBL->SetCubeMap(originalCubemap);
            }
            break;

        case RenderCompareMode::SINGLE_PROBE:
            if (singleProbeCubemap) {
                skybox->UpdateCubemap(singleProbeCubemap);
                shGenPass->SetCubeMap(singleProbeCubemap);
                genIBL->SetCubeMap(singleProbeCubemap);
            }
            break;

        case RenderCompareMode::MULTI_PROBE:
            if (multiProbeCubemap) {
                skybox->UpdateCubemap(multiProbeCubemap);
                shGenPass->SetCubeMap(multiProbeCubemap);
                genIBL->SetCubeMap(multiProbeCubemap);
            }
            break;

        case RenderCompareMode::NORMAL:
        case RenderCompareMode::SPLIT_VIEW:
        default:
            // 保持当前
            break;
    }

    // 重新生成SH和IBL
    if (mode != RenderCompareMode::NORMAL && mode != RenderCompareMode::SPLIT_VIEW) {
        VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        shGenPass->Draw(cmdBuf);
        genIBL->Draw(cmdBuf);
        vulkanDevice->flushCommandBuffer(cmdBuf, queue);
        mainPass->UpdateBindings();
    }

    std::cout << "[VulkanExample] Compare mode set to: " << static_cast<int>(mode) << std::endl;
}

// =============================================================================
// PRT (Precomputed Radiance Transfer) 相关函数
// =============================================================================

void VulkanExample::PrecomputePRT()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "[VulkanExample] Starting PRT precomputation..." << std::endl;
    std::cout << "========================================\n" << std::endl;

    // ============================================================
    // 第1步: 生成采样方向
    // ============================================================
    std::cout << "[Step 1] Generating sample directions..." << std::endl;
    auto directions = SphericalHarmonics::GenerateFibonacciSamples(shSamples);
    std::cout << "  - Generated " << directions.size() << " sample directions" << std::endl;

    // ============================================================
    // 第2步: 预计算Lighting (光源的球谐系数)
    // ============================================================
    std::cout << "\n[Step 2] Precomputing Lighting (Light Source)..." << std::endl;

    // 生成采样光照 (使用当前光源颜色)
    // 在实际应用中，这应该从环境贴图或光源采样
    std::vector<glm::vec3> radiances;
    for (int i = 0; i < shSamples; i++) {
        radiances.push_back(lightColor * lightIntensity);
    }

    // 预计算光照的球谐系数
    SHCoefficients lightingCoeffs = PRTPrecomputer::PrecomputeLighting(directions, radiances);
    std::cout << "  - Lighting SH coefficients computed" << std::endl;
    std::cout << "  - Light Color: (" << lightColor.x << ", " << lightColor.y << ", " << lightColor.z << ")" << std::endl;
    std::cout << "  - Light Intensity: " << lightIntensity << std::endl;

    // ============================================================
    // 第3步: 预计算Light Transport (物体表面对光照的响应)
    // ============================================================
    std::cout << "\n[Step 3] Precomputing Light Transport (Surface Response)..." << std::endl;

    // 这里我们为Cornell Box的主表面预计算LT
    // 实际应用中应该对所有顶点/像素进行预计算
    glm::vec3 cornellNormal = glm::vec3(0.0f, 1.0f, 0.0f);  // 水平表面
    glm::vec3 cornellAlbedo = glm::vec3(0.8f, 0.8f, 0.8f);  // 灰色表面

    SHCoefficients ltCoeffs = PRTPrecomputer::PrecomputeLightTransport(
        glm::vec3(0.0f, 0.0f, 0.0f),
        cornellNormal,
        cornellAlbedo,
        directions
    );
    std::cout << "  - Light Transport SH coefficients computed" << std::endl;
    std::cout << "  - Surface Normal: (" << cornellNormal.x << ", " << cornellNormal.y << ", " << cornellNormal.z << ")" << std::endl;
    std::cout << "  - Surface Albedo: (" << cornellAlbedo.x << ", " << cornellAlbedo.y << ", " << cornellAlbedo.z << ")" << std::endl;

    // ============================================================
    // 第4步: 预计算不同旋转角度的光照系数
    // ============================================================
    std::cout << "\n[Step 4] Precomputing Light Rotations..." << std::endl;

    // 预计算不同旋转角度的系数 (24个旋转, 每15度一个)
    prtData = PRTPrecomputer::PrecomputeRotations(lightingCoeffs, 24, 360.0f);
    std::cout << "  - Precomputed " << prtData.size() << " rotations" << std::endl;
    std::cout << "  - Rotation step: " << (360.0f / prtData.size()) << " degrees" << std::endl;

    // ============================================================
    // 第5步: 导出到文件 (三项数据)
    // ============================================================
    std::cout << "\n[Step 5] Exporting PRT Data (Three Components)..." << std::endl;

    std::string baseFilename = "prt_data";

    // 导出第1项: Lighting (原始光源系数)
    std::cout << "\n  [5.1] Exporting Original Lighting..." << std::endl;
    std::string lightingOriginalFile = baseFilename + "_lighting_original.txt";
    std::vector<PRTPrecomputer::RotatedCoefficients> singleLighting;
    singleLighting.push_back({0.0f, lightingCoeffs});
    if (DataExporter::ExportLighting(lightingOriginalFile, singleLighting)) {
        std::cout << "    ✓ Exported to: " << lightingOriginalFile << std::endl;
    }

    // 导出第2项: Light Transport (物体表面响应)
    std::cout << "\n  [5.2] Exporting Light Transport..." << std::endl;
    std::string ltFile = baseFilename + "_lt.txt";
    if (DataExporter::ExportLightTransport(ltFile, ltCoeffs)) {
        std::cout << "    ✓ Exported to: " << ltFile << std::endl;
    }


/* MOVED ExportPRTDataGPU() below, after PrecomputePRT() ends */

    // 导出第3项: Rotated Lighting (旋转后的光源系数)
    std::cout << "\n  [5.3] Exporting Rotated Lighting (24 rotations)..." << std::endl;
    std::string rotatedLightingFile = baseFilename + "_lighting.txt";
    if (DataExporter::ExportLighting(rotatedLightingFile, prtData)) {
        std::cout << "    ✓ Exported to: " << rotatedLightingFile << std::endl;
        std::cout << "    ✓ Contains " << prtData.size() << " rotations" << std::endl;
    }

    // 设置当前系数
    currentSHCoefficients = lightingCoeffs;
    usePRT = true;

    std::cout << "\n========================================" << std::endl;
    std::cout << "[VulkanExample] PRT precomputation completed successfully!" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "\nSummary of exported files:" << std::endl;
    std::cout << "  1. " << lightingOriginalFile << " (Original Lighting)" << std::endl;
    std::cout << "  2. " << ltFile << " (Light Transport)" << std::endl;
    std::cout << "  3. " << rotatedLightingFile << " (Rotated Lighting - 24 angles)" << std::endl;
    std::cout << "\nYou can now use PRTRenderer to render with these precomputed data." << std::endl;
    std::cout << "========================================\n" << std::endl;
}

// GPU PRT export entry (currently CPU fallback; will switch to GPU compute)
void VulkanExample::ExportPRTDataGPU()
{
    if (isExportingPRT) { return; }
    isExportingPRT = true;
    prtExportStatus = "Generating samples";

    // 1) Generate sample directions and radiance (use current UI light)
    const int numSamples = glm::clamp(shSamples, 4, 64);
    auto directions = SphericalHarmonics::GenerateFibonacciSamples(numSamples);
    // Spotlight radiance: only strong near a given direction (flashlight-like)
    // Use current UI lightRotationAngle (radians) to rotate around Y-axis
    glm::vec3 lightDir = glm::normalize(glm::vec3(-sinf(lightRotationAngle), 0.0f, -cosf(lightRotationAngle)));
    // Use UI-driven inner/outer cone angles (degrees)
    const float cosOuter = cosf(glm::radians(spotOuterDeg));
    const float cosInner = cosf(glm::radians(spotInnerDeg));

    auto smoothstep = [](float edge0, float edge1, float x) {
        float t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    std::vector<glm::vec3> radiances;
    radiances.reserve(directions.size());
    for (const auto& w : directions) {
        float c = glm::dot(glm::normalize(w), lightDir);
        float falloff = (c <= cosOuter) ? 0.0f : smoothstep(cosOuter, cosInner, c);
        radiances.push_back(lightColor * lightIntensity * falloff);
    }

    // 2) Lighting SH (try GPU, fallback to CPU) + optional CPU vs GPU diff log
    SHCoefficients lightingSH{};
    bool usedGPU = false;
    if (prtCompute) {
        prtExportStatus = "Projecting lighting to SH (GPU)";
        PRT::GPUSHCoefficients gpuOut{};
        if (prtCompute->ComputeLightingProjection(directions, radiances, gpuOut)) {
            // Map GPU coeffs to CPU struct for downstream use
            for (int i = 0; i < 9; ++i) {
                lightingSH.coeffs[i] = glm::vec3(gpuOut.coeffs[i].x, gpuOut.coeffs[i].y, gpuOut.coeffs[i].z);
            }
            usedGPU = true;
            // Do CPU projection as reference to compare
            SHCoefficients cpuSH = SphericalHarmonics::ProjectLight(directions, radiances);
            std::cout << "[PRT Align] GPU vs CPU SH diffs (per i: |gpu-cpu| length)" << std::endl;
            for (int i = 0; i < 9; ++i) {
                glm::vec3 diff = lightingSH.coeffs[i] - cpuSH.coeffs[i];
                float err = glm::length(diff);
                std::cout << "  i=" << i << " err=" << err
                          << " gpu=(" << lightingSH.coeffs[i].x << "," << lightingSH.coeffs[i].y << "," << lightingSH.coeffs[i].z << ")"
                          << " cpu=(" << cpuSH.coeffs[i].x << "," << cpuSH.coeffs[i].y << "," << cpuSH.coeffs[i].z << ")"
                          << std::endl;
            }
        }
    }
    if (!usedGPU) {
        prtExportStatus = "Projecting lighting to SH (CPU)";
        lightingSH = SphericalHarmonics::ProjectLight(directions, radiances);
    }

    // 2.1 Optional: Apply irradiance multipliers A_l for l=0,1,2 (π, 2π/3, π/4)
    if (useIrradianceAL) {
        const float A0 = 3.14159265f;   // π
        const float A1 = 2.09439510f;   // 2π/3
        const float A2 = 0.78539816f;   // π/4
        // indices: 0 -> l=0, 1..3 -> l=1, 4..8 -> l=2
        lightingSH.coeffs[0] *= A0;
        for (int i = 1; i <= 3; ++i) lightingSH.coeffs[i] *= A1;
        for (int i = 4; i <= 8; ++i) lightingSH.coeffs[i] *= A2;
    }

    // 3) Light Transport (placeholder: a single canonical surface normal)
    prtExportStatus = "Computing LT (placeholder)";
    glm::vec3 ltNormal = glm::normalize(glm::vec3(0, 1, 0));
    glm::vec3 ltAlbedo = glm::vec3(0.8f);
    SHCoefficients ltSH = PRTPrecomputer::PrecomputeLightTransport(
        glm::vec3(0.0f), ltNormal, ltAlbedo, directions);

    // 4) Precompute rotations (24 samples over 360 degrees)
    prtExportStatus = "Precomputing rotations";
    auto rotations = PRTPrecomputer::PrecomputeRotations(lightingSH, 24, 360.0f);

    // 5) Export files
    prtExportStatus = "Exporting to txt";
    // Ensure output directory exists and print absolute path for debugging
    try {
        std::filesystem::create_directories("prt_output");
        std::cout << "[ExportPRTDataGPU] current_path=" << std::filesystem::current_path().string() << std::endl;
        std::cout << "[ExportPRTDataGPU] output dir = " << std::filesystem::absolute("prt_output").string() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ExportPRTDataGPU] Failed to create output dir: " << e.what() << std::endl;
    }
    std::string base = std::string("prt_output/") + "prt_data";

    bool ok1 = DataExporter::ExportLighting(base + "_lighting.txt", rotations);
    std::cout << "[ExportPRTDataGPU] Export lighting rotations => "
              << (ok1 ? "OK" : "FAILED") << ": "
              << std::filesystem::absolute(base + "_lighting.txt").string() << std::endl;

    bool ok2 = DataExporter::ExportLightTransport(base + "_lt.txt", ltSH);
    std::cout << "[ExportPRTDataGPU] Export LT (single) => "
              << (ok2 ? "OK" : "FAILED") << ": "
              << std::filesystem::absolute(base + "_lt.txt").string() << std::endl;

    std::vector<PRTPrecomputer::RotatedCoefficients> single;
    single.push_back({0.0f, lightingSH});
    bool ok3 = DataExporter::ExportLighting(base + "_lighting_original.txt", single);
    std::cout << "[ExportPRTDataGPU] Export lighting original => "
              << (ok3 ? "OK" : "FAILED") << ": "
              << std::filesystem::absolute(base + "_lighting_original.txt").string() << std::endl;

    prtExportStatus = (ok1 && ok2 && ok3) ? "Done" : "Done (with errors)";
    isExportingPRT = false;
}


void VulkanExample::UpdatePRTLighting()
{
    if (!usePRT || prtData.empty()) {
        return;
    }

    // 将弧度转换为度数
    float angleDegrees = lightRotationAngle * 180.0f / PI;

    // 查询对应旋转角度的球谐系数 (带插值)
    // 这是预计算的Lighting系数，对应当前光源旋转角度
    currentSHCoefficients = Relighter::QueryCoefficients(angleDegrees, prtData);

    // ============================================================
    // Relighting计算公式:
    // L_out = Σ(i=0 to 8) Lighting[i] * LightTransport[i]
    //
    // 其中:
    // - Lighting[i]: 当前旋转角度的光源球谐系数
    // - LightTransport[i]: 物体表面对光照的响应系数
    // ============================================================

    // 注意: 实际的relighting计算应该在着色器中进行
    // 这里只是更新了Lighting系数
    // 着色器会使用: currentSHCoefficients (Lighting) 和 ltCoeffs (Light Transport)
    // 来计算最终的relighting结果
}

VULKAN_EXAMPLE_MAIN()