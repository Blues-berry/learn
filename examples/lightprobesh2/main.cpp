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

    // ✅ 准备gltfModel - 为MainPass和CapturePass都准备PSO
    if (!gltfModels.empty()) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this); // 创建 glTF 模型对象。
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN); // 为MainPass准备PSO
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE); // 为CapturePass准备PSO
        gltfModel->UpdateModel(gltfModels[0]); // 设置第一个模型

        // 设置gltfModel的位置和缩放
        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(-20.0f, 0.0f, 0.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        gltfModel->SetTransform(t * s);
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
    uint32_t res = probeGridConfig.resolution > 0 ? probeGridConfig.resolution : 32; // 默认 32

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
                if (gltfModel) p->SetGltfModel(gltfModel.get());
                lightProbes.push_back(std::move(p));
            }
        }
    }
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

    skybox->Update(camera.matrices.view); // 更新天空盒的视图矩阵。
}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    if (probe) {
        probe->CaptureCubeMap(queue, cmd);
    }

    // 绘制单帧。
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        // 匿名函数：记录绘制命令。
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); // 绘制天空盒。
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); // 绘制预览模型。
        if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); } // ✅ 添加空指针检查
        for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }

        // ✅ 修复：删除重复的数据更新，数据已在prepareData中更新

        // 新增：渲染探针为球体
        if (showProbes) {
            for (const auto& probe : lightProbes) {
                probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
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
        if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
            if (useMultipleProbes) {
                PrepareProbes();
            } else {
                lightProbes.clear();
            }
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
            if (overlay->button("Generate Probes")) {
                PrepareProbes();
            }
        }

        // Add toggle for probe visualization
        overlay->checkBox("Show Probes", &showProbes);
    }

    previewModel->ShowUI(overlay); // 显示预览模型的 UI 控件。
}
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    probe = std::make_unique<LightProbe>(vulkanDevice, this, 1024, 1024);
    probe->SetPosition(position);
    probe->setSkybox(skybox.get());
    probe->setPreviewModel(previewModel.get());

    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());

        // ✅ 为MAIN技术准备PSO（用于主渲染）
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
        gltfModel->SetUseSHAndReflection(true, true);
    }

    lightProbes.push_back(std::move(probe));
}  
   
VULKAN_EXAMPLE_MAIN()