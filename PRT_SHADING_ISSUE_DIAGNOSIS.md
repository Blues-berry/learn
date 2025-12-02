# PRT 着色问题诊断

## 问题描述

开启光照后，PRT 无法正常着色。

## 根本原因分析

### 问题 1: PRT 着色器缺少 Light Transport 应用

**PRT 着色器** (`prt_relighting.frag` 第 119 行)：
```glsl
// 应用材质颜色和光照
vec3 finalColor = albedo * lighting;
```

**问题**：
- `lighting` 只是从 Lighting SH 系数重建的光照
- 没有应用 Light Transport (LT) 系数
- LT 包含了表面的 cosine 项和 albedo 信息
- 这导致着色完全错误

### 问题 2: PBR 着色器的光照计算

**PBR 着色器** (`gltfmesh_main.frag` 第 169 行)：
```glsl
vec3 color = albedo * global.lightColor * global.lightIntensity * NdotL * 0.1;
```

**特点**：
- 包含 `NdotL` 项（Lambert cosine term）
- 这个 cosine 项应该在 PRT 的 Light Transport 中

### 问题 3: PRT 的数学模型

**正确的 PRT 公式**：
```
最终光照 = Lighting_SH ⊗ LightTransport_SH
         = Σ L_i * LT_i
```

其中：
- `L_i` = Lighting SH 系数（光源的球谐投影）
- `LT_i` = Light Transport SH 系数（表面对光照的响应，包含 cosine 和 albedo）

**当前实现**：
```glsl
lighting = ReconstructLighting(N);  // 只重建 Lighting
finalColor = albedo * lighting;     // 错误地应用 albedo
```

这相当于：
```
最终光照 = albedo * L_i * Y_i(N)
```

但应该是：
```
最终光照 = Σ L_i * LT_i
```

## 解决方案

### 方案 1: 在着色器中进行 SH 卷积（推荐）

修改 `prt_relighting.frag`：

```glsl
// 添加 Light Transport SH 系数
layout (set = 1, binding = 2) uniform LightTransportSH {
    vec4 lt00, lt1m1, lt10, lt1p1, lt2m2, lt2m1, lt20, lt2p1, lt2p2;
} ltSH;

// 修改主函数
void main() {
    vec3 N = normalize(inNormal);
    
    // 计算 SH 基函数
    vec3 shBasis[9] = vec3[](
        vec3(0.282095),
        vec3(0.488603 * N.y),
        vec3(0.488603 * N.z),
        vec3(0.488603 * N.x),
        vec3(1.092548 * N.x * N.y),
        vec3(1.092548 * N.y * N.z),
        vec3(0.315392 * (3.0 * N.z * N.z - 1.0)),
        vec3(1.092548 * N.x * N.z),
        vec3(0.546274 * (N.x * N.x - N.y * N.y))
    );
    
    // 进行 SH 卷积：L ⊗ LT
    vec3 lighting = vec3(0.0);
    lighting += lightingSH.l00.rgb * ltSH.lt00.rgb * shBasis[0];
    lighting += lightingSH.l1m1.rgb * ltSH.lt1m1.rgb * shBasis[1];
    lighting += lightingSH.l10.rgb * ltSH.lt10.rgb * shBasis[2];
    lighting += lightingSH.l1p1.rgb * ltSH.lt1p1.rgb * shBasis[3];
    lighting += lightingSH.l2m2.rgb * ltSH.lt2m2.rgb * shBasis[4];
    lighting += lightingSH.l2m1.rgb * ltSH.lt2m1.rgb * shBasis[5];
    lighting += lightingSH.l20.rgb * ltSH.lt20.rgb * shBasis[6];
    lighting += lightingSH.l2p1.rgb * ltSH.lt2p1.rgb * shBasis[7];
    lighting += lightingSH.l2p2.rgb * ltSH.lt2p2.rgb * shBasis[8];
    
    // 不再应用 albedo（已在 LT 中）
    vec3 finalColor = lighting;
    
    // Tone mapping 和 gamma correction
    finalColor = Uncharted2Tonemap(finalColor * global.exposure);
    finalColor = finalColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    finalColor = pow(finalColor, vec3(1.0f / global.gamma));
    
    outColor = vec4(finalColor, 1.0);
}
```

### 方案 2: 在 CPU 端进行卷积

在 `UpdatePRTLighting()` 中：

```cpp
// 进行 SH 卷积：L ⊗ LT
SHCoefficients convolvedCoeffs;
for (int i = 0; i < 9; ++i) {
    convolvedCoeffs.coeffs[i] = currentSHCoefficients.coeffs[i] * precomputedLTCoefficients[0].coeffs[i];
}

// 应用光源颜色和强度
float intensityScale = (lightIntensity / 100.0f) * 0.1f;
for (int i = 0; i < 9; ++i) {
    convolvedCoeffs.coeffs[i] *= lightColor * intensityScale;
}

// 更新 UBO
PRT::GPUSHCoefficients gpuLighting{};
for (int i = 0; i < 9; ++i) {
    gpuLighting.coeffs[i] = glm::vec4(convolvedCoeffs.coeffs[i], 0.0f);
}
memcpy(lightingSHBuffer.mapped, &gpuLighting, sizeof(PRT::GPUSHCoefficients));
```

然后在着色器中简化为：

```glsl
void main() {
    vec3 N = normalize(inNormal);
    vec3 lighting = ReconstructLighting(N);  // 已经是卷积后的结果
    vec3 finalColor = lighting;  // 不再应用 albedo
    
    // Tone mapping 和 gamma correction
    finalColor = Uncharted2Tonemap(finalColor * global.exposure);
    finalColor = finalColor * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    finalColor = pow(finalColor, vec3(1.0f / global.gamma));
    
    outColor = vec4(finalColor, 1.0);
}
```

## 推荐方案

**方案 2（CPU 端卷积）** 更好，因为：

1. **性能**：卷积在 CPU 端只做一次，而不是每个像素都做
2. **简洁**：着色器代码更简单
3. **灵活**：支持多个表面的不同 LT 系数

## 实现步骤

1. 在 `UpdatePRTLighting()` 中添加 SH 卷积
2. 修改 `prt_relighting.frag` 移除 albedo 应用
3. 编译和测试

## 验证

修复后应该看到：
- ✅ PRT 显示正确的着色
- ✅ 光源颜色改变时有响应
- ✅ 旋转光源时着色改变
- ✅ 与 PBR 的效果相似

