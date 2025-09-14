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
#include <fstream>


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
        camera.setRotation({ -3.75f, 180.0f, 0.0f });//设置相机初始旋转角度（欧拉角：偏航-3.75度，
        camera.setPosition({ 0.55f, 0.85f, 12.0f });//设置相机初始位置为(0.55, 0.85, 12.0)。
    }

    ~VulkanExample() override
    {
    }

	void LoadAssets();
	void LoadCubeMap(const std::string& name, const std::string& cubemapPath, VkFormat format);
    void LoadScene();

    void PrepareProbes();
    void PreparePasses();
    void PreparePipelines();

    void prepare() override
    {
        VulkanExampleBase::prepare();
        PreparePasses();
        PreparePipelines();
        LoadAssets();
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

    std::vector<std::unique_ptr<LightProbe>> lightProbes;
	std::unordered_map<std::string, std::shared_ptr<vks::TextureCubeMap>> cubemaps;

    // pipeline
    bool globalDirty = true;
    MainPass::GlobalUbo mainPassData = {};
    std::unique_ptr<MainPass> mainPass;

    // scene
    std::unique_ptr<Skybox> skybox;
};

void VulkanExample::LoadAssets()
{
	LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
	LoadCubeMap("gcanyon", "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
	LoadCubeMap("uffizi", "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);

    LoadScene();
}

void VulkanExample::LoadScene()
{
    skybox = std::make_unique<Skybox>(vulkanDevice, this);

    skybox->LoadFromPath("models/cube.gltf", queue);
    skybox->PreparePSO(renderPass, mainPass->descriptorSetLayout);
    skybox->UpdateCubemap(cubemaps["pisa"]);
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
	cubemaps.emplace(name, cubemap);
}

void VulkanExample::PreparePasses()
{
    mainPass = std::make_unique<MainPass>(vulkanDevice);
    mainPass->SetUp(renderPass);
}

void VulkanExample::PreparePipelines()
{
}

void VulkanExample::PrepareProbes()
{

}

void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;

    mainPass->UpdateGlobal(mainPassData);

}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {

        skybox->Draw(cmd, mainPass->descriptorSet);

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
    }
}

VULKAN_EXAMPLE_MAIN()