# GltfModel 在 CapturePass 中的实现 - 快速开始

## 📋 核心修改 (4 个地方)

### 修改 1: gltfload.cpp - Draw() 函数

**位置**: `gltfload.cpp` 第 49-103 行

**改动**: 根据 `tech` 参数选择不同的绘制方式

```cpp
if (tech == ETechnique::CAPTURE_SCENE) {
    // ✅ CAPTURE_SCENE: 单实例，原点
    pc.modelOffset = glm::mat4(1.0f);
    pc.tint = glm::vec4(1.0f);
    vkCmdPushConstants(...);
    model->draw(cmd, vkglTF::RenderFlags::BindImages, 
               techniques[techIdx].pipelineLayout, 1);
} else {
    // ✅ MAIN: 4 实例，不同位置和颜色
    for (int i = 0; i < 4; ++i) {
        pc.modelOffset = glm::translate(...) * glm::scale(...);
        pc.tint = glm::vec4(colors[i % 3], 1.0f);
        vkCmdPushConstants(...);
        model->draw(cmd);
    }
}
```

---

### 修改 2: main.cpp - CaptureCubemap() 函数

**位置**: `main.cpp` 第 566-637 行

**验证**: 确保为两个 Pass 都准备了 PSO

```cpp
if (!gltfModel) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());

    // ✅ 为 MAIN 技术准备 PSO
    gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, 
                         ETechnique::MAIN);

    // ✅ 为 CAPTURE_SCENE 技术准备 PSO
    gltfModel->PreparePSO(capturePass->renderPass, 
                         capturePass->descriptorSetLayout, 
                         ETechnique::CAPTURE_SCENE);
} else {
    // 如果已存在，检查 CAPTURE_SCENE PSO
    gltfModel->PreparePSO(capturePass->renderPass, 
                         capturePass->descriptorSetLayout, 
                         ETechnique::CAPTURE_SCENE);
}

probe->SetGltfModel(gltfModel.get());
```

---

### 修改 3: main.cpp - drawFrame() 函数

**位置**: `main.cpp` 第 486-511 行

**验证**: 在 MainPass 中绘制 gltfModel

```cpp
mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, 
               [this](VkCommandBuffer cmd) {
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
```

---

### 修改 4: LightProbe.cpp - drawScene() 函数

**位置**: `LightProbe.cpp` 第 23-36 行

**验证**: 在 CapturePass 中绘制 gltfModel

```cpp
void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, 
                        ETechnique::CAPTURE_SCENE);
        }
        
        // ✅ 在 CapturePass 中绘制 gltfModel
        if (gltfModel) {
            gltfModel->Draw(cmd, capturePass->descriptorSet, 
                           ETechnique::CAPTURE_SCENE);
        }
    });
}
```

---

## 🎯 执行流程

```
1. 用户点击 "Capture Cubemap at Camera"
   ↓
2. CaptureCubemap() 准备两个 PSO
   ↓
3. probe->CaptureCubeMap() 开始捕获
   ↓
4. LightProbe::drawScene() 绘制场景
   ├─ skybox->Draw(CAPTURE_SCENE)
   └─ gltfModel->Draw(CAPTURE_SCENE)  ← 关键！
   ↓
5. GltfModel::Draw(CAPTURE_SCENE)
   ├─ 绑定 CAPTURE_SCENE PSO
   ├─ 设置 Push Constant (单位矩阵 + 白色)
   └─ model->draw(BindImages)
   ↓
6. Multiview 着色器处理 6 个立方体面
   ↓
7. 生成 SH 系数和 IBL 贴图
   ↓
8. 下一帧使用新的光照信息
```

---

## 📊 关键参数对比

| 参数 | MAIN | CAPTURE_SCENE |
|------|------|---------------|
| 实例数 | 4 | 1 |
| 位置 | 4 个不同位置 | 原点 (0,0,0) |
| 颜色 | 红/绿/蓝 | 白色 |
| modelOffset | translate * scale | 单位矩阵 |
| tint | colors[i % 3] | (1,1,1,1) |
| model->draw() 参数 | 无 | BindImages + pipelineLayout + 1 |
| 目标 | 屏幕 | 立方体贴图 |
| 着色器 | 标准 | Multiview |

---

## 🧪 测试步骤

1. **编译**
   ```bash
   cmake --build . --config Release
   ```

2. **运行**
   ```bash
   ./lightprobesh2.exe
   ```

3. **加载模型**
   - 在 UI 中选择一个 glTF 模型

4. **捕获立方体贴图**
   - 点击 "Capture Cubemap at Camera" 按钮
   - 应该看到立方体贴图被捕获
   - 检查生成的 SH 系数和 IBL 贴图

5. **验证结果**
   - 模型在主视图中显示新的光照效果
   - 检查生成的 Captured_*.ppm 文件

---

## 🐛 常见问题

### Q1: 捕获的立方体贴图是黑色的
**原因**: GltfModel 没有为 CAPTURE_SCENE 准备 PSO
**解决**: 确保在 CaptureCubemap() 中调用了 PreparePSO()

### Q2: 模型在 MainPass 中不显示
**原因**: 没有在 drawFrame() 中调用 gltfModel->Draw()
**解决**: 添加 `if (gltfModel) { gltfModel->Draw(...); }`

### Q3: 光照信息没有被捕获
**原因**: CapturePass 中没有绘制 gltfModel
**解决**: 确保在 LightProbe::drawScene() 中调用了 gltfModel->Draw()

### Q4: 性能下降
**原因**: 每帧都在捕获立方体贴图
**解决**: 只在按钮点击时捕获，不要在每帧都捕获

---

## 📁 相关文件

| 文件 | 修改内容 |
|------|---------|
| gltfload.cpp | Draw() 函数 - 添加条件分支 |
| main.cpp | CaptureCubemap() - 准备两个 PSO |
| main.cpp | drawFrame() - 在 MainPass 中绘制 |
| LightProbe.cpp | drawScene() - 在 CapturePass 中绘制 |

---

## 📚 详细文档

- **GLTFMODEL_DRAWING_ANALYSIS.md** - 绘制逻辑详细分析
- **CAPTURE_GLTFMODEL_IMPLEMENTATION_GUIDE.md** - 完整实现指南
- **MAINPASS_VS_CAPTUREPASS_COMPARISON.md** - 两个 Pass 的对比
- **CAPTURE_IMPLEMENTATION_SUMMARY.md** - 实现总结

---

## ✅ 验证清单

- [ ] 修改 gltfload.cpp - Draw() 函数
- [ ] 验证 main.cpp - CaptureCubemap() 函数
- [ ] 验证 main.cpp - drawFrame() 函数
- [ ] 验证 LightProbe.cpp - drawScene() 函数
- [ ] 编译成功
- [ ] 加载 glTF 模型
- [ ] 点击 "Capture Cubemap" 按钮
- [ ] 立方体贴图被捕获
- [ ] 生成 SH 系数
- [ ] 生成 IBL 贴图
- [ ] 模型显示新的光照效果

---

## 💡 关键概念

- **MAIN 模式**: 主渲染，显示 4 个实例，每帧执行
- **CAPTURE_SCENE 模式**: 捕获光照，显示 1 个实例，按需执行
- **Multiview**: 单次渲染到 6 个立方体面
- **Push Constant**: 快速更新的小数据块
- **Descriptor Set**: 着色器资源的集合


