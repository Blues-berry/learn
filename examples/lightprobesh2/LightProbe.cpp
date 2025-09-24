#include "LightProbe.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>



LightProbe::LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width_, uint32_t height_)
    : device(device_), iLoader(example), width(width_), height(height_)
{    
    capturePass = std::make_unique<CaptureScenePass>(device_, example, VK_FORMAT_R16G16B16A16_SFLOAT, width, height);
}
LightProbe::~LightProbe()
{

}
void LightProbe::SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_)
{
    cubemap = cubemap_;
}

void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
    });
}

void LightProbe::setSkybox(Skybox* skybox_)
{
    skybox = skybox_;
    skybox->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE); // 准备天空盒的管线状态对象（PSO）。
}

void LightProbe::CaptureCubeMap(VkQueue queue, VkCommandBuffer cmd)
{
    // 不要 SetExternalCubeMap
// 正确初始化UBO
    CaptureScenePass::GlobalUbo ubo = {};
    // 设置投影矩阵
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 256.0f);
    // 设置视图矩阵
    std::array<glm::mat4, 6> viewMatrices = {
        glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // +X面
        glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // -X面
        glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),  // +Y面
        glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),  // -Y面
        glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // +Z面
        glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))   // -Z面
    };
    for (uint32_t face = 0; face < 6; ++face) {
        ubo.viewproj[face] = projection * glm::mat4(glm::mat3(viewMatrices[face]));
        ubo.cameraPos[face] = glm::vec4(position.x, position.y, position.z, 1.f);
    }
    capturePass->UpdateGlobal(ubo);

    if (cmd == VK_NULL_HANDLE) {
        VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true); // 创建主命令缓冲区。
        drawScene(cmdBuf);
        device->flushCommandBuffer(cmdBuf, queue); // 提交并刷新命令缓冲区。
    }
    else {
        drawScene(cmd);
    }
    
}

void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue)
{
    GenSHComputePass shPass(device, iLoader);
    shPass.SetCubeMap(cubemap);
    shPass.Generate(queue);
    shPass.FeedSH(shCoeffs);
}