# 第一阶段第一步：现有CPU端预计算代码结构分析

## 1. 项目概述

本项目目标是实现基于PRT (Precomputed Radiance Transfer) 的Cornell Box场景relighting系统。当前代码已有CPU端的完整预计算实现，需要将其转移到GPU端进行加速。

## 2. 当前代码结构分析

### 2.1 核心数据结构

#### SHCoefficients (球谐系数)
```cpp
struct SHCoefficients {
    std::array<glm::vec3, 9> coeffs;  // 2阶球谐函数，9个系数
    // 每个系数是RGB三通道的向量
};
```

**特点：**
- 使用2阶球谐函数（9个系数）
- 每个系数包含RGB三个通道
- 支持基本的算术运算（加法、乘法、除法）

#### PRTPrecomputer::RotatedCoefficients (旋转系数)
```cpp
struct RotatedCoefficients {
    float angle;           // 旋转角度（度数）
    SHCoefficients coeffs; // 该角度下的球谐系数
};
```

### 2.2 核心类和功能

#### SphericalHarmonics 类
**主要功能：**
1. `EvaluateBasis()` - 计算球谐基函数值
   - 输入：方向向量
   - 输出：9个基函数值
   - 使用标准的2阶球谐基函数公式

2. `ProjectLight()` - 光照投影
   - 输入：采样方向、辐射度
   - 输出：球谐系数
   - 公式：`coeff[i] = Σ(radiance * basis[i]) * (4π / numSamples)`

3. `ReconstructLight()` - 光照重建
   - 输入：球谐系数、方向
   - 输出：该方向的光照值
   - 公式：`light = Σ(coeff[i] * basis[i])`

4. `GenerateFibonacciSamples()` - 生成均匀采样方向
   - 使用Fibonacci球采样算法
   - 生成均匀分布在球面上的采样点

5. `RotateSHY()` - 绕Y轴旋转球谐系数
   - 使用旋转矩阵应用于球谐系数
   - 支持绕Y轴的旋转

#### PRTPrecomputer 类
**主要功能：**
1. `PrecomputeRotations()` - 预计算多个旋转角度的系数
   - 输入：原始系数、旋转数量、最大旋转角度
   - 输出：多个旋转角度的系数数组

2. `PrecomputeLighting()` - 预计算光照系数
   - 从采样方向和辐射度计算球谐系数

3. `PrecomputeLightTransport()` - 预计算Light Transport
   - 输入：位置、法向量、反射率、采样方向
   - 输出：Light Transport球谐系数
   - 公式：`LT[i] = Σ(albedo * max(0, dot(normal, dir)) * basis[i]) * (4π / numSamples)`

#### DataExporter 类
**主要功能：**
1. `ExportLighting()` - 导出旋转光照数据到txt文件
   - 格式：`angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz`

2. `ExportLightTransport()` - 导出Light Transport数据
   - 格式：`coeff[0].xyz coeff[1].xyz ... coeff[8].xyz`

3. `ImportLighting()` - 从txt文件导入旋转光照数据

4. `ImportLightTransport()` - 从txt文件导入Light Transport数据

#### Relighter 类
**主要功能：**
1. `ComputeRelighting()` - 计算relighting结果
   - 公式：`result = albedo * ReconstructLight(coeffs, normal)`

2. `QueryCoefficients()` - 查询指定角度的系数（带线性插值）
   - 找到最近的两个数据点进行插值

#### PRTRenderer 类
**主要功能：**
1. `Initialize()` - 初始化PRT数据
   - 从文件导入Lighting和Light Transport数据

2. `UpdateRotation()` - 更新旋转角度

3. `ComputeShading()` - 计算着色颜色

4. `ComputeShadingBatch()` - 批量计算着色

## 3. 当前预计算流程（CPU端）

### 3.1 PrecomputePRT() 函数流程

**位置：** main.cpp 第1185-1290行

**流程步骤：**

1. **第1步：生成采样方向**
   ```cpp
   auto directions = SphericalHarmonics::GenerateFibonacciSamples(shSamples);
   ```
   - 生成均匀分布的采样方向
   - 默认采样数：16-32个

2. **第2步：预计算Lighting（光源球谐系数）**
   ```cpp
   std::vector<glm::vec3> radiances;
   for (int i = 0; i < shSamples; i++) {
       radiances.push_back(lightColor * lightIntensity);
   }
   SHCoefficients lightingCoeffs = PRTPrecomputer::PrecomputeLighting(directions, radiances);
   ```
   - 为每个采样方向分配光照值
   - 计算光照的球谐系数

3. **第3步：预计算Light Transport（物体表面响应）**
   ```cpp
   SHCoefficients ltCoeffs = PRTPrecomputer::PrecomputeLightTransport(
       glm::vec3(0.0f, 0.0f, 0.0f),
       cornellNormal,
       cornellAlbedo,
       directions
   );
   ```
   - 对Cornell Box表面计算Light Transport系数
   - 当前仅计算单个点（应该是每个顶点）

