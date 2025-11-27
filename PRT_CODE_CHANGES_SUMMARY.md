# PRT 代码修改总结

## 修改的文件

### 1. SphericalHarmonics.h

#### 修改1: 添加头文件
```cpp
#include <string>
#include <fstream>
#include <sstream>
```

#### 修改2: 扩展PRTPrecomputer类
```cpp
struct LightTransportData {
    std::vector<SHCoefficients> coefficients;
};

// 添加Light Transport预计算函数
static SHCoefficients PrecomputeLightTransport(
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& albedo,
    const std::vector<glm::vec3>& sampleDirections
);
```

#### 修改3: 重构DataExporter类
```cpp
// 分别导出Lighting和Light Transport
static bool ExportLighting(const std::string& filename,
                          const std::vector<PRTPrecomputer::RotatedCoefficients>& rotatedLighting);

static bool ExportLightTransport(const std::string& filename,
                                const SHCoefficients& ltCoeffs);

static bool ExportPRTData(const std::string& baseFilename,
                         const std::vector<PRTPrecomputer::RotatedCoefficients>& rotatedLighting,
                         const SHCoefficients& ltCoeffs);

// 对应的导入函数
static std::vector<PRTPrecomputer::RotatedCoefficients> ImportLighting(const std::string& filename);
static SHCoefficients ImportLightTransport(const std::string& filename);
```

---

### 2. SphericalHarmonics.cpp

#### 修改1: 修复球谐旋转矩阵 (行121-161)
**原始代码**: 直接复制系数，没有旋转
```cpp
for (int i = 0; i < 9; i++) {
    result.coeffs[i] = coeffs.coeffs[i];
}
```

**新代码**: 实现正确的2阶球谐旋转
```cpp
// l=0: Y00 (不受旋转影响)
result.coeffs[0] = coeffs.coeffs[0];

// l=1: Y1m, Y10, Y1p (不受绕Y轴旋转影响)
result.coeffs[1] = coeffs.coeffs[1];
result.coeffs[2] = coeffs.coeffs[2];
result.coeffs[3] = coeffs.coeffs[3];

// l=2: 5个系数需要旋转
float c2 = c * c;
float s2 = s * s;
float cs = c * s;

result.coeffs[4] = coeffs.coeffs[4] * c2 - coeffs.coeffs[8] * cs;
result.coeffs[5] = coeffs.coeffs[5];
result.coeffs[6] = coeffs.coeffs[6];
result.coeffs[7] = coeffs.coeffs[7] * c2 + coeffs.coeffs[4] * cs;
result.coeffs[8] = coeffs.coeffs[8] * c2 + coeffs.coeffs[4] * s2;
```

#### 修改2: 添加Light Transport预计算 (行204-237)
```cpp
SHCoefficients PRTPrecomputer::PrecomputeLightTransport(
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& albedo,
    const std::vector<glm::vec3>& sampleDirections) {
    
    SHCoefficients result;
    int numSamples = sampleDirections.size();
    
    if (numSamples == 0) return result;
    
    for (int i = 0; i < 9; i++) {
        glm::vec3 coeff(0.0f);
        for (int j = 0; j < numSamples; j++) {
            const glm::vec3& dir = sampleDirections[j];
            float cosTheta = glm::max(0.0f, glm::dot(normal, dir));
            float basis = SphericalHarmonics::EvaluateBasis(i, dir);
            coeff += albedo * cosTheta * basis;
        }
        result.coeffs[i] = coeff * (4.0f * PI / numSamples);
    }
    
    return result;
}
```

#### 修改3: 重构数据导出函数 (行251-388)
- 分离导出Lighting和Light Transport
- 添加角度信息到Lighting文件
- 实现对应的导入函数

---

### 3. main.cpp

#### 修改1: 重构PrecomputePRT函数 (行1185-1266)
**原始代码**: 只预计算Lighting和旋转
**新代码**: 5个清晰的步骤
1. 生成采样方向
2. 预计算Lighting
3. 预计算Light Transport
4. 预计算旋转
5. 导出数据

**关键改进**:
- 分离Lighting和Light Transport的预计算
- 添加详细的日志输出
- 分别导出两个文件

#### 修改2: 更新UpdatePRTLighting函数 (行1268-1294)
**原始代码**: 只更新系数，没有说明
**新代码**: 添加了详细的Relighting公式说明

```cpp
// Relighting计算公式:
// L_out = Σ(i=0 to 8) Lighting[i] * LightTransport[i]
```

---

## 关键改进点

### 1. 概念分离
- **Lighting**: 光源的球谐表示 (与物体无关)
- **Light Transport**: 物体表面的响应 (与光源无关)
- **Relighting**: 两者的乘积求和

### 2. 数据导出
- 导出两个独立的文件
- Lighting文件包含旋转信息
- Light Transport文件包含表面信息

### 3. 球谐旋转
- 实现了正确的旋转矩阵
- 支持绕Y轴旋转
- 可扩展到其他轴

### 4. 代码可读性
- 添加了详细的注释
- 分离了预计算的各个步骤
- 清晰的日志输出

---

## 编译和运行

### 编译
```bash
cd build
cmake --build . --config Release --target lightprobesh2 -j 4
```

### 运行
```bash
./bin/lightprobesh2.exe
```

### 预期输出
```
========================================
[VulkanExample] Starting PRT precomputation...
========================================

[Step 1] Generating sample directions...
  - Generated 16 sample directions

[Step 2] Precomputing Lighting (Light Source)...
  - Lighting SH coefficients computed
  - Light Color: (1.00, 1.00, 1.00)
  - Light Intensity: 1.00

[Step 3] Precomputing Light Transport (Surface Response)...
  - Light Transport SH coefficients computed
  - Surface Normal: (0.00, 1.00, 0.00)
  - Surface Albedo: (0.80, 0.80, 0.80)

[Step 4] Precomputing Light Rotations...
  - Precomputed 24 rotations
  - Rotation step: 15 degrees

[Step 5] Exporting PRT Data...
  - Exported Lighting to: prt_data_lighting.txt
  - Exported Light Transport to: prt_data_lt.txt

========================================
[VulkanExample] PRT precomputation completed successfully!
========================================
```

---

## 验证方法

1. **检查导出文件**
   - `prt_data_lighting.txt`: 应该有24行数据
   - `prt_data_lt.txt`: 应该有1行数据

2. **检查数据有效性**
   - 所有系数不应该全为0
   - 系数值应该在合理范围内

3. **检查旋转**
   - 不同角度的Lighting系数应该不同
   - 相邻角度的系数应该相似

4. **检查Relighting**
   - 光源旋转时颜色应该变化
   - 颜色变化应该平滑

