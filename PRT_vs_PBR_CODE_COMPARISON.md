# PRT vs PBR: 代码实现对比

## 1. PBR 着色器实现

### 运行时计算（每帧）
```glsl
// PBR Fragment Shader
#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

layout(binding = 0) uniform UBO {
    vec3 lightPos;      // 实时光源位置
    vec3 viewPos;       // 实时视点位置
    vec3 lightColor;
} ubo;

// PBR 参数
layout(binding = 1) uniform sampler2D albedoMap;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D roughnessMap;
layout(binding = 4) uniform sampler2D metallicMap;

// ==================== 关键点 ====================
// 这些计算在每个像素、每一帧都要进行
// ==========================================

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159 * denom * denom;
    
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

void main() {
    vec3 N = normalize(inNormal);
    vec3 V = normalize(ubo.viewPos - inPos);  // 实时计算视线方向
    vec3 L = normalize(ubo.lightPos - inPos); // 实时计算光线方向
    vec3 H = normalize(V + L);
    
    float distance = length(ubo.lightPos - inPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = ubo.lightColor * attenuation;
    
    vec3 albedo = texture(albedoMap, inUV).rgb;
    float roughness = texture(roughnessMap, inUV).r;
    float metallic = texture(metallicMap, inUV).r;
    
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    
    // ==================== 核心 BRDF 计算 ====================
    // 这些都依赖于 L 和 V 的方向
    // 光源移动 → L 改变 → BRDF 改变 → 着色改变
    // ========================================================
    
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.001;
    vec3 specular = numerator / denominator;
    
    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / 3.14159 + specular) * radiance * NdotL;
    
    outFragColor = vec4(Lo, 1.0);
}
```

### 关键特性
- ✅ **实时计算**：每帧都计算 BRDF
- ✅ **光源敏感**：L 改变 → 着色改变
- ✅ **视线敏感**：V 改变 → 着色改变
- ❌ **计算成本高**：大量三角函数和浮点运算

---

## 2. PRT 着色器实现

### 离线预计算（一次性）
```cpp
// 离线预计算程序（C++）
// 这个程序只运行一次，生成 PRT 系数

struct PRTData {
    std::vector<glm::vec3> coefficients; // 存储 SH 系数
};

void computePRTCoefficients(const Mesh& mesh, PRTData& prtData) {
    const int SH_BANDS = 3;  // 使用 3 阶球谐函数（9 个系数）
    
    for (const auto& vertex : mesh.vertices) {
        glm::vec3 normal = vertex.normal;
        
        // 对于这个顶点，计算其对每个 SH 基函数的响应
        std::vector<glm::vec3> shCoeffs(SH_BANDS * SH_BANDS, glm::vec3(0.0f));
        
        // 采样半球上的方向
        int numSamples = 10000;
        for (int i = 0; i < numSamples; i++) {
            glm::vec3 sampleDir = uniformSampleHemisphere(normal);
            
            // 计算这个方向上的 BRDF 响应
            // 注意：这里 L 是固定的采样方向，不是实时光源
            float visibility = computeVisibility(vertex.pos, sampleDir);
            float cosine = glm::dot(normal, sampleDir);
            
            if (cosine > 0.0f && visibility > 0.5f) {
                // 计算 SH 基函数值
                std::vector<float> shBasis = evaluateSHBasis(sampleDir, SH_BANDS);
                
                // 累加到系数中
                for (int j = 0; j < SH_BANDS * SH_BANDS; j++) {
                    shCoeffs[j] += glm::vec3(cosine * visibility) * shBasis[j];
                }
            }
        }
        
        prtData.coefficients.push_back(shCoeffs);
    }
    
    // 保存到文件
    savePRTData("model.prt", prtData);
}
```

### 运行时渲染（简单快速）
```glsl
// PRT Fragment Shader
#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in flat uint inVertexIndex;

layout(location = 0) out vec4 outFragColor;

layout(binding = 0) uniform UBO {
    vec3 lightPos;      // 这个值改变也不会影响 PRT 的着色
    vec3 viewPos;
    vec3 lightColor;
} ubo;

// ==================== 关键点 ====================
// PRT 系数是预计算的，存储在 SSBO 或纹理中
// 运行时只需要做简单的点积
// ==========================================

layout(binding = 1) buffer PRTCoefficients {
    vec3 prtCoeffs[];  // 预计算的系数
};

layout(binding = 2) uniform SHCoefficients {
    vec3 shCoeffs[9];  // 当前光照环境的 SH 系数
};

void main() {
    // ==================== 极其简单的计算 ====================
    // 只需要一个点积操作！
    // ============================================================
    
    vec3 color = vec3(0.0);
    
    // 对每个 SH 系数做点积
    for (int i = 0; i < 9; i++) {
        color += prtCoeffs[inVertexIndex * 9 + i] * shCoeffs[i];
    }
    
    // 应用光源颜色
    color *= ubo.lightColor;
    
    outFragColor = vec4(color, 1.0);
}
```