4. **第4步：预计算旋转系数**
   ```cpp
   prtData = PRTPrecomputer::PrecomputeRotations(lightingCoeffs, 24, 360.0f);
   ```
   - 预计算24个旋转角度的系数（每15度一个）

5. **第5步：导出到文件**
   - 导出原始Lighting：`prt_data_lighting_original.txt`
   - 导出Light Transport：`prt_data_lt.txt`
   - 导出旋转Lighting：`prt_data_lighting.txt`

### 3.2 数据导出格式

**Lighting文件格式：**
```
# PRT Lighting Data (Rotated)
# Generated: 2025-11-27
# Rotations: 24
# SH Order: 2 (9 coefficients)
# Format: angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

0.0 c0x c0y c0z c1x c1y c1z ... c8x c8y c8z
15.0 c0x c0y c0z c1x c1y c1z ... c8x c8y c8z
...
```

**Light Transport文件格式：**
```
# PRT Light Transport Data
# Generated: 2025-11-27
# SH Order: 2 (9 coefficients)
# Format: coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

c0x c0y c0z c1x c1y c1z ... c8x c8y c8z
```

## 4. 当前存在的问题

### 4.1 主要问题

1. **预计算在CPU端进行**
   - 计算量大，速度慢
   - 对于大规模场景（多个顶点）不可行
   - 需要转移到GPU端

2. **Light Transport计算不完整**
   - 当前仅计算单个点的LT系数
   - 应该对Cornell模型的每个顶点计算
   - 需要逐顶点计算并存储

3. **缺少GPU计算框架**
   - 没有compute shader实现
   - 没有GPU buffer管理
   - 没有CPU-GPU数据传输机制

### 4.2 需要改进的地方

1. **创建Compute Shader**
   - 实现球谐基函数计算
   - 实现光照投影计算
   - 实现Light Transport计算

2. **创建GPU计算管道**
   - Vulkan compute pipeline
   - Descriptor sets和buffers
   - 同步机制

3. **实现数据传输**
   - CPU到GPU的数据上传
   - GPU到CPU的结果下载
   - 中间结果缓存

4. **优化计算流程**
   - 批量处理顶点
   - 并行计算
   - 内存优化

## 5. 关键技术点

### 5.1 球谐基函数（2阶）

```
Y00 = 0.282095
Y1-1 = 0.488603 * y
Y10 = 0.488603 * z
Y11 = 0.488603 * x
Y2-2 = 1.092548 * x * y
Y2-1 = 1.092548 * y * z
Y20 = 0.315392 * (3*z² - 1)
Y21 = 1.092548 * x * z
Y22 = 0.546274 * (x² - y²)
```

### 5.2 光照投影公式

```
coeff[i] = (4π / N) * Σ(j=0 to N-1) radiance[j] * basis[i](direction[j])
```

### 5.3 Light Transport公式

```
LT[i] = (4π / N) * Σ(j=0 to N-1) albedo * max(0, dot(normal, direction[j])) * basis[i](direction[j])
```

### 5.4 Relighting公式

```
result = albedo * Σ(i=0 to 8) lighting[i] * LT[i]
```

## 6. 文件清单

### 核心文件
- `SphericalHarmonics.h/cpp` - 球谐函数和PRT预计算核心
- `PreviewModel.h/cpp` - 模型渲染
- `main.cpp` - 主程序和预计算流程
- `PRT_Test.cpp` - 测试代码

### 相关文件
- `Pass.h/cpp` - 渲染通道
- `LightProbe.h/cpp` - 光照探针
- `Skybox.h/cpp` - 天空盒
- `GltfScene.h/cpp` - glTF场景

## 7. 下一步计划

### 7.1 立即需要做的
1. 创建Compute Shader框架
2. 实现GPU端球谐基函数计算
3. 实现GPU端光照投影计算
4. 实现GPU端Light Transport计算

### 7.2 然后需要做的
1. 创建Vulkan compute pipeline
2. 实现数据上传和下载
3. 集成到主程序
4. 测试和验证

### 7.3 最后需要做的
1. UI集成
2. 文件读取和应用
3. Relighting着色器
4. 完整流程测试

## 8. 性能预期

### CPU端性能
- 采样数：32个
- 旋转数：24个
- 单点LT计算：~1ms
- 完整预计算：~100ms

### GPU端性能预期
- 采样数：32个
- 旋转数：24个
- 单点LT计算：<0.1ms
- 完整预计算：~10ms
- **性能提升：10倍以上**

## 9. 总结

当前代码已有完整的CPU端PRT预计算实现，包括：
- 球谐基函数计算
- 光照投影
- Light Transport计算
- 旋转和插值
- 文件导入导出

主要工作是将这些计算转移到GPU端，使用Vulkan compute shader实现，以获得显著的性能提升。

---

**分析完成时间：** 2025-11-28
**分析者：** Cascade AI
**下一步：** 创建GPU计算Shader框架

