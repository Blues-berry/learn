#include <cstdint>
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
#include "ProbeVisualizer.h"
#include <fstream>
#include "tiny_gltf.h"
#include "../base/VulkanTools.h"

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

// 配置：探针网格参数
struct ProbeGridConfig {
    glm::vec3 minBounds{ -10.0f, 0.0f, -10.0f };
    glm::vec3 maxBounds{ 10.0f, 4.0f, 10.0f };
    glm::ivec3 dimensions{ 4, 2, 4 };
    uint32_t resolution{ 32 };
};

// ✅ 新增：多探针数据结构体
struct ProbeData {
    glm::vec3 position;
    std::shared_ptr<vks::TextureCubeMap> cubemap;
    VkDescriptorImageInfo irradianceCube;
    VkDescriptorImageInfo prefilteredCube;
    VkDescriptorBufferInfo shCoeffs;
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
        camera.setRotation({ -3.75f, 180.0f, 0.0f }); // 设置初始旋转（俯仰、偏航、滚转）。
        camera.setPosition({ 0.55f, 0.85f, 12.0f }); // 设置初始位置 (x, y, z)。
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
    // ✅ 新增：自动捕获所有探针的立方体贴图

    int findNearestProbe(const glm::vec3& position);
    // ✅ 新增：根据位置找到最近的探针

    void updateProbeBindings(int probeIndex);
    // ✅ 新增：更新探针绑定（SH 和 IBL）

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

    void prepareData();
    // 声明准备数据函数，更新相机和全局数据。

    void draw()
    {
        // 绘制一帧的逻辑。
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
        VulkanExampleBase::submitFrame(); // 提交帧（基类方法，可能包括呈现交换链）。
    }

    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;
    // 声明 UI 覆盖更新函数，用于交互式设置。

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
    // ✅ 新增：多探针数据列表
    std::vector<ProbeData> multiProbeData;
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

    // ✅ 新增：探针可视化器
    std::unique_ptr<ProbeVisualizer> probeVisualizer;

    // ✅ 新增：探针显示模式
    enum class ProbeDisplayMode {
        NONE = 0,           // 不显示探针
        SINGLE = 1,         // 显示单个探针
        ALL = 2,            // 显示所有探针
        INTERPOLATED = 3    // 显示插值探针
    };
    ProbeDisplayMode probeDisplayMode = ProbeDisplayMode::NONE;

    // ✅ 新增：探针可视化参数
    float probeVisualizationScale = 0.2f;
    bool showProbes = false; // Toggle for visualizing probes
};

