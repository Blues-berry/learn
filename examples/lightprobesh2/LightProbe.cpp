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
// void LightProbe::SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_)
// {
//     cubemap = cubemap_;
// }

void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
        if (gltfModel) {
            gltfModel->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
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
    // --- 1. 准备 UBO（修复 viewproj）---
    CaptureScenePass::GlobalUbo ubo = {};
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 256.0f);

    std::array<glm::mat4, 6> viewMatrices = {
        glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0, -1,  0)), // +X
        glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1,  0)), // -X
        glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0,  0,  1)), // +Y
        glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0,  0, -1)), // -Y
        glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0, -1,  0)), // +Z
        glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0, -1,  0))  // -Z
    };

    for (uint32_t face = 0; face < 6; ++face) {
        ubo.viewproj[face] = projection * viewMatrices[face];  // 保留平移！
        ubo.cameraPos[face] = glm::vec4(position, 1.0f);
    }
    capturePass->UpdateGlobal(ubo);

    // --- 2. 执行渲染 ---
    VkCommandBuffer cmdBuf = cmd;
    bool needFlush = (cmd == VK_NULL_HANDLE);

    if (needFlush) {
        cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    }

    drawScene(cmdBuf);

    if (needFlush) {
        device->flushCommandBuffer(cmdBuf, queue);
    }

    // --- 3. 同步 + 布局转换（关键！）---
    if (needFlush) {
        // 等待渲染完成
        vkQueueWaitIdle(queue);

        // 转换 cubemap 布局为 SHADER_READ_ONLY_OPTIMAL
        VkCommandBuffer transitionCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = cubemap->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 6;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            transitionCmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        device->flushCommandBuffer(transitionCmd, queue);
    }
}
void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue)
{
    GenSHComputePass shPass(device, iLoader);
    shPass.SetCubeMap(cubemap);
    shPass.Generate(queue);
    shPass.FeedSH(shCoeffs);
}