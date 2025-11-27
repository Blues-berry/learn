# PRT (Precomputed Radiance Transfer) 正确实现指南

## 核心问题修复

### 问题1: 只预计算了Lighting，没有预计算Light Transport
**原因**: 原始代码没有区分这两个概念
**修复**: 添加了 `PrecomputeLightTransport()` 函数

### 问题2: 球谐旋转矩阵不正确
**原因**: 原始实现直接复制系数，没有进行实际旋转
**修复**: 实现了正确的2阶球谐旋转矩阵

### 问题3: 数据导出不完整
**原因**: 只导出了一个文件，没有分离Lighting和Light Transport
**修复**: 分别导出为两个文件

---

## 正确的PRT预计算流程

### 第1步: 生成采样方向
```cpp
auto directions = SphericalHarmonics::GenerateFibonacciSamples(numSamples);
// 生成均匀分布在球面上的采样方向
```

### 第2步: 预计算Lighting (光源的球谐系数)
```cpp
// 从光源环境采样辐射度
std::vector<glm::vec3> radiances;
for (int i = 0; i < numSamples; i++) {
    radiances[i] = SampleLightEnvironment(directions[i]);
}

// 投影到球谐基函数
SHCoefficients lightingCoeffs = ProjectLight(directions, radiances);
```

**输出**: 9个vec3系数，表示光源的球谐表示

### 第3步: 预计算Light Transport (物体表面响应)
```cpp
SHCoefficients ltCoeffs = PrecomputeLightTransport(
    position,
    normal,
    albedo,
    directions
);
```

**计算公式**:
```
LT[i] = (4π/N) * Σ(j=0 to N-1) {
    albedo * max(0, dot(normal, direction[j])) * basis[i](direction[j])
}
```

**输出**: 9个vec3系数，表示物体表面对光照的响应

### 第4步: 预计算光源旋转
```cpp
std::vector<RotatedCoefficients> rotations;
for (int i = 0; i < numRotations; i++) {
    float angle = (i / numRotations) * 360.0f;
    SHCoefficients rotated = RotateSHY(lightingCoeffs, angle);
    rotations.push_back({angle, rotated});
}
```

**输出**: 24-36个旋转角度对应的Lighting系数

### 第5步: 导出数据
```cpp
// 导出Lighting (带旋转)
ExportLighting("prt_data_lighting.txt", rotations);

// 导出Light Transport
ExportLightTransport("prt_data_lt.txt", ltCoeffs);
```

**文件格式**:

**prt_data_lighting.txt**:
```
# PRT Lighting Data (Rotated)
# Rotations: 24
# SH Order: 2 (9 coefficients)
# Format: angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

0 c00.x c00.y c00.z c1m1.x c1m1.y c1m1.z ... c22.x c22.y c22.z
15 ...
30 ...
...
```

**prt_data_lt.txt**:
```
# PRT Light Transport Data
# SH Order: 2 (9 coefficients)
# Format: coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

lt00.x lt00.y lt00.z lt1m1.x lt1m1.y lt1m1.z ... lt22.x lt22.y lt22.z
```

---

## 运行时Relighting计算

### 公式
```
L_out = Σ(i=0 to 8) Lighting[i] * LightTransport[i]
```

### 实现步骤

1. **查询当前旋转角度的Lighting系数**
```cpp
float currentAngle = lightRotationAngle * 180.0f / PI;
SHCoefficients currentLighting = QueryCoefficients(currentAngle, rotations);
```

2. **计算Relighting**
```cpp
glm::vec3 relitColor = glm::vec3(0.0f);
for (int i = 0; i < 9; i++) {
    relitColor += currentLighting.coeffs[i] * ltCoeffs.coeffs[i];
}
```

3. **应用到着色器**
```glsl
// 在片段着色器中
vec3 lighting = vec3(0.0);
for (int i = 0; i < 9; i++) {
    lighting += lightingCoeffs[i] * ltCoeffs[i];
}
finalColor = albedo * lighting;
```

---

## 数据流图

```
┌─────────────────────────────────────────────────────────────┐
│                    预计算阶段 (离线)                          │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  光源环境 ──→ 采样方向 ──→ 投影到SH ──→ Lighting系数        │
│                                                               │
│  物体表面 ──→ 采样方向 ──→ 投影到SH ──→ Light Transport系数  │
│                                                               │
│  Lighting系数 ──→ 旋转预计算 ──→ 旋转Lighting系数            │
│                                                               │
│  导出: prt_data_lighting.txt, prt_data_lt.txt               │
│                                                               │
└─────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────┐
│                    运行时阶段 (实时)                          │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  导入: prt_data_lighting.txt, prt_data_lt.txt               │
│                                                               │
│  当前旋转角度 ──→ 查询Lighting系数 (带插值)                  │
│                                                               │
│  Lighting[i] * LightTransport[i] ──→ 求和 ──→ 最终颜色      │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| SH阶数 | 2 | 9个系数 |
| 采样数 | 16-64 | 越多越精确，但计算越慢 |
| 旋转数 | 24-36 | 每15-10度一个 |
| 插值方式 | 线性 | 运行时查询时使用 |

---

## 验证检查清单

- [ ] Lighting系数已正确计算并导出
- [ ] Light Transport系数已正确计算并导出
- [ ] 旋转系数已正确预计算
- [ ] 文件格式正确
- [ ] 运行时能正确导入数据
- [ ] Relighting公式正确实现
- [ ] 光源旋转时颜色正确变化