void VulkanExample::LoadAssets()
{
    // 加载资产，包括立方体贴图和 glTF 模型。
    LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT); // 加载“pisa”立方体贴图。
    LoadCubeMap("gcanyon", "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT); // 加载“gcanyon”立方体贴图。
    LoadCubeMap("uffizi", "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT); // 加载“uffizi”立方体贴图。

    uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY; // 设置 glTF 加载标志：预变换顶点，翻转 Y 轴。

    LoadPreviewModel("sphere", "models/sphere.gltf", glTFLoadingFlags); // 加载球体模型。
    LoadPreviewModel("teapot", "models/teapot.gltf", glTFLoadingFlags); // 加载茶壶模型。
    LoadPreviewModel("torusknot", "models/torusknot.gltf", glTFLoadingFlags); // 加载环面结模型。
    LoadPreviewModel("venus", "models/venus.gltf", glTFLoadingFlags); // 加载维纳斯模型。

   
    LoadgltfModel("FlightHelmet", "models/FlightHelmet/glTF/FlightHelmet.gltf", glTFLoadingFlags); // 
    LoadgltfModel("CesiumMan", "models/CesiumMan/glTF/CesiumMan.gltf", glTFLoadingFlags); // 
    skyboxModel = std::make_shared<vkglTF::Model>(); // 创建天空盒模型对象。
    skyboxModel->loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags); // 加载立方体模型作为天空盒。
    
    // auto gltfModel = std::make_shared<vkglTF::Model>(); // 创建 glTF 模型对象。
    // gltfModel->loadFromFile(getAssetPath() + "models/FlightHelmet/glTF/FlightHelmet.gltf", vulkanDevice, queue, glTFLoadingFlags); // 从文件加载模型。

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
    // ✅ 修复：初始化材质参数，不使用SH和反射（使用默认天空盒的IBL）
    previewModel->SetUseSHAndReflection(false, false);
    std::cout << "[PrepareScene] PreviewModel initialized: useSH=0, useReflection=0 (using default skybox IBL)" << std::endl;

    // ✅ 准备gltfModel - 为MainPass和CapturePass都准备PSO
    if (!gltfModels.empty()) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this); // 创建 glTF 模型对象。
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN); // 为MainPass准备PSO
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE); // 为CapturePass准备PSO
        gltfModel->UpdateModel(gltfModels[gltfmodelIndex]); // 设置第一个模型

        // 设置gltfModel的位置和缩放
        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(-20.0f, 0.0f, 0.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        gltfModel->SetTransform(t * s);
        // ✅ 修复：初始化材质参数，不使用SH和反射（使用默认天空盒的IBL）
        gltfModel->SetUseSHAndReflection(false, false);
        std::cout << "[PrepareScene] GltfModel initialized: useSH=0, useReflection=0 (using default skybox IBL)" << std::endl;
    }

    // ✅ 新增：初始化探针可视化器
    probeVisualizer = std::make_unique<ProbeVisualizer>(vulkanDevice, this);
    probeVisualizer->Initialize();
    // ✅ 设置球体模型（使用已加载的 preview 模型中的球体）
    if (!previewModels.empty()) {
        probeVisualizer->SetSphereModel(previewModels[0]);  // 使用第一个球体模型
    }
    probeVisualizer->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    probeVisualizer->SetProbeScale(probeVisualizationScale);
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

    // 执行一次 BRDF、球谐和 IBL 的渲染。
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
    // 重新生成预过滤通道（未实现）。
}

void VulkanExample::PrepareProbes()
{
    // 清理已有探针
    for (auto &p : lightProbes) {
        // if (p) p->Destroy();
    }
    lightProbes.clear();

    // 自动布置光照探针：在 probeGridConfig 定义的包围盒内使用指定的维度放置探针。
    // 如果 probeGridConfig 没有被设置（dimensions 为 0），则创建一个中心探针。
    glm::vec3 minB = probeGridConfig.minBounds;
    glm::vec3 maxB = probeGridConfig.maxBounds;
    glm::ivec3 dims = probeGridConfig.dimensions;
    // ✅ 修改：多探针模式使用 16×16 分辨率
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

// ✅ 新增：自动捕获所有探针的立方体贴图
void VulkanExample::CaptureAllProbes()
{
    if (lightProbes.empty()) {
        std::cerr << "[VulkanExample::CaptureAllProbes] No probes to capture!" << std::endl;
        return;
    }

    std::cout << "[VulkanExample::CaptureAllProbes] Starting capture for " << lightProbes.size() << " probes..." << std::endl;

    // 清空之前的多探针数据
    multiProbeData.clear();

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

        // ✅ 新增：为每个探针生成 SH 和 IBL
        std::cout << "    Generating SH and IBL for probe " << i << "..." << std::endl;

        // 生成 SH 系数
        shGenPass->SetCubeMap(capturedCubemap);
        shGenPass->Generate(queue);

        // 生成 IBL 贴图
        genIBL->SetCubeMap(capturedCubemap);
        genIBL->Generate(queue);

        // ✅ 修复：保存每个探针的数据到 ProbeData
        ProbeData data;
        data.position = p->GetPosition();
        data.cubemap = capturedCubemap;

        // 获取 SH 系数
        shGenPass->FeedSH(data.shCoeffs);

        // 获取 IBL 贴图
        genIBL->FeedIrradianceMap(data.irradianceCube);
        genIBL->FeedPrefilteredMap(data.prefilteredCube);

        multiProbeData.push_back(data);

        // 添加到全局 cubeMaps 列表（用于 UI 显示）
        cubeMaps.push_back(capturedCubemap);
        std::string probeName = "Probe_" + std::to_string(i) + "_" + std::to_string(cubeMaps.size() - 1);
        cubemapNames.push_back(probeName);

        std::cout << "    ✓ Probe " << i << " captured and processed successfully" << std::endl;
    }

    // 等待所有操作完成
    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    // ✅ 修复：不自动更新天空盒，让用户选择
    std::cout << "[VulkanExample::CaptureAllProbes] All " << multiProbeData.size() << " probes captured!" << std::endl;
    std::cout << "  Multi-probe data ready for interpolation" << std::endl;
}

