#include <cstdint>
#include <cmath>
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
        camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 512.0f); // 设置透视投影：60° 视角，宽高比基于窗口尺寸，近裁剪面 1.0（增加以改善深度精度），远裁剪面 512.0（支持大场景）。
        camera.rotationSpeed = 0.25f; // 设置相机旋转速度为 0.25。

        // 设置相机初始位置和朝向，正对模型。
        camera.setRotation({ 0.0f, 0.0f, 0.0f }); // 设置初始旋转（俯仰、偏航、滚转）。
        camera.setPosition({ 0.0f, 5.0f, 30.0f }); // 设置初始位置 (x, y, z)，距离模型30个单位，高度5个单位。

        // 启用深度钳制特性，防止超出远裁剪面的几何体被裁剪
        enabledFeatures.depthClamp = VK_TRUE;

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

    void LoadSibenikTextures();
    // 声明加载Sibenik纹理函数

    void ApplySibenikMaterial();
    // 应用Sibenik模型材质

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

    void drawSplitView(VkCommandBuffer cmd);
    // 分屏对比渲染：同时显示原始、单探针、多探针效果

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
        VulkanExampleBase::submitFrame(); // 提交帧（基类方法，包括呈现交换链）。
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
    // glTF模型名称列表。
    int32_t gltfmodelIndex = 2;  // 当前显示的glTF模型索引 (0=FlightHelmet, 1=CesiumMan, 2=vulkanscenemodels, 3=sibenik)

    // Sibenik 模型专用纹理（因为模型本身没有纹理）
    std::shared_ptr<vks::Texture2D> sibenikBaseColor;
    std::shared_ptr<vks::Texture2D> sibenikNormal;
    float sibenikMetallic = 0.0f;      // 石头金属度很低
    float sibenikRoughness = 0.8f;     // 石头粗糙度较高
    bool useSibenikTexture = true;     // 是否使用纹理
    float sibenikScale = 0.3f;         // Sibenik模型缩放因子（原始模型太大）
    glm::vec3 sibenikPosition = glm::vec3(0.0f, 30.0f, 0.0f);  // Sibenik模型位置（Y轴控制上下）

    // Sibenik纹理列表（多种不同类型的纹理）
    std::vector<std::shared_ptr<vks::Texture2D>> sibenikTextures;
    std::vector<std::string> sibenikTextureNames;
    int32_t sibenikTextureIndex = 0;   // 当前选中的纹理索引

    // Sibenik多材质纹理映射（为模型的不同部分分配不同纹理）
    std::vector<int32_t> sibenikMaterialTextureMap;  // 材质索引 -> 纹理索引映射

    // 渲染管线相关成员。
    bool globalDirty = true;
    // 全局数据脏标志，表示是否需要更新全局数据。
    MainPass::GlobalUbo mainPassData = {
        .exposure = 6.0f,  // 设置默认曝光度
        .gamma = 2.2f      // 设置默认伽马值
    };
    // 主渲染通道的统一缓冲区对象（UBO）。

    // 光照开关（4个动态光源）
    bool lightSwitches[4] = {true, true, true, true};  // 默认全部开启
    float lightIntensity[4] = {500000.0f, 500000.0f, 500000.0f, 500000.0f};  // 各光源强度
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

    // glTF模型对象（非 sibenik：用于切换展示的主模型）
    std::unique_ptr<GltfModel> gltfModel;

    // sibenik 独立模型：作为“环境场景/捕获源”，也可单独显示
    std::unique_ptr<GltfModel> sibenikModel;

    // 是否显示sibenik / 是否显示当前gltfModel（非sibenik）
    bool showSibenik = true;
    bool showSceneModel = false;

    // 启动时是否已经用 sibenik 场景生成过环境光( SH / IBL )
    bool sibenikEnvReady = false;

    void EnsureSibenikEnvironment();

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
    LoadPreviewModel("teapot", "models/teapot.gltf", glTFLoadingFlags); // 加载茶壶模型。
    LoadPreviewModel("torusknot", "models/torusknot.gltf", glTFLoadingFlags); // 加载环面结模型。
    LoadPreviewModel("venus", "models/venus.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    LoadPreviewModel("armor", "models/armor/armor.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    LoadPreviewModel("chinesedragon", "models/chinesedragon.gltf", glTFLoadingFlags); // 加载中国龙模型。
    LoadPreviewModel("fireplace", "models/fireplace.gltf", glTFLoadingFlags); // 加载壁炉模型。
    LoadPreviewModel("glowsphere", "models/glowsphere.gltf", glTFLoadingFlags); // 加载维纳斯模型。
    LoadPreviewModel("", "models/rock01.gltf", glTFLoadingFlags); // 加载模型。


    LoadgltfModel("vulkanscenemodels", "models/vulkanscenemodels.gltf", glTFLoadingFlags); //
    LoadgltfModel("CesiumMan", "models/CesiumMan/glTF/CesiumMan.gltf", glTFLoadingFlags); //
    //LoadgltfModel("Buggy", "models/Buggy.gltf", glTFLoadingFlags); //
    //LoadgltfModel("Buggy", "models/Buggy.gltf", glTFLoadingFlags); //
    LoadgltfModel("FlightHelmet", "models/FlightHelmet/glTF/FlightHelmet.gltf", glTFLoadingFlags); //
    LoadgltfModel("sibenik", "models/sibenik.gltf", glTFLoadingFlags); //
    skyboxModel = std::make_shared<vkglTF::Model>(); // 创建天空盒模型对象。
    skyboxModel->loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags); // 加载立方体模型作为天空盒。

    // 加载Sibenik专用纹理
    LoadSibenikTextures();
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

    // --- sibenik 独立模型：作为场景/捕获源，同时可选择单独显示 ---
    if (!gltfModels.empty()) {
        int32_t sibenikIdx = -1;
        for (int32_t i = 0; i < static_cast<int32_t>(gltfModelNames.size()); ++i) {
            if (gltfModelNames[i] == "sibenik") {
                sibenikIdx = i;
                break;
            }
        }

        if (sibenikIdx >= 0) {
            sibenikModel = std::make_unique<GltfModel>(vulkanDevice, this, queue);
            sibenikModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
            sibenikModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
            sibenikModel->UpdateModel(gltfModels[sibenikIdx]);

            // 复用现有材质设置逻辑（它基于 gltfModelNames[gltfmodelIndex]），
            // 所以这里临时切换 index，应用完再切回。
            const int32_t savedIndex = gltfmodelIndex;
            gltfmodelIndex = sibenikIdx;

            // 临时让 ApplySibenikMaterial() 作用于 sibenikModel
            // 直接在这里对 sibenikModel 做必要的设置，避免错误地操作 unique_ptr 所有权
            {
                const float savedMetallic = sibenikMetallic;
                const float savedRoughness = sibenikRoughness;
                const bool savedUseTex = useSibenikTexture;
                const float savedScale = sibenikScale;
                const glm::vec3 savedPos = sibenikPosition;

                // 手动复用 ApplySibenikMaterial 的关键行为
                sibenikModel->SetMetallic(savedMetallic);
                sibenikModel->SetRoughness(savedRoughness);

                auto model = sibenikModel->getModel();
                if (savedUseTex && !sibenikTextures.empty() && model && !model->materials.empty()) {
                    sibenikBaseColor = sibenikTextures[sibenikTextureIndex];
                    sibenikModel->SetUseTexture(true);
                    sibenikModel->SetBaseColorTexture(sibenikBaseColor.get());
                } else {
                    sibenikModel->SetUseTexture(false);
                    sibenikModel->SetAlbedo(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
                }

                glm::mat4 transformMatrix = glm::translate(glm::mat4(1.0f), savedPos) *
                                            glm::scale(glm::mat4(1.0f), glm::vec3(savedScale));
                sibenikModel->SetTransform(transformMatrix);
                sibenikModel->UpdateMaterial();
            }

            gltfmodelIndex = savedIndex;

            // 启动时默认只显示 sibenik
            showSibenik = true;
            showSceneModel = false;
        }

        // --- 其他 glTF 模型：用于演示 IBL 受环境影响的效果 ---
        // 默认选一个非 sibenik 的模型
        int32_t defaultSceneIdx = -1;
        for (int32_t i = 0; i < static_cast<int32_t>(gltfModelNames.size()); ++i) {
            if (gltfModelNames[i] != "sibenik") {
                defaultSceneIdx = i;
                break;
            }
        }

        if (defaultSceneIdx >= 0) {
            gltfmodelIndex = defaultSceneIdx;
            gltfModel = std::make_unique<GltfModel>(vulkanDevice, this, queue);
            gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
            gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
            gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
        }
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

void VulkanExample::LoadSibenikTextures()
{
    // 为Sibenik教堂模型加载多种不同类型的纹理
    std::cout << "[VulkanExample] Loading Sibenik textures..." << std::endl;

    // 定义纹理列表（多种材质类型）
    struct TextureInfo {
        std::string path;
        std::string name;
    };

    std::vector<TextureInfo> textureList = {
        // 石材类
        {"textures/stonefloor01_color_rgba.ktx", "Stone Floor 01"},
        {"textures/stonefloor02_color_rgba.ktx", "Stone Floor 02"},
        {"textures/stonefloor03_color_height_rgba.ktx", "Stone Floor 03"},

        // 岩石类
        {"textures/rocks_color_rgba.ktx", "Rocks"},

        // 金属类
        {"textures/metalplate01_rgba.ktx", "Metal Plate"},
        {"models/cerberus/albedo.ktx", "Cerberus Metal"},

        // 木质/箱子类
        {"textures/crate01_color_height_rgba.ktx", "Wood Crate 01"},
        {"textures/crate02_color_height_rgba.ktx", "Wood Crate 02"},

        // 地面类
        {"textures/ground_dry_rgba.ktx", "Dry Ground"},
        {"textures/gratefloor_rgba.ktx", "Grate Floor"},

        // 特殊材质
        {"textures/colored_glass_rgba.ktx", "Colored Glass"},
        {"textures/vulkan_cloth_rgba.ktx", "Cloth"},
        {"textures/lavaplanet_rgba.ktx", "Lava"},
        {"textures/vulkan_11_rgba.ktx", "Vulkan Logo"},

        // Sponza建筑纹理（适合教堂）
        {"models/sponza/16299174074766089871.ktx", "Sponza Wall"},
        {"models/sponza/10381718147657362067.ktx", "Sponza Brick"},
        {"models/sponza/10388182081421875623.ktx", "Sponza Arc"},
        {"models/sponza/11474523244911310074.ktx", "Sponza Ceiling"},
        {"models/sponza/11490520546946913238.ktx", "Sponza Floor"},

        // 其他
        {"textures/skysphere_rgba.ktx", "Sky"},
        {"textures/gridlines.ktx", "Grid"}
    };

    sibenikTextures.clear();
    sibenikTextureNames.clear();

    for (const auto& texInfo : textureList) {
        try {
            auto texture = std::shared_ptr<vks::Texture2D>(new vks::Texture2D(), [](vks::Texture2D* tex) {
                if (tex) {
                    tex->destroy();
                    delete tex;
                }
            });

            texture->loadFromFile(
                getAssetPath() + texInfo.path,
                VK_FORMAT_R8G8B8A8_UNORM,
                vulkanDevice,
                queue
            );

            sibenikTextures.push_back(texture);
            sibenikTextureNames.push_back(texInfo.name);

            std::cout << "  - Loaded: " << texInfo.name << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "  - Failed to load " << texInfo.name << ": " << e.what() << std::endl;
        }
    }

    // 默认使用第一个纹理
    if (!sibenikTextures.empty()) {
        sibenikBaseColor = sibenikTextures[0];
    }

    std::cout << "[VulkanExample] Loaded " << sibenikTextures.size() << " Sibenik textures!" << std::endl;
}

void VulkanExample::ApplySibenikMaterial()
{
    if (!gltfModel || gltfModelNames[gltfmodelIndex] != "sibenik") {
        return;
    }

    std::cout << "[VulkanExample] Applying Sibenik material settings..." << std::endl;

    // 应用材质参数
    gltfModel->SetMetallic(sibenikMetallic);
    gltfModel->SetRoughness(sibenikRoughness);

    // 获取模型材质数量
    auto model = gltfModel->getModel();
    if (!model) {
        std::cerr << "  ❌ Model is null!" << std::endl;
        return;
    }

    size_t materialCount = model->materials.size();
    std::cout << "  - Model has " << materialCount << " materials" << std::endl;

    // 检查Sibenik模型是否只有一个材质（根据之前的分析）
    if (materialCount == 1) {
        std::cout << "  ⚠️  Sibenik model has only 1 material!" << std::endl;
        std::cout << "  → All parts will use the same texture" << std::endl;
        std::cout << "  → To use different textures for different parts," << std::endl;
        std::cout << "    the model needs to be split into multiple materials" << std::endl;
    }

    if (useSibenikTexture && !sibenikTextures.empty() && materialCount > 0) {
        // 初始化材质-纹理映射（如果尚未初始化）
        if (sibenikMaterialTextureMap.size() != materialCount) {
            sibenikMaterialTextureMap.resize(materialCount);
            // 为每个材质分配不同的纹理（循环使用纹理列表）
            for (size_t i = 0; i < materialCount; i++) {
                sibenikMaterialTextureMap[i] = i % sibenikTextures.size();
            }
            std::cout << "  - Initialized texture mapping for " << materialCount << " materials" << std::endl;
        }

        // 当前限制：由于vkglTF::Model的draw实现，我们只能为整个模型设置一个纹理
        // 这里我们使用第一个材质的纹理设置
        sibenikBaseColor = sibenikTextures[sibenikTextureIndex];
        gltfModel->SetUseTexture(true);
        gltfModel->SetBaseColorTexture(sibenikBaseColor.get());
        std::cout << "  - Using texture: " << sibenikTextureNames[sibenikTextureIndex] << std::endl;

        // 打印材质-纹理映射（用于信息显示）
        if (materialCount > 1) {
            for (size_t i = 0; i < materialCount && i < sibenikMaterialTextureMap.size(); i++) {
                int texIdx = sibenikMaterialTextureMap[i];
                if (texIdx >= 0 && texIdx < sibenikTextures.size()) {
                    std::cout << "    Material " << i << " -> " << sibenikTextureNames[texIdx] << std::endl;
                }
            }
        }
    } else {
        gltfModel->SetUseTexture(false);
        // 使用灰色作为基础颜色
        gltfModel->SetAlbedo(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
        std::cout << "  - Using gray color (no texture)" << std::endl;
    }

    // 应用缩放和平移（组合变换）
    glm::mat4 transformMatrix = glm::translate(glm::mat4(1.0f), sibenikPosition) *
                                glm::scale(glm::mat4(1.0f), glm::vec3(sibenikScale));
    gltfModel->SetTransform(transformMatrix);
    std::cout << "  - Position: (" << sibenikPosition.x << ", " << sibenikPosition.y << ", " << sibenikPosition.z << ")" << std::endl;
    std::cout << "  - Scale: " << sibenikScale << std::endl;

    gltfModel->UpdateMaterial();
    std::cout << "  - Metallic: " << sibenikMetallic << std::endl;
    std::cout << "  - Roughness: " << sibenikRoughness << std::endl;
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
    if (sibenikModel) {
        genIBL->SetModel(sibenikModel->getModel()); // 用 sibenik 场景作为 IBL 生成/捕获的模型源
    } else if (gltfModel) {
        genIBL->SetModel(gltfModel->getModel()); // 兜底：没有 sibenik 时用当前 glTF 模型
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

    // 将SH和IBL资源传递给capturePass，确保捕获时使用正确的光照
    capturePass->FeedSH(mainPass->environmemts.shCoeffs);
    capturePass->FeedBRDF(mainPass->environmemts.brdfView);
    capturePass->FeedIrradiance(mainPass->environmemts.irradianceCube);
    capturePass->FeedPrefiltered(mainPass->environmemts.prefilteredCube);
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

    // 捕获源：默认用 sibenik 场景作为“环境来源”
    GltfModel* captureSourceModel = nullptr;
    if (sibenikModel) {
        captureSourceModel = sibenikModel.get();
    } else if (gltfModel) {
        captureSourceModel = gltfModel.get();
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
        if (captureSourceModel) p->SetGltfModel(captureSourceModel);
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
                if (captureSourceModel) {
                    p->SetGltfModel(captureSourceModel);
                }
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

        // 为最后一个探针生成SH和IBL，并启用球谐和反射
        auto lastCapturedCubemap = lightProbes.back()->GetCubemap();
        if (lastCapturedCubemap) {
            // --- 生成 SH ---
            shGenPass->SetCubeMap(lastCapturedCubemap);
            shGenPass->Generate(queue);

            VkDescriptorBufferInfo shBufferInfo;
            shGenPass->FeedSH(shBufferInfo);
            mainPass->environmemts.shCoeffs = shBufferInfo;

            // --- 生成 IBL ---
            genIBL->SetCubeMap(lastCapturedCubemap);
            genIBL->Generate(queue);
            genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
            genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

            // 更新主通道绑定
            mainPass->UpdateBindings();

            // 启用球谐和反射
            if (previewModel) {
                previewModel->SetUseSHAndReflection(true, true);
            }
            if (gltfModel) {
                gltfModel->SetUseSHAndReflection(true, true);
            }

            std::cout << "[VulkanExample::CaptureAllProbes] Generated SH/IBL for the last probe and enabled SH/reflection on models" << std::endl;
        }
    }

    // 更新天空盒为最后一个捕获的探针
    if (!cubeMaps.empty()) {
        skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
        UpdateSkyBox();
        std::cout << "[VulkanExample::CaptureAllProbes] All probes captured! Updated skybox to probe " << skyboxIndex << std::endl;
    }
} // <--- Added closing bracket here

// =============================================================================
// 渲染相关函数
// =============================================================================

void VulkanExample::prepareData()
{
    // 准备渲染数据。
    // 使用分开的 projection 和 view 矩阵，与着色器匹配
    mainPassData.projection = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f); // 设置相机位置（齐次坐标）。

    const float angle = timer * glm::two_pi<float>();
    const float radius = 180.0f;
    const float height = 120.0f;

    // 根据光照开关设置光源（w分量为强度，0表示关闭）
    mainPassData.light[0] = glm::vec4(
        radius * std::cos(angle + 0.0f),
        height,
        radius * std::sin(angle + 0.0f),
        lightSwitches[0] ? lightIntensity[0] : 0.0f
    );
    mainPassData.light[1] = glm::vec4(
        radius * std::cos(angle + glm::half_pi<float>()),
        height * 0.6f,
        radius * std::sin(angle + glm::half_pi<float>()),
        lightSwitches[1] ? lightIntensity[1] : 0.0f
    );
    mainPassData.light[2] = glm::vec4(
        radius * std::cos(angle + glm::pi<float>()),
        height,
        radius * std::sin(angle + glm::pi<float>()),
        lightSwitches[2] ? lightIntensity[2] : 0.0f
    );
    mainPassData.light[3] = glm::vec4(
        radius * std::cos(angle + glm::three_over_two_pi<float>()),
        height * 0.6f,
        radius * std::sin(angle + glm::three_over_two_pi<float>()),
        lightSwitches[3] ? lightIntensity[3] : 0.0f
    );

    mainPassData.lightColor[0] = glm::vec4(1.0f, 0.35f, 0.25f, 1.0f);
    mainPassData.lightColor[1] = glm::vec4(0.25f, 1.0f, 0.35f, 1.0f);
    mainPassData.lightColor[2] = glm::vec4(0.25f, 0.45f, 1.0f, 1.0f);
    mainPassData.lightColor[3] = glm::vec4(1.0f, 1.0f, 0.25f, 1.0f);

    mainPass->UpdateGlobal(mainPassData); // 更新主渲染通道的全局 UBO 数据。

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
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); // 绘制预览模型。
        if (showSibenik && sibenikModel) { sibenikModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
        if (showSceneModel && gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); } // 绘制glTF模型

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
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
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
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
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
            previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            if (gltfModel) { gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }
        }
    });
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

    // 捕获源：优先用 sibenik 场景作为环境来源
    if (sibenikModel) {
        probe->SetGltfModel(sibenikModel.get());
    } else if (gltfModel) {
        probe->SetGltfModel(gltfModel.get());
    }
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
        gltfModel->SetUseSHAndReflection(true, true);  // 启用 glTF 模型的 SH 和反射
    }
    if (sibenikModel) {
        sibenikModel->SetUseSHAndReflection(true, true);
    }

    lightProbes.push_back(std::move(probe));

    // 保存单探针捕获结果用于对比
    singleProbeCubemap = capturedCubemap;

    // 保存第一个cubemap作为original如果还未设置
    if (!originalCubemap && !cubeMaps.empty()) {
        originalCubemap = cubeMaps[0];
    }
}