### 关键特性
- ✅ **预计算**：BRDF 已经编码到系数中
- ❌ **光源不敏感**：改变 lightPos 不会改变着色
- ✅ **计算成本低**：只需 9 次乘法和加法
- ✅ **支持全局光照**：预计算可以包含 GI 信息

---

## 3. 性能对比

### PBR 性能
```
每像素计算量：
- Fresnel 计算：1 次
- Distribution 计算：1 次（包含 pow）
- Geometry 计算：2 次（包含 sqrt）
- 总计：大量三角函数和浮点运算

典型性能：
- 1080p 分辨率：10-30 ms/frame（取决于光源数量）
- 4K 分辨率：40-100 ms/frame
```

### PRT 性能
```
每像素计算量：
- 9 次乘法
- 8 次加法
- 总计：18 次基本操作

典型性能：
- 1080p 分辨率：1-2 ms/frame
- 4K 分辨率：3-5 ms/frame
```

### 性能提升
```
PRT 相比 PBR：5-20 倍性能提升
```

---

## 4. 光源移动时的行为对比

### 场景：光源从右上移动到下方

#### PBR 行为
```
时刻 1：光源在右上
┌─────────────────┐
│ 右侧面向光源    │
│ → 亮色          │
│ 左侧背离光源    │
│ → 暗色          │
└─────────────────┘

时刻 2：光源在下方
┌─────────────────┐
│ 右侧背离光源    │
│ → 暗色          │
│ 下侧面向光源    │
│ → 亮色          │
└─────────────────┘

结果：着色效果明显改变 ✅
```

#### PRT 行为
```
时刻 1：光源在右上
┌─────────────────┐
│ 预计算的着色    │
│ 固定不变        │
└─────────────────┘

时刻 2：光源在下方
┌─────────────────┐
│ 预计算的着色    │
│ 仍然固定不变    │
└─────────────────┘

结果：着色效果完全相同 ✅
```

---

## 5. 为什么 PRT 的着色看起来"不如" PBR？

这不是质量问题，而是**应用场景**不同：

### PRT 的优势场景
```glsl
// 场景：静态环境光照
// 例如：室内场景，光照来自天空盒

// PRT 可以预计算：
// - 全局光照（GI）
// - 环境遮挡（AO）
// - 软阴影

// 结果：看起来比 PBR 更逼真（因为包含了 GI）
```

### PBR 的优势场景
```glsl
// 场景：动态光源
// 例如：手电筒、汽车灯、爆炸光

// PBR 可以：
// - 实时响应光源移动
// - 处理任意数量的光源
// - 支持光源颜色实时改变

// 结果：灵活性更高
```

---

## 6. 总结表格

| 方面 | PBR | PRT |
|------|-----|-----|
| **计算时机** | 运行时每帧 | 离线一次 |
| **光源响应** | 实时改变 | 完全不改变 |
| **计算复杂度** | O(n) 其中 n 是光源数 | O(1) |
| **内存占用** | 低 | 高（存储系数） |
| **全局光照** | 不支持 | 支持 |
| **动态光源** | 支持 | 不支持 |
| **预计算成本** | 无 | 高 |
| **适用场景** | 动态场景 | 静态场景 |

---

## 7. 你的实现中为什么 PRT 显示"Inactive"？

### 可能的原因

```cpp
// 检查点 1：PRT 系数是否已加载？
if (prtCoefficients.empty()) {
    // PRT 还未初始化
    // 需要运行预计算程序
    return;
}

// 检查点 2：PRT 着色器是否已启用？
if (!usePRT) {
    // 仍在使用 PBR 着色器
    return;
}

// 检查点 3：SH 系数是否已更新？
if (!updateSHCoefficients(lightEnvironment)) {
    // 光照环境的 SH 系数未更新
    return;
}
```

### 启用 PRT 的步骤

1. **离线预计算**
   ```cpp
   computePRTCoefficients(mesh, prtData);
   savePRTData("model.prt", prtData);
   ```

2. **加载 PRT 数据**
   ```cpp
   PRTData prtData = loadPRTData("model.prt");
   uploadToGPU(prtData);
   ```

3. **启用 PRT 着色器**
   ```cpp
   useShader(prtShader);
   setUniform("usePRT", true);
   ```

4. **更新 SH 系数**
   ```cpp
   updateSHCoefficients(currentLightEnvironment);
   ```

---

## 8. 关键理解

```
❌ 错误理解：
   "PRT 是 PBR 的优化版本"
   → 这会导致期望 PRT 有 PBR 的所有特性

✅ 正确理解：
   "PRT 和 PBR 是两种完全不同的技术"
   
   PBR = 实时计算 BRDF
   PRT = 预计算 BRDF 响应
   
   → 它们有不同的优势和劣势
   → 应该根据场景选择合适的技术
```

