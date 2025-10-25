# 探针可视化和立方体贴图插值功能实现

## 📋 概述

本文档描述了为 lightprobesh2 示例添加的新功能：
1. **探针可视化** - 在场景中可视化显示光照探针
2. **立方体贴图插值** - 在多个探针之间进行立方体贴图插值，生成完整的场景立方体贴图

---

## ✨ 新增功能

### 1. 探针可视化

**功能描述**：
- 在场景中以球体形式显示光照探针的位置
- 可通过 UI 复选框 "Show Probes" 切换显示/隐藏

**实现方式**：
- 使用 PreviewModel 在探针位置渲染球体
- 在 `drawFrame()` 中调用 `probe->Draw()` 方法
- 支持在 MainPass 中渲染探针

**代码位置**：
- `main.cpp` - `drawFrame()` 函数 (第 556-560 行)
- `LightProbe.h` - `Draw()` 方法 (第 36-40 行)

---

### 2. 立方体贴图插值

**功能描述**：
- 在多个探针捕获的立方体贴图之间进行插值
- 使用反距离加权 (IDW) 算法计算插值权重
- 生成完整的场景立方体贴图

**核心类**：`CubemapInterpolation`

**主要方法**：
```cpp
// 添加探针数据
void AddProbe(const glm::vec3& position, 
              const std::shared_ptr<vks::TextureCubeMap>& cubemap);

// 在指定位置进行插值
std::shared_ptr<vks::TextureCubeMap> InterpolateAt(
    const glm::vec3& position, 
    float maxDistance = 50.0f);

// 清空所有探针
void ClearProbes();
```

**插值算法**：
- 使用反距离加权 (IDW) 方法
- 权重 = 1 / (distance + epsilon)
- 权重归一化处理
- 当没有有效探针时，使用最近的探针

**文件**：
- `CubemapInterpolation.h` - 类定义
- `CubemapInterpolation.cpp` - 实现

---

## 🔧 集成到主程序

### 1. 头文件包含

在 `main.cpp` 中添加：
```cpp
#include "CubemapInterpolation.h"
```

### 2. 成员变量

在 `VulkanExample` 类中添加：
```cpp
std::unique_ptr<CubemapInterpolation> cubemapInterpolation;
```

### 3. 初始化

在 `PrepareProbes()` 中初始化：
```cpp
if (!cubemapInterpolation) {
    cubemapInterpolation = std::make_unique<CubemapInterpolation>(vulkanDevice);
} else {
    cubemapInterpolation->ClearProbes();
}
```

### 4. 添加探针数据

在 `CaptureAllProbes()` 中添加：
```cpp
if (cubemapInterpolation) {
    cubemapInterpolation->AddProbe(p->GetPosition(), capturedCubemap);
}
```

### 5. UI 控制

在 `OnUpdateUIOverlay()` 中添加按钮：
```cpp
if (overlay->button("Interpolate Cubemap")) {
    if (cubemapInterpolation && cubemapInterpolation->GetProbeCount() > 0) {
        auto interpolatedCubemap = cubemapInterpolation->InterpolateAt(
            camera.position, 50.0f);
        if (interpolatedCubemap) {
            cubeMaps.push_back(interpolatedCubemap);
            cubemapNames.push_back("Interpolated_" + 
                std::to_string(cubeMaps.size() - 1));
            skyboxIndex = static_cast<int>(cubeMaps.size() - 1);
            UpdateSkyBox();
        }
    }
}
```

---

## 📝 使用流程

### 步骤 1: 生成探针网格
1. 勾选 "Use Multiple Probes"
2. 调整探针网格参数（最小/最大边界、维度）
3. 点击 "Generate Probes" 按钮

### 步骤 2: 捕获所有探针
1. 点击 "Capture All Probes" 按钮
2. 系统会自动捕获所有探针的立方体贴图
3. 探针数据被添加到插值系统

### 步骤 3: 可视化探针
1. 勾选 "Show Probes" 复选框
2. 场景中会显示所有探针的位置（球体）

### 步骤 4: 进行插值
1. 移动相机到目标位置
2. 点击 "Interpolate Cubemap" 按钮
3. 系统会在当前相机位置进行立方体贴图插值
4. 新的插值立方体贴图会被添加到天空盒列表中

---

## 🔍 关键改动

### 文件修改

| 文件 | 改动 |
|------|------|
| `main.cpp` | 添加插值对象、初始化、UI 按钮 |
| `LightProbe.h` | 添加 `GetPosition()` 方法 |
| `CubemapInterpolation.h` | 新文件 - 插值类定义 |
| `CubemapInterpolation.cpp` | 新文件 - 插值实现 |

### 新增 UI 按钮

- **"Capture All Probes"** - 捕获所有探针的立方体贴图
- **"Interpolate Cubemap"** - 在当前相机位置进行插值
- **"Show Probes"** - 切换探针可视化显示

---

## 🎯 功能特性

✅ **探针可视化**
- 以球体形式显示探针位置
- 支持切换显示/隐藏
- 实时渲染

✅ **立方体贴图插值**
- 反距离加权算法
- 支持多探针插值
- 自动权重归一化
- 距离限制参数

✅ **完整的场景立方体贴图**
- 通过插值生成完整的环境贴图
- 支持在任意位置进行插值
- 插值结果可用于天空盒和 IBL

---

## 📊 编译信息

- **编译状态**: ✅ 成功
- **警告**: 4 个浮点转换警告（来自 Pass.cpp，不影响功能）
- **输出**: `lightprobesh2.exe`

---

## 🚀 下一步改进

1. **GPU 加速插值** - 在 GPU 上进行像素级立方体贴图插值
2. **高级插值算法** - 支持三线性插值、球面插值等
3. **插值可视化** - 显示插值权重分布
4. **性能优化** - 缓存插值结果

---

## 📚 相关文件

- `examples/lightprobesh2/main.cpp` - 主程序
- `examples/lightprobesh2/LightProbe.h/cpp` - 光照探针类
- `examples/lightprobesh2/CubemapInterpolation.h/cpp` - 插值类
- `examples/lightprobesh2/PreviewModel.h/cpp` - 预览模型类


