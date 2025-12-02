# 修改前后对比

## 1. Project() 方法

### ❌ 修改前
```cpp
// SphericalHarmonics.h
// 没有 Project() 方法

// main.cpp (行 1700)
ltCpuBatch.push_back(SphericalHarmonics::Project(visibilityFunc, directions));
// 编译错误: C3861: "Project": 找不到标识符
```

### ✅ 修改后
```cpp
// SphericalHarmonics.h (行 59-77)
template<typename Func>
static SHCoefficients Project(Func func, const std::vector<glm::vec3>& directions) {
    SHCoefficients result;
    int numSamples = directions.size();
    
    if (numSamples == 0) return result;
    
    for (int i = 0; i < 9; i++) {
        glm::vec3 coeff(0.0f);
        for (int j = 0; j < numSamples; j++) {
            float basis = EvaluateBasis(i, directions[j]);
            float value = func(directions[j]);
            coeff += glm::vec3(value) * basis;
        }
        result.coeffs[i] = coeff * (4.0f * PI / numSamples);
    }
    
    return result;
}

// 编译成功 ✅
```

---

## 2. PI 常数定义

### ❌ 修改前
```cpp
// SphericalHarmonics.h
const float PI = 3.14159265359f;

// SphericalHarmonics.cpp
const float PI = 3.14159265359f;
const float PHI = 1.61803398875f;

// 编译错误: C2374: "PI": 重定义
```

### ✅ 修改后
```cpp
// SphericalHarmonics.h (行 12)
inline constexpr float PI = 3.14159265359f;

// SphericalHarmonics.cpp (行 1-9)
// const float PI = 3.14159265359f;  // 删除
const float PHI = 1.61803398875f;

// 编译成功 ✅
```

---

## 3. 着色公式

### ❌ 修改前
```cpp
// SphericalHarmonics.cpp (行 498-503)
glm::vec3 PRTRenderer::ComputeShading(const PRTData& prtData,
                                     const glm::vec3& normal,
                                     const glm::vec3& albedo) {
    return Relighter::ComputeRelighting(prtData.lighting, normal, albedo) * albedo;
    //                                                                      ^^^^^^^^
    //                                                                   重复乘 albedo!
}

// 结果: shading = albedo × (albedo × lighting) = albedo² × lighting
// 效果: 画面过暗（albedo² 会大幅降低亮度）
```

### ✅ 修改后
```cpp
// SphericalHarmonics.cpp (行 498-503)
glm::vec3 PRTRenderer::ComputeShading(const PRTData& prtData,
                                     const glm::vec3& normal,
                                     const glm::vec3& albedo) {
    // Note: ComputeRelighting already applies albedo, so we don't multiply again
    return Relighter::ComputeRelighting(prtData.lighting, normal, albedo);
}

// 结果: shading = albedo × lighting
// 效果: 亮度正确 ✅
```

**对比**:
```
albedo = 0.8
修改前: 0.8² = 0.64 (亮度降低 20%)
修改后: 0.8 (正确)

albedo = 0.5
修改前: 0.5² = 0.25 (亮度降低 50%)
修改后: 0.5 (正确)
```

---

## 4. LT 计算

### ❌ 修改前
```cpp
// main.cpp (行 1697-1701)
auto visibilityFunc = [&](const glm::vec3& dir) -> float {
    return glm::dot(dir, normals[i]) > 0.0f ? 1.0f : 0.0f;
    //     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //     只有可见性 (0 或 1)，缺少 Lambert 余弦项
};
ltCpuBatch.push_back(SphericalHarmonics::Project(visibilityFunc, directions));

// 问题: LT 不包含 cosine 加权，物理不准确
// 结果: 光传输计算不完整
```

### ✅ 修改后
```cpp
// main.cpp (行 1697-1701)
// Use Lambertian cosine term for LT projection (no albedo here)
auto lambertFunc = [&](const glm::vec3& dir) -> float {
    return glm::max(0.0f, glm::dot(normals[i], dir));
    //     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //     包含 Lambert 余弦项，物理正确
};
ltCpuBatch.push_back(SphericalHarmonics::Project(lambertFunc, directions));

// 改进: LT 包含 cosine 加权，物理准确
// 结果: 光传输计算完整 ✅
```

**对比**:
```
方向 (0, 1, 0) 垂直于表面：
修改前: 1.0 (无论角度如何)
修改后: cos(0°) = 1.0 ✓

方向 (0.707, 0.707, 0) 45° 角：
修改前: 1.0 (无论角度如何)
修改后: cos(45°) = 0.707 ✓

方向 (1, 0, 0) 平行于表面：
修改前: 0.0 (背面)
修改后: cos(90°) = 0.0 ✓
```

---

## 5. 编译环境

### ❌ 修改前
```bash
# 直接编译
cmake --build build --config Debug --target lightprobesh2 -j 4

# 错误: C1083: 无法打开包括文件: "vcruntime.h"
# 原因: MSVC 编译器环境变量未设置
```

### ✅ 修改后
```bash
# 使用编译脚本
powershell -ExecutionPolicy Bypass -File "examples\lightprobesh2\compile.ps1"

# 脚本自动:
# 1. 检测 VS 2022 安装位置
# 2. 运行 vcvars64.bat 设置环境
# 3. 执行 CMake 编译

# 结果: Build successful! ✅
```

---

## 总体效果对比

| 方面 | 修改前 | 修改后 |
|------|--------|--------|
| **编译** | ❌ 编译失败 | ✅ 编译成功 |
| **错误** | C2039, C3861, C2374 | 无错误 |
| **警告** | 多个 | 仅类型转换（非关键） |
| **着色** | ❌ 过暗（albedo²） | ✅ 正确亮度 |
| **LT 计算** | ❌ 不完整 | ✅ 物理准确 |
| **编译环境** | ❌ 需要手动设置 | ✅ 自动设置 |

---

## 数值示例

### 着色亮度对比

假设：
- 光源强度: 1.0
- 表面法线: (0, 1, 0)
- 表面 albedo: 0.8

**修改前**:
```
lighting = 1.0 × 0.8 = 0.8
shading = 0.8 × lighting = 0.8 × 0.8 = 0.64
结果: 画面亮度 = 64% (过暗!)
```

**修改后**:
```
lighting = 1.0 × 0.8 = 0.8
shading = lighting = 0.8
结果: 画面亮度 = 80% (正确!)
```

### LT 计算对比

假设：
- 表面法线: (0, 1, 0)
- 采样方向: (0, 0.707, 0.707) [45° 角]

**修改前**:
```
LT = 1.0 (无论角度)
```

**修改后**:
```
LT = max(0, dot((0,1,0), (0,0.707,0.707))) = 0.707
结果: 物理准确的 Lambert 加权 ✓
```

---

## 总结

所有修改都是为了：
1. ✅ 消除编译错误
2. ✅ 修复物理错误
3. ✅ 改进计算准确性
4. ✅ 简化编译流程

**结果**: 系统现在编译成功且物理正确 🎉

