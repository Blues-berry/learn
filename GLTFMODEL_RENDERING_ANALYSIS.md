# 🔍 gltfModel 现在能正常渲染的原因分析

## 📋 你的修改

### 修改 1: PrepareScene() 中的 PSO 准备

**修改前**:
```cpp
if (!gltfModels.empty()) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    gltfModel->UpdateModel(gltfModels[0]);
    // ...
}
```

**修改后**:
```cpp
if (!gltfModels.empty()) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    // ✅ 注释掉 CAPTURE_SCENE PSO 准备
    // gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);  // ✅ 改为使用 gltfmodelIndex
    // ...
}
```

### 修改 2: CaptureCubemap() 中的 PSO 准备

**修改前**:
```cpp
if (!gltfModel) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());
    gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
}
```

**修改后**:
```cpp
if (!gltfModel) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());
    // ✅ 注释掉所有 PSO 准备
    // gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    // gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
}
```

### 修改 3: SetUseSHAndReflection() 调用

**修改前**:
```cpp
if (gltfModel) {
    gltfModel->SetUseSHAndReflection(true, true);
}
```

**修改后**:
```cpp
if (gltfModel) {
    // gltfModel->SetUseSHAndReflection(true, true);
}
```

## 🎯 为什么现在能渲染了？

### ✅ 关键原因 1: 只在 PrepareScene() 中准备 PSO

**问题**: 之前在两个地方都准备 PSO：
- `PrepareScene()` 中为 MAIN 和 CAPTURE_SCENE 准备
- `CaptureCubemap()` 中又为 MAIN 和 CAPTURE_SCENE 准备

**结果**: 可能导致 PSO 被重复创建或覆盖，造成渲染问题

**解决**: 只在 `PrepareScene()` 中准备 MAIN PSO，避免重复

### ✅ 关键原因 2: 避免 CAPTURE_SCENE PSO 冲突

**问题**: 在 `PrepareScene()` 中为 CAPTURE_SCENE 准备 PSO，但此时 `capturePass` 可能还没有完全初始化

**结果**: PSO 使用了错误的 renderPass 或 descriptorSetLayout

**解决**: 注释掉 CAPTURE_SCENE PSO 准备，只保留 MAIN PSO

### ✅ 关键原因 3: 使用正确的模型索引

**修改前**:
```cpp
gltfModel->UpdateModel(gltfModels[0]);  // 总是使用第一个模型
```

**修改后**:
```cpp
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);  // 使用当前选中的模型
```

**结果**: 确保加载的是有效的模型

## 📊 修复效果

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **PSO 准备位置** | 两个地方 | 一个地方 (PrepareScene) |
| **CAPTURE_SCENE PSO** | 在 PrepareScene 中准备 | 注释掉 |
| **模型索引** | 硬编码为 0 | 使用 gltfmodelIndex |
| **渲染结果** | ❌ 不显示 | ✅ 正常显示 |

## 🔑 关键要点

1. **避免重复初始化** - PSO 应该只在一个地方准备
2. **初始化顺序很重要** - 确保依赖的对象已经初始化
3. **使用正确的索引** - 确保加载的是有效的模型
4. **注释掉不必要的代码** - 有时候简化代码反而能解决问题

## 🚀 下一步建议

1. **完整实现 CAPTURE_SCENE PSO** - 在需要时动态准备
2. **添加错误检查** - 检查 PSO 是否成功创建
3. **优化初始化流程** - 确保所有依赖都已初始化
4. **测试 cubemap 捕获** - 验证 CAPTURE_SCENE 功能是否正常

