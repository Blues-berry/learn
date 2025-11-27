# Cornell Box Relighting 研究报告

## 1. 项目概述

本项目目标是实现基于PRT (Precomputed Radiance Transfer) 的Cornell Box场景relighting功能，使用球谐函数预计算光照信息。

---

## 2. Cornell Box 相关资源

### 2.1 模型文件
- **位置**: `assets/models/`
- **文件**:
  - `cornell/cornell.gltf` - 新版Cornell Box模型（1834个顶点，5976个索引）
  - `CornellBox-Original.gltf` - 原始Cornell Box模型（多个primitive，8个材质）
  - `scene.gltf` - 场景模型

### 2.2 模型特性
- **材质**: 8个不同的材质（红墙、绿墙、白墙、天花板、地板等）
- **顶点属性**: POSITION, TEXCOORD_0, NORMAL
- **缩放**: 0.025倍（原始模型缩放）
- **旋转**: 四元数旋转 (0.707, 0, 0, 0.707)

### 2.3 光照配置 (main.cpp)
```cpp
// 光源位置（固定）
lightPosition = glm::vec3(0.0f, 5.5f, -9.0f);

// 光源旋转（可选）
if (autoRotateLight) {
    lightPosition.x += radius * sin(lightRotationAngle) * 0.3f;
    lightPosition.z += radius * cos(lightRotationAngle) * 0.3f;
    lightPosition.y += radius * cos(lightRotationAngle) * 0.3f;
}

// 光源参数
lightColor = glm::vec3(1.0f, 1.0f, 1.0f);  // 默认白光
lightIntensity = 1.0f;
```

---

## 3. Preview Model 相关资源

### 3.1 核心类
- **文件**: `examples/lightprobesh2/PreviewModel.cpp/h`
- **功能**: 渲染光源预览球体

### 3.2 材质结构
```cpp
struct MaterialBuffer {
    float roughness = 1.f;
    float metallic = 0.5;
    float specular = 0.5;
    int32_t useLighting = 1;
    glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);  // 颜色
    int32_t useSH = 1;
    int32_t useReflection = 0;
};
```

### 3.3 关键方法
- `SetLightColor(const glm::vec3& color)` - 设置光源颜色
  - 更新材质的albedo
  - 设置roughness = 0.1f, metallic = 0.0f, specular = 1.0f

### 3.4 Shader文件
- **light_source.frag** - 光源着色器
  - 使用material.albedo直接作为颜色
  - 添加specular强度和fresnel效果

---

## 4. 着色器分析

### 4.1 Cornell Box 着色器
- **gltfmesh_mvr.frag** - 主要着色器
  - 支持球谐函数 (SH) 光照
  - 支持IBL反射
  - 使用multiview渲染（6个视角）

### 4.2 光照计算流程
```glsl
// 1. 球谐函数评估
vec3 diffuse = albedo * 0.2;
if (material.useSH != 0) {
    diffuse += max(evaluateSH(N), vec3(0.0)) * albedo;
}

// 2. 直接光照
vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
float NdotL = max(dot(N, lightDir), 0.0);
diffuse += albedo * NdotL * 0.5;

// 3. 镜面反射
vec3 H = normalize(V + lightDir);
float NdotH = max(dot(N, H), 0.0);
vec3 specular = vec3(material.specular) * pow(NdotH, ...);
```

---

## 5. 颜色对应问题分析

### 5.1 问题描述
Preview Model颜色改变时，Cornell Box的着色不对应

### 5.2 根本原因
- **light_source.frag**: 使用 `material.albedo` 直接作为颜色
- **gltfmesh_mvr.frag**: 使用固定的 `lightDir = normalize(vec3(1.0, 1.0, 1.0))`
- **不匹配**: 光源颜色未传递到Cornell Box着色器

### 5.3 解决方案
需要在Global UBO中添加lightColor，并在gltfmesh_mvr.frag中使用

---

## 6. PRT 相关代码

### 6.1 预计算结构
```cpp
std::vector<SHCoefficients> precomputedSHCoefficients;
int32_t shSamples = 16;  // 采样数量
```

### 6.2 SH系数结构
```cpp
struct SHCoefficients {
    vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;  // 9个系数
};
```

### 6.3 关键函数
- `PrecomputePRT()` - 预计算PRT
- `UpdatePRTLighting()` - 应用预计算的光照

---

## 7. 关键文件位置

| 文件 | 用途 |
|------|------|
| `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` | Cornell Box着色 |
| `shaders/glsl/lightprobesh2/light_source.frag` | 光源着色 |
| `examples/lightprobesh2/PreviewModel.cpp` | 光源预览 |
| `examples/lightprobesh2/main.cpp` | 主程序逻辑 |
| `assets/models/cornell/cornell.gltf` | Cornell Box模型 |

---

## 8. 下一步行动

### 阶段2: 修复颜色对应问题
1. 在Global UBO中添加lightColor字段
2. 修改gltfmesh_mvr.frag使用lightColor
3. 测试颜色对应

### 阶段3: 实现PRT
1. 实现球谐函数库
2. 预计算光照信息
3. 导出为txt文件

### 阶段4: 应用PRT
1. 实现数据导入
2. 实现relighting着色器
3. 集成到主渲染管线

