# 立方体贴图反射方向反转问题 - 诊断与修复

## 问题描述

启用反射（useReflect）后，前后左右方向反了。这通常发生在：
1. 单探针捕获时正常
2. 多探针捕获后异常

## 根本原因分析

### 1. 坐标系不一致

**捕获阶段** (`LightProbe.cpp`)：
- 使用 6 个视图矩阵渲染到立方体贴图的 6 个面
- 每个面对应一个方向（+X, -X, +Y, -Y, +Z, -Z）

**IBL 处理阶段** (`Pass.cpp` - GenIBLCubeMipPass::PrepareData)：
- 使用旋转矩阵生成 MVP 矩阵
- 这些矩阵必须与捕获时的视图矩阵完全一致

**采样阶段** (着色器)：
- `irradiancecube.frag` 第 38 行：`sampleVector.y = -sampleVector.y;`
- `prefilterenvmap.frag` 第 97 行：`L.y = -L.y;`
- 这表明采样时需要翻转 Y 坐标

### 2. 关键发现

在 `irradiancecube.frag` 中：
```glsl
sampleVector.y = -sampleVector.y;  // Y 坐标翻转
color += texture(samplerEnv, sampleVector).rgb * cos(theta) * sin(theta);
```

这个翻转是为了适配 Vulkan 的坐标系（Y 轴向下）与立方体贴图采样坐标系的差异。

### 3. 多探针问题

当使用多探针时，如果：
1. 捕获时的视图矩阵与 IBL 处理时的视图矩阵不一致
2. 或者 SH/IBL 生成没有正确执行

就会导致反射方向错误。

## 修复方案

### 修复 1: 统一视图矩阵 ✅

**文件**: `LightProbe.cpp` (第 56-65 行)

**问题**: 原来使用 `glm::lookAt()` 生成视图矩阵

**修复**: 改用与 IBL 生成相同的旋转矩阵方式

```cpp
// ✅ 修复：使用与 IBL 生成相同的视图矩阵方式（旋转矩阵而非 lookAt）
std::array<glm::mat4, 6> viewMatrices = {
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // +X
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // -X
    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // +Y
    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // -Y
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // +Z
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f))  // -Z
};
```

**原因**: 这确保捕获的立方体贴图与 IBL 处理的坐标系一致

### 修复 2: 为多探针生成 SH 和 IBL ✅

**文件**: `main.cpp` - `CaptureAllProbes()` 函数

**问题**: 原来只捕获立方体贴图，没有生成 SH 和 IBL

**修复**: 为每个探针生成 SH 系数和 IBL 贴图

```cpp
// 生成 SH 系数
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);

// 生成 IBL 贴图
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
```

**原因**: 
- SH 系数用于漫反射光照
- IBL 贴图（irradiance + prefiltered）用于镜面反射
- 没有这些，反射会使用错误的数据

## 立方体贴图面顺序

Vulkan 中立方体贴图的标准面顺序（数组层）：
```
Layer 0: +X (右)
Layer 1: -X (左)
Layer 2: +Y (上)
Layer 3: -Y (下)
Layer 4: +Z (前)
Layer 5: -Z (后)
```

## 验证修复

修复后，应该：
1. ✅ 前后左右方向正确
2. ✅ 反射效果正常
3. ✅ 多探针捕获与单探针捕获结果一致

## 相关代码位置

| 文件 | 行号 | 说明 |
|------|------|------|
| `LightProbe.cpp` | 56-65 | 视图矩阵定义 |
| `main.cpp` | 476-529 | CaptureAllProbes 函数 |
| `Pass.cpp` | 719-736 | IBL MVP 矩阵定义 |
| `irradiancecube.frag` | 38 | Y 坐标翻转 |
| `prefilterenvmap.frag` | 97 | Y 坐标翻转 |

## 调试建议

如果反射仍然有问题，检查：
1. 立方体贴图是否正确保存（使用 SaveCubeMapFaces）
2. SH 系数是否正确生成
3. IBL 贴图是否正确生成
4. 着色器中的采样坐标是否正确


