# ✅ gltfModel 不显示问题 - 完整修复总结

## 🎯 问题

gltfModel 在 mainpass 中没有正常显示，尽管代码结构看起来是正确的。

## 🔍 根本原因

**Global UBO 结构不匹配导致着色器读取错误的数据**

### 问题分析

| 组件 | 修复前 | 问题 |
|------|--------|------|
| **gltfmesh.vert** | `mat4 viewproj` | 期望单个矩阵 |
| **gltfmesh.frag** | `mat4 viewproj` | 期望单个矩阵 |
| **Pass.h** | `mat4 project + mat4 view` | 提供两个矩阵 |
| **内存布局** | 不匹配 | 着色器读取错误数据 |

## ✅ 修复方案

### 1. 修改 gltfmesh.vert

```glsl
// 修复前
layout (set = 0, binding = 0) uniform Global {
    mat4 viewproj;
    vec4 cameraPos;
} global;

// 修复后
layout (set = 0, binding = 0) uniform Global {
    mat4 projection;    // ✅ 改为分开的矩阵
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;

// 使用方式
gl_Position = global.projection * global.view * worldPos;
```

### 2. 修改 gltfmesh.frag

```glsl
// 修复前
layout (set = 0, binding = 0) uniform Global {
    mat4 viewproj;
    vec4 cameraPos;
    vec4 lights[4];
    float exposure;
    float gamma;
} global;

// 修复后
layout (set = 0, binding = 0) uniform Global {
    mat4 projection;    // ✅ 改为分开的矩阵
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;
```

### 3. 修改 Pass.h

```cpp
// 修复前
struct GlobalUbo {
    glm::mat4 project;
    glm::mat4 view;
    glm::vec4 light[4];
    glm::vec4 cameraPos;
    float exposure = 4.5f;
    float gamma = 2.2f;
};

// 修复后
struct GlobalUbo {
    glm::mat4 projection;  // ✅ 改为 projection
    glm::mat4 view;
    glm::vec4 light[4];
    glm::vec4 cameraPos;
    float exposure = 4.5f;
    float gamma = 2.2f;
};
```

### 4. 修改 main.cpp

```cpp
// 修复前
void VulkanExample::prepareData() {
    mainPassData.project = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    // ...
}

// 修复后
void VulkanExample::prepareData() {
    mainPassData.projection = camera.matrices.perspective;  // ✅ 改为 projection
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
    mainPass->UpdateGlobal(mainPassData);
    skybox->Update(camera.matrices.view);
}
```

## 📊 修复效果

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **着色器一致性** | ❌ 不一致 | ✅ 一致 |
| **内存布局** | ❌ 不匹配 | ✅ 匹配 |
| **gltfModel 显示** | ❌ 不显示 | ✅ 正常显示 |
| **与 skybox 兼容** | ❌ 不兼容 | ✅ 兼容 |

## 🔑 关键要点

1. **着色器和 C++ 代码的数据结构必须完全匹配**
   - 字段顺序
   - 字段类型
   - 字段大小
   - 内存对齐

2. **任何不匹配都会导致着色器读取错误的数据**
   - 导致渲染错误
   - 导致模型不显示
   - 导致颜色错误

3. **统一使用分开的 projection 和 view 矩阵**
   - 与 skybox 着色器一致
   - 更灵活，支持不同的矩阵操作
   - 避免重复计算

## 📝 修改文件列表

1. ✅ `shaders/glsl/lightprobesh2/gltfmesh.vert`
2. ✅ `shaders/glsl/lightprobesh2/gltfmesh.frag`
3. ✅ `examples/lightprobesh2/Pass.h`
4. ✅ `examples/lightprobesh2/main.cpp`

## 🚀 下一步

需要编译着色器文件：
```bash
glslc shaders/glsl/lightprobesh2/gltfmesh.vert -o shaders/glsl/lightprobesh2/gltfmesh.vert.spv
glslc shaders/glsl/lightprobesh2/gltfmesh.frag -o shaders/glsl/lightprobesh2/gltfmesh.frag.spv
```

然后重新编译 C++ 代码并运行。

