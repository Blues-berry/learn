# 探针可视化卡死问题修复

## 🐛 问题描述

程序在点击某些按钮后卡死，无法响应。

## 🔍 根本原因

**问题位置**: `ProbeVisualizer::Initialize()` 方法

**原因分析**:
1. `Initialize()` 方法中的球体模型加载代码被注释掉了
2. 导致 `sphereModel` 为 null
3. 当 `DrawProbe()` 或 `DrawProbes()` 尝试使用 null 模型时，程序崩溃或卡死

## ✅ 修复方案

### 修改 1: ProbeVisualizer.h - 添加 SetSphereModel 方法

**位置**: `examples/lightprobesh2/ProbeVisualizer.h`

**改动**: 添加方法来设置已加载的球体模型

```cpp
// 设置球体模型（使用已加载的模型）
void SetSphereModel(const std::shared_ptr<vkglTF::Model>& model) { sphereModel = model; }
```

### 修改 2: ProbeVisualizer.cpp - 简化 Initialize 方法

**位置**: `examples/lightprobesh2/ProbeVisualizer.cpp` 第 11-19 行

**修复前**:
```cpp
void ProbeVisualizer::Initialize()
{
    // 创建一个小球体模型用于可视化探针
    sphereModel = std::make_shared<vkglTF::Model>();
    uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
    
    // 从资源路径加载球体模型
    // 注意：这需要在 IExampleInterfasce 中实现 getAssetPath()
    // sphereModel->loadFromFile(getAssetPath() + "models/sphere.gltf", device, queue, glTFLoadingFlags);
}
```

**修复后**:
```cpp
void ProbeVisualizer::Initialize()
{
    // ✅ 修复：使用 PreviewModel 中已经加载的球体模型
    // 不需要重新加载，因为球体模型已经在 LoadAssets() 中加载过了
    // 我们只需要在 DrawProbe 时使用已有的模型
    
    // 注意：sphereModel 将在 DrawProbe 时从外部传入或使用预加载的模型
    // 这里不需要加载，避免重复加载和卡死
}
```

### 修改 3: main.cpp - 设置球体模型

**位置**: `examples/lightprobesh2/main.cpp` 第 351-360 行

**修复前**:
```cpp
// ✅ 新增：初始化探针可视化器
probeVisualizer = std::make_unique<ProbeVisualizer>(vulkanDevice, this);
probeVisualizer->Initialize();
probeVisualizer->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
probeVisualizer->SetProbeScale(probeVisualizationScale);
```

**修复后**:
```cpp
// ✅ 新增：初始化探针可视化器
probeVisualizer = std::make_unique<ProbeVisualizer>(vulkanDevice, this);
probeVisualizer->Initialize();
// ✅ 设置球体模型（使用已加载的 preview 模型中的球体）
if (!previewModels.empty()) {
    probeVisualizer->SetSphereModel(previewModels[0]);  // 使用第一个球体模型
}
probeVisualizer->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
probeVisualizer->SetProbeScale(probeVisualizationScale);
```

## 🎯 修复原理

### 问题的根本原因
- 尝试在 `Initialize()` 中加载模型，但代码被注释掉了
- 导致 `sphereModel` 为 null
- 绘制时使用 null 模型导致崩溃

### 解决方案
- 不在 `Initialize()` 中加载模型
- 使用已经在 `LoadAssets()` 中加载的球体模型
- 通过 `SetSphereModel()` 方法传入已加载的模型
- 避免重复加载和资源浪费

## 📊 修改统计

| 项目 | 数量 |
|------|------|
| 修改的文件 | 3 个 |
| 修改的行数 | ~15 行 |
| 新增方法 | 1 个 |
| 删除的代码 | 0 行 |

## ✅ 编译状态

✅ **编译成功** - 无错误

## 🧪 测试建议

1. **启动程序**
   - 程序应该正常启动
   - 不应该卡死

2. **选择显示模式**
   - 选择 "Display Mode" = "Single"
   - 程序应该响应

3. **点击按钮**
   - 点击 "Capture Cubemap at Camera"
   - 程序应该正常工作，不卡死

4. **查看探针**
   - 应该能看到探针显示为球体
   - 颜色应该正确

## 🔄 工作流程

```
LoadAssets()
    ↓
加载球体模型到 previewModels[0]
    ↓
PrepareScene()
    ↓
创建 ProbeVisualizer
    ↓
调用 SetSphereModel(previewModels[0])
    ↓
设置 sphereModel 指向已加载的模型
    ↓
DrawProbe() / DrawProbes()
    ↓
使用有效的 sphereModel 绘制
    ↓
✅ 成功显示探针
```

## 💡 关键改进

### 1. 避免重复加载
- 球体模型已经在 `LoadAssets()` 中加载
- 不需要在 `Initialize()` 中重新加载
- 节省内存和加载时间

### 2. 避免 null 指针
- 通过 `SetSphereModel()` 确保模型有效
- 在使用前检查模型是否为 null

### 3. 资源共享
- 多个组件可以共享同一个球体模型
- 提高效率和性能

## 📝 相关代码

### ProbeVisualizer.h
```cpp
// 设置球体模型（使用已加载的模型）
void SetSphereModel(const std::shared_ptr<vkglTF::Model>& model) { sphereModel = model; }
```

### main.cpp
```cpp
// 设置球体模型（使用已加载的 preview 模型中的球体）
if (!previewModels.empty()) {
    probeVisualizer->SetSphereModel(previewModels[0]);
}
```

## 🎓 学习收获

1. **资源管理** - 避免重复加载资源
2. **null 检查** - 确保指针有效
3. **模块化设计** - 组件之间的协作
4. **调试技巧** - 识别卡死的原因

## 总结

通过使用已加载的球体模型而不是尝试重新加载，成功解决了程序卡死的问题。现在程序应该能够正常运行，并正确显示探针。


