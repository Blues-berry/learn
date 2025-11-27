# Preview Model 颜色改变与Cornell Box着色不对应问题分析

## 1. 问题描述

当用户通过UI改变Preview Model（光源预览球体）的颜色时，Cornell Box的着色颜色不会相应改变，导致视觉上的不对应。

---

## 2. 颜色改变流程分析

### 2.1 UI颜色选择 (main.cpp:846-852)
```cpp
// 光源颜色控制
float color[3] = { lightColor.r, lightColor.g, lightColor.b };
if (overlay->colorPicker("Light Color", color)) {
    lightColor = glm::vec3(color[0], color[1], color[2]);
    previewModel->SetLightColor(lightColor);  // ← 关键调用
    globalDirty = true;
}
```

### 2.2 Preview Model颜色更新 (PreviewModel.cpp:330-367)
```cpp
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    // 1. 更新材质数据
    materialData.elbedo = glm::vec4(color, 1.0f);
    materialData.roughness = 0.1f;
    materialData.metallic = 0.0f;
    materialData.specular = 1.0f;
    materialData.useLighting = 1;
    
    // 2. 更新GPU缓冲区
    if (materialBuffer.mapped) {
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
        vkFlushMappedMemoryRanges(...);  // 刷新内存
    }
    
    // 3. 标记为脏并更新描述符集
    materialDirty = true;
    UpdateSet();
}
```

### 2.3 Preview Model着色 (light_source.frag)
```glsl
void main() {
    // 使用material.albedo作为光源颜色
    vec3 finalColor = material.albedo.rgb;
    
    // 添加强度和fresnel效果
    finalColor = finalColor * (1.0 + material.specular * 0.5);
    
    vec3 viewDir = normalize(global.cameraPos.xyz - inWorldPos);
    vec3 normal = normalize(inWorldPos);
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    finalColor += finalColor * fresnel * 0.5;
    
    outColor = vec4(finalColor, 1.0);
}
```

---

## 3. Cornell Box着色流程分析

### 3.1 着色器 (gltfmesh_mvr.frag)
```glsl
void main()
{
    vec3 N = normalize(inNormal);
    vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);
    
    vec4 baseColor = sampleBaseColor(inUV);
    vec3 albedo = baseColor.rgb;
    
    // 1. 球谐函数光照
    vec3 diffuse = albedo * 0.2;
    if (material.useSH != 0) {
        diffuse += max(evaluateSH(N), vec3(0.0)) * albedo;
    }
    
    // 2. 直接光照（固定方向！）
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));  // ← 问题！
    float NdotL = max(dot(N, lightDir), 0.0);
    diffuse += albedo * NdotL * 0.5;
    
    // 3. 镜面反射
    vec3 H = normalize(V + lightDir);
    float NdotH = max(dot(N, H), 0.0);
    vec3 specular = vec3(material.specular) * pow(NdotH, ...);
    
    vec3 color = diffuse + specular;
    outColor = vec4(color, baseColor.a);
}
```

---

## 4. 问题根本原因

### 4.1 光源颜色未传递
- **Preview Model**: 颜色存储在 `material.albedo` 中
- **Cornell Box**: 使用固定的 `lightDir = normalize(vec3(1.0, 1.0, 1.0))`
- **缺失**: 光源颜色 (`lightColor`) 未传递到Cornell Box着色器

### 4.2 光照计算不一致
- **Preview Model**: 直接使用 `material.albedo` 作为最终颜色
- **Cornell Box**: 使用 `albedo * NdotL * 0.5` 计算漫反射
- **结果**: 即使颜色相同，着色也不同

### 4.3 Global UBO缺失
- **main.cpp** 中定义了 `lightColor`
- **mainPassData** 中存储了 `lightColor`
- **但**: gltfmesh_mvr.frag 的 Global UBO 中没有 `lightColor` 字段

---

## 5. Global UBO结构对比

### 5.1 当前Global UBO (gltfmesh_mvr.frag)
```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 viewProject[6];
    vec4 cameraPos[6];
    vec4 mainLight;           // ← 未使用
    float exposure;
    float gamma;
} global;
```

### 5.2 需要的Global UBO
```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 viewProject[6];
    vec4 cameraPos[6];
    vec4 mainLight;
    float exposure;
    float gamma;
    vec3 lightColor;          // ← 需要添加
    float lightIntensity;     // ← 需要添加
    vec3 lightPosition;       // ← 需要添加
} global;
```

---

## 6. 解决方案

### 6.1 修改Pass.h中的Global UBO结构
添加光源相关字段

### 6.2 修改gltfmesh_mvr.frag
- 使用 `global.lightColor` 调制光照
- 使用 `global.lightPosition` 计算光照方向

### 6.3 修改main.cpp中的prepareData()
- 将 `lightColor` 传递到 `mainPassData`
- 将 `lightPosition` 传递到 `mainPassData`

### 6.4 修改Pass.cpp中的UpdateGlobal()
- 更新Global UBO以包含新字段

---

## 7. 实现步骤

1. **修改Pass.h** - 扩展Global UBO结构
2. **修改Pass.cpp** - 更新UBO更新逻辑
3. **修改gltfmesh_mvr.frag** - 使用lightColor和lightPosition
4. **修改main.cpp** - 传递光源参数
5. **测试** - 验证颜色对应

---

## 8. 预期结果

修改后，当用户改变Preview Model颜色时：
- Preview Model显示新颜色
- Cornell Box的着色也会相应改变
- 光照效果与光源颜色一致

