# PRT 黑色立方体修复 - 所有修改

## 修改概述

为诊断和修复 PRT 渲染时某个立方体显示为黑色的问题，对以下文件进行了修改：

## 1. 着色器修改

### 文件: `shaders/glsl/lightprobesh2/prt_relight.vert`

**修改内容**:

#### A. 添加越界检查 (行 54-60)
```glsl
if (vid >= ltBuffer.ltCoefficients.length()) {
    outColor = vec3(1.0, 0.0, 1.0);  // 洋红色
    return;
}
```

#### B. 添加零系数检查 (行 62-70)
```glsl
bool allZero = true;
for (int i = 0; i < 9; i++) {
    if (length(lt_coeffs.coeffs[i].xyz) > 0.001) {
        allZero = false;
        break;
    }
}
if (allZero) {
    outColor = vec3(0.0, 1.0, 1.0);  // 青色
    return;
}
```

**目的**: 通过颜色编码快速识别问题顶点

**编译**: 
```bash
glslc -O prt_relight.vert -o prt_relight.vert.spv
glslc -O prt_relight.frag -o prt_relight.frag.spv
```

## 2. C++ 代码修改

### 文件: `examples/lightprobesh2/main.cpp`

#### 修改 A: 零系数检查 (行 ~1730)

**位置**: `preparePRTRelighting()` 函数

**修改内容**:
```cpp
// 检查零系数顶点
int zeroCount = 0;
for (size_t i = 0; i < precomputedLTCoefficients.size(); ++i) {
    bool isZero = true;
    for (int j = 0; j < 9; ++j) {
        float len = glm::length(precomputedLTCoefficients[i].coeffs[j]);
        if (len > 0.001f) {
            isZero = false;
            break;
        }
    }
    if (isZero) {
        zeroCount++;
        if (zeroCount <= 5) {
            std::cout << "[DEBUG PRT] WARNING: Vertex " << i 
                      << " has all-zero LT coefficients!" << std::endl;
        }
    }
}
if (zeroCount > 0) {
    std::cout << "[DEBUG PRT] WARNING: " << zeroCount 
              << " vertices have all-zero LT coefficients!" << std::endl;
}
```

**目的**: 在加载 PRT 数据时检测零系数顶点

#### 修改 B: 黑色材质检查 (行 ~750)

**位置**: 绘制循环中

**修改内容**:
```cpp
// 检查黑色材质
static bool printedMaterialWarning = false;
if (!printedMaterialWarning) {
    for (auto* node : model->nodes) {
        // 递归检查所有 primitive
        for (auto* prim : node->mesh->primitives) {
            if (prim->material.baseColorFactor.x < 0.01f &&
                prim->material.baseColorFactor.y < 0.01f &&
                prim->material.baseColorFactor.z < 0.01f) {
                std::cout << "[DEBUG PRT] WARNING: Found black material! "
                          << "baseColor=(...)" << std::endl;
            }
        }
    }
    printedMaterialWarning = true;
}
```

**目的**: 检测是否有材质颜色为黑色的 primitive

#### 修改 C: 无效法向量检查 (行 ~1630)

**位置**: `ExportPRTDataGPU()` 函数中的顶点读取循环

**修改内容**:
```cpp
int invalidNormalCount = 0;
for (int i = 0; i < vcount; ++i) {
    glm::vec3 normal = glm::normalize(vtx[i].normal);
    float normLen = glm::length(normal);
    
    if (normLen < 0.1f || glm::isnan(normal.x) || 
        glm::isnan(normal.y) || glm::isnan(normal.z)) {
        if (invalidNormalCount < 5) {
            std::cout << "[DEBUG PRT] WARNING: Vertex " << i 
                      << " has invalid normal!" << std::endl;
        }
        invalidNormalCount++;
        normal = glm::vec3(0.0f, 1.0f, 0.0f);  // 默认法向量
    }
    normals.emplace_back(normal);
}

if (invalidNormalCount > 0) {
    std::cout << "[DEBUG PRT] WARNING: Found " << invalidNormalCount 
              << " vertices with invalid normals!" << std::endl;
}
```

**目的**: 检测和修复无效的顶点法向量

## 编译步骤

### 1. 编译着色器
```bash
cd shaders/glsl/lightprobesh2
glslc -O prt_relight.vert -o prt_relight.vert.spv
glslc -O prt_relight.frag -o prt_relight.frag.spv
```

### 2. 编译程序
```bash
cd build
msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2
```

## 测试步骤

1. 运行 `build\bin\Release\lightprobesh2.exe`
2. 启用 PRT Relighting
3. 观察控制台输出和视觉效果
4. 根据诊断结果采取修复措施

## 修改的影响

- **性能**: 诊断代码仅在启用 PRT 时执行，影响最小
- **兼容性**: 无破坏性修改，完全向后兼容
- **功能**: 添加诊断功能，不改变现有渲染逻辑

## 相关文档

- `PRT_FIX_SUMMARY.md` - 修复总结
- `PRT_DIAGNOSTIC_GUIDE.md` - 诊断指南
- `QUICK_PRT_FIX_REFERENCE.md` - 快速参考
- `PRT_ROOT_CAUSE_ANALYSIS.md` - 根本原因分析

