# 立方体贴图捕获范围问题 - 诊断与修复

## 问题描述

超出一定范围后，天空盒只能捕获一个面，其他面变成黑色或不可见。

## 根本原因分析

### 问题 1: 投影矩阵的裁剪范围

**文件**: `LightProbe.cpp` 第 54 行

```cpp
glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 256.0f);
```

**问题**:
- 近裁剪面: 0.1
- 远裁剪面: 256.0
- 当天空盒超出这个范围时，会被裁剪

**但这不是主要问题**，因为天空盒应该是无限远的。

### 问题 2: 天空盒位置不随探针移动 ✅ 主要问题

**文件**: `LightProbe.cpp` 第 23-36 行

**原始代码**:
```cpp
void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
        // ...
    });
}
```

**问题**:
- 没有调用 `skybox->Update()` 来更新天空盒的位置
- 天空盒仍然使用上一次的视图矩阵（来自主渲染通道）
- 当探针移动到远处时，天空盒的位置不变，导致只能看到一个面

### 问题 3: 天空盒的 Update 方法

**文件**: `Skybox.cpp` 第 172-177 行

```cpp
void Skybox::Update(const glm::mat4& view)
{
    LocalBuffer empty = {};
    empty.transform = glm::mat3(view);  // 只取旋转部分
    memcpy(localBuffer.mapped, &empty, sizeof(LocalBuffer));
}
```

**说明**:
- 天空盒只使用视图矩阵的 3×3 部分（旋转）
- 这是正确的做法，因为天空盒应该始终围绕相机
- 但需要在捕获时为每个探针位置更新

## 修复方案

### 修复: 在捕获时更新天空盒位置 ✅

**文件**: `LightProbe.cpp` 第 23-49 行

**修复内容**:
```cpp
void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    // ✅ 修复：为捕获时的天空盒更新位置
    if (skybox) {
        // 创建一个以探针位置为中心的视图矩阵
        glm::mat4 skyboxView = glm::lookAt(
            position,                           // 相机位置 = 探针位置
            position + glm::vec3(0, 0, -1),    // 看向 -Z 方向
            glm::vec3(0, 1, 0)                 // Y 轴向上
        );
        skybox->Update(skyboxView);
    }

    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
        if (gltfModel) {
            gltfModel->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
    });
}
```

**原因**:
- 天空盒需要知道探针的位置
- 通过创建一个以探针位置为中心的视图矩阵
- 天空盒就能正确地围绕探针进行渲染
- 这样无论探针在哪里，都能捕获完整的 6 个面

## 工作流程

### 捕获前
```
探针位置: (x, y, z)
天空盒位置: 上一次的位置（错误！）
```

### 捕获后（修复）
```
探针位置: (x, y, z)
天空盒位置: 更新为 (x, y, z)（正确！）
↓
为 6 个面创建视图矩阵
↓
从探针位置看向各方向
↓
完整捕获 6 个面
```

## 相关参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 投影 FOV | 90° | 每个面的视角 |
| 近裁剪面 | 0.1 | 最近距离 |
| 远裁剪面 | 256.0 | 最远距离 |
| 天空盒 FOV | 无限 | 天空盒应该无限远 |

## 验证修复

修复后，应该：
1. ✅ 任何位置都能捕获完整的 6 个面
2. ✅ 天空盒始终围绕探针
3. ✅ 多探针捕获时每个探针都能看到完整的环境
4. ✅ 没有黑色面或缺失的面

## 调试建议

如果仍然有问题，检查：
1. 天空盒模型是否正确加载
2. 投影矩阵的远裁剪面是否足够大
3. 天空盒的着色器是否正确处理坐标
4. 立方体贴图是否正确绑定

## 相关代码位置

| 文件 | 行号 | 说明 |
|------|------|------|
| `LightProbe.cpp` | 23-49 | drawScene 函数（修复） |
| `Skybox.cpp` | 172-177 | Update 方法 |
| `LightProbe.cpp` | 54 | 投影矩阵 |


