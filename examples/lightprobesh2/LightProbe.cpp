#include "LightProbe.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>
#include <fstream>



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
    if (skybox) {
        // Only prepare PSO if a valid skybox was provided
        skybox->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    } else {
        // Defensive: log when setting a null skybox so we can trace call order issues
        std::cerr << "LightProbe::setSkybox received nullptr - skipping PreparePSO\n";
    }
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
    
    // 设置光照参数
    ubo.mainLight = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // 默认白光
    ubo.exposure = 4.5f;  // 默认曝光值
    ubo.gamma = 2.2f;     // 默认伽马值

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
        // 等待渲染完成（capturePass 的提交已经在 flushCommandBuffer 中完成，但仍然确保队列空闲）
        vkQueueWaitIdle(queue);

        // 尝试从 capturePass 获取 cubemap（渲染后 capturePass 应该持有结果）
        if (capturePass) {
            if (!cubemap) {
                cubemap = capturePass->GetCubeMap();
            }
        }

        if (!cubemap || !cubemap->image) {
            std::cerr << "[LightProbe::CaptureCubeMap] Error: Failed to get valid cubemap from capturePass!" << std::endl;
            return;
        }

        // 如果 cubemap 已经处于着色器可读布局，则无需转换
        if (cubemap->imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // nothing to do
        } else {
            // 执行布局转换到 SHADER_READ_ONLY_OPTIMAL
            VkCommandBuffer transitionCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

            VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
            barrier.oldLayout = cubemap->imageLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

            // 更新跟踪的布局状态
            cubemap->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }
}
void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue)
{
    GenSHComputePass shPass(device, iLoader);
    shPass.SetCubeMap(cubemap);
    shPass.Generate(queue);
    shPass.FeedSH(shCoeffs);
}

void LightProbe::SaveCubeMapFaces(VkQueue queue, const std::string& basePath)
{
    if (!cubemap) {
        std::cerr << "Error: No cubemap available to save!" << std::endl;
        return;
    }

    // 创建一个临时命令缓冲区
    VkCommandBuffer cmdBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    
    // 定义立方体贴图六个面的名称
    const std::vector<std::string> faceNames = {
        "_pos_x.png",  // +X
        "_neg_x.png",  // -X
        "_pos_y.png",  // +Y
        "_neg_y.png",  // -Y
        "_pos_z.png",  // +Z
        "_neg_z.png"   // -Z
    };

    // 为每个面创建一个线性图像用于读取
    for (uint32_t face = 0; face < 6; ++face) {
        // 创建线性图像用于读取
        VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  // 保存为8位UNORM格式
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        VkImage dstImage;
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &dstImage));
        
        // 分配内存
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device->logicalDevice, dstImage, &memRequirements);
        
        VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
        memAllocInfo.allocationSize = memRequirements.size;
        // 内存必须主机可见以读取数据
        memAllocInfo.memoryTypeIndex = device->getMemoryType(
            memRequirements.memoryTypeBits, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
        VkDeviceMemory dstImageMemory;
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &dstImageMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, dstImage, dstImageMemory, 0));
        
        // 从立方体贴图的一个面复制到线性图像
        VkImageCopy imageCopyRegion = {};
        imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.srcSubresource.mipLevel = 0;
        imageCopyRegion.srcSubresource.baseArrayLayer = face;  // 指定立方体贴图的特定面
        imageCopyRegion.srcSubresource.layerCount = 1;
        imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopyRegion.dstSubresource.mipLevel = 0;
        imageCopyRegion.dstSubresource.baseArrayLayer = 0;
        imageCopyRegion.dstSubresource.layerCount = 1;
        imageCopyRegion.extent.width = width;
        imageCopyRegion.extent.height = height;
        imageCopyRegion.extent.depth = 1;
        
        // 转换目标图像布局为传输目标
        vks::tools::insertImageMemoryBarrier(
            cmdBuffer,
            dstImage,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
            
        // 转换源图像布局为传输源
        vks::tools::insertImageMemoryBarrier(
            cmdBuffer,
            cubemap->image,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, face, 1 });
            
        // 执行复制
        vkCmdCopyImage(
            cmdBuffer,
            cubemap->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageCopyRegion);
            
        // 转换目标图像布局为通用布局，以便映射内存
        vks::tools::insertImageMemoryBarrier(
            cmdBuffer,
            dstImage,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
            
        // 转换源图像布局回着色器只读
        vks::tools::insertImageMemoryBarrier(
            cmdBuffer,
            cubemap->image,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, face, 1 });
        
        // 提交命令缓冲区并等待完成
        device->flushCommandBuffer(cmdBuffer, queue);
        
        // 映射图像内存
        void* data;
        vkMapMemory(device->logicalDevice, dstImageMemory, 0, VK_WHOLE_SIZE, 0, &data);
        
        // 获取图像子资源布局
        VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
        VkSubresourceLayout subResourceLayout;
        vkGetImageSubresourceLayout(device->logicalDevice, dstImage, &subResource, &subResourceLayout);
        
        // 保存为PPM格式（类似screenshot示例中的方法）
        const char* imageData = (const char*)data;
        std::string filename = basePath + faceNames[face];
        // 将.png扩展名替换为.ppm
        filename = filename.substr(0, filename.find_last_of('.')) + ".ppm";
        
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        
        // PPM头部
        file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";
        
        // 简化处理：逐行写入RGB数据（忽略Alpha）
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t* row = (const uint8_t*)(imageData + y * subResourceLayout.rowPitch);
            for (uint32_t x = 0; x < width; ++x) {
                // 直接写入RGB（忽略Alpha）
                file.write((const char*)row, 3);
                row += 4; // 跳过Alpha通道
            }
        }
        
        file.close();
        
        // 取消映射内存
        vkUnmapMemory(device->logicalDevice, dstImageMemory);
        
        // 清理资源
        vkFreeMemory(device->logicalDevice, dstImageMemory, nullptr);
        vkDestroyImage(device->logicalDevice, dstImage, nullptr);
        
        // 为下一个面创建新的命令缓冲区
        cmdBuffer = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    }
    
    std::cout << "Successfully saved 6 cubemap faces with base path: " << basePath << std::endl;
}