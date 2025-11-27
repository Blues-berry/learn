# PRT (Precomputed Radiance Transfer) 实现指南

## 概述

本项目实现了基于球谐函数的PRT系统，用于Cornell Box场景的实时relighting。

## 文件结构

### 核心实现文件

1. **SphericalHarmonics.h / SphericalHarmonics.cpp**
   - 球谐函数库的核心实现
   - 包含所有PRT相关的类和函数

### 主要类

#### 1. SphericalHarmonics
球谐函数计算类，提供以下功能：
- `EvaluateBasis()` - 计算球谐基函数值
- `ProjectLight()` - 投影光照到球谐函数
- `ReconstructLight()` - 从球谐系数重建光照
- `GenerateFibonacciSamples()` - 生成Fibonacci球采样
- `RotateSHY()` - 绕Y轴旋转球谐系数
- `Lerp()` - 线性插值球谐系数

#### 2. LightSampler
光照采样器，用于从环境中采样光照：
- `SampleFromCubemap()` - 从立方体贴图采样
- `SampleUniformColor()` - 从均匀颜色采样

#### 3. PRTPrecomputer
PRT预计算器，用于离线预计算：
- `PrecomputeRotations()` - 预计算不同旋转角度的系数
- `PrecomputeLighting()` - 预计算光照

#### 4. DataExporter
数据导出/导入器：
- `ExportToTxt()` - 导出到txt文件
- `ImportFromTxt()` - 从txt文件导入

#### 5. Relighter
实时relighting计算器：
- `ComputeRelighting()` - 计算relighting结果
- `QueryCoefficients()` - 查询旋转角度对应的系数

### 数据结构

#### SHCoefficients
```cpp
struct SHCoefficients {
    std::array<glm::vec3, 9> coeffs;  // 9个球谐系数
};
```

## 使用流程

### 1. 预计算阶段

```cpp
// 在VulkanExample中调用
void VulkanExample::PrecomputePRT() {
    // 生成采样方向
    auto directions = SphericalHarmonics::GenerateFibonacciSamples(shSamples);
    
    // 生成采样光照
    std::vector<glm::vec3> radiances;
    for (int i = 0; i < shSamples; i++) {
        radiances.push_back(lightColor * lightIntensity);
    }
    
    // 预计算光照的球谐系数
    SHCoefficients originalCoeffs = SphericalHarmonics::ProjectLight(directions, radiances);
    
    // 预计算不同旋转角度的系数
    prtData = PRTPrecomputer::PrecomputeRotations(originalCoeffs, 24, 360.0f);
    
    // 导出到文件
    DataExporter::ExportToTxt("prt_data.txt", prtData);
}
```

### 2. 实时应用阶段

```cpp
// 每帧更新
void VulkanExample::UpdatePRTLighting() {
    if (!usePRT || prtData.empty()) {
        return;
    }
    
    // 将弧度转换为度数
    float angleDegrees = lightRotationAngle * 180.0f / PI;
    
    // 查询对应旋转角度的球谐系数
    currentSHCoefficients = Relighter::QueryCoefficients(angleDegrees, prtData);
    
    // 应用到着色器
    // ...
}
```

### 3. 着色器应用

在片段着色器中使用球谐系数进行relighting：

```glsl
// 从球谐系数重建光照
vec3 lighting = ReconstructLighting(normal);

// 应用材质颜色
vec3 finalColor = albedo * lighting;
```

## 数据格式

### txt文件格式

```
# PRT Precomputed Radiance Transfer Data
# Generated: 2025-11-27
# Rotations: 24
# SH Order: 2 (9 coefficients)

# Light Rotation: 0 degrees
0.5 0.5 0.5 0.1 0.1 0.1 0.2 0.2 0.2 ...

# Light Rotation: 15 degrees
0.48 0.52 0.5 0.12 0.09 0.1 0.19 0.21 0.2 ...
```

## 性能特性

### 内存占用
- 单个旋转: 9 * vec3 = 108字节
- 24个旋转: 2.5 KB

### 计算复杂度
- 预计算: O(numSamples * 9)
- 实时查询: O(1)
- Relighting: O(9)

## 测试

运行PRT_Test.cpp中的测试函数来验证系统：

```cpp
TestSphericalHarmonics();
```

测试包括：
1. 基函数计算
2. 采样生成
3. 光照投影
4. 光照重建
5. 旋转计算
6. 预计算旋转
7. 数据导出/导入
8. 旋转查询和插值
9. Relighting计算
10. 系数插值

## 集成到主程序

### 1. 添加包含头文件
```cpp
#include "SphericalHarmonics.h"
```

### 2. 添加成员变量
```cpp
std::vector<PRTPrecomputer::RotatedCoefficients> prtData;
SHCoefficients currentSHCoefficients;
std::string prtDataFile = "prt_data.txt";
```

### 3. 调用预计算
```cpp
PrecomputePRT();
```

### 4. 每帧更新
```cpp
UpdatePRTLighting();
```

## 扩展功能

### 支持更高阶球谐
修改SHCoefficients结构体以支持3阶或4阶球谐：

```cpp
struct SHCoefficients3 {
    std::array<glm::vec3, 16> coeffs;  // 3阶: 16个系数
};
```

### 支持多个光源
创建多个SHCoefficients数组，分别存储每个光源的系数。

### 支持动态光源颜色
在预计算时使用不同的光源颜色，生成多组系数。

## 常见问题

### Q: 如何改变采样数量？
A: 修改`shSamples`变量，然后重新调用`PrecomputePRT()`。

### Q: 如何提高精度？
A: 增加采样数量或使用更高阶的球谐函数。

### Q: 如何加载预计算的数据？
A: 使用`DataExporter::ImportFromTxt()`函数。

### Q: 如何应用到着色器？
A: 创建一个新的UBO来存储球谐系数，然后在着色器中使用。

## 参考资源

- Sloan et al., "Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments" (2003)
- 球谐函数的数学基础
- 光照传输的理论

## 下一步

1. 编译项目
2. 运行测试
3. 集成到主渲染管线
4. 优化性能
5. 添加UI控制

---

**实现日期**: 2025-11-27
**版本**: 1.0
**状态**: 完成

