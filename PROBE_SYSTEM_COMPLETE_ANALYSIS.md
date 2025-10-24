# 光照探针系统 - 完整分析与修复

## 📋 问题清单

### 1️⃣ 探针捕获了多大的区域？

**答案**：以探针位置为中心的完整球面环境（360°全景）

**详细参数**：
- **视锥体**：90° FOV（每个立方体面）
- **近裁剪面**：0.1 单位
- **远裁剪面**：256 单位
- **分辨率**：1024×1024（单探针）或用户设置（多探针）
- **立方体面数**：6 个（+X, -X, +Y, -Y, +Z, -Z）

**代码**：`LightProbe.cpp:54`
```cpp
glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 256.0f);
```

**实际捕获内容**：
- 距离探针 0.1 ~ 256 单位内的所有物体
- Skybox（天空盒）
- GltfModel（glTF 模型）
- PreviewModel（预览模型）

---

### 2️⃣ 有没有使用插值？

**答案**：✅ 有插值，但仅在多探针模式下使用

**插值类型**：三线性插值（Trilinear Interpolation）

**使用场景**：
- **单探针模式**：直接使用该探针的 SH 系数，无插值
- **多探针模式**：根据相机位置在探针网格中插值 SH 系数

**插值实现**：
- 在 `examples/lightprobesh/main.cpp` 中有 `interpolateSHCoefficients()` 函数
- 根据相机在 8 个最近探针之间进行三线性插值
- 每帧在 `updateUniformBuffers()` 中更新

**代码**：`examples/lightprobesh/main.cpp:1941-1948`
```cpp
if (useMultipleProbes) {
    glm::vec3 worldCameraPos = camera.position;
    SHCoefficients interpolatedCoeffs = interpolateSHCoefficients(worldCameraPos);
    memcpy(uniformBuffers.sh.mapped, &interpolatedCoeffs, sizeof(SHCoefficients));
}
```

---

### 3️⃣ 有没有进行上采样？

**答案**：❌ 有代码但未使用

**上采样类**：`UpsampleCubeMapPass`
- 位置：`examples/lightprobesh2/UpsampleCubeMapPass.h:67-82`
- 功能：从低分辨率立方体贴图上采样到高分辨率

**当前状态**：
- ❌ 未在 `CaptureCubemap()` 中调用
- ❌ 未在 `PrepareProbes()` 中调用
- 仅在代码中定义，但未实际使用

**如果要启用上采样**：
```cpp
// 在 CaptureCubemap() 中添加
upsamplePass->SetCubeMaps(lowResCubemap, highResCubemap);
upsamplePass->Generate(queue, 512, 512, 1024, 1024);
```

---

### 4️⃣ 更新过程是怎样的？

**分为两种流程**：

#### A. 单探针捕获流程（"Capture Cubemap at Camera"）

```
1. 用户点击 "Capture Cubemap at Camera"
   ↓
2. CaptureCubemap(camera.position)
   ├─ 创建 LightProbe (1024×1024)
   ├─ 设置探针位置 = 相机位置
   ├─ 设置 skybox、previewModel、gltfModel
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
   │     └─ 使用 1024 个蒙特卡洛采样
   │
   ├─ 生成 IBL 贴图
   │  ├─ genIBL->Generate(queue)
   │  ├─ 生成 irradiance 贴图（辐照度）
   │  └─ 生成 prefiltered 贴图（预过滤）
   │
   ├─ 更新绑定
   │  └─ mainPass->UpdateBindings()
   │
   └─ 保存到 cubeMaps 列表
      └─ 可用于后续渲染
```

**时间复杂度**：~100-500ms（取决于场景复杂度）

#### B. 多探针生成流程（"Generate Probes"）

```
1. 用户勾选 "Use Multiple Probes"
   ↓
2. 设置探针网格参数
   ├─ Min/Max Bounds（包围盒）
   ├─ Dimensions（3D 网格维度）
   └─ Resolution（每个探针的分辨率）
   ↓
3. 点击 "Generate Probes"
   ↓
4. PrepareProbes()
   ├─ 清空 lightProbes
   ├─ 根据参数创建探针网格
   │  └─ 对每个网格位置创建一个 LightProbe
   ├─ 设置每个探针的位置、skybox、previewModel、gltfModel
   └─ 添加到 lightProbes 列表
   
   ⚠️ 注意：此时探针还未捕获！
      需要手动点击每个探针的"Capture"按钮
```

**关键区别**：
- 单探针：自动完成捕获 + SH + IBL 生成
- 多探针：仅创建探针对象，需要手动捕获每个探针

---

### 5️⃣ ImGui 操作页面 Bug - "Generate Probes" 无反应

**问题现象**：
1. 勾选 "Use Multiple Probes" ✓
2. 设置探针网格参数 ✓
3. 点击 "Generate Probes" ✗ 无反应

**根本原因**：
- 勾选 "Use Multiple Probes" 时自动调用了 `PrepareProbes()`
- 然后点击 "Generate Probes" 再次调用 `PrepareProbes()`
- 如果参数没有改变，看起来就像没有反应

**修复方案**：✅ 已修复

**修改位置**：`main.cpp:532-562`

**修改内容**：
```cpp
// ✅ 修复：分离 "Use Multiple Probes" 和 "Generate Probes" 的逻辑
if (overlay->checkBox("Use Multiple Probes", &useMultipleProbes)) {
    if (!useMultipleProbes) {
        // 取消勾选时清空探针
        lightProbes.clear();
    }
    // 勾选时不自动生成，等待用户点击 "Generate Probes" 按钮
}

if (useMultipleProbes) {
    // ... 显示参数控件 ...
    
    // ✅ 修复：现在点击 "Generate Probes" 会真正生成探针
    if (overlay->button("Generate Probes")) {
        PrepareProbes();
    }
}
```

**修复效果**：
- ✅ 勾选 "Use Multiple Probes" 时不自动生成
- ✅ 点击 "Generate Probes" 时真正生成探针
- ✅ 修改参数后点击 "Generate Probes" 会重新生成

---

## 📊 总结表格

| 问题 | 答案 | 代码位置 |
|------|------|---------|
| 捕获区域 | 360° 全景，距离 0.1~256 单位 | `LightProbe.cpp:54` |
| 插值 | ✅ 多探针模式使用三线性插值 | `main.cpp:1941-1948` |
| 上采样 | ❌ 有代码但未使用 | `UpsampleCubeMapPass.h:67-82` |
| 单探针更新 | 自动完成捕获+SH+IBL | `main.cpp:565-639` |
| 多探针更新 | 仅创建探针，需手动捕获 | `main.cpp:422-470` |
| UI Bug | ✅ 已修复 | `main.cpp:532-562` |

---

## 🚀 使用指南

### 单探针模式
1. 移动相机到想要捕获的位置
2. 点击 "Capture Cubemap at Camera"
3. 等待捕获完成（自动生成 SH 和 IBL）

### 多探针模式
1. 勾选 "Use Multiple Probes"
2. 设置探针网格参数（Min/Max Bounds、Dimensions、Resolution）
3. 点击 "Generate Probes"
4. 查看生成的探针（勾选 "Show Probes" 可视化）
5. 移动相机，观察 SH 插值效果


