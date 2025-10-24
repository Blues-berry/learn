# 🔧 gltfModel 不显示问题 - 根本原因和修复

## 🐛 问题描述

gltfModel 在 mainpass 中没有正常显示，虽然代码结构看起来是正确的。

## 🔍 根本原因

**Global UBO 结构不匹配**

着色器和 C++ 代码中的 Global UBO 结构定义不一致：

### ❌ 问题代码

**gltfmesh.vert (修复前)**:
```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 viewproj;      // ← 单个矩阵
    vec4 cameraPos;
} global;
```

**Pass.h (修复前)**:
```cpp
struct GlobalUbo {
    glm::mat4 project;   // ← 分开的矩阵
    glm::mat4 view;
    glm::vec4 cameraPos;
    // ...
};
```

**问题**:
- 着色器期望 `viewproj` (64 字节)
- C++ 提供 `project + view` (128 字节)
- **内存布局完全不匹配！** → 着色器读取错误的数据

### ✅ 修复方案

统一使用分开的 `projection` 和 `view` 矩阵（与 skybox 着色器一致）

#### 1. 修改 gltfmesh.vert

```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 projection;    // ✅ 改为分开的矩阵
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;

void main()
{
    // ...
    gl_Position = global.projection * global.view * worldPos;  // ✅ 使用分开的矩阵
}
```

#### 2. 修改 gltfmesh.frag

```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 projection;    // ✅ 改为分开的矩阵
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;
```

#### 3. 修改 Pass.h

```cpp
struct GlobalUbo {
    glm::mat4 projection;  // ✅ 改为分开的矩阵
    glm::mat4 view;
    glm::vec4 light[4];
    glm::vec4 cameraPos;
    float exposure = 4.5f;
    float gamma = 2.2f;
};
```

#### 4. 修改 main.cpp

```cpp
void VulkanExample::prepareData()
{
    // ✅ 使用分开的矩阵
    mainPassData.projection = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);

    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

## 📊 修复前后对比

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **gltfmesh.vert** | `mat4 viewproj` | `mat4 projection + mat4 view` |
| **gltfmesh.frag** | `mat4 viewproj` | `mat4 projection + mat4 view` |
| **Pass.h** | `project + view` | `projection + view` |
| **main.cpp** | 计算 viewproj | 分开设置 projection 和 view |
| **一致性** | ❌ 不一致 | ✅ 与 skybox 一致 |

## ✅ 修复效果

- ✅ gltfModel 现在能正常显示
- ✅ Global UBO 结构与所有着色器一致
- ✅ 内存布局正确匹配
- ✅ 与 skybox 和 previewModel 使用相同的 UBO 结构

## 🎯 关键要点

**着色器和 C++ 代码中的数据结构必须完全匹配**，包括：
1. 字段顺序
2. 字段类型
3. 字段大小
4. 内存对齐

任何不匹配都会导致着色器读取错误的数据，造成渲染问题。

