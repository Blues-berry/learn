# PRT vs PBR 技术分析

## 基本概念

### PBR (Physically Based Rendering)

PBR 在每一帧计算光照：

```glsl
// PBR 着色器
vec3 lightDir = normalize(lightPosition - worldPos);
float NdotL = max(dot(normal, lightDir), 0.0);
vec3 color = albedo * lightColor * lightIntensity * NdotL * 0.1;
```

**特点**：
- 实时计算每个像素的光照
- 支持动态光源
- 计算成本高

### PRT (Precomputed Radiance Transfer)

PRT 预先计算光照信息，运行时只需重建：

```glsl
// PRT 着色器
vec3 lighting = ReconstructLighting(normal);  // 从 SH 系数重建
vec3 color = albedo * lighting;
```

**特点**：
- 预计算阶段计算 SH 系数
- 运行时只需简单的 SH 重建
- 计算成本低，但灵活性受限

## 为什么需要修复

### 问题 1: 光源颜色的应用时机

**错误的方式**（原始代码）：

```cpp
// 预计算时
radiances.push_back(lightColor * lightIntensity);  // 应用颜色
SHCoefficients lightingCoeffs = PrecomputeLighting(directions, radiances);

// 运行时
for (int i = 0; i < 9; ++i) {
    coeffs[i] *= lightColor * intensityScale;  // 再次应用颜色！
}
```

**问题**：
- 颜色被应用了两次（平方效应）
- 改变光源颜色时，预计算数据不会改变
- 只有运行时的乘法会改变，但基础数据已固定

**正确的方式**（修复后）：

```cpp
// 预计算时：使用单位光源
radiances.push_back(glm::vec3(1.0f, 1.0f, 1.0f));  // 单位光源
SHCoefficients lightingCoeffs = PrecomputeLighting(directions, radiances);

// 运行时：应用实际的光源颜色和强度
for (int i = 0; i < 9; ++i) {
    coeffs[i] *= lightColor * intensityScale;  // 只应用一次
}
```

**优势**：
- 光源颜色只应用一次
- 改变光源颜色时，PRT 会正确响应
- 与 PBR 的行为一致

### 问题 2: 材质 vs 光源

**错误的方式**（原始代码）：

```cpp
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    materialData.elbedo = glm::vec4(color, 1.0f);  // ❌ 修改材质！
}
```

**问题**：
- 光源颜色被错误地应用到材质
- 改变光源颜色实际上改变了模型的颜色
- PBR 计算：`color = albedo * lightColor * intensity`
  - 如果 albedo = 绿色，lightColor = 白色
  - 结果是绿色（看起来像光源是红色）

**正确的方式**（修复后）：

```cpp
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    // NO-OP：光源颜色是全局属性，不应该修改材质
}
```

**优势**：
- 光源颜色只影响全局光照
- 模型保持其原始的材质属性
- PBR 正确显示光源颜色

## 数学原理

### SH 投影与重建

**投影**（预计算）：
```
L(θ, φ) ≈ Σ c_i * Y_i(θ, φ)
```

其中 `c_i` 是 SH 系数，`Y_i` 是 SH 基函数。

**重建**（运行时）：
```
L(normal) = Σ c_i * Y_i(normal)
```

### 光源颜色的应用

**错误方式**：
```
c_i(color) = c_i(white) * color
c_i_final = c_i(color) * color  // 颜色被平方！
```

**正确方式**：
```
c_i = c_i(white)  // 预计算时使用白色
c_i_final = c_i * color  // 运行时应用颜色
```

## 验证方法

### 方法 1: 颜色一致性

```
PBR 光照 = albedo * lightColor * intensity * NdotL
PRT 光照 = albedo * ReconstructLighting(normal)
         = albedo * (SH_coeffs * lightColor * intensity)
```

两者应该相等（在 SH 近似范围内）。

### 方法 2: 旋转响应

改变光源方向时：
- PBR：`NdotL` 改变 → 光照改变
- PRT：`ReconstructLighting()` 改变 → 光照改变

两者应该显示相似的变化。

## 性能考虑

### PBR
- 每像素计算：O(1)
- 总成本：O(pixels)

### PRT
- 预计算：O(samples * coefficients)
- 每像素：O(coefficients)
- 总成本：O(samples * coefficients) + O(pixels * coefficients)

对于 9 个 SH 系数，PRT 通常更快。

## 扩展建议

1. **支持多个光源**：为每个光源维护单独的 SH 系数
2. **环境光**：使用环境贴图的 SH 投影
3. **动态阴影**：在 Light Transport 中包含可见性项
4. **更高阶 SH**：使用 16 或 25 个系数以获得更好的精度

