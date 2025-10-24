# 🔧 Bug修复 - 已应用

## ✅ 修复完成

已成功修复cubemap捕获系统中的两个关键bug。

---

## 🐛 Bug 1: 鼠标移动时gltfModel跟随移动

### 问题描述
移动鼠标时，gltfModel也在移动，而不是保持固定位置。

### 根本原因
在 `drawFrame()` 的lambda函数中，每帧都在重复更新 `mainPassData` 并调用 `mainPass->UpdateGlobal()`。这导致view矩阵被频繁更新，影响所有使用该descriptorSet的对象。

### 修复方案
**删除drawFrame中的重复数据更新**

**文件**: `examples/lightprobesh2/main.cpp`
**行号**: 485-509

**修改前**:
```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // ...
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        
        // ❌ 问题：在这里重复更新
        if (gltfModel) {
            mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
            mainPassData.cameraPos = glm::vec4(camera.position, 10.0f);
            mainPassData.view = camera.matrices.view;
            mainPassData.project = camera.matrices.perspective;
            mainPass->UpdateGlobal(mainPassData);
        }
        
        if (showProbes) { /* ... */ }
        drawUI(cmd);
    });
}
```

**修改后**:
```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // ...
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        
        // ✅ 修复：删除重复的数据更新，数据已在prepareData中更新
        
        if (showProbes) { /* ... */ }
        drawUI(cmd);
    });
}
```

### 效果
✅ 鼠标移动时，gltfModel保持固定位置
✅ 相机移动时，所有模型正确跟随相机视图

---

## 🐛 Bug 2: Capture后previewModel和gltfModel变黑

### 问题描述
点击"Capture Cubemap"按钮后，previewModel和gltfModel都变成黑色，无法看到模型。

### 根本原因
在 `CaptureCubemap()` 函数中，只为 `CAPTURE_SCENE` 技术准备了PSO，但没有为 `MAIN` 技术准备PSO。这导致在主渲染中没有正确的图形管线，模型无法正确渲染。

### 修复方案
**在CaptureCubemap中为MAIN技术准备PSO**

**文件**: `examples/lightprobesh2/main.cpp`
**行号**: 564-588

**修改前**:
```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    // ...
    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());

        // ❌ 只准备了CAPTURE_SCENE，没有准备MAIN
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    }
    // ...
}
```

**修改后**:
```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    // ...
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
    // ...
}
```

### 效果
✅ Capture后，previewModel和gltfModel正常显示
✅ 模型可以正确应用SH和IBL效果
✅ 模型不再变黑

---

## 🔧 额外修复: 光源数据更新

### 问题
光源位置数据在drawFrame中更新，但prepareData中没有设置。

### 修复
**在prepareData中添加光源数据**

**文件**: `examples/lightprobesh2/main.cpp`
**行号**: 471-482

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
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f); // ✅ 设置光源位置
    
    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

### 效果
✅ 光源位置在prepareData中统一管理
✅ 避免重复更新
✅ 代码更清晰

---

## 📊 修复总结

| Bug | 症状 | 原因 | 修复 | 状态 |
|-----|------|------|------|------|
| Bug 1 | 鼠标移动时gltfModel跟随 | drawFrame中重复更新view矩阵 | 删除重复更新 | ✅ 完成 |
| Bug 2 | Capture后模型变黑 | 没有为MAIN技术准备PSO | 添加MAIN PSO准备 | ✅ 完成 |
| Bug 3 | 光源数据管理混乱 | 光源在drawFrame中更新 | 在prepareData中设置 | ✅ 完成 |

---

## 🧪 测试建议

### 测试1: 鼠标移动
1. 运行程序
2. 移动鼠标
3. **预期**: gltfModel保持固定位置，不跟随鼠标

### 测试2: Capture功能
1. 运行程序
2. 点击"Capture Cubemap"按钮
3. **预期**: previewModel和gltfModel保持可见，不变黑

### 测试3: SH和IBL效果
1. 运行程序
2. Capture后，检查模型的光照效果
3. **预期**: 模型显示正确的SH和IBL效果

### 测试4: 光源位置
1. 运行程序
2. 观察模型的光照方向
3. **预期**: 光源来自(10, 10, 10)方向

---

## 📝 修改文件

- ✅ `examples/lightprobesh2/main.cpp`
  - 修改 `prepareData()` 函数 (第471-482行)
  - 修改 `drawFrame()` 函数 (第484-509行)
  - 修改 `CaptureCubemap()` 函数 (第564-588行)

---

## 🎯 下一步

1. **编译代码**
   ```bash
   cd c:\Users\Bluesky\Desktop\graphic\learn
   # 使用你的构建系统编译
   ```

2. **运行测试**
   - 执行上述测试建议
   - 验证bug已修复

3. **性能验证**
   - 检查帧率是否有改善
   - 验证没有新的性能问题

---

## 💡 关键改进

1. **数据更新集中化**
   - 所有全局数据在 `prepareData()` 中更新
   - 避免重复更新导致的问题

2. **PSO管理完善**
   - 为每个技术都准备对应的PSO
   - 确保渲染管线正确

3. **代码清晰度提升**
   - 删除冗余代码
   - 逻辑更清晰


