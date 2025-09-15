#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "LightProbe.h"
#include "GltfScene.h"
#include "Skybox.h"
#include "Pass.h"
#include "ILoader.h"
#include "PreviewModel.h"
#include <fstream>


class VulkanExample : public VulkanExampleBase, public IExampleInterfasce
{
public:
    VulkanExample() : VulkanExampleBase()
    {
        camera.type = Camera::CameraType::firstperson;//设置相机为第一人称模式。
        camera.movementSpeed = 4.0f;//设置相机移动速度为4.0单位/秒。
        camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);//设置透视投影，视场角60度，宽高比基于窗口尺寸，近裁剪面0.1，远裁剪面256.0。
        camera.rotationSpeed = 0.25f;//设置相机旋转速度为0.25。

        // 设置相机初始位置和朝向
        camera.setRotation({ -3.75f, 180.0f, 0.0f });
        camera.setPosition({ 0.55f, 0.85f, 12.0f });//设置相机初始位置为(0.55, 0.85, 12.0)。
    }

        ~VulkanExample() override
    {
        vkDeviceWaitIdle(device); // 等待设备空闲，确保所有命令完成

        if (skybox)
        {
            skybox->Destroy(); // 销毁天空盒资源
            skybox = nullptr; // 清空指针
        }

        if (previewModel)
        {
            previewModel->Destroy(); // 销毁预览模型资源
            previewModel = nullptr; // 清空指针
        }
        
        cubeMaps.clear(); // 清空立方体贴图列表

        mainPass = nullptr; // 清空主渲染通道（智能指针自动释放）
    }

	void LoadAssets(); // 加载资产（立方体贴图、预览模型、场景）
    void LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format); // 加载立方体贴图
    void LoadPreviewModel(const std::string& name, const std::string& cubemapPath); // 加载预览模型
    void LoadScene(); // 加载场景（天空盒和预览模型）

    void PrepareProbes(); // 准备光照探针（未实现）
    void PreparePasses(); // 准备渲染通道

    void ReginPrefilterPasses(); // 预滤波通道（未实现，拼写错误：应为 RegisterPrefilterPasses）

    void prepare() override
    {
        VulkanExampleBase::prepare(); // 调用基类 prepare 方法，初始化 Vulkan 资源
        PreparePasses(); // 准备渲染通道
        LoadAssets(); // 加载资产
        prepared = true; // 标记准备完成
    }

    void render() override
    {
        if (!prepared)
            return; // 如果未准备好，直接返回
        draw(); // 调用绘制方法
    }

        void drawFrame(VkCommandBuffer cmd); // 绘制单帧

    void prepareData(); // 准备渲染数据（如相机矩阵）

    void draw()
    {
        prepareData(); // 准备渲染数据

        VulkanExampleBase::prepareFrame(); // 准备帧（基类方法，可能设置信号量等）

        VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer]; // 获取当前帧的命令缓冲区

        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo(); // 初始化命令缓冲区开始信息
        VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo)); // 开始记录命令

        drawFrame(cmdBuffer); // 绘制帧

        VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer)); // 结束命令记录

        submitInfo.commandBufferCount = 1; // 设置提交的命令缓冲区数量
        submitInfo.pCommandBuffers = &cmdBuffer; // 指定命令缓冲区
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE)); // 提交命令到队列
        VulkanExampleBase::submitFrame(); // 提交帧（基类方法，可能等待信号量）
    }
    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override; // 更新 UI 覆盖层

    VkPipelineShaderStageCreateInfo LoadShader(const std::string& path, VkShaderStageFlagBits stage) override
    {
        return loadShader(getShadersPath() + path, stage); // 加载着色器文件
    }

private:
    // VulkanglTFScene glTFScene; // glTF 场景（注释掉，未使用）

    std::vector<std::unique_ptr<LightProbe>> lightProbes; // 光照探针列表（未使用）

    // skybox
    std::vector<std::shared_ptr<vks::TextureCubeMap>> cubeMaps; // 立方体贴图列表
    std::vector<std::string> cubemapNames; // 立方体贴图名称
    int32_t skyboxIndex = 0; // 当前天空盒索引

    // preview model
    std::vector<std::shared_ptr<vkglTF::Model>> previewModels; // 预览模型列表
    std::vector<std::string> previewModelNames; // 预览模型名称
    int32_t modelIndex = 0; // 当前模型索引

    // pipeline
    bool globalDirty = true; // 全局数据是否需要更新
    MainPass::GlobalUbo mainPassData = {}; // 主渲染通道的统一缓冲区对象
    std::unique_ptr<MainPass> mainPass; // 主渲染通道
    std::unique_ptr<GenBRDFLutPass> brdfPass; // BRDF 查找表生成通道

    // scene
    struct SceneTextures
    {
        VkImageView brdfView; // BRDF 查找表图像视图（弱引用）
        VkImageView irradianceCube; // 辐照度立方体贴图（弱引用，未使用）
        VkImageView prefilteredCube; // 预滤波立方体贴图（弱引用，未使用）
    };

    std::unique_ptr<Skybox> skybox; // 天空盒
    std::unique_ptr<PreviewModel> previewModel; // 预览模型
    SceneTextures sceneTextures; // 场景纹理
};

