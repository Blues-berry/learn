# Cubemap捕获系统 - Bug分析和修复方案

## 🐛 发现的问题

### 问题1: 鼠标移动时gltfModel跟随移动
**症状**: 移动鼠标时，gltfModel也在移动

**根本原因**:
在 `drawFrame()` 中，每帧都在更新 `mainPassData`，包括相机位置和视图矩阵。这导致gltfModel的渲染受到相机移动的影响。

**问题代码** (main.cpp:499-511):
```cpp
if (gltfModel) {
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
    mainPassData.cameraPos = glm::vec4(camera.position, 10.0f);  // ← 每帧更新
    mainPassData.view = camera.matrices.view;                     // ← 每帧更新
    mainPassData.project = camera.matrices.perspective;           // ← 每帧更新
    mainPass->UpdateGlobal(mainPassData);
}
```

**为什么会跟随移动**:
- gltfModel使用的是 `mainPass->descriptorSet`
- 每帧更新的view矩阵会影响所有使用该descriptorSet的对象
- gltfModel的位置是通过Push Constant设置的，但view矩阵是全局的

---

### 问题2: Capture后previewModel和gltfModel变黑
**症状**: 点击"Capture Cubemap"后，两个模型都变成黑色

**根本原因**: 多个因素导致：

#### 原因2a: 描述符绑定冲突
在 `CaptureCubemap()` 中：
```cpp
gltfModel->PreparePSO(
    capturePass->renderPass,
    capturePass->descriptorSetLayout,
    ETechnique::CAPTURE_SCENE
);
```

这会创建一个新的PSO，但可能没有正确绑定所有必要的描述符。

#### 原因2b: 材质数据未初始化
```cpp
void GltfModel::SetUseSHAndReflection(bool useSH, bool useReflection) {
    materialData.useSH = useSH ? 1 : 0;
    materialData.useReflection = useReflection ? 1 : 0;
    memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
}
```

启用SH和Reflection后，着色器可能在查找这些资源时失败。

#### 原因2c: 着色器资源绑定问题
着色器可能期望特定的描述符集布局，但在capture后没有正确更新。

---

## ✅ 修复方案

### 修复1: 分离相机数据更新

**问题**: 每帧都在drawFrame中更新mainPassData

**解决方案**: 只在prepareData中更新一次

**修改位置**: main.cpp

```cpp
// 修改前 (drawFrame中)
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    if (probe) {
        probe->CaptureCubeMap(queue, cmd);
    }

    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);

        // ❌ 问题：在这里更新会导致每帧都改变
        if (gltfModel) {
            mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
            mainPassData.cameraPos = glm::vec4(camera.position, 10.0f);
            mainPassData.view = camera.matrices.view;
            mainPassData.project = camera.matrices.perspective;
            mainPass->UpdateGlobal(mainPassData);
        }

        drawUI(cmd);
    });
}

// 修改后 (分离到prepareData中)
void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);

    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}

void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    if (probe) {
        probe->CaptureCubeMap(queue, cmd);
    }

    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);

        // ✅ 移除重复的数据更新

        if (showProbes) {
            for (const auto& probe : lightProbes) {
                probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            }
        }

        drawUI(cmd);
    });
}
```

---

### 修复2: 修复Capture后的黑色问题

**问题**: SetUseSHAndReflection启用后，着色器资源绑定失败

**解决方案**: 确保所有必要的描述符都已正确绑定

