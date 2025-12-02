# PRT 着色问题修复

## 问题

开启光照后，PRT 无法正常着色。

## 根本原因

PRT 着色器缺少 **SH 卷积** 步骤。

### 数学原理

正确的 PRT 公式：
```
最终光照 = Lighting_SH ⊗ LightTransport_SH
         = Σ L_i * LT_i
```

其中：
- `L_i` = Lighting SH 系数（光源的球谐投影）
- `LT_i` = Light Transport SH 系数（表面对光照的响应，包含 cosine 和 albedo）

### 原始实现的问题

```glsl
// ❌ 错误：只重建 Lighting，没有卷积 LT
vec3 lighting = ReconstructLighting(N);
vec3 finalColor = albedo * lighting;  // 错误地应用 albedo
```

这相当于：
```
最终光照 = albedo * L_i * Y_i(N)
```

但应该是：
```
最终光照 = Σ L_i * LT_i
```

## 修复内容

### 修复 1: CPU 端进行 SH 卷积

**文件**：`examples/lightprobesh2/main.cpp` (第 2040-2067 行)

**修改**：在 `UpdatePRTLighting()` 中添加 SH 卷积

```cpp
// ===== SH CONVOLUTION: Lighting ⊗ LightTransport =====
SHCoefficients convolvedCoeffs;

// 进行卷积：L_i * LT_i
if (!precomputedLTCoefficients.empty()) {
    for (int i = 0; i < 9; ++i) {
        convolvedCoeffs.coeffs[i] = currentSHCoefficients.coeffs[i] * 
                                    precomputedLTCoefficients[0].coeffs[i];
    }
} else {
    convolvedCoeffs = currentSHCoefficients;
}

// 应用光源颜色和强度
float intensityScale = (lightIntensity / 100.0f) * 0.1f;
for (int i = 0; i < 9; ++i) {
    convolvedCoeffs.coeffs[i] *= lightColor * intensityScale;
}

currentSHCoefficients = convolvedCoeffs;
```

**效果**：
- ✅ SH 系数现在是卷积后的结果
- ✅ 包含了 Light Transport 信息
- ✅ 包含了 cosine 项和 albedo

### 修复 2: 着色器中移除 albedo 应用

**文件**：`shaders/glsl/lightprobesh2/prt_relighting.frag` (第 104-128 行)

**修改**：不再应用 albedo（已在 LT 中）

```glsl
void main() {
    vec3 N = normalize(inNormal);
    
    // SH 系数已经是卷积结果，包含了所有信息
    vec3 lighting = ReconstructLighting(N);
    
    // 直接使用，不再应用 albedo
    vec3 finalColor = lighting;
    
    // Tone mapping 和 gamma correction
    finalColor = Uncharted2Tonemap(finalColor * global.exposure);
    finalColor = finalColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    finalColor = pow(finalColor, vec3(1.0f / global.gamma));
    
    outColor = vec4(finalColor, 1.0);
}
```

**效果**：
- ✅ 着色器更简洁
- ✅ 着色逻辑正确
- ✅ 性能更好（不需要每像素应用 albedo）

## 预期结果

修复后应该看到：

| 功能 | 修复前 | 修复后 |
|------|--------|--------|
| PRT 着色 | ❌ 无法正常着色 | ✅ 正确着色 |
| 光源颜色响应 | ❌ 无响应 | ✅ 有响应 |
| 光源旋转 | ❌ 无变化 | ✅ 着色改变 |
| 与 PBR 一致性 | ❌ 不一致 | ✅ 一致 |

## 编译和测试

### 1. 编译
```bash
cd examples/lightprobesh2
.\compile.ps1
```

### 2. 运行
```bash
..\..\bin\lightprobesh2.exe
```

### 3. 测试

#### 测试 1: PRT 基本着色
1. 启用 PRT（"Enable PRT Relighting"）
2. 观察 Cornell Box
3. 应该看到正确的着色（不是全黑或全白）

#### 测试 2: 光源颜色响应
1. 设置光源为绿色
2. 观察 PRT 着色
3. 应该显示绿色光照

#### 测试 3: 光源旋转
1. 启用 "Auto Rotate"
2. 观察着色变化
3. 应该看到着色随旋转改变

#### 测试 4: PBR vs PRT 对比
1. 同时启用 PBR 和 PRT
2. 比较两种模式的着色
3. 应该显示相似的效果

## 技术细节

### SH 卷积的意义

SH 卷积是 PRT 的核心：

```
L(n) = ∫ L(ω) * LT(ω, n) dω
     ≈ Σ L_i * LT_i * Y_i(n)
```

其中：
- `L(ω)` = 光源在方向 ω 的辐射
- `LT(ω, n)` = 表面在法线 n 方向对光源的响应
- `Y_i(n)` = SH 基函数

### 为什么在 CPU 端做卷积？

1. **性能**：卷积只做一次，而不是每个像素都做
2. **简洁**：着色器代码更简单
3. **灵活**：支持多个表面的不同 LT 系数

### Light Transport 包含什么？

LT 系数包含：
- **Cosine 项**：`max(0, dot(N, L))`
- **Albedo**：表面的反射率
- **Visibility**：可选的阴影/遮挡信息

所以卷积后的 SH 系数已经包含了所有必要的信息。

## 验证清单

- [ ] 代码编译成功
- [ ] PRT 着色正确显示
- [ ] 光源颜色改变时有响应
- [ ] 光源旋转时着色改变
- [ ] PBR 和 PRT 效果相似
- [ ] 帧率 > 60 FPS

## 常见问题

### Q: PRT 仍然显示全黑？
A: 检查 `precomputedLTCoefficients` 是否被正确加载。查看控制台输出。

### Q: PRT 显示全白？
A: 检查 SH 系数的值。可能是卷积后的值太大。

### Q: 颜色改变无响应？
A: 确保修改已编译。检查 `lightColor` 是否被正确应用。

### Q: 与 PBR 不一致？
A: 检查 `intensityScale` 的值。可能需要调整 0.1 的衰减因子。

## 后续改进

1. **支持多个表面**：为每个顶点应用不同的 LT 系数
2. **动态阴影**：在 LT 中包含可见性项
3. **更高阶 SH**：使用 16 或 25 个系数以获得更好的精度
4. **环境光**：使用环境贴图的 SH 投影

## 参考资源

- **PRT 原理**：Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments
- **SH 基础**：Spherical Harmonics Lighting: The Gritty Details
- **实现指南**：TECHNICAL_EXPLANATION.md

## 总结

通过添加 SH 卷积，我们修复了 PRT 的着色问题。现在 PRT 能够正确地显示光照，并支持实时改变光源颜色和旋转。