void VulkanExample::LoadAssets()
{
    LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
    LoadCubeMap("gcanyon", "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
    LoadCubeMap("uffizi", "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);

    LoadPreviewModel("sphere", "models/sphere.gltf");
    LoadPreviewModel("teapot", "models/teapot.gltf");
    LoadPreviewModel("torusknot", "models/torusknot.gltf");
    LoadPreviewModel("venus", "models/venus.gltf");

    LoadScene();
}

void VulkanExample::LoadScene()
{
    skybox = std::make_unique<Skybox>(vulkanDevice, this); // 创建天空盒对象
    skybox->LoadFromPath("models/cube.gltf", queue); // 加载立方体模型作为天空盒
    skybox->PreparePSO(renderPass, mainPass->descriptorSetLayout); // 准备天空盒的管线状态对象（PSO）
    skybox->UpdateCubemap(cubeMaps[skyboxIndex]); // 更新天空盒的立方体贴图

    previewModel = std::make_unique<PreviewModel>(vulkanDevice, this); // 创建预览模型对象
    previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout); // 准备预览模型的 PSO
    previewModel->UpdateModel(previewModels[modelIndex]); // 更新为当前选中的模型
}
void VulkanExample::LoadPreviewModel(const std::string& name, const std::string& cubemapPath)
{
    uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY; // 设置 glTF 加载标志：预变换顶点，翻转 Y 轴

    auto model = std::make_shared<vkglTF::Model>(); // 创建 glTF 模型对象
    model->loadFromFile(getAssetPath() + cubemapPath, vulkanDevice, queue, glTFLoadingFlags); // 从文件加载模型

    previewModels.emplace_back(model); // 添加到预览模型列表
    previewModelNames.emplace_back(name); // 添加模型名称
}
void VulkanExample::LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format)
{
    auto cubemap = std::shared_ptr<vks::TextureCubeMap>(new vks::TextureCubeMap(), [](vks::TextureCubeMap* cubemap) {
        if (cubemap)
        {
            cubemap->destroy(); // 销毁立方体贴图
            delete cubemap; // 删除对象
        }
        }); // 创建智能指针管理立方体贴图

    cubemap->loadFromFile(getAssetPath() + cubemapPath, VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue); // 从文件加载立方体贴图
    cubeMaps.emplace_back(cubemap); // 添加到立方体贴图列表
    cubemapNames.emplace_back(name); // 添加贴图名称
}

void VulkanExample::PreparePasses()
{
    mainPass = std::make_unique<MainPass>(vulkanDevice); // 创建主渲染通道
    mainPass->SetUp(renderPass); // 设置主渲染通道的渲染通道句柄

    brdfPass = std::make_unique<GenBRDFLutPass>(vulkanDevice, this); // 创建 BRDF 查找表生成通道
    brdfPass->Prepare(); // 准备 BRDF 通道（创建渲染通道、管线、帧缓冲区）
    

    // pipeline only draw once
    {
        VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true); // 创建主命令缓冲区
        brdfPass->Draw(cmdBuf); // 执行 BRDF 查找表绘制
        vulkanDevice->flushCommandBuffer(cmdBuf, queue); // 提交并刷新命令缓冲区

        sceneTextures.brdfView = brdfPass->view; // 存储 BRDF 查找表的图像视图


    }
}

void VulkanExample::ReginPrefilterPasses()
{

}

void VulkanExample::PrepareProbes()
{

}

void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective; // 设置投影矩阵
    mainPassData.view = camera.matrices.view; // 设置视图矩阵
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f); // 设置相机位置（齐次坐标）

    mainPass->UpdateGlobal(mainPassData); // 更新主渲染通道的全局 UBO

    // update skybox
    skybox->Update(camera.matrices.view); // 更新天空盒的视图矩阵
}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {

        skybox->Draw(cmd, mainPass->descriptorSet); // 绘制天空盒

        previewModel->Draw(cmd, mainPass->descriptorSet); // 绘制预览模型

        drawUI(cmd); // 绘制 UI
        });
}

void VulkanExample::OnUpdateUIOverlay(vks::UIOverlay* overlay)
{
    if (overlay->header("Settings")) {
        if (overlay->inputFloat("Exposure", &mainPassData.exposure, 0.1f, 2)) {
            globalDirty = true;
        }
        if (overlay->inputFloat("Gamma", &mainPassData.gamma, 0.1f, 2)) {
            globalDirty = true;
        }
        if (overlay->comboBox("Skybox", &skyboxIndex, cubemapNames)) {
            skybox->UpdateCubemap(cubeMaps[skyboxIndex]);
        }
        if (overlay->comboBox("PreviewModel", &modelIndex, previewModelNames)) {
            previewModel->UpdateModel(previewModels[modelIndex]);
        }
    }

    previewModel->ShowUI(overlay);
}

VULKAN_EXAMPLE_MAIN()