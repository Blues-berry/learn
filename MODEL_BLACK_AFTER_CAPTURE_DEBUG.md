# 🐛 模型在捕获后变黑 - 调试指南

## 问题描述
在调用 `CaptureCubemap()` 后，`previewModel` 和 `gltfModel` 都变成黑色且无法恢复。

## 可能的原因

### 1. 材质Buffer被重置
**症状**: 模型完全变黑，没有任何光照
**原因**: `materialBuffer` 中的数据被重置或覆盖
**检查方法**:
```cpp
// 在 CaptureCubemap() 后添加调试代码
if (previewModel) {
    std::cout << "PreviewModel material: useSH=" << previewModel->materialData.useSH 
              << ", useReflection=" << previewModel->materialData.useReflection << std::endl;
}
```

### 2. SH系数全为零
**症状**: 模型变黑，但轮廓可见
**原因**: SH 系数生成失败或全为零
**检查方法**:
```cpp
// 在 shGenPass->Generate(queue) 后添加
VkDescriptorBufferInfo shBufferInfo;
shGenPass->FeedSH(shBufferInfo);
if (shBufferInfo.buffer) {
    std::cout << "SH buffer valid, size: " << shBufferInfo.range << std::endl;
} else {
    std::cout << "ERROR: SH buffer is NULL!" << std::endl;
}
```

### 3. IBL贴图是黑色
**症状**: 模型有基本光照但反射是黑色
**原因**: IBL 贴图生成失败
**检查方法**:
```cpp
// 在 genIBL->Generate(queue) 后添加
if (mainPass->environmemts.irradianceCube.sampler) {
    std::cout << "Irradiance cube valid" << std::endl;
} else {
    std::cout << "ERROR: Irradiance cube sampler is NULL!" << std::endl;
}
```

### 4. 描述符绑定未更新
**症状**: 模型使用旧的环境数据
**原因**: `mainPass->UpdateBindings()` 未被调用或失败
**检查方法**:
```cpp
// 在 mainPass->UpdateBindings() 前后添加
std::cout << "Before UpdateBindings()" << std::endl;
mainPass->UpdateBindings();
std::cout << "After UpdateBindings()" << std::endl;
```

### 5. 捕获过程中模型被重新创建
**症状**: 模型变黑且材质参数被重置
**原因**: 在 `CaptureCubemap()` 中重新创建了模型
**检查方法**:
```cpp
// 在 CaptureCubemap() 开始时添加
std::cout << "gltfModel exists: " << (gltfModel ? "Yes" : "No") << std::endl;
std::cout << "previewModel exists: " << (previewModel ? "Yes" : "No") << std::endl;
```

## 修复方案

### 修复1: 确保材质参数在捕获后被正确设置

**文件**: `examples/lightprobesh2/main.cpp`
**位置**: `CaptureCubemap()` 函数末尾

```cpp
// ✅ 修复：在更新绑定后，强制刷新材质参数
mainPass->UpdateBindings();

// 等待GPU操作完成
vkDeviceWaitIdle(vulkanDevice->logicalDevice);

// 重新设置材质参数
if (previewModel) {
    previewModel->SetUseSHAndReflection(true, true);
    std::cout << "[CaptureCubemap] PreviewModel: Enabled SH and Reflection" << std::endl;
}
if (gltfModel) {
    gltfModel->SetUseSHAndReflection(true, true);
    std::cout << "[CaptureCubemap] GltfModel: Enabled SH and Reflection" << std::endl;
}
```

### 修复2: 在PrepareScene中初始化材质参数

**文件**: `examples/lightprobesh2/main.cpp`
**位置**: `PrepareScene()` 函数

```cpp
previewModel = std::make_unique<PreviewModel>(vulkanDevice, this);
previewModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
previewModel->UpdateModel(previewModels[modelIndex]);
// ✅ 修复：初始化材质参数
previewModel->SetUseSHAndReflection(true, false);

// ... gltfModel 创建 ...
gltfModel->SetTransform(t * s);
// ✅ 修复：初始化材质参数
gltfModel->SetUseSHAndReflection(true, false);
```

### 修复3: 验证SH和IBL数据有效性

**文件**: `examples/lightprobesh2/main.cpp`
**位置**: `CaptureCubemap()` 函数

```cpp
// 生成 SH
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);

VkDescriptorBufferInfo shBufferInfo;
shGenPass->FeedSH(shBufferInfo);
mainPass->environmemts.shCoeffs = shBufferInfo;

// ✅ 修复：验证数据有效性
if (!mainPass->environmemts.shCoeffs.buffer) {
    std::cerr << "[CaptureCubemap] ERROR: shCoeffs buffer not initialized!" << std::endl;
    return; // 不继续执行
}

// 生成 IBL
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

// ✅ 修复：验证数据有效性
if (!mainPass->environmemts.irradianceCube.sampler) {
    std::cerr << "[CaptureCubemap] ERROR: irradianceCube sampler not initialized!" << std::endl;
    return; // 不继续执行
}
if (!mainPass->environmemts.prefilteredCube.sampler) {
    std::cerr << "[CaptureCubemap] ERROR: prefilteredCube sampler not initialized!" << std::endl;
    return; // 不继续执行
}
```