**修改位置**: CaptureCubemap() 函数

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
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);

        // 为CAPTURE_SCENE技术准备PSO（用于捕获）
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    }

    probe->SetGltfModel(gltfModel.get());
    probe->CaptureCubeMap(queue);

    std::string basePath = "Captured_" + std::to_string(cubeMaps.size()) + "_";
    probe->SaveCubeMapFaces(queue, basePath);

    auto capturedCubemap = probe->GetCubemap();
    cubeMaps.push_back(capturedCubemap);
    cubemapNames.push_back("Captured_" + std::to_string(cubeMaps.size() - 1));
    skyboxIndex = static_cast<int>(cubeMaps.size() - 1);

    vkDeviceWaitIdle(vulkanDevice->logicalDevice);

    // 生成SH和IBL
    shGenPass->SetCubeMap(capturedCubemap);
    shGenPass->Generate(queue);

    VkDescriptorBufferInfo shBufferInfo;
    shGenPass->FeedSH(shBufferInfo);
    mainPass->environmemts.shCoeffs = shBufferInfo;

    genIBL->SetCubeMap(capturedCubemap);
    genIBL->Generate(queue);
    genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
    genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

    // ✅ 关键：更新所有绑定
    mainPass->UpdateBindings();
    skybox->SetCubeMap(capturedCubemap);

    // ✅ 确保previewModel也更新绑定
    if (previewModel) {
        previewModel->SetUseSHAndReflection(true, true);
    }

    // ✅ 确保gltfModel也更新绑定
    if (gltfModel) {
        gltfModel->SetUseSHAndReflection(true, true);
    }

    lightProbes.push_back(std::move(probe));
}
```

---

## 🔧 完整修复步骤

### 步骤1: 修改drawFrame()
删除drawFrame中的数据更新代码

### 步骤2: 修改prepareData()
确保所有数据在prepareData中更新

### 步骤3: 修改CaptureCubemap()
为gltfModel准备MAIN技术的PSO

### 步骤4: 验证着色器绑定
检查gltfmesh.frag.spv中的资源绑定

### 步骤5: 测试
1. 移动鼠标，确保gltfModel不跟随
2. 点击Capture，确保模型不变黑
3. 检查SH和IBL效果是否正确应用

---

## 📋 检查清单

- [ ] 从drawFrame中移除数据更新
- [ ] 在prepareData中添加数据更新
- [ ] 为gltfModel准备MAIN技术PSO
- [ ] 验证着色器资源绑定
- [ ] 测试鼠标移动
- [ ] 测试Capture功能
- [ ] 验证SH和IBL效果

---

## 🔧 实际代码修复

### 当前代码分析

**prepareData()** (第471-483行):
```cpp
void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);

    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

**drawFrame()** (第485-523行):
```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    if (probe) {
        probe->CaptureCubeMap(queue, cmd);
    }

    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        for (auto& m : gltfClones) { m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); }

        // ❌ 问题代码：在这里重复更新数据
        if (gltfModel) {
            mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
            mainPassData.cameraPos = glm::vec4(camera.position, 10.0f);  // ← 每帧改变
            mainPassData.view = camera.matrices.view;                     // ← 每帧改变
            mainPassData.project = camera.matrices.perspective;           // ← 每帧改变
            mainPass->UpdateGlobal(mainPassData);
        }

        if (showProbes) {
            for (const auto& probe : lightProbes) {
                probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            }
        }

        drawUI(cmd);
    });
}
```

### 问题诊断

1. **问题1根源**: 在drawFrame的lambda中重复更新mainPassData
   - 每帧都调用`mainPass->UpdateGlobal(mainPassData)`
   - 这会导致UBO被频繁更新
   - 虽然prepareData已经更新过一次，但这里又更新了一次

2. **问题2根源**: 在CaptureCubemap中没有为MAIN技术准备PSO
   - 只为CAPTURE_SCENE准备了PSO
   - 导致gltfModel在主渲染中没有正确的管线

### 修复方案

#### 修复1: 删除drawFrame中的重复更新

**修改位置**: main.cpp 第499-511行

**修改前**:
```cpp
if (gltfModel) {
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
    mainPassData.cameraPos = glm::vec4(camera.position, 10.0f);
    mainPassData.view = camera.matrices.view;
    mainPassData.project = camera.matrices.perspective;
    mainPass->UpdateGlobal(mainPassData);
}
```

**修改后**:
```cpp
// ✅ 删除这个if块，数据已在prepareData中更新
```

#### 修复2: 在prepareData中添加光源数据

**修改位置**: main.cpp 第471-483行

**修改前**:
```cpp
void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);

    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

**修改后**:
```cpp
void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);  // ✅ 添加光源

    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

#### 修复3: 在CaptureCubemap中为MAIN技术准备PSO

**修改位置**: main.cpp 第578-596行

**修改前**:
```cpp
if (!gltfModel) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());

    // CAPTURE_SCENE: 使用 capturePass 的 renderPass
    gltfModel->PreparePSO(
        capturePass->renderPass,
        capturePass->descriptorSetLayout,
        ETechnique::CAPTURE_SCENE
    );
}
```

**修改后**:
```cpp
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
```

---

## 🎯 修复总结

| 问题 | 原因 | 修复 |
|------|------|------|
| 鼠标移动时gltfModel跟随 | drawFrame中重复更新view矩阵 | 删除drawFrame中的数据更新 |
| Capture后模型变黑 | 没有为MAIN技术准备PSO | 在CaptureCubemap中添加MAIN PSO准备 |
| 光源位置不正确 | 光源数据在drawFrame中更新 | 在prepareData中添加光源数据 |



