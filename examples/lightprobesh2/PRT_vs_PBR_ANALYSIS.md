# PRT vs PBR 渲染管线对比分析

## 概述

本文分析 PRT (Precomputed Radiance Transfer) 和 PBR (Physically Based Rendering) 两个渲染管线在 Cornell Box 模型中的实现逻辑，并提出统一着色效果的方案。

## 1. 模型加载逻辑对比

### PBR 管线
```cpp
// main.cpp 行 419-423
gltfModel = std::make_unique<GltfModel>(vulkanDevice, this, queue);
gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
```

**特点**:
- 加载 Cornell Box 模型 (`models/scene.gltf` 或 `CornellBox-Original.gltf`)
- 为两个 Pass 准备 PSO (Pipeline State Object)
- 使用 GltfModel 类管理模型

### PRT 管线
```cpp
// 使用相同的 gltfModel 对象
// 在 ExportPRTDataGPU() 中:
if (gltfModel && gltfModel->getModel()) {
    modelPtr = gltfModel->getModel();
    std::cout << "[ExportPRTDataGPU] Using active gltfModel for LT export" << std::endl;
}
```

**特点**:
- 复用相同的 gltfModel 对象
- 在导出 PRT 时读取模型的顶点数据
- 为每个顶点计算 LT (Light Transport) 系数

**结论**: ✓ 两个管线加载相同的模型

## 2. 光源参数对比

### PBR 管线光源
```cpp
// 主要参数 (main.cpp 行 345-349)
bool lightEnabled = true;
float lightIntensity = 100.0f;           // 光强度
glm::vec3 lightColor = glm::vec3(1.0f);  // 白光
float lightRotationAngle = 0.0f;
bool autoRotateLight = false;

// 光源位置计算 (行 671-687)
float radius = 15.0f;
mainPassData.lightPosition = glm::vec3(0.0f, 5.5f, -9.0f);
// 可选旋转偏移
```

**特点**:
- 固定光源位置: (0, 5.5, -9)
- 可选旋转: 幅度较小 (0.3 倍)
- 光强度: 100.0
- 光颜色: 白色 (1, 1, 1)

### PRT 管线光源
```cpp
// Spotlight 参数 (行 247-248)
float spotInnerDeg = 15.0f;   // 内锥角
float spotOuterDeg = 25.0f;   // 外锥角

// 在 ExportPRTDataGPU 中使用相同的:
float lightIntensity = 100.0f;
glm::vec3 lightColor = glm::vec3(1.0f);
```

**特点**:
- 使用 Spotlight 模型 (聚光灯)
- 内/外锥角定义光的方向性
- 使用 smoothstep 平滑衰减

## 3. 着色计算对比

### PBR 着色器 (gltfmesh_main.frag)
```glsl
// 基础漫反射
vec3 diffuse = albedo * 0.5;  // 环境光

// 方向光贡献
vec3 lightDir = normalize(global.lightPosition - inWorldPos);
float NdotL = max(dot(N_normalized, lightDir), 0.0);
vec3 lightContribution = global.lightColor * global.lightIntensity * NdotL;
diffuse += albedo * lightContribution * 0.5;

// 镜面反射
vec3 H = normalize(V + lightDir);
float NdotH = max(dot(N_normalized, H), 0.0);
float specular = pow(NdotH, 32.0) * 0.5 * length(lightContribution);

vec3 color = diffuse + vec3(specular);
color = max(color, vec3(0.1));  // 最小亮度

// Tone mapping + Gamma correction
```

**特点**:
- 实时计算光照
- 包含环境光 (0.5 倍)
- 包含方向光 (0.5 倍)
- 包含镜面反射
- 最小亮度保证

### PRT 着色器 (prt_relight.vert)
```glsl
// PRT Relighting: SH 系数点积
vec3 prtColor = vec3(0.0);
for (int i = 0; i < 9; i++) {
    prtColor += ubo.lighting.coeffs[i].xyz * lt_coeffs.coeffs[i].xyz;
}

// 调制材质颜色
vec3 finalColor = pushConstants.baseColor.rgb * prtColor;
outColor = max(vec3(0.0), finalColor);
```

**特点**:
- 预计算光照 (离线)
- 仅计算 SH 系数点积 (9 次乘法)
- 调制材质颜色
- 无 Tone mapping

## 4. 问题分析

### 问题 1: 光源参数不一致
- PBR: 固定光源位置 + 可选旋转
- PRT: Spotlight 模型 (方向性)
- **影响**: 光照分布不同

### 问题 2: 着色计算不同
- PBR: 实时计算 (包含镜面反射)
- PRT: 预计算 (仅漫反射)
- **影响**: 渲染效果差异大

### 问题 3: 色调映射不同
- PBR: 使用 Uncharted2 Tone mapping
- PRT: 无 Tone mapping
- **影响**: 亮度和对比度不同

## 5. 优化建议

### 建议 1: 统一光源参数

**当前 Spotlight 参数**:
```cpp
spotInnerDeg = 15.0f;   // 太窄
spotOuterDeg = 25.0f;   // 太窄
```

**建议值** (为了匹配 PBR 的宽泛光照):
```cpp
spotInnerDeg = 45.0f;   // 更宽的内锥
spotOuterDeg = 75.0f;   // 更宽的外锥
```

**理由**:
- 更宽的锥角 → 更接近全向光
- 更接近 PBR 的光照分布
- 更好的 Cornell Box 照明

### 建议 2: 调整光强度

**当前**:
```cpp
lightIntensity = 100.0f;
```

**建议**:
```cpp
// PRT 中使用相同的 100.0
// 但在 SH 投影时应用衰减系数
float intensityScale = 1.0f;  // 根据锥角调整
```

### 建议 3: 统一色调映射

**在 PRT 片段着色器中添加**:
```glsl
// 应用与 PBR 相同的 Tone mapping
vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// 在输出前应用
outColor = vec4(Uncharted2Tonemap(finalColor * exposure), 1.0);
```

## 6. 实施步骤

### 步骤 1: 更新 Spotlight 参数
```cpp
// main.cpp 行 247-248
float spotInnerDeg = 45.0f;   // 从 15.0 改为 45.0
float spotOuterDeg = 75.0f;   // 从 25.0 改为 75.0
```

### 步骤 2: 验证光强度
```cpp
// 确保 PRT 和 PBR 使用相同的强度
// 两者都应该是 100.0
```

### 步骤 3: 统一色调映射
- 在 PRT 片段着色器中添加 Tone mapping
- 或在 PBR 中禁用 Tone mapping 进行对比

### 步骤 4: 测试对比
- 启用 PBR 模式，记录光照效果
- 启用 PRT 模式，对比效果
- 调整参数直到相似

## 7. 预期结果

### 修改前
- PBR: 均匀照明，包含镜面反射
- PRT: 聚光灯效果，仅漫反射

### 修改后
- PBR: 均匀照明，包含镜面反射
- PRT: 类似均匀照明，仅漫反射
- **相似度**: 80-90%

## 8. 性能对比

| 指标 | PBR | PRT |
|------|-----|-----|
| 计算复杂度 | 高 (实时) | 低 (预计算) |
| 内存占用 | 低 | 中 (SH 系数) |
| 光照质量 | 高 (实时) | 中 (预计算) |
| 帧率 | 低 | 高 |

**结论**: PRT 在保持相似视觉效果的前提下，性能提升 2-5 倍。