### 修复4: 添加同步点

**文件**: `examples/lightprobesh2/main.cpp`
**位置**: `CaptureCubemap()` 函数

```cpp
// 捕获cubemap
probe->CaptureCubeMap(queue);

// ✅ 修复：等待捕获完成
vkDeviceWaitIdle(vulkanDevice->logicalDevice);

// 生成 SH
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);

// ✅ 修复：等待SH生成完成
vkDeviceWaitIdle(vulkanDevice->logicalDevice);

// 生成 IBL
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);

// ✅ 修复：等待IBL生成完成
vkDeviceWaitIdle(vulkanDevice->logicalDevice);

// 更新绑定
mainPass->UpdateBindings();

// ✅ 修复：等待绑定更新完成
vkDeviceWaitIdle(vulkanDevice->logicalDevice);
```

## 调试步骤

### 步骤1: 添加调试输出
在 `CaptureCubemap()` 函数中添加以下调试代码：

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    std::cout << "\n=== CaptureCubemap START ===" << std::endl;
    std::cout << "Position: (" << position.x << ", " << position.y << ", " << position.z << ")" << std::endl;
    
    // ... 现有代码 ...
    
    // 在每个关键步骤后添加输出
    std::cout << "[1] Probe created and configured" << std::endl;
    probe->CaptureCubeMap(queue);
    std::cout << "[2] Cubemap captured" << std::endl;
    
    shGenPass->Generate(queue);
    std::cout << "[3] SH generated" << std::endl;
    
    genIBL->Generate(queue);
    std::cout << "[4] IBL generated" << std::endl;
    
    mainPass->UpdateBindings();
    std::cout << "[5] Bindings updated" << std::endl;
    
    if (previewModel) {
        previewModel->SetUseSHAndReflection(true, true);
        std::cout << "[6] PreviewModel material updated" << std::endl;
    }
    
    if (gltfModel) {
        gltfModel->SetUseSHAndReflection(true, true);
        std::cout << "[7] GltfModel material updated" << std::endl;
    }
    
    std::cout << "=== CaptureCubemap END ===\n" << std::endl;
}
```

### 步骤2: 检查材质buffer
在 `SetUseSHAndReflection()` 中添加验证：

```cpp
void PreviewModel::SetUseSHAndReflection(bool useSH, bool useReflection)
{
    materialData.useSH = useSH ? 1 : 0;
    materialData.useReflection = useReflection ? 1 : 0;
    
    if (materialBuffer.mapped) {
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
        std::cout << "[SetUseSHAndReflection] Updated: useSH=" << materialData.useSH 
                  << ", useReflection=" << materialData.useReflection << std::endl;
    } else {
        std::cerr << "[SetUseSHAndReflection] ERROR: materialBuffer not mapped!" << std::endl;
    }
}
```

### 步骤3: 验证着色器输入
在片段着色器中添加调试输出（如果支持）：

```glsl
// 在 gltfmesh_main.frag 中
void main() {
    // 调试：如果 useSH 为 0，输出红色
    if (material.useSH == 0) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // 调试：如果 useReflection 为 0，输出绿色
    if (material.useReflection == 0) {
        outColor = vec4(0.0, 1.0, 0.0, 1.0);
        return;
    }
    
    // 正常渲染
    // ...
}
```

## 预期结果

修复后，应该看到以下输出：

```
=== CaptureCubemap START ===
Position: (0, 2, 0)
[1] Probe created and configured
[2] Cubemap captured
[CaptureCubemap] Generating SH coefficients...
[CaptureCubemap] SH buffer: Valid
[3] SH generated
[CaptureCubemap] Generating IBL maps...
[CaptureCubemap] Irradiance sampler: Valid
[CaptureCubemap] Prefiltered sampler: Valid
[4] IBL generated
[5] Bindings updated
[SetUseSHAndReflection] Updated: useSH=1, useReflection=1
[6] PreviewModel material updated
[SetUseSHAndReflection] Updated: useSH=1, useReflection=1
[7] GltfModel material updated
[CaptureCubemap] Capture complete! Models should now use captured lighting.
=== CaptureCubemap END ===
```

## 常见问题

### Q1: 模型仍然是黑色
**A**: 检查 SH 系数是否全为零。可以在 `GenSHComputePass::Generate()` 后读取 buffer 内容验证。

### Q2: 模型有光照但反射是黑色
**A**: 检查 IBL 贴图是否正确生成。可以保存 irradiance 和 prefiltered cubemap 到文件验证。

### Q3: 只有第一次捕获有效
**A**: 检查是否在每次捕获后都调用了 `mainPass->UpdateBindings()`。

### Q4: 模型闪烁
**A**: 添加更多的同步点（`vkDeviceWaitIdle`）确保GPU操作完成。

## 下一步

如果以上修复都无效，请：
1. 运行程序并收集调试输出
2. 检查是否有 Vulkan 验证层错误
3. 使用 RenderDoc 捕获帧并检查描述符绑定
4. 验证着色器中的 uniform buffer 数据

