# PRT (Precomputed Radiance Transfer) 实现逻辑检查

## 核心概念

PRT的基本公式：
```
L_out(x, ω_o) = ∫ L_in(ω_i) * T(x, ω_i, ω_o) * dω_i
```

其中：
- **L_in(ω_i)**: 入射光照 (Lighting)
- **T(x, ω_i, ω_o)**: Light Transport (物体表面对光照的响应)
- **L_out(x, ω_o)**: 出射光照

## PRT预计算的三个关键步骤

### 1. 预计算Lighting (光源的球谐系数)

**目的**: 将光源环境表示为球谐函数系数

**实现**:
```cpp
// 从光源环境采样
auto directions = GenerateFibonacciSamples(numSamples);
std::vector<glm::vec3> radiances;
for (int i = 0; i < numSamples; i++) {
    radiances[i] = SampleLightEnvironment(directions[i]);
}

// 投影到球谐基函数
SHCoefficients lightingCoeffs = ProjectLight(directions, radiances);
```

**关键点**:
- 采样方向应该均匀分布在球面上
- 辐射度应该从实际光源环境采样 (环境贴图、点光源等)
- 投影公式: `coeff[i] = (4π/N) * Σ(radiance * basis[i])`

### 2. 预计算Light Transport (物体表面响应)

**目的**: 计算物体表面对入射光的响应

**实现**:
```cpp
SHCoefficients ltCoeffs = PrecomputeLightTransport(
    position,
    normal,
    albedo,
    sampleDirections
);
```

**Light Transport计算**:
```
LT(x, ω_i) = albedo * max(0, dot(normal, ω_i)) * visibility(x, ω_i)
```

**关键点**:
- **albedo**: 表面反射率
- **cosine term**: Lambert's law (max(0, dot(N, L)))
- **visibility**: 阴影/遮挡项 (可选，简化版本可忽略)
- 对每个顶点/像素预计算一次

### 3. 预计算光源旋转

**目的**: 预计算不同旋转角度下的光照系数

**实现**:
```cpp
std::vector<RotatedCoefficients> rotations;
for (int i = 0; i < numRotations; i++) {
    float angle = (i / numRotations) * 360.0f;
    SHCoefficients rotated = RotateSHY(lightingCoeffs, angle);
    rotations.push_back({angle, rotated});
}
```

**关键点**:
- 使用球谐旋转矩阵进行旋转
- 预计算24-36个旋转角度 (每15-10度一个)
- 运行时通过插值查询

## 运行时Relighting

**公式**:
```
L_out = ∫ L_in(ω_i) * LT(ω_i) * dω_i
      ≈ Σ L_in_coeff[i] * LT_coeff[i]
```

**实现**:
```cpp
// 查询当前旋转角度的光照系数
SHCoefficients currentLighting = QueryCoefficients(currentAngle, precomputedRotations);

// 计算relighting
for (int i = 0; i < 9; i++) {
    outColor += currentLighting.coeffs[i] * ltCoeffs.coeffs[i];
}
```

## 代码逻辑检查结果

### ✅ 正确的部分
1. 球谐基函数计算 (EvaluateBasis)
2. 投影公式 (ProjectLight)
3. 重建公式 (ReconstructLight)
4. Fibonacci采样
5. 线性插值

### ❌ 需要修复的部分

#### 1. 球谐旋转矩阵 (已修复)
- **问题**: 原始实现直接复制系数，没有进行实际旋转
- **修复**: 实现了正确的2阶球谐旋转矩阵

#### 2. Light Transport预计算 (已添加)
- **问题**: 原始代码只预计算了Lighting，没有预计算Light Transport
- **修复**: 添加了PrecomputeLightTransport函数

#### 3. 预计算流程 (已改进)
- **问题**: PrecomputePRT函数没有区分Lighting和Light Transport
- **修复**: 分离为4个清晰的步骤

## 数据导出格式

```
# PRT Precomputed Radiance Transfer Data
# Generated: 2025-11-27
# Rotations: 24
# SH Order: 2 (9 coefficients)

# Light Rotation: 0 degrees
c00.x c00.y c00.z c1m1.x c1m1.y c1m1.z ... c22.x c22.y c22.z

# Light Rotation: 15 degrees
...
```

## 性能优化建议

1. **预计算缓存**: 将预计算结果存储在GPU纹理中
2. **LOD**: 对不同距离的物体使用不同阶数的球谐
3. **压缩**: 使用量化存储球谐系数
4. **并行化**: 对多个顶点并行预计算

## 参考资源

- Sloan et al. "Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments"
- GAMES202 Assignment 2
- Spherical Harmonics Lighting: The Gritty Details

