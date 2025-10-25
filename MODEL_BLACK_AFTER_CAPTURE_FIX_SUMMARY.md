# 🎯 模型在捕获后变黑 - 完整修复方案

## 问题描述
在调用 `CaptureCubemap()` 后，`previewModel` 和 `gltfModel` 都变成黑色且无法恢复。

## 根本原因

### 原因1: 重复调用 PreparePSO 导致资源泄漏
**文件**: `examples/lightprobesh2/main.cpp` 第817-825行

**问题**:
```cpp
} else {
    // ❌ 如果 gltfModel 已存在，重复调用 PreparePSO
    gltfModel->PreparePSO(
        capturePass->renderPass,
        capturePass->descriptorSetLayout,
        ETechnique::CAPTURE_SCENE
    );
}
```

**影响**:
- `PreparePSO` 函数会创建新的 `pipelineLayout` 和 `pipeline`
- 每次调用都会覆盖旧的资源，但不会销毁它们
- 导致资源泄漏和状态混乱
- 可能导致描述符绑定失效

### 原因2: 材质参数未在初始化时设置
**文件**: `examples/lightprobesh2/main.cpp` 第334-349行

**问题**:
```cpp
previewModel = std::make_unique<PreviewModel>(vulkanDevice, this);
previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
previewModel->UpdateModel(previewModels[modelIndex]);
// ❌ 没有初始化材质参数

gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
// ... PSO 准备 ...
gltfModel->SetTransform(t * s);
// ❌ 没有初始化材质参数
```

**影响**:
- 材质buffer中的数据可能是未初始化的垃圾值
- 虽然结构体有默认值 `useSH = 1`，但可能没有被写入GPU buffer
- 导致着色器读取到错误的材质参数

### 原因3: 缺少同步点
**文件**: `examples/lightprobesh2/main.cpp` 第841-870行

**问题**:
```cpp
shGenPass->Generate(queue);
// ❌ 没有等待SH生成完成

genIBL->Generate(queue);
// ❌ 没有等待IBL生成完成

mainPass->UpdateBindings();
// ❌ 没有等待绑定更新完成

previewModel->SetUseSHAndReflection(true, true);
// ❌ 可能在GPU操作完成前就设置了材质参数
```

**影响**:
- GPU操作可能还没完成就开始下一步
- 描述符绑定可能指向未完成的资源
- 导致渲染结果不确定

## 修复方案

### 修复1: 避免重复调用 PreparePSO

**文件**: `examples/lightprobesh2/main.cpp` 第798-826行

**修改前**:
```cpp
if (!gltfModel) {
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());
    gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
} else {
    // ❌ 重复调用 PreparePSO
    gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
}
```

**修改后**:
```cpp
if (!gltfModel) {
    std::cout << "[CaptureCubemap] Creating new gltfModel..." << std::endl;
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    gltfModel->UpdateModel(previewModel->getModel());
    
    gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    
    // ✅ 初始化材质参数
    gltfModel->SetUseSHAndReflection(false, false);
    std::cout << "[CaptureCubemap] gltfModel created and initialized" << std::endl;
} else {
    std::cout << "[CaptureCubemap] Using existing gltfModel (PSOs already prepared in PrepareScene)" << std::endl;
    // ✅ 不要重复调用 PreparePSO，因为在 PrepareScene 中已经准备好了
}
```

### 修复2: 在 PrepareScene 中初始化材质参数

**文件**: `examples/lightprobesh2/main.cpp` 第334-353行

**修改前**:
```cpp
previewModel = std::make_unique<PreviewModel>(vulkanDevice, this);
previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
previewModel->UpdateModel(previewModels[modelIndex]);

gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
gltfModel->SetTransform(t * s);
```

**修改后**:
```cpp
previewModel = std::make_unique<PreviewModel>(vulkanDevice, this);
previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
previewModel->UpdateModel(previewModels[modelIndex]);
// ✅ 初始化材质参数，使用SH但不使用反射（因为还没有捕获）
previewModel->SetUseSHAndReflection(true, false);

gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
gltfModel->SetTransform(t * s);
// ✅ 初始化材质参数，使用SH但不使用反射（因为还没有捕获）
gltfModel->SetUseSHAndReflection(true, false);
```

### 修复3: 添加同步点和调试输出

**文件**: `examples/lightprobesh2/main.cpp` 第841-891行

