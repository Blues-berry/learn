# PRTRenderer 使用指南

## 概述

`PRTRenderer` 是一个高级接口，用于直接使用预计算的PRT数据进行实时渲染。它封装了所有复杂的球谐函数计算，提供简单易用的API。

---

## 核心概念

### 三项预计算数据

1. **Lighting** - 光源的球谐系数
   - 文件: `prt_data_lighting_original.txt`
   - 描述: 光源在球谐基函数上的投影

2. **Light Transport** - 物体表面的响应
   - 文件: `prt_data_lt.txt`
   - 描述: 物体表面对入射光的响应

3. **Rotated Lighting** - 旋转后的光源系数
   - 文件: `prt_data_lighting.txt`
   - 描述: 24个不同旋转角度的光源系数

### Relighting公式

```
color = Σ(Lighting[i] × LightTransport[i]) × albedo
```

---

## 使用步骤

### 1. 初始化PRTRenderer

```cpp
#include "SphericalHarmonics.h"

// 创建PRT数据结构
PRTRenderer::PRTData prtData;

// 初始化 (加载预计算数据)
bool success = PRTRenderer::Initialize(
    prtData,
    "prt_data_lighting.txt",      // 旋转后的Lighting
    "prt_data_lt.txt"              // Light Transport
);

if (!success) {
    std::cerr << "Failed to initialize PRT data!" << std::endl;
    return;
}
```

### 2. 更新光源旋转

```cpp
// 更新光源旋转角度 (度数)
float rotationAngle = 45.0f;  // 45度
PRTRenderer::UpdateRotation(prtData, rotationAngle);

// 或者在循环中动态更新
for (float angle = 0.0f; angle < 360.0f; angle += 1.0f) {
    PRTRenderer::UpdateRotation(prtData, angle);
    // 进行渲染...
}
```

### 3. 计算单点着色

```cpp
// 定义表面属性
glm::vec3 normal = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));  // 表面法线
glm::vec3 albedo = glm::vec3(0.8f, 0.8f, 0.8f);                  // 表面反射率

// 计算着色颜色
glm::vec3 shadingColor = PRTRenderer::ComputeShading(prtData, normal, albedo);

std::cout << "Shading color: (" << shadingColor.x << ", " 
          << shadingColor.y << ", " << shadingColor.z << ")" << std::endl;
```

### 4. 批量计算多点着色

```cpp
// 定义多个点的法线和反射率
std::vector<glm::vec3> normals = {
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f)
};

std::vector<glm::vec3> albedos = {
    glm::vec3(0.8f, 0.8f, 0.8f),
    glm::vec3(0.5f, 0.5f, 0.5f),
    glm::vec3(0.9f, 0.9f, 0.9f)
};

// 批量计算着色
std::vector<glm::vec3> shadingResults = PRTRenderer::ComputeShadingBatch(
    prtData,
    normals,
    albedos
);

// 使用结果
for (size_t i = 0; i < shadingResults.size(); i++) {
    std::cout << "Point " << i << " shading: (" 
              << shadingResults[i].x << ", " 
              << shadingResults[i].y << ", " 
              << shadingResults[i].z << ")" << std::endl;
}
```

### 5. 获取当前系数

```cpp
// 获取当前Lighting系数
const SHCoefficients& currentLighting = PRTRenderer::GetCurrentLighting(prtData);

// 获取Light Transport系数
const SHCoefficients& lightTransport = PRTRenderer::GetLightTransport(prtData);

// 访问系数
for (int i = 0; i < 9; i++) {
    std::cout << "Lighting[" << i << "]: (" 
              << currentLighting.coeffs[i].x << ", "
              << currentLighting.coeffs[i].y << ", "
              << currentLighting.coeffs[i].z << ")" << std::endl;
}
```

### 6. 导出着色结果

```cpp
// 导出当前旋转角度的着色结果
bool exported = PRTRenderer::ExportShadingResult(
    "shading_results.txt",
    prtData,
    normals,
    albedos
);

if (exported) {
    std::cout << "Shading results exported successfully!" << std::endl;
}
```

---

## 完整示例