void VulkanExample::EnsureSibenikEnvironment()
{
    if (sibenikEnvReady) {
        return;
    }
    if (!sibenikModel || !skybox || !shGenPass || !genIBL || !mainPass) {
        return;
    }

    // 通过 CaptureCubemap() 走完整的：捕获 -> 生成 SH/IBL -> 更新 mainPass 绑定
    // CaptureCubemap 内部已优先使用 sibenikModel 作为捕获源
    CaptureCubemap(camera.position);
    sibenikEnvReady = true;
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

// UI覆盖更新函数
void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    // 曝光度和伽马值调节
    if (overlay->inputFloat("Exposure", &mainPassData.exposure, 0.1f, 2)) {
        globalDirty = true;
    }
    if (overlay->inputFloat("Gamma", &mainPassData.gamma, 0.1f, 2)) {
        globalDirty = true;
    }

    // 天空盒选择
    if (overlay->comboBox("Skybox", &skyboxIndex, cubemapNames)) {
        UpdateSkyBox();
    }

    // 预览模型选择
    if (overlay->comboBox("PreviewModel", &modelIndex, previewModelNames)) {
        previewModel->UpdateModel(previewModels[modelIndex]);
    }

    // GLTF 模型切换（排除 sibenik，sibenik 作为独立环境场景模型）
    if (!gltfModelNames.empty() && gltfModel) {
        // 构建一个不包含 sibenik 的可选列表
        static std::vector<std::string> sceneModelNames;
        static std::vector<int32_t> sceneModelIndices;
        if (sceneModelNames.empty()) {
            sceneModelNames.clear();
            sceneModelIndices.clear();
            for (int32_t i = 0; i < static_cast<int32_t>(gltfModelNames.size()); ++i) {
                if (gltfModelNames[i] != "sibenik") {
                    sceneModelNames.push_back(gltfModelNames[i]);
                    sceneModelIndices.push_back(i);
                }
            }
        }

        // 当前 scene 下拉框索引
        static int32_t sceneComboIndex = 0;
        // 尝试让 comboIndex 与当前 gltfmodelIndex 对齐
        bool needUpdateCombo = false;
        for (int32_t i = 0; i < static_cast<int32_t>(sceneModelIndices.size()); ++i) {
            if (sceneModelIndices[i] == gltfmodelIndex) {
                if (sceneComboIndex != i) {
                    sceneComboIndex = i;
                    needUpdateCombo = true;
                }
                break;
            }
        }

        // 确保 gltfModel 已初始化
        if (!gltfModel && !gltfModels.empty()) {
            gltfModel = std::make_unique<GltfModel>(vulkanDevice, this, queue);
            gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
            gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
            gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
            gltfModel->SetUseSHAndReflection(true, true);
            std::cout << "[VulkanExample] Initialized GLTF model: " << gltfModelNames[gltfmodelIndex] << std::endl;
        }

        if (overlay->comboBox("GLTF Model", &sceneComboIndex, sceneModelNames) || needUpdateCombo) {
            if (sceneComboIndex >= 0 && sceneComboIndex < static_cast<int32_t>(sceneModelIndices.size())) {
                int32_t newIndex = sceneModelIndices[sceneComboIndex];
                if (newIndex != gltfmodelIndex) {
                    gltfmodelIndex = newIndex;
                    if (gltfModel) {
                        gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
                        std::cout << "[VulkanExample] Switched to GLTF model: " << gltfModelNames[gltfmodelIndex] << std::endl;
                    }
                }
            }
        }

        // 显示开关：默认 sibenik 独立显示，其他模型可单独打开
        overlay->checkBox("Show Sibenik", &showSibenik);
        overlay->checkBox("Show GLTF Model", &showSceneModel);
    }

    // Sibenik 模型材质控制
    if (!gltfModelNames.empty() && gltfmodelIndex >= 0 && gltfmodelIndex < gltfModelNames.size()) {
        if (gltfModelNames[gltfmodelIndex] == "sibenik") {
            if (overlay->header("Sibenik Material")) {
                overlay->text("Model has no embedded textures");
                overlay->text("Size: ~40x30x17 units");
                overlay->text("Vertices: 225,849");

                // 使用纹理开关
                if (overlay->checkBox("Use Texture", &useSibenikTexture)) {
                    std::cout << "[VulkanExample] Use texture: " << (useSibenikTexture ? "YES" : "NO") << std::endl;
                    ApplySibenikMaterial();
                }

                // 获取模型材质数量
                int materialCount = 0;
                if (gltfModel && gltfModel->getModel()) {
                    materialCount = static_cast<int>(gltfModel->getModel()->materials.size());
                }

                // 为每个材质选择纹理
                if (useSibenikTexture && !sibenikTextureNames.empty() && materialCount > 0) {
                    overlay->text("Material Texture Assignment:");

                    // 确保材质纹理映射数组大小匹配
                    if (sibenikMaterialTextureMap.size() != materialCount) {
                        sibenikMaterialTextureMap.resize(materialCount);
                        for (int i = 0; i < materialCount; i++) {
                            sibenikMaterialTextureMap[i] = i % sibenikTextures.size();
                        }
                    }

                    // 为每个材质显示纹理选择下拉框（最多显示8个以避免UI过长）
                    int displayCount = std::min(materialCount, 8);
                    for (int i = 0; i < displayCount; i++) {
                        std::string label = std::string("Mat ") + std::to_string(i);
                        if (overlay->comboBox(label.c_str(), &sibenikMaterialTextureMap[i], sibenikTextureNames)) {
                            std::cout << "[VulkanExample] Material " << i
                                      << " -> " << sibenikTextureNames[sibenikMaterialTextureMap[i]] << std::endl;
                            ApplySibenikMaterial();
                        }
                    }

                    if (materialCount > 8) {
                        char buffer[128];
                        sprintf(buffer, "... and %d more materials", materialCount - 8);
                        overlay->text(buffer);
                    }
                }

                // 金属度调整
                if (overlay->sliderFloat("Metallic", &sibenikMetallic, 0.0f, 1.0f)) {
                    std::cout << "[VulkanExample] Sibenik metallic: " << sibenikMetallic << std::endl;
                    ApplySibenikMaterial();
                }

                // 粗糙度调整
                if (overlay->sliderFloat("Roughness", &sibenikRoughness, 0.0f, 1.0f)) {
                    std::cout << "[VulkanExample] Sibenik roughness: " << sibenikRoughness << std::endl;
                    ApplySibenikMaterial();
                }

                // 模型缩放调整
                if (overlay->sliderFloat("Scale", &sibenikScale, 0.1f, 2.0f)) {
                    std::cout << "[VulkanExample] Sibenik scale: " << sibenikScale << std::endl;
                    ApplySibenikMaterial();
                }
            }
        }
    }

    // 捕获立方体贴图按钮
    if (overlay->button("Capture Cubemap at Camera")) {
        CaptureCubemap(camera.position);
    }

    // 使用LightProbe的静态UI方法（包含探针网格配置）
    LightProbe::ShowProbeGridUI(overlay, probeGridConfig, showProbes);

    if (probeGridConfig.enabled) {
        // 生成探针按钮
        if (overlay->button("Generate Probes")) {
            PrepareProbes();
        }

        // 捕获所有探针的按钮
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
                auto interpolatedCubemap = cubemapInterpolation->InterpolateAt(
                    camera.position,
                    50.0f,
                    256,
                    queue
                );
                if (interpolatedCubemap) {
                    cubeMaps.push_back(interpolatedCubemap);
                    cubemapNames.push_back("Interpolated_" + interpolationModeNames[interpolationModeIndex] + "_" + std::to_string(cubeMaps.size() - 1));
                    skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
                    UpdateSkyBox();
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
                auto weightVisualization = cubemapInterpolation->VisualizeWeights(
                    256,
                    queue,
                    1
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
                auto probeIDVisualization = cubemapInterpolation->VisualizeWeights(
                    256,
                    queue,
                    2
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

    // 光照控制UI
    if (overlay->header("Lighting Control")) {
        // 全局光照开关
        static bool allLightsOn = true;
        if (overlay->checkBox("All Lights", &allLightsOn)) {
            for (int i = 0; i < 4; i++) {
                lightSwitches[i] = allLightsOn;
            }
            globalDirty = true;
        }

        overlay->text("Individual Lights:");

        // 每个光源的单独控制
        const char* lightNames[] = { "Light 1 (Red)", "Light 2 (Green)", "Light 3 (Blue)", "Light 4 (Yellow)" };
        for (int i = 0; i < 4; i++) {
            if (overlay->checkBox(lightNames[i], &lightSwitches[i])) {
                globalDirty = true;
            }
        }

        overlay->text("Light Intensity:");

        // 每个光源的强度滑块
        for (int i = 0; i < 4; i++) {
            std::string sliderLabel = std::string("Int ") + std::to_string(i + 1);
            if (overlay->sliderFloat(sliderLabel.c_str(), &lightIntensity[i], 0.0f, 1000000.0f)) {
                globalDirty = true;
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

    previewModel->ShowUI(overlay);
}

VULKAN_EXAMPLE_MAIN()