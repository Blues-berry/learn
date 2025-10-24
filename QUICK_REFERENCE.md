# 🚀 快速参考 - Bug修复

## 📌 三个Bug一览

### Bug 1: 鼠标移动时gltfModel跟随
- **症状**: 移动鼠标，模型也移动
- **原因**: drawFrame中重复更新view矩阵
- **修复**: 删除drawFrame中的重复更新
- **文件**: main.cpp 第485-509行
- **状态**: ✅ 已修复

### Bug 2: Capture后模型变黑
- **症状**: 点击Capture后，模型变黑
- **原因**: 没有为MAIN技术准备PSO
- **修复**: 添加MAIN技术的PSO准备
- **文件**: main.cpp 第564-588行
- **状态**: ✅ 已修复

### Bug 3: 光源数据混乱
- **症状**: 光源数据在drawFrame中更新
- **原因**: 数据管理不统一
- **修复**: 在prepareData中设置光源
- **文件**: main.cpp 第471-482行
- **状态**: ✅ 已修复

---

## 🔧 修改代码片段

### 修改1: prepareData (添加光源)
```cpp
void VulkanExample::prepareData()
{
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f); // ✅ 新增
    
    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

### 修改2: drawFrame (删除重复更新)
```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // ...
    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        previewModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
        
        // ✅ 删除了这个if块
        
        if (showProbes) { /* ... */ }
        drawUI(cmd);
    });
}
```

### 修改3: CaptureCubemap (添加MAIN PSO)
```cpp
if (!gltfModel) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());

    // ✅ 新增：为MAIN技术准备PSO
    gltfModel->PreparePSO(
        renderPass,
        mainPass->descriptorSetLayout,
        ETechnique::MAIN
    );

    // 为CAPTURE_SCENE技术准备PSO
    gltfModel->PreparePSO(
        capturePass->renderPass,
        capturePass->descriptorSetLayout,
        ETechnique::CAPTURE_SCENE
    );
}
```

---

## ✅ 测试清单

- [ ] 编译代码
- [ ] 运行程序
- [ ] 移动鼠标，确保gltfModel不跟随
- [ ] 点击Capture，确保模型不变黑
- [ ] 检查光照效果是否正确

---

## 📊 修改统计

| 项目 | 数量 |
|------|------|
| 修改的函数 | 3个 |
| 修改的行数 | ~30行 |
| 删除的代码 | 13行 |
| 添加的代码 | 10行 |
| 修改的文件 | 1个 |

---

## 🎯 预期效果

### 修复前
- ❌ 鼠标移动时gltfModel跟随
- ❌ Capture后模型变黑
- ❌ 光源数据管理混乱

### 修复后
- ✅ gltfModel保持固定位置
- ✅ Capture后模型正常显示
- ✅ 光源数据统一管理

---

## 📁 相关文档

- **BUG_ANALYSIS_AND_FIXES.md** - 详细的bug分析
- **BUG_FIXES_APPLIED.md** - 修复的详细说明
- **FINAL_SUMMARY.md** - 完整总结

---

## 💻 编译和测试

```bash
# 进入项目目录
cd c:\Users\Bluesky\Desktop\graphic\learn

# 编译（根据你的构建系统）
# 例如使用CMake:
cmake --build build --config Release

# 运行程序
./build/Release/lightprobesh2.exe
```

---

## 🔍 验证修复

### 验证1: 鼠标移动
```
操作: 移动鼠标
预期: gltfModel保持固定位置
验证: 模型不跟随鼠标
```

### 验证2: Capture功能
```
操作: 点击Capture Cubemap
预期: 模型保持可见
验证: 模型不变黑
```

### 验证3: 光照效果
```
操作: 观察模型光照
预期: 光源来自(10,10,10)
验证: 光照方向正确
```

---

## 🎓 学习要点

1. **数据管理**
   - 集中管理全局数据
   - 避免重复更新

2. **PSO配置**
   - 为每个技术准备PSO
   - 确保完整配置

3. **代码组织**
   - 相关代码放在一起
   - 使用清晰的注释

---

## 📞 快速查询

| 问题 | 答案 |
|------|------|
| 哪个文件被修改? | main.cpp |
| 修改了哪些函数? | prepareData, drawFrame, CaptureCubemap |
| 删除了多少行? | 13行 |
| 添加了多少行? | 10行 |
| 需要重新编译? | 是 |
| 需要重新配置? | 否 |

---

**修复完成！准备好测试了吗？** 🚀


