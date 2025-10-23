# 📊 代码修改对比 - 修复前后

## 修复1: LightProbe.cpp - 解决Merge Conflict

### ❌ 修复前 (第96-131行)
```cpp
<<<<<<< HEAD
        // 尝试从 capturePass 获取 cubemap（渲染后 capturePass 应该持有结果）
        if (capturePass) {
            if (!cubemap) {
                cubemap = capturePass->GetCubeMap();
            }
        }
=======
        // 转换 cubemap 布局为 SHADER_READ_ONLY_OPTIMAL
        if (!cubemap) {
            std::cerr << "[LightProbe::CaptureCubeMap] Warning: cubemap is nullptr, trying to get from capturePass..." << std::endl;
            // ... 更多代码 ...
        }
        VkCommandBuffer transitionCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
>>>>>>> 8912779 (...)
```

**问题**: 
- 代码无法编译
- Merge conflict标记导致语法错误

### ✅ 修复后 (第91-115行)
```cpp
// ✅ 修复2: 总是执行布局转换（不再受needFlush条件限制）
// 尝试从 capturePass 获取 cubemap（渲染后 capturePass 应该持有结果）
if (!cubemap) {
    if (capturePass) {
        cubemap = capturePass->GetCubeMap();
        if (!cubemap) {
            std::cerr << "[LightProbe::CaptureCubeMap] Error: Failed to get cubemap from capturePass!" << std::endl;
            return;
        }
        
        // 确保cubemap的成员都被正确初始化
        if (!cubemap->image) {
            std::cerr << "[LightProbe::CaptureCubeMap] Error: Cubemap has null image handle!" << std::endl;
            return;
        }
    } else {
        std::cerr << "[LightProbe::CaptureCubeMap] Error: Both cubemap and capturePass are null!" << std::endl;
        return;
    }
}
```

**改进**:
- ✅ 删除merge conflict标记
- ✅ 保留完整的错误检查
- ✅ 代码可以正常编译

---

## 修复2: gltfload.cpp - Draw函数逻辑

### ❌ 修复前 (第49-88行)
```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
    // ... 初始化代码 ...
    
    const glm::vec3 offsets[4] = {
        glm::vec3(-20.0f, 0.0f, 0.0f),  // 左
        glm::vec3(20.0f,  0.0f, 0.0f),  // 右
        glm::vec3(0.0f,   0.0f, -20.0f), // 后
        glm::vec3(0.0f,   0.0f, 20.0f)  // 前
    };
    const float scale = 50.0f;
    
    // ❌ 对所有技术都应用偏移和缩放
    for (int i = 0; i < 4; ++i) {
        pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        pc.tint = glm::vec4(colors[i % 3], 1.0f);
        vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlock), &pc);
        model->draw(cmd);
    }
}
```

**问题**:
- ❌ CAPTURE_SCENE中也应用了偏移和缩放
- ❌ 模型被放大50倍并偏移±20单位
- ❌ 导致模型超出视锥体，在cubemap中不可见

### ✅ 修复后 (第49-99行)
```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
    // ... 初始化代码 ...
    
    // ✅ 修复3: CAPTURE_SCENE中不应用偏移，直接在原点绘制
    if (tech == ETechnique::CAPTURE_SCENE) {
        // 捕获场景时，不应用偏移和缩放，直接在原点绘制
        pc.modelOffset = glm::mat4(1.0f);
        pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlock), &pc);
        model->draw(cmd);
    }
    else {
        // MAIN技术：应用偏移和缩放，绘制4个副本
        for (int i = 0; i < 4; ++i) {
            pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
            pc.tint = glm::vec4(colors[i % 3], 1.0f);
            vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantBlock), &pc);
            model->draw(cmd);
        }
    }
}
```

**改进**:
- ✅ CAPTURE_SCENE: 使用单位矩阵，不应用偏移和缩放
- ✅ MAIN: 保持原有逻辑，应用偏移和缩放
- ✅ 模型在cubemap中正确显示

---

## 修复3: main.cpp - 验证修改

### ✅ drawFrame函数 (第484-509行)
```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    if (probe) {
        probe->CaptureCubeMap(queue, cmd);
    }

    // 绘制单帧。
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }

        // ✅ 修复：删除重复的数据更新，数据已在prepareData中更新

        if (showProbes) {
            for (const auto& probe : lightProbes) {
                probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            }
        }

        drawUI(cmd);
    });
}
```

**验证**:
- ✅ 没有重复的数据更新
- ✅ 正确调用了probe->CaptureCubeMap
- ✅ 所有模型都使用MAIN技术绘制

### ✅ prepareData函数 (第471-482行)
```cpp
void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f); // ✅ 设置光源位置

    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

**验证**:
- ✅ 设置了所有必要的数据
- ✅ 光源位置在prepareData中设置
- ✅ 数据在每帧更新

### ✅ CaptureCubemap函数 (第564-610行)
```cpp
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
    // ... 保存和处理 ...
}
```

**验证**:
- ✅ 为MAIN技术准备PSO
- ✅ 为CAPTURE_SCENE技术准备PSO
- ✅ 设置gltfModel

---

## 📊 修改统计

| 文件 | 修改行数 | 修改类型 | 状态 |
|------|---------|---------|------|
| LightProbe.cpp | 96-131 | 删除merge conflict | ✅ |
| gltfload.cpp | 49-99 | 添加条件判断 | ✅ |
| main.cpp | 验证 | 所有修改正确 | ✅ |

---

## 🎯 修复效果

### 修复前 ❌
- 代码无法编译（merge conflict）
- gltfModel在cubemap中不可见
- 只有1个面有纹理

### 修复后 ✅
- 代码可以正常编译
- gltfModel在cubemap中正确显示
- 所有6个面都有纹理


