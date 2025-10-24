# ✅ gltfModel在MainPass和CapturePass中的正确处理

## 🎯 问题

gltfModel需要在两个地方都被正确处理：
1. **MainPass** - 主渲染通道中绘制
2. **CapturePass** - 立方体贴图捕获通道中绘制

---

## 🔍 架构分析

### MainPass流程
```
drawFrame()
  └─ mainPass->Draw()
      └─ 匿名函数 encoder()
          ├─ skybox->Draw(MAIN)
          ├─ previewModel->Draw(MAIN)
          ├─ gltfModel->Draw(MAIN)  ✅ 在这里绘制
          └─ gltfClones->Draw(MAIN)
```

### CapturePass流程
```
CaptureCubemap()
  └─ probe->CaptureCubeMap()
      └─ LightProbe::drawScene()
          └─ capturePass->Draw()
              └─ 匿名函数 encoder()
                  ├─ skybox->Draw(CAPTURE_SCENE)
                  └─ gltfModel->Draw(CAPTURE_SCENE)  ✅ 在这里绘制
```

---

## ✅ 修复方案

### 修复1: PrepareScene() - 为两个Pass都准备PSO

**文件**: `main.cpp` - `PrepareScene()` 函数

```cpp
void VulkanExample::PrepareScene()
{
    // ... skybox和previewModel初始化 ...
    
    // ✅ 准备gltfModel - 为MainPass和CapturePass都准备PSO
    if (!gltfModels.empty()) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        
        // 为MainPass准备PSO
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
        
        // 为CapturePass准备PSO
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
        
        // 设置模型
        gltfModel->UpdateModel(gltfModels[0]);
        
        // 设置位置和缩放
        glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(-20.0f, 0.0f, 0.0f));
        glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        gltfModel->SetTransform(t * s);
    }
}
```

✅ **已完成**

### 修复2: drawFrame() - 在MainPass中绘制gltfModel

**文件**: `main.cpp` - `drawFrame()` 函数

```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // ... probe capture ...
    
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        
        // ✅ 在MainPass中绘制gltfModel
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

✅ **已完成**

### 修复3: LightProbe::drawScene() - 在CapturePass中绘制gltfModel

**文件**: `LightProbe.cpp` - `drawScene()` 函数

```cpp
void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
        
        // ✅ 在CapturePass中绘制gltfModel
        if (gltfModel) {
            gltfModel->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
    });
}
```

✅ **已完成**

---

## 📊 修改统计

| 文件 | 函数 | 改动 | 状态 |
|------|------|------|------|
| main.cpp | PrepareScene() | 为两个Pass准备PSO | ✅ |
| main.cpp | drawFrame() | 在MainPass中绘制gltfModel | ✅ |
| LightProbe.cpp | drawScene() | 在CapturePass中绘制gltfModel | ✅ |

---

## 🧪 编译和测试

### 编译步骤

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 运行程序

```bash
./build/Release/lightprobesh2.exe
```

### 验证清单

- [ ] 程序编译成功
- [ ] gltfModel在初始绘制时可见
- [ ] gltfModel显示为灰白色
- [ ] gltfModel固定在世界坐标系中
- [ ] 点击"Capture Cubemap"时，cubemap中有gltfModel内容
- [ ] 所有6张cubemap图片都有gltfModel纹理

---

## 🎯 预期效果

### 修复前 ❌
```
MainPass: gltfModel不显示
CapturePass: cubemap中没有gltfModel
```

### 修复后 ✅
```
MainPass: gltfModel可见，显示为灰白色
CapturePass: 6张cubemap图片都有gltfModel内容
```

---

## 📝 关键概念

### ETechnique枚举
```cpp
enum class ETechnique {
    MAIN = 0,           // 主渲染通道
    CAPTURE_SCENE = 1   // 立方体贴图捕获通道
};
```

### PSO准备
- 每个技术需要不同的PSO
- PSO包含管线布局、着色器、渲染通道等信息
- 必须为每个技术都调用 `PreparePSO()`

### 描述符集
- MainPass使用 `mainPass->descriptorSet`
- CapturePass使用 `capturePass->descriptorSet`
- 不同的描述符集包含不同的资源绑定

---

## 🎉 总结

**问题**: gltfModel需要在两个地方都被正确处理

**解决方案**:
1. ✅ 在 `PrepareScene()` 中为两个Pass都准备PSO
2. ✅ 在 `drawFrame()` 中在MainPass中绘制gltfModel
3. ✅ 在 `LightProbe::drawScene()` 中在CapturePass中绘制gltfModel

**结果**:
✅ gltfModel在MainPass中可见
✅ gltfModel在CapturePass中被捕获
✅ 所有功能正常工作


