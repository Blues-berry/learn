# PRT和球谐函数理论基础

## 1. PRT (Precomputed Radiance Transfer) 概述

### 定义
PRT是一种离线预计算技术，用于加速实时全局光照渲染。它将光照信息预先计算并存储，在运行时快速查询和应用。

### 核心思想
```
离线阶段:
  1. 对每个表面点，计算入射光照
  2. 使用球谐函数表示光照
  3. 存储预计算的系数

实时阶段:
  1. 加载预计算的系数
  2. 根据当前光照条件快速计算
  3. 实时relighting
```

### 优势
- ✅ 支持动态光源旋转
- ✅ 支持实时relighting
- ✅ 计算速度快
- ✅ 内存占用相对较小

---

## 2. 球谐函数 (Spherical Harmonics)

### 基本概念
球谐函数是定义在球面上的正交基函数，类似于傅里叶级数在圆周上的应用。

### 数学表示
```
Y_l^m(θ, φ) = 球谐函数
其中:
  l = 阶数 (degree)
  m = 阶内索引 (order)
  θ = 极角 (polar angle)
  φ = 方位角 (azimuthal angle)
```

### 二阶球谐函数 (9个系数)

#### 0阶 (l=0)
```
Y_0^0 = 0.282095
```

#### 1阶 (l=1)
```
Y_1^-1 = 0.488603 * sin(θ) * sin(φ)
Y_1^0  = 0.488603 * cos(θ)
Y_1^1  = 0.488603 * sin(θ) * cos(φ)
```

#### 2阶 (l=2)
```
Y_2^-2 = 1.092548 * sin(θ) * cos(θ) * sin(2φ)
Y_2^-1 = 1.092548 * sin(θ) * cos(θ) * sin(φ)
Y_2^0  = 0.315392 * (3*cos²(θ) - 1)
Y_2^1  = 1.092548 * sin(θ) * cos(θ) * cos(φ)
Y_2^2  = 0.546274 * sin²(θ) * cos(2φ)
```

### 在代码中的表示
```cpp
struct SHCoefficients {
    glm::vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
};
```

---

## 3. 光照投影到球谐函数

### 投影过程
```
给定入射光照 L(θ, φ)，投影到球谐函数:

c_i = ∫∫ L(θ, φ) * Y_i(θ, φ) * sin(θ) dθ dφ

其中:
  c_i = 第i个球谐系数
  Y_i = 第i个球谐基函数
```

### 离散采样
```
c_i ≈ (4π / N) * Σ L(θ_j, φ_j) * Y_i(θ_j, φ_j)

其中:
  N = 采样点数
  (θ_j, φ_j) = 第j个采样点
```

### 采样策略
- **均匀采样**: 简单但可能不均匀
- **Fibonacci球**: 更均匀的分布
- **重要性采样**: 根据光照强度采样

---

## 4. 光照重建

### 重建公式
```
L_reconstructed(θ, φ) = Σ c_i * Y_i(θ, φ)
```

### 精度与系数数量
```
2阶 (9个系数):
  - 精度: 中等
  - 适用: 大多数场景
  - 内存: 最小

3阶 (16个系数):
  - 精度: 较高
  - 适用: 高质量场景
  - 内存: 中等

4阶 (25个系数):
  - 精度: 很高
  - 适用: 高精度场景
  - 内存: 较大
```

---

## 5. PRT中的应用

### 光照传输
```
对于表面点p，入射光照为L(ω_i):

Radiance(p, ω_o) = ∫ L(ω_i) * T(p, ω_i, ω_o) dω_i

其中:
  T = 传输函数 (包含BRDF、可见性等)
```

### 预计算步骤
```
1. 对每个表面点p:
   a. 计算传输函数T(p, ω_i, ω_o)
   b. 投影到球谐函数: t_i(p) = ∫ T(p, ω_i, ω_o) * Y_i(ω_i) dω_i
   c. 存储系数t_i(p)

2. 对每个光照方向:
   a. 计算入射光照L(ω_i)
   b. 投影到球谐函数: l_i = ∫ L(ω_i) * Y_i(ω_i) dω_i
   c. 存储系数l_i
```

### 实时计算
```
Radiance(p, ω_o) ≈ Σ t_i(p) * l_i

计算复杂度: O(9) 或 O(16)，非常快！
```

---

## 6. 光源旋转

### 旋转矩阵
```
当光源旋转时，球谐系数需要旋转:

l'_i = Σ R_ij * l_j

其中:
  R_ij = 旋转矩阵元素
  l_j = 原始球谐系数
```

### 旋转矩阵计算
```
对于绕Y轴旋转角度θ:

R = [
  cos(θ)   0  sin(θ)
    0      1    0
  -sin(θ)  0  cos(θ)
]

然后应用到球谐系数
```

### 预计算旋转
```
离线预计算多个旋转角度的系数:
  - 0°, 15°, 30°, ..., 345°
  - 存储为txt文件
  - 运行时快速查询
```

---

## 7. 实现要点

### 采样点生成
```cpp
// Fibonacci球采样
for (int i = 0; i < numSamples; i++) {
    float phi = acos(1.0 - 2.0 * i / numSamples);
    float theta = 2.0 * PI * i / PHI;
    // 使用(theta, phi)作为采样点
}
```

### 球谐基函数计算
```cpp
vec3 evaluateSH(vec3 N) {
    float x = N.x, y = N.y, z = N.z;
    float x2 = x*x, y2 = y*y, z2 = z*z;
    
    vec3 shBasis[9] = {
        vec3(0.282095),
        vec3(0.488603 * y),
        vec3(0.488603 * z),
        vec3(0.488603 * x),
        vec3(1.092548 * x * y),
        vec3(1.092548 * y * z),
        vec3(0.315392 * (3.0*z2 - 1.0)),
        vec3(1.092548 * x * z),
        vec3(0.546274 * (x2 - y2))
    };
    
    // 计算投影
    return shBasis[0] * coeff[0] + ... + shBasis[8] * coeff[8];
}
```

### 系数投影
```cpp
// 投影光照到球谐函数
for (int i = 0; i < 9; i++) {
    coeff[i] = 0;
    for (int j = 0; j < numSamples; j++) {
        vec3 dir = sampleDirections[j];
        float light = sampleLight(dir);
        float basis = evaluateBasis(i, dir);
        coeff[i] += light * basis;
    }
    coeff[i] *= (4.0 * PI / numSamples);
}
```

---

## 8. 数据格式

### txt文件格式
```
# PRT Precomputed Data
# Format: SH Coefficients for different light rotations

# Light Rotation: 0 degrees
l00: 0.5 0.5 0.5
l1m1: 0.1 0.1 0.1
l10: 0.2 0.2 0.2
l1p1: 0.1 0.1 0.1
l2m2: 0.05 0.05 0.05
l2m1: 0.05 0.05 0.05
l20: 0.1 0.1 0.1
l2p1: 0.05 0.05 0.05
l2p2: 0.05 0.05 0.05

# Light Rotation: 15 degrees
l00: 0.5 0.5 0.5
...
```

---

## 9. 参考资源

### 论文
- "Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments" (Sloan et al., 2003)

### 关键概念
- 球谐函数的正交性
- 光照投影的数值积分
- 旋转矩阵的应用
- 实时relighting的优化

---

## 10. 下一步

### 实现计划
1. 实现球谐函数库
2. 实现光照采样和投影
3. 实现旋转矩阵计算
4. 实现数据导出
5. 实现数据导入和应用