```cpp
#include "SphericalHarmonics.h"
#include <iostream>
#include <glm/glm.hpp>

int main() {
    // 1. 初始化
    PRTRenderer::PRTData prtData;
    if (!PRTRenderer::Initialize(prtData, "prt_data_lighting.txt", "prt_data_lt.txt")) {
        std::cerr << "Failed to initialize!" << std::endl;
        return 1;
    }

    // 2. 定义表面
    std::vector<glm::vec3> normals = {
        glm::vec3(0.0f, 1.0f, 0.0f),   // 顶部
        glm::vec3(1.0f, 0.0f, 0.0f),   // 右侧
        glm::vec3(0.0f, 0.0f, 1.0f)    // 前面
    };

    std::vector<glm::vec3> albedos = {
        glm::vec3(0.8f, 0.8f, 0.8f),   // 灰色
        glm::vec3(0.9f, 0.2f, 0.2f),   // 红色
        glm::vec3(0.2f, 0.2f, 0.9f)    // 蓝色
    };

    // 3. 旋转光源并渲染
    std::cout << "Rendering with different light rotations:" << std::endl;
    for (float angle = 0.0f; angle < 360.0f; angle += 45.0f) {
        // 更新旋转
        PRTRenderer::UpdateRotation(prtData, angle);

        // 计算着色
        auto shadingResults = PRTRenderer::ComputeShadingBatch(prtData, normals, albedos);

        // 输出结果
        std::cout << "\nAngle: " << angle << " degrees" << std::endl;
        for (size_t i = 0; i < shadingResults.size(); i++) {
            std::cout << "  Point " << i << ": (" 
                      << shadingResults[i].x << ", "
                      << shadingResults[i].y << ", "
                      << shadingResults[i].z << ")" << std::endl;
        }
    }

    // 4. 导出最终结果
    PRTRenderer::ExportShadingResult("final_shading.txt", prtData, normals, albedos);

    return 0;
}
```

---

## API 参考

### PRTRenderer::PRTData

```cpp
struct PRTData {
    SHCoefficients lighting;                                    // 当前Lighting系数
    SHCoefficients lightTransport;                              // Light Transport系数
    std::vector<PRTPrecomputer::RotatedCoefficients> rotations; // 所有旋转的Lighting
    float currentRotationAngle;                                 // 当前旋转角度
};
```

### PRTRenderer 函数

| 函数 | 说明 |
|------|------|
| `Initialize()` | 初始化PRT数据，加载预计算文件 |
| `UpdateRotation()` | 更新光源旋转角度 |
| `ComputeShading()` | 计算单点着色 |
| `ComputeShadingBatch()` | 批量计算多点着色 |
| `GetCurrentLighting()` | 获取当前Lighting系数 |
| `GetLightTransport()` | 获取Light Transport系数 |
| `ExportShadingResult()` | 导出着色结果到文件 |

---

## 输出文件格式

### prt_data_lighting_original.txt
```
# PRT Lighting Data (Original)
# Format: angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

0 c00.x c00.y c00.z c1m1.x c1m1.y c1m1.z ... c22.x c22.y c22.z
```

### prt_data_lt.txt
```
# PRT Light Transport Data
# Format: coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

lt00.x lt00.y lt00.z lt1m1.x lt1m1.y lt1m1.z ... lt22.x lt22.y lt22.z
```

### prt_data_lighting.txt
```
# PRT Lighting Data (Rotated)
# Format: angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

0 ...
15 ...
30 ...
...
345 ...
```

### shading_results.txt
```
# PRT Shading Results
# Rotation Angle: 45 degrees
# Format: normal.xyz albedo.xyz shading.xyz

0 1 0 0.8 0.8 0.8 0.64 0.64 0.64
1 0 0 0.9 0.2 0.2 0.72 0.16 0.16
...
```

---

## 性能优化

### 1. 缓存着色结果
```cpp
// 如果法线和反射率不变，缓存结果
std::vector<glm::vec3> cachedResults;
bool resultsValid = false;

for (float angle = 0.0f; angle < 360.0f; angle += 1.0f) {
    PRTRenderer::UpdateRotation(prtData, angle);
    
    if (!resultsValid) {
        cachedResults = PRTRenderer::ComputeShadingBatch(prtData, normals, albedos);
        resultsValid = true;
    }
    
    // 使用cachedResults进行渲染
}
```

### 2. 使用GPU计算
```cpp
// 将SH系数上传到GPU
const auto& lighting = PRTRenderer::GetCurrentLighting(prtData);
const auto& lt = PRTRenderer::GetLightTransport(prtData);

// 在着色器中计算: color = Σ(lighting[i] × lt[i]) × albedo
// 这样可以充分利用GPU的并行计算能力
```

### 3. 预计算更多旋转
```cpp
// 在PrecomputePRT中增加旋转数量
prtData = PRTPrecomputer::PrecomputeRotations(lightingCoeffs, 36, 360.0f);  // 36个旋转，每10度一个
```

---

## 常见问题

**Q: 如何支持多个光源?**
A: 为每个光源预计算Lighting，然后在ComputeShading中求和。

**Q: 如何支持动态物体?**
A: 为每个顶点预计算Light Transport，运行时查询。

**Q: 如何提高精度?**
A: 增加采样数量或使用更高阶的球谐函数。

**Q: 如何集成到Vulkan渲染管线?**
A: 将SH系数上传到UBO，在着色器中计算Relighting。

---

## 下一步

1. 编译代码
2. 运行预计算生成三个文件
3. 使用PRTRenderer进行渲染
4. 集成到Vulkan渲染管线
5. 优化性能