// ✅ 新增：根据位置找到最近的探针
int VulkanExample::findNearestProbe(const glm::vec3& position)
{
    if (multiProbeData.empty()) {
        return -1;
    }

    int nearestIndex = 0;
    float minDistance = glm::distance(position, multiProbeData[0].position);

    for (size_t i = 1; i < multiProbeData.size(); ++i) {
        float distance = glm::distance(position, multiProbeData[i].position);
        if (distance < minDistance) {
            minDistance = distance;
            nearestIndex = static_cast<int>(i);
        }
    }

    return nearestIndex;
}

// ✅ 新增：更新探针绑定（SH 和 IBL）
void VulkanExample::updateProbeBindings(int probeIndex)
{
    if (probeIndex < 0 || probeIndex >= static_cast<int>(multiProbeData.size())) {
        std::cerr << "[VulkanExample::updateProbeBindings] Invalid probe index: " << probeIndex << std::endl;
        return;
    }

    const ProbeData& data = multiProbeData[probeIndex];

    // 更新 SH 系数
    mainPass->environmemts.shCoeffs = data.shCoeffs;

    // 更新 IBL 贴图
    mainPass->environmemts.irradianceCube = data.irradianceCube;
    mainPass->environmemts.prefilteredCube = data.prefilteredCube;

    // 更新描述符集
    mainPass->UpdateBindings();
}

