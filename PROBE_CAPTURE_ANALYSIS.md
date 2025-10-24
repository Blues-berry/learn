# 光照探针捕获系统 - 详细分析

## 问题 1: 探针捕获了多大的区域？

### 答案：
探针捕获的是**以探针位置为中心的完整球面环境**（360°全景）

### 详细说明：

**捕获范围**：
- **视锥体**：90° FOV（每个面）
- **近裁剪面**：0.1 单位
- **远裁剪面**：256 单位
- **分辨率**：1024×1024（默认）或用户设置的分辨率

**代码位置**：`LightProbe.cpp:54`
```cpp
glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 256.0f);
```

**6 个立方体面**：
```cpp
// 从探针位置看向 6 个方向
glm::lookAt(position, position + glm::vec3( 1, 0, 0), ...) // +X
glm::lookAt(position, position + glm::vec3(-1, 0, 0), ...) // -X
glm::lookAt(position, position + glm::vec3( 0, 1, 0), ...) // +Y
glm::lookAt(position, position + glm::vec3( 0,-1, 0), ...) // -Y
glm::lookAt(position, position + glm::vec3( 0, 0, 1), ...) // +Z
glm::lookAt(position, position + glm::vec3( 0, 0,-1), ...) // -Z
```

**实际捕获区域**：
- 距离探针 0.1 ~ 256 单位内的所有物体
- 包括 skybox、gltfModel、previewModel 等

---

## 问题 2: 有没有使用插值？

### 答案：
**有插值，但仅在多探针模式下使用**

### 详细说明：

**单探针模式**：
- 直接使用该探针的 SH 系数
- 无插值

**多探针模式**：
- ✅ **使用三线性插值**
- 根据相机位置在探针网格中插值 SH 系数

**代码位置**：`main.cpp:533-539`
```cpp
if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
    if (useMultipleProbes) {
        PrepareProbes();  // 创建探针网格
    } else {
        lightProbes.clear();
    }
}
```

**插值实现**：
- 在 `examples/lightprobesh/main.cpp` 中有 `interpolateSHCoefficients()` 函数
- 根据相机位置在 8 个最近的探针之间进行三线性插值

---

## 问题 3: 有没有进行上采样？

### 答案：
**有上采样功能，但目前未被使用**

### 详细说明：

**上采样类**：`UpsampleCubeMapPass`
- 位置：`examples/lightprobesh2/UpsampleCubeMapPass.h:67-82`
- 功能：从低分辨率立方体贴图上采样到高分辨率

**代码**：
```cpp
class UpsampleCubeMapPass : public ComputePass {
public:
    void SetCubeMaps(const std::shared_ptr<vks::TextureCubeMap>& lowResCube, 
                     const std::shared_ptr<vks::TextureCubeMap>& highResCube);
    void Generate(VkQueue queue, uint32_t lowResWidth, uint32_t lowResHeight, 
                  uint32_t highResWidth, uint32_t highResHeight);
};
```

**当前状态**：
- ❌ 未在 `CaptureCubemap()` 中调用
- ❌ 未在 `PrepareProbes()` 中调用
- 仅在代码中定义，但未实际使用

**如果要启用上采样**：
需要在 `CaptureCubemap()` 或 `PrepareProbes()` 中添加：
```cpp
upsamplePass->SetCubeMaps(lowResCubemap, highResCubemap);
upsamplePass->Generate(queue, 512, 512, 1024, 1024);
```

---

## 问题 4: 更新过程是怎样的？

### 答案：
分为 **单探针捕获** 和 **多探针生成** 两种流程

### 单探针捕获流程（"Capture Cubemap at Camera"）：

```
1. CaptureCubemap(camera.position)
   ├─ 创建 LightProbe (1024×1024)
   ├─ 设置探针位置 = 相机位置
   ├─ 设置 skybox、previewModel、gltfModel
   │
   ├─ 准备 PSO（如果需要）
   │
   ├─ probe->CaptureCubeMap(queue)
   │  ├─ 准备 UBO（6 个视图投影矩阵）
   │  ├─ 执行 drawScene()
   │  │  └─ 渲染 skybox + gltfModel 到立方体贴图
   │  ├─ 布局转换到 SHADER_READ_ONLY_OPTIMAL
   │  └─ 返回立方体贴图
   │
   ├─ 生成 SH 系数
   │  └─ shGenPass->Generate(queue)
   │
   ├─ 生成 IBL 贴图
   │  ├─ genIBL->Generate(queue)
   │  ├─ 生成 irradiance 贴图
   │  └─ 生成 prefiltered 贴图
   │
   ├─ 更新绑定
   │  └─ mainPass->UpdateBindings()
   │
   └─ 保存到 cubeMaps 列表
```

### 多探针生成流程（"Generate Probes"）：

```
1. PrepareProbes()
   ├─ 清空 lightProbes
   ├─ 根据 probeGridConfig 创建探针网格
   │  ├─ minBounds、maxBounds（包围盒）
   │  ├─ dimensions（3D 网格维度）
   │  └─ resolution（每个探针的分辨率）
   │
   ├─ 对每个探针位置：
   │  ├─ 创建 LightProbe
   │  ├─ 设置位置
   │  ├─ 设置 skybox、previewModel、gltfModel
   │  └─ 添加到 lightProbes 列表
   │
   └─ 注意：此时探针还未捕获！
      需要手动点击每个探针的"Capture"按钮
```

**关键区别**：
- 单探针：自动完成捕获 + SH + IBL 生成
- 多探针：仅创建探针对象，需要手动捕获每个探针

---

## 问题 5: ImGui 操作页面 Bug - "Generate Probes" 无反应

### 问题分析：

**现象**：
1. 勾选 "Use Multiple Probes" ✓
2. 设置探针网格参数 ✓
3. 点击 "Generate Probes" ✗ 无反应

### 根本原因：

**代码位置**：`main.cpp:533-556`

```cpp
if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
    if (useMultipleProbes) {
        PrepareProbes();  // ← 这里已经调用过了
    } else {
        lightProbes.clear();
    }
}

if (useMultipleProbes) {  // ← 这个条件成立
    // ... 显示参数控件 ...
    if (overlay->button("Generate Probes")) {
        PrepareProbes();  // ← 再次调用
    }
}
```

**问题**：
- 当勾选 "Use Multiple Probes" 时，已经调用了 `PrepareProbes()`
- 此时 `useMultipleProbes = true`
- 然后显示参数控件，但参数修改后点击 "Generate Probes" 时，`PrepareProbes()` 再次调用
- **但是，如果参数没有改变，看起来就像没有反应**

### 解决方案：

修改 UI 逻辑，使 "Generate Probes" 按钮在参数改变时才有效：

```cpp
if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
    if (useMultipleProbes) {
        // 不在这里调用 PrepareProbes()
        // PrepareProbes();  // ← 删除这行
    } else {
        lightProbes.clear();
    }
}

if (useMultipleProbes) {
    // ... 显示参数控件 ...
    if (overlay->button("Generate Probes")) {
        PrepareProbes();  // ← 只在这里调用
    }
}
```

---

## 总结表格

| 问题 | 答案 |
|------|------|
| 捕获区域 | 以探针为中心的 360° 全景，距离 0.1~256 单位 |
| 插值 | 仅多探针模式使用三线性插值 |
| 上采样 | 有代码但未使用 |
| 更新流程 | 单探针自动完成，多探针需手动捕获 |
| UI Bug | "Generate Probes" 逻辑重复，需要修改 |


