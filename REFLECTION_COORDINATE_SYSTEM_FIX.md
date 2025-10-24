# 反射坐标系问题 - 完整诊断与修复

## 问题描述

启用反射（useReflection）后，前后左右方向反了。

## 根本原因分析

### 问题 1: 反射采样坐标系不一致 ✅ 已修复

**IBL 生成阶段**:
- `irradiancecube.frag` 第 38 行：`sampleVector.y = -sampleVector.y;` ✅ Y翻转
- `prefilterenvmap.frag` 第 97 行：`L.y = -L.y;` ✅ Y翻转

**反射采样阶段**:
- `lightprobesh.frag` 第 185 行：`prefilteredReflection(R, roughness)` ❌ 没有Y翻转
- `gltfmesh.frag` 第 80-89 行：`prefilteredReflection()` ❌ 没有Y翻转
- `gltfmesh_main.frag` 第 81-90 行：`prefilteredReflection()` ❌ 没有Y翻转
- `gltfmesh_mvr.frag` 第 80-89 行：`prefilteredReflection()` ❌ 没有Y翻转

**修复**: 在所有 `prefilteredReflection()` 函数中添加 Y 翻转

```glsl
vec3 prefilteredReflection(vec3 R, float roughness)
{
    const float MAX_REFLECTION_LOD = 9.0;
    float lod = roughness * MAX_REFLECTION_LOD;
    float lodf = floor(lod);
    float lodc = ceil(lod);
    
    // ✅ 修复：与 prefilterenvmap.frag 保持一致，采样前翻转 Y 坐标
    vec3 sampleR = R;
    sampleR.y = -sampleR.y;
    
    vec3 a = textureLod(prefilteredMap, sampleR, lodf).rgb;
    vec3 b = textureLod(prefilteredMap, sampleR, lodc).rgb;
    return mix(a, b, lod - lodf);
}
```

### 问题 2: Irradiance 采样坐标系不一致 ✅ 已修复

**IBL 生成阶段**:
- `irradiancecube.frag` 第 38 行：`sampleVector.y = -sampleVector.y;` ✅ Y翻转

**反射采样阶段**:
- `lightprobesh.frag` 第 177 行：`texture(samplerIrradiance, N)` ❌ 没有Y翻转

**修复**: 在采样前翻转 Y 坐标

```glsl
// ✅ 修复：与 irradiancecube.frag 保持一致，采样前翻转 Y 坐标
vec3 sampleN = N;
sampleN.y = -sampleN.y;
vec3 irradiance = texture(samplerIrradiance, sampleN).rgb;
```

### 问题 3: gltfmesh.frag 和 gltfmesh_main.frag 的 main 函数被简化 ❌ 严重问题

**当前状态**:
- gltfmesh.frag 的 main 函数使用简单的光照计算
- 没有使用 IBL（irradiance 和 prefiltered）
- 没有使用 SH 系数
- 没有使用 BRDF LUT

**应该的状态**:
- 应该使用完整的 PBR 光照模型
- 根据 material.useSH 选择使用 SH 或 irradiance
- 根据 material.useReflection 选择是否使用反射

**影响**:
- 即使修复了坐标系，反射也不会显示
- 因为根本没有采样 prefilteredMap

## 修复清单

### ✅ 已完成

1. **lightprobesh.frag**
   - 修复 `prefilteredReflection()` 添加 Y 翻转
   - 修复 irradiance 采样添加 Y 翻转

2. **gltfmesh.frag**
   - 修复 `prefilteredReflection()` 添加 Y 翻转

3. **gltfmesh_main.frag**
   - 修复 `prefilteredReflection()` 添加 Y 翻转

4. **gltfmesh_mvr.frag**
   - 修复 `prefilteredReflection()` 添加 Y 翻转

### ❌ 待完成

1. **gltfmesh.frag 的 main 函数**
   - 需要恢复完整的 PBR 光照计算
   - 需要使用 IBL 采样
   - 需要支持 material.useSH 和 material.useReflection

2. **gltfmesh_main.frag 的 main 函数**
   - 需要恢复完整的 PBR 光照计算
   - 需要使用 IBL 采样
   - 需要支持 material.useSH 和 material.useReflection

## 坐标系说明

### Vulkan 坐标系
- X: 右
- Y: 下（与 OpenGL 相反）
- Z: 前

### 立方体贴图采样坐标系
- 需要与 IBL 生成时的坐标系一致
- IBL 生成时进行了 Y 翻转
- 采样时也需要进行 Y 翻转

## 验证修复

修复后，应该：
1. ✅ 反射方向正确（前后左右不反）
2. ✅ 反射颜色正确
3. ✅ 漫反射光照正确
4. ✅ 镜面反射正确

## 相关文件

| 文件 | 修复内容 |
|------|--------|
| `lightprobesh.frag` | ✅ prefilteredReflection Y翻转 + irradiance Y翻转 |
| `gltfmesh.frag` | ✅ prefilteredReflection Y翻转 + ❌ main函数需要恢复 |
| `gltfmesh_main.frag` | ✅ prefilteredReflection Y翻转 + ❌ main函数需要恢复 |
| `gltfmesh_mvr.frag` | ✅ prefilteredReflection Y翻转 |
| `irradiancecube.frag` | ✅ 已有 Y翻转 |
| `prefilterenvmap.frag` | ✅ 已有 Y翻转 |


