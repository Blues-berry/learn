# Cubemap捕获 - 关键代码片段

## 1. 入口点：CaptureCubemap()

**文件**: `main.cpp:578-636`

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    // 创建探针
    probe = std::make_unique<LightProbe>(vulkanDevice, this, 1024, 1024);
    probe->SetPosition(position);
    probe->setSkybox(skybox.get());
    probe->setPreviewModel(previewModel.get());

    // 设置glTF模型
    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    }

    probe->SetGltfModel(gltfModel.get());

    // 执行捕获
    probe->CaptureCubeMap(queue);

    // 保存文件
    std::string basePath = "Captured_" + std::to_string(cubeMaps.size()) + "_";
    probe->SaveCubeMapFaces(queue, basePath);

    // 获取cubemap
    auto capturedCubemap = probe->GetCubemap();
    cubeMaps.push_back(capturedCubemap);
    cubemapNames.push_back("Captured_" + std::to_string(cubeMaps.size() - 1));
    skyboxIndex = static_cast<int>(cubeMaps.size() - 1);

    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    // 生成SH系数
    shGenPass->SetCubeMap(capturedCubemap);
    shGenPass->Generate(queue);
    VkDescriptorBufferInfo shBufferInfo;
    shGenPass->FeedSH(shBufferInfo);
    mainPass->environmemts.shCoeffs = shBufferInfo;

    // 生成IBL贴图
    genIBL->SetCubeMap(capturedCubemap);
    genIBL->Generate(queue);
    genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
    genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

    // 更新绑定
    mainPass->UpdateBindings();
    skybox->SetCubeMap(capturedCubemap);

    // 启用SH和反射
    if (previewModel) {
        previewModel->SetUseSHAndReflection(true, true);
    }
    if (gltfModel) {
        gltfModel->SetUseSHAndReflection(true, true);
    }

    lightProbes.push_back(std::move(probe));
}
```

---

## 2. 核心捕获逻辑：CaptureCubeMap()

**文件**: `LightProbe.cpp:50-141`

```cpp
void LightProbe::CaptureCubeMap(VkQueue queue, VkCommandBuffer cmd)
{
    // 准备投影矩阵
    CaptureScenePass::GlobalUbo ubo = {};
    glm::mat4 projection = glm::perspective(
        glm::radians(90.0f),  // 90度视角
        1.0f,                 // 正方形
        0.1f,                 // 近裁剪面
        256.0f                // 远裁剪面
    );

    // 生成6个视图矩阵
    std::array<glm::mat4, 6> viewMatrices = {
        glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0, -1,  0)), // +X
        glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1,  0)), // -X
        glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0,  0,  1)), // +Y
        glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0,  0, -1)), // -Y
        glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0, -1,  0)), // +Z
        glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0, -1,  0))  // -Z
    };

    // 计算viewproj矩阵
    for (uint32_t face = 0; face < 6; ++face) {
        ubo.viewproj[face] = projection * viewMatrices[face];
        ubo.cameraPos[face] = glm::vec4(position, 1.0f);
    }

    // 设置光照参数
    ubo.mainLight = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ubo.exposure = 4.5f;
    ubo.gamma = 2.2f;

    // 更新UBO
    capturePass->UpdateGlobal(ubo);

    // 执行渲染
    VkCommandBuffer cmdBuf = cmd;
    bool needFlush = (cmd == VK_NULL_HANDLE);

    if (needFlush) {
        cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    }

    drawScene(cmdBuf);

    if (needFlush) {
        device->flushCommandBuffer(cmdBuf, queue);
    }

    // 等待完成
    if (needFlush) {
        vkQueueWaitIdle(queue);

        // 获取cubemap
        if (capturePass) {
            if (!cubemap) {
                cubemap = capturePass->GetCubeMap();
            }
        }

        if (!cubemap || !cubemap->image) {
            std::cerr << "[LightProbe::CaptureCubeMap] Error: Failed to get valid cubemap!" << std::endl;
            return;
        }

        // 布局转换
        if (cubemap->imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
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
            barrier.subresourceRange.layerCount = 6;  // 所有6个面
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(
                transitionCmd,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            device->flushCommandBuffer(transitionCmd, queue);
            cubemap->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }
}
```

---

## 3. 场景绘制：drawScene()

**文件**: `LightProbe.cpp:23-36`

```cpp
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
```

---

## 4. 渲染通道：CaptureScenePass::Draw()

**文件**: `UpsampleCubeMapPass.cpp:218-242`

```cpp
void CaptureScenePass::Draw(VkCommandBuffer cmd, std::function<void(VkCommandBuffer)>&& encoder)
{
    // 设置渲染区域
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer;

    // 开始渲染通道
    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 设置视口
    VkViewport viewport = vks::initializers::viewport(
        (float)width, (float)height, 0.0f, 1.0f);
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    // 设置裁剪矩形
    VkRect2D scissor = vks::initializers::rect2D(
        width, height, 0, 0);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 执行用户的绘制命令
    // Multiview会自动处理渲染到不同的立方体面
    encoder(cmd);

    // 结束渲染通道
    vkCmdEndRenderPass(cmd);
}
```

---

## 5. UBO更新：UpdateGlobal()

**文件**: `UpsampleCubeMapPass.cpp:212-216`

```cpp
void CaptureScenePass::UpdateGlobal(const GlobalUbo& ubo)
{
    // 复制UBO数据到缓冲区
    memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo));
}
```

---

## 6. 获取cubemap：GetCubeMap()

**文件**: `UpsampleCubeMapPass.cpp:7-10`

```cpp
std::shared_ptr<vks::TextureCubeMap> CaptureScenePass::GetCubeMap() const {
    if (!cube) return nullptr;
    return cube->GetTextureCubeMap();
}
```

---

## 关键数据结构

### GlobalUbo
```cpp
struct GlobalUbo {
    glm::mat4 viewproj[6];      // 6个面的视图投影矩阵
    glm::vec4 cameraPos[6];     // 6个面的相机位置
    glm::vec4 mainLight;        // 光源信息
    float exposure = 4.5f;      // 曝光值
    float gamma = 2.2f;         // 伽马值
};
```

### 6个视图矩阵方向
```
+X: (1,0,0)   Up: (0,-1,0)
-X: (-1,0,0)  Up: (0,-1,0)
+Y: (0,1,0)   Up: (0,0,1)   ← 特殊
-Y: (0,-1,0)  Up: (0,0,-1)  ← 特殊
+Z: (0,0,1)   Up: (0,-1,0)
-Z: (0,0,-1)  Up: (0,-1,0)
```



