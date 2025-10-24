# GltfModel 在 CapturePass 中的完整实现指南

## 📋 目标

在点击 "Capture Cubemap at Camera" 后，将 gltfModel 在 MainPass 中绘制的内容重新在 CapturePass 中绘制一遍，同时捕获包含光照信息的立方体贴图。

---

## 🏗️ 架构概览

### 两个渲染通道

```
MainPass (主渲染)
├─ 渲染到屏幕
├─ 绘制: skybox + previewModel + gltfModel
└─ 使用 mainPass->descriptorSet

CapturePass (捕获渲染)
├─ 渲染到立方体贴图 (6 个面)
├─ 绘制: skybox + gltfModel
└─ 使用 capturePass->descriptorSet
```

### 关键区别

| 特性 | MainPass | CapturePass |
|------|----------|------------|
| 目标 | 屏幕 | 立方体贴图 |
| 视图 | 单一相机视图 | 6 个立方体面视图 |
| 着色器 | gltfmesh.vert/frag | gltfmesh_mvr.vert/frag (multiview) |
| 描述符集 | mainPass->descriptorSet | capturePass->descriptorSet |

---

## 🔧 实现步骤

### 步骤 1: 确保 GltfModel 为两个 Pass 都准备了 PSO

**文件**: `main.cpp` - `CaptureCubemap()` 函数

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

        // ✅ 为 MAIN 技术准备 PSO
        gltfModel->PreparePSO(
            renderPass,
            mainPass->descriptorSetLayout,
            ETechnique::MAIN
        );

        // ✅ 为 CAPTURE_SCENE 技术准备 PSO
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    } else {
        // 如果 gltfModel 已存在但没有 CAPTURE_SCENE PSO，则准备它
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    }

    probe->SetGltfModel(gltfModel.get());
    probe->CaptureCubeMap(queue);
    
    // ... 后续 SH/IBL 生成 ...
}
```

**关键点**:
- 两个 PSO 使用不同的 renderPass
- 两个 PSO 使用不同的 descriptorSetLayout
- 同一个 gltfModel 对象支持两种技术

---

### 步骤 2: 在 MainPass 中绘制 GltfModel

**文件**: `main.cpp` - `drawFrame()` 函数

```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    if (probe) {
        probe->CaptureCubeMap(queue, cmd);
    }

    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, 
                   [this](VkCommandBuffer cmd) {
        // 绘制场景
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        
        // ✅ 在 MainPass 中绘制 gltfModel
        if (gltfModel) { 
            gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); 
        }
        
        for (auto& m : gltfClones) { 
            m->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); 
        }

        drawUI(cmd);
    });
}
```

---

### 步骤 3: 在 CapturePass 中绘制 GltfModel

**文件**: `LightProbe.cpp` - `drawScene()` 函数

```cpp
void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        // 绘制天空盒
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
        
        // ✅ 在 CapturePass 中绘制 gltfModel
        if (gltfModel) {
            gltfModel->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
    });
}
```

---

## 🎨 GltfModel::Draw() 的两种模式

### 模式 1: MAIN (主渲染)

```cpp
// 绘制 4 个不同位置的模型实例
for (int i = 0; i < 4; ++i) {
    pc.modelOffset = glm::translate(...) * glm::scale(...);
    pc.tint = glm::vec4(colors[i % 3], 1.0f);
    vkCmdPushConstants(...);
    model->draw(cmd);  // 使用默认绑定
}
```

**特点**:
- 多实例绘制（4 个位置）
- 使用 Push Constant 控制位置和颜色
- 简单的绘制调用

### 模式 2: CAPTURE_SCENE (捕获渲染)

```cpp
// 只绘制一次，在原点
pc.modelOffset = glm::mat4(1.0f);  // 单位矩阵
pc.tint = glm::vec4(1.0f);         // 白色
vkCmdPushConstants(...);
model->draw(cmd, vkglTF::RenderFlags::BindImages, 
           pipelineLayout, 1);  // 传递额外参数
```

**特点**:
- 单实例绘制
- 使用 BindImages 标志绑定纹理
- 传递 pipelineLayout 和 bindImageSet 参数

---

## 📊 数据流向

### MainPass 数据流

```
CPU: mainPass->UpdateGlobal(mainPassData)
  ↓
GPU: Set 0 (globalSet)
  ├─ projection, view 矩阵
  ├─ 光照信息
  └─ 环境光照 (SH, IBL)
  ↓
GltfModel::Draw(cmd, mainPass->descriptorSet, MAIN)
  ├─ 绑定 Set 0 + Set 1
  ├─ 绑定 MAIN PSO
  └─ 绘制 4 个实例
```

### CapturePass 数据流

```
CPU: capturePass->UpdateGlobal(ubo)
  ↓
GPU: Set 0 (capturePass->descriptorSet)
  ├─ viewproj[6] (6 个立方体面的视图投影)
  ├─ cameraPos[6]
  └─ 光照信息
  ↓
GltfModel::Draw(cmd, capturePass->descriptorSet, CAPTURE_SCENE)
  ├─ 绑定 Set 0 + Set 1
  ├─ 绑定 CAPTURE_SCENE PSO
  └─ 绘制 1 个实例 (multiview 自动处理 6 个面)
```

---

## 🔍 关键代码位置

| 功能 | 文件 | 函数 | 行号 |
|------|------|------|------|
| 准备 PSO | main.cpp | CaptureCubemap() | 577-596 |
| MainPass 绘制 | main.cpp | drawFrame() | 493-510 |
| CapturePass 绘制 | LightProbe.cpp | drawScene() | 23-36 |
| 捕获立方体贴图 | LightProbe.cpp | CaptureCubeMap() | 50-155 |
| 生成 SH/IBL | main.cpp | CaptureCubemap() | 613-626 |

---

## ✅ 验证清单

- [ ] GltfModel 为 MAIN 和 CAPTURE_SCENE 都准备了 PSO
- [ ] MainPass 中调用 `gltfModel->Draw(cmd, mainPass->descriptorSet, MAIN)`
- [ ] CapturePass 中调用 `gltfModel->Draw(cmd, capturePass->descriptorSet, CAPTURE_SCENE)`
- [ ] CapturePass 使用 multiview 着色器 (gltfmesh_mvr.vert/frag)
- [ ] 捕获后生成 SH 和 IBL 贴图
- [ ] 验证捕获的立方体贴图包含光照信息

---

## 🐛 常见问题

### Q1: 捕获的立方体贴图是黑色的

**原因**: 
- GltfModel 没有为 CAPTURE_SCENE 准备 PSO
- 描述符集绑定错误

**解决**:
```cpp
// 确保在 CaptureCubemap() 中准备了 CAPTURE_SCENE PSO
gltfModel->PreparePSO(
    capturePass->renderPass,
    capturePass->descriptorSetLayout,
    ETechnique::CAPTURE_SCENE
);
```

### Q2: 捕获的内容与 MainPass 不一致

**原因**:
- 使用了不同的模型或材质参数
- 光照参数不同

**解决**:
```cpp
// 在 CaptureCubeMap() 中设置相同的光照参数
ubo.mainLight = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
ubo.exposure = mainPassData.exposure;
ubo.gamma = mainPassData.gamma;
```

### Q3: 性能下降

**原因**:
- 每帧都在捕获立方体贴图

**解决**:
```cpp
// 只在需要时捕获（按钮触发）
if (overlay->button("Capture Cubemap at Camera")) {
    CaptureCubemap(camera.position);
}
```