void VulkanExample::prepareData()
{
    // 准备渲染数据。
    // ✅ 修复：使用分开的 projection 和 view 矩阵，与着色器匹配
    mainPassData.projection = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f); // 设置相机位置（齐次坐标）。
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f); // ✅ 设置光源位置

    mainPass->UpdateGlobal(mainPassData); // 更新主渲染通道的全局 UBO 数据。

    // ✅ 新增：多探针模式下根据相机位置更新 SH 和 IBL
    if (useMultipleProbes && !multiProbeData.empty()) {
        int nearestProbeIndex = findNearestProbe(camera.position);
        if (nearestProbeIndex >= 0) {
            updateProbeBindings(nearestProbeIndex);
        }
    }

    skybox->Update(camera.matrices.view); // 更新天空盒的视图矩阵。
}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // ✅ 删除：不应该在每帧都捕获立方体贴图
    // 捕获应该只在用户点击按钮时执行（在 CaptureCubemap() 中）

    // 绘制单帧。
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        // 匿名函数：记录绘制命令。
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); // 绘制天空盒。
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); // 绘制预览模型。
        if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); } // ✅ 添加空指针检查
        for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }

        // ✅ 修复：删除重复的数据更新，数据已在prepareData中更新

        // ✅ 新增：根据模式绘制探针
        if (probeDisplayMode != ProbeDisplayMode::NONE && probeVisualizer) {
            switch (probeDisplayMode) {
                case ProbeDisplayMode::SINGLE: {
                    // 显示单个探针（最后捕获的探针）
                    if (!lightProbes.empty()) {
                        probeVisualizer->DrawProbe(cmd, mainPass->descriptorSet, ETechnique::MAIN,
                                                  lightProbes.back()->GetPosition(),
                                                  glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // 红色
                    }
                    break;
                }
                case ProbeDisplayMode::ALL: {
                    // 显示所有探针
                    std::vector<glm::vec3> positions;
                    for (const auto& probe : lightProbes) {
                        positions.push_back(probe->GetPosition());
                    }
                    if (!positions.empty()) {
                        probeVisualizer->DrawProbes(cmd, mainPass->descriptorSet, ETechnique::MAIN, positions);
                    }
                    break;
                }
                case ProbeDisplayMode::INTERPOLATED: {
                    // 显示多探针插值（相机周围的探针）
                    if (useMultipleProbes && !multiProbeData.empty()) {
                        std::vector<glm::vec3> positions;
                        for (const auto& probeData : multiProbeData) {
                            positions.push_back(probeData.position);
                        }
                        if (!positions.empty()) {
                            probeVisualizer->DrawProbes(cmd, mainPass->descriptorSet, ETechnique::MAIN, positions);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        drawUI(cmd); // 绘制 UI（基类方法）。
    });
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    // 更新 UI 覆盖界面。
    if (overlay->header("Settings")) { // 显示“设置”标题。
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

        if (overlay->button("Capture Cubemap at Camera")) { // 捕获立方体贴图按钮。
            CaptureCubemap(camera.position); // 在相机位置捕获立方体贴图。
        }
        // Probe grid controls
        // ✅ 修复：分离 "Use Multiple Probes" 和 "Generate Probes" 的逻辑
        // 不在勾选时自动调用 PrepareProbes()，而是让用户手动点击 "Generate Probes"
        if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
            if (!useMultipleProbes) {
                // 取消勾选时清空探针
                lightProbes.clear();
            }
            // 勾选时不自动生成，等待用户点击 "Generate Probes" 按钮
        }

        if (useMultipleProbes) {
            float step = 0.1f;
            overlay->inputFloat("Probe Min X", &probeGridConfig.minBounds.x, step, 3);
            overlay->inputFloat("Probe Min Y", &probeGridConfig.minBounds.y, step, 3);
            overlay->inputFloat("Probe Min Z", &probeGridConfig.minBounds.z, step, 3);
            overlay->inputFloat("Probe Max X", &probeGridConfig.maxBounds.x, step, 3);
            overlay->inputFloat("Probe Max Y", &probeGridConfig.maxBounds.y, step, 3);
            overlay->inputFloat("Probe Max Z", &probeGridConfig.maxBounds.z, step, 3);
            overlay->sliderInt("Probe Dim X", &probeGridConfig.dimensions.x, 1, 20);
            overlay->sliderInt("Probe Dim Y", &probeGridConfig.dimensions.y, 1, 20);
            overlay->sliderInt("Probe Dim Z", &probeGridConfig.dimensions.z, 1, 20);
            int res = static_cast<int>(probeGridConfig.resolution);
            overlay->sliderInt("Probe Resolution", &res, 4, 256);
            probeGridConfig.resolution = static_cast<uint32_t>(res);

            // ✅ 修复：现在点击 "Generate Probes" 会真正生成探针
            if (overlay->button("Generate Probes")) {
                PrepareProbes();
            }

            // ✅ 新增：自动捕获所有探针的按钮
            if (overlay->button("Capture All Probes")) {
                CaptureAllProbes();
            }
        }

        // ✅ 新增：探针可视化模式选择
        if (overlay->header("Probe Visualization")) {
            std::vector<std::string> displayModes = { "None", "Single", "All", "Interpolated" };
            int modeIndex = static_cast<int>(probeDisplayMode);
            if (overlay->comboBox("Display Mode", &modeIndex, displayModes)) {
                probeDisplayMode = static_cast<ProbeDisplayMode>(modeIndex);
            }

            // 探针大小控制
            if (overlay->sliderFloat("Probe Scale", &probeVisualizationScale, 0.05f, 1.0f)) {
                if (probeVisualizer) {
                    probeVisualizer->SetProbeScale(probeVisualizationScale);
                }
            }
        }
    }

    previewModel->ShowUI(overlay); // 显示预览模型的 UI 控件。
}
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    probe = std::make_unique<LightProbe>(vulkanDevice, this, 1024, 1024);
    probe->SetPosition(position);
    probe->setSkybox(skybox.get());
    probe->setPreviewModel(previewModel.get());

    // ✅ 使用已经存在的 gltfModel（在 PrepareScene 中创建）
    // 如果 gltfModel 不存在，则创建一个新的
    if (!gltfModel) {
        std::cout << "[CaptureCubemap] Creating new gltfModel..." << std::endl;
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());

        // ✅ 为MAIN技术准备PSO（用于主渲染）
        gltfModel->PreparePSO(
            renderPass,
            mainPass->descriptorSetLayout,
            ETechnique::MAIN
        );

        // ✅ CAPTURE_SCENE: 使用 capturePass 的 renderPass
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );

        // ✅ 修复：初始化材质参数
        gltfModel->SetUseSHAndReflection(false, false);
        std::cout << "[CaptureCubemap] gltfModel created and initialized" << std::endl;
    } else {
        std::cout << "[CaptureCubemap] Using existing gltfModel (PSOs already prepared in PrepareScene)" << std::endl;
        // ✅ 修复：不要重复调用 PreparePSO，因为在 PrepareScene 中已经准备好了
        // 重复调用会导致资源泄漏和状态混乱
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
    std::cout << "[CaptureCubemap] Generating SH coefficients..." << std::endl;
    shGenPass->SetCubeMap(capturedCubemap);
    shGenPass->Generate(queue);

    VkDescriptorBufferInfo shBufferInfo;
    shGenPass->FeedSH(shBufferInfo);                    // 正确！
    mainPass->environmemts.shCoeffs = shBufferInfo;     // 存入 MainPass
    std::cout << "[CaptureCubemap] SH buffer: " << (shBufferInfo.buffer ? "Valid" : "NULL") << std::endl;

    // --- 生成 IBL ---
    std::cout << "[CaptureCubemap] Generating IBL maps..." << std::endl;
    genIBL->SetCubeMap(capturedCubemap);
    genIBL->Generate(queue);
    genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
    genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);
    std::cout << "[CaptureCubemap] Irradiance sampler: " << (mainPass->environmemts.irradianceCube.sampler ? "Valid" : "NULL") << std::endl;
    std::cout << "[CaptureCubemap] Prefiltered sampler: " << (mainPass->environmemts.prefilteredCube.sampler ? "Valid" : "NULL") << std::endl;

    // ✅ 修复：验证数据有效性后再更新绑定
    if (!mainPass->environmemts.shCoeffs.buffer) {
        std::cerr << "[CaptureCubemap] Warning: shCoeffs buffer not initialized!" << std::endl;
    }
    if (!mainPass->environmemts.brdfView.sampler) {
        std::cerr << "[CaptureCubemap] Warning: brdfView sampler not initialized!" << std::endl;
    }
    if (!mainPass->environmemts.irradianceCube.sampler) {
        std::cerr << "[CaptureCubemap] Warning: irradianceCube sampler not initialized!" << std::endl;
    }
    if (!mainPass->environmemts.prefilteredCube.sampler) {
        std::cerr << "[CaptureCubemap] Warning: prefilteredCube sampler not initialized!" << std::endl;
    }

    // ✅ 修复：先更新绑定，再设置材质参数
    mainPass->UpdateBindings();

    // ✅ 修复：等待所有GPU操作完成
    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    skybox->SetCubeMap(capturedCubemap);

    // ✅ 修复：启用SH和反射，并强制刷新材质buffer
    if (previewModel) {
        previewModel->SetUseSHAndReflection(true, true);
        std::cout << "[CaptureCubemap] PreviewModel: Enabled SH and Reflection" << std::endl;
    }
    if (gltfModel) {
        gltfModel->SetUseSHAndReflection(true, true);
        std::cout << "[CaptureCubemap] GltfModel: Enabled SH and Reflection" << std::endl;
    }

    lightProbes.push_back(std::move(probe));

    std::cout << "[CaptureCubemap] Capture complete! Models should now use captured lighting." << std::endl;
}
   
VULKAN_EXAMPLE_MAIN()