**修改前**:
```cpp
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);

VkDescriptorBufferInfo shBufferInfo;
shGenPass->FeedSH(shBufferInfo);
mainPass->environmemts.shCoeffs = shBufferInfo;

genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

mainPass->UpdateBindings();
skybox->SetCubeMap(capturedCubemap);

if (previewModel) {
    previewModel->SetUseSHAndReflection(true, true);
}
if (gltfModel) {
    gltfModel->SetUseSHAndReflection(true, true);
}
```

**修改后**:
```cpp
// --- 生成 SH ---
std::cout << "[CaptureCubemap] Generating SH coefficients..." << std::endl;
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);

VkDescriptorBufferInfo shBufferInfo;
shGenPass->FeedSH(shBufferInfo);
mainPass->environmemts.shCoeffs = shBufferInfo;
std::cout << "[CaptureCubemap] SH buffer: " << (shBufferInfo.buffer ? "Valid" : "NULL") << std::endl;

// --- 生成 IBL ---
std::cout << "[CaptureCubemap] Generating IBL maps..." << std::endl;
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);
std::cout << "[CaptureCubemap] Irradiance sampler: " << (mainPass->environmemts.irradianceCube.sampler ? "Valid" : "NULL") << std::endl;
std::cout << "[CaptureCubemap] Prefiltered sampler: " << (mainPass->environmemts.prefilteredCube.sampler ? "Valid" : "NULL") << std::endl;

// ✅ 验证数据有效性
if (!mainPass->environmemts.shCoeffs.buffer) {
    std::cerr << "[CaptureCubemap] Warning: shCoeffs buffer not initialized!" << std::endl;
}
// ... 其他验证 ...

// ✅ 先更新绑定，再设置材质参数
mainPass->UpdateBindings();

// ✅ 等待所有GPU操作完成
vkDeviceWaitIdle(vulkanDevice->logicalDevice);

skybox->SetCubeMap(capturedCubemap);

// ✅ 启用SH和反射，并强制刷新材质buffer
if (previewModel) {
    previewModel->SetUseSHAndReflection(true, true);
    std::cout << "[CaptureCubemap] PreviewModel: Enabled SH and Reflection" << std::endl;
}
if (gltfModel) {
    gltfModel->SetUseSHAndReflection(true, true);
    std::cout << "[CaptureCubemap] GltfModel: Enabled SH and Reflection" << std::endl;
}

std::cout << "[CaptureCubemap] Capture complete! Models should now use captured lighting." << std::endl;
```

## 修复效果

### 修复前
- 模型在捕获后变成黑色
- 无法恢复，即使切换天空盒也不行
- 可能有资源泄漏

### 修复后
- 模型在捕获后正确显示
- 使用捕获的光照信息
- 没有资源泄漏
- 有详细的调试输出

## 预期输出

修复后，运行程序并点击 "Capture Cubemap" 按钮，应该看到以下输出：

```
[CaptureCubemap] Using existing gltfModel (PSOs already prepared in PrepareScene)
[CaptureCubemap] Generating SH coefficients...
[CaptureCubemap] SH buffer: Valid
[CaptureCubemap] Generating IBL maps...
[CaptureCubemap] Irradiance sampler: Valid
[CaptureCubemap] Prefiltered sampler: Valid
[CaptureCubemap] PreviewModel: Enabled SH and Reflection
[SetUseSHAndReflection] Updated: useSH=1, useReflection=1
[CaptureCubemap] GltfModel: Enabled SH and Reflection
[SetUseSHAndReflection] Updated: useSH=1, useReflection=1
[CaptureCubemap] Capture complete! Models should now use captured lighting.
```

## 验证步骤

1. **编译并运行程序**
2. **观察初始状态**: 模型应该正常显示（使用默认天空盒的光照）
3. **点击 "Capture Cubemap" 按钮**
4. **检查控制台输出**: 应该看到上述调试信息
5. **观察模型**: 模型应该仍然正常显示，使用捕获的光照
6. **切换天空盒**: 模型应该继续正常显示

## 如果问题仍然存在

如果修复后模型仍然变黑，请检查：

1. **SH系数是否有效**: 在 `GenSHComputePass::Generate()` 后读取buffer内容
2. **IBL贴图是否正确**: 保存 irradiance 和 prefiltered cubemap 到文件
3. **描述符绑定是否正确**: 使用 RenderDoc 捕获帧并检查
4. **着色器是否正确**: 检查 `gltfmesh_main.frag` 中的光照计算

## 相关文件

- `examples/lightprobesh2/main.cpp` - 主要修复
- `examples/lightprobesh2/PreviewModel.cpp` - 材质设置
- `examples/lightprobesh2/gltfload.cpp` - 材质设置
- `MODEL_BLACK_AFTER_CAPTURE_DEBUG.md` - 详细调试指南

