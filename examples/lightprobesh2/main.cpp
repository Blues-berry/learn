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

        // Enable extension required for multiview
        enabledDeviceExtensions.push_back(VK_KHR_MULTIVIEW_EXTENSION_NAME);

        // Reading device properties and features for multiview requires VK_KHR_get_physical_device_properties2 to be enabled
        enabledInstanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        // Enable required extension features
        physicalDeviceMultiviewFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES_KHR;
        physicalDeviceMultiviewFeatures.multiview = VK_TRUE;
        deviceCreatepNextChain = &physicalDeviceMultiviewFeatures;
    }

    ~VulkanExample() override
    {
        vkDeviceWaitIdle(device);

        if (skybox)
        {
            skybox->Destroy();
            skybox = nullptr;
        }

        if (previewModel)
        {
            previewModel->Destroy();
            previewModel = nullptr;
        }
        
        cubeMaps.clear();

        mainPass = nullptr;
        shGenPass = nullptr;
        brdfPass = nullptr;
        genIBL = nullptr;
    }

	void LoadAssets();
	void LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format);
    void LoadPreviewModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags);
    void PrepareScene();
    void UpdateSkyBox();

    void PrepareProbes();
    void PreparePasses();

    void ReginPrefilterPasses();

    void prepare() override
    {
        VulkanExampleBase::prepare();
        LoadAssets();
        PreparePasses();
        PrepareScene();
        prepared = true;
    }

    void render() override
    {
        if (!prepared)
            return;
        draw();
    }

    void drawFrame(VkCommandBuffer cmd);

    void prepareData();

    void draw()
    {
        prepareData();

        VulkanExampleBase::prepareFrame();

        VkCommandBuffer cmdBuffer = drawCmdBuffers[currentBuffer];

        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

        drawFrame(cmdBuffer);

        VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        VulkanExampleBase::submitFrame();
    }

    void OnUpdateUIOverlay(vks::UIOverlay* overlay) override;

	VkPipelineShaderStageCreateInfo LoadShader(const std::string& path, VkShaderStageFlagBits stage) override
	{
		return loadShader(getShadersPath() + path, stage);
	}

private:
    // VulkanglTFScene glTFScene;
    VkPhysicalDeviceMultiviewFeaturesKHR physicalDeviceMultiviewFeatures{};

    std::vector<std::unique_ptr<LightProbe>> lightProbes;

    // skybox
    std::vector<std::shared_ptr<vks::TextureCubeMap>> cubeMaps;
    std::vector<std::string> cubemapNames;
    int32_t skyboxIndex = 0;

    // preview model
    std::shared_ptr<vkglTF::Model> skyboxModel;
    std::vector<std::shared_ptr<vkglTF::Model>> previewModels;
    std::vector<std::string> previewModelNames;
    int32_t modelIndex = 0;

    // pipeline
    bool globalDirty = true;
    MainPass::GlobalUbo mainPassData = {};
    std::unique_ptr<MainPass> mainPass;
    std::unique_ptr<GenBRDFLutPass> brdfPass;
    std::unique_ptr<GenSHComputePass> shGenPass;
    std::unique_ptr<GenIBLPass> genIBL;

    // scene
    std::unique_ptr<Skybox> skybox;
    std::unique_ptr<PreviewModel> previewModel;
};

void VulkanExample::LoadAssets()
{
    LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
    LoadCubeMap("gcanyon", "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
    LoadCubeMap("uffizi", "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);

    uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;

    LoadPreviewModel("sphere", "models/sphere.gltf", glTFLoadingFlags);
    LoadPreviewModel("teapot", "models/teapot.gltf", glTFLoadingFlags);
    LoadPreviewModel("torusknot", "models/torusknot.gltf", glTFLoadingFlags);
    LoadPreviewModel("venus", "models/venus.gltf", glTFLoadingFlags);

    skyboxModel = std::make_shared<vkglTF::Model>();
    skyboxModel->loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
}

void VulkanExample::PrepareScene()
{
    skybox = std::make_unique<Skybox>(vulkanDevice, this);
    skybox->SetModel(skyboxModel);
    skybox->PreparePSO(renderPass, mainPass->descriptorSetLayout);
    skybox->UpdateCubemap(cubeMaps[skyboxIndex]);

    previewModel = std::make_unique<PreviewModel>(vulkanDevice, this);
    previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout);
    previewModel->UpdateModel(previewModels[modelIndex]);
}

void VulkanExample::UpdateSkyBox()
{
    skybox->UpdateCubemap(cubeMaps[skyboxIndex]);
    shGenPass->SetCubeMap(cubeMaps[skyboxIndex]);
    genIBL->SetCubeMap(cubeMaps[skyboxIndex]);

    // gen sh and env map
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    shGenPass->Draw(cmdBuf);
    genIBL->Draw(cmdBuf);
    vulkanDevice->flushCommandBuffer(cmdBuf, queue);
}

void VulkanExample::LoadPreviewModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags)
{
    auto model = std::make_shared<vkglTF::Model>();
    model->loadFromFile(getAssetPath() + cubemapPath, vulkanDevice, queue, glTFLoadingFlags);

    previewModels.emplace_back(model);
    previewModelNames.emplace_back(name);
}

void VulkanExample::LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format)
{
    auto cubemap = std::shared_ptr<vks::TextureCubeMap>(new vks::TextureCubeMap(), [](vks::TextureCubeMap* cubemap) {
        if (cubemap)
        {
            cubemap->destroy();
            delete cubemap;
        }
        });

    cubemap->loadFromFile(getAssetPath() + cubemapPath, VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
    cubeMaps.emplace_back(cubemap);
    cubemapNames.emplace_back(name);
}

void VulkanExample::PreparePasses()
{
    mainPass = std::make_unique<MainPass>(vulkanDevice);
    mainPass->SetUp(renderPass);

    brdfPass = std::make_unique<GenBRDFLutPass>(vulkanDevice, this);
    brdfPass->Prepare();
    brdfPass->FeedDescriptor(mainPass->environmemts.brdfView);

    shGenPass = std::make_unique<GenSHComputePass>(vulkanDevice, this);
    shGenPass->SetCubeMap(cubeMaps[skyboxIndex]);
    shGenPass->FeedSH(mainPass->environmemts.shCoeffs);

    genIBL = std::make_unique<GenIBLPass>(vulkanDevice, this, 256);
    genIBL->SetCubeMap(cubeMaps[skyboxIndex]);
    genIBL->SetModel(skyboxModel);
    genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
    genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

    // pipeline only draw once
    {
        VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        brdfPass->Draw(cmdBuf);
        shGenPass->Draw(cmdBuf);
        genIBL->Draw(cmdBuf);
        vulkanDevice->flushCommandBuffer(cmdBuf, queue);
    }

    mainPass->UpdateBinngs();
}

void VulkanExample::ReginPrefilterPasses()
{

}

void VulkanExample::PrepareProbes()
{

}

void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);

    mainPass->UpdateGlobal(mainPassData);

    // update skybox
    skybox->Update(camera.matrices.view);
}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {

        skybox->Draw(cmd, mainPass->descriptorSet);

        previewModel->Draw(cmd, mainPass->descriptorSet);
        
        drawUI(cmd);
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
            UpdateSkyBox();
        }
        if (overlay->comboBox("PreviewModel", &modelIndex, previewModelNames)) {
            previewModel->UpdateModel(previewModels[modelIndex]);
        }
    }

    previewModel->ShowUI(overlay);
}

VULKAN_EXAMPLE_MAIN()