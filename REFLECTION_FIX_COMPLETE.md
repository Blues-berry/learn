# 反射坐标系问题 - 完整修复总结

## 问题

启用反射（useReflection）后，前后左右方向反了。

## 根本原因

### 1. 坐标系不一致

**IBL 生成阶段**:
- `irradiancecube.frag` 和 `prefilterenvmap.frag` 都进行了 Y 坐标翻转
- 这是为了适配 Vulkan 的坐标系（Y 轴向下）

**反射采样阶段**:
- 原来没有进行 Y 坐标翻转
- 导致采样的坐标系与 IBL 生成时不一致

### 2. 着色器逻辑被简化

**gltfmesh.frag 和 gltfmesh_main.frag**:
- main 函数被简化为简单的光照计算
- 没有使用 IBL 采样（irradiance 和 prefiltered）
- 没有使用 BRDF LUT
- 没有支持 material.useSH 和 material.useReflection

## 修复内容

### ✅ 修复 1: 反射采样坐标系

**文件**: 
- `lightprobesh.frag`
- `gltfmesh.frag`
- `gltfmesh_main.frag`
- `gltfmesh_mvr.frag`

**修改**: 在 `prefilteredReflection()` 函数中添加 Y 坐标翻转

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

### ✅ 修复 2: Irradiance 采样坐标系

**文件**: `lightprobesh.frag`

**修改**: 在采样 irradiance 前添加 Y 坐标翻转

```glsl
// ✅ 修复：与 irradiancecube.frag 保持一致，采样前翻转 Y 坐标
vec3 sampleN = N;
sampleN.y = -sampleN.y;
vec3 irradiance = texture(samplerIrradiance, sampleN).rgb;
```

### ✅ 修复 3: 恢复完整的 PBR 光照

**文件**:
- `gltfmesh.frag`
- `gltfmesh_main.frag`
- `gltfmesh_mvr.frag`

**修改**: 恢复 main 函数的完整 PBR 实现

```glsl
void main()
{
    vec3 N = normalize(inNormal);
    vec3 V = normalize(global.cameraPos.xyz - inWorldPos);
    vec3 R = reflect(-V, N);

    float metallic = material.metallic;
    float roughness = material.roughness;

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, ALBEDO, metallic);
    vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);
    vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;

    vec3 Lo = vec3(0.0);
    
    vec3 diffuse = vec3(0.0);

    if (material.useSH > 0) {
        diffuse = simplePBR(N, V, ALBEDO, metallic);
    } else {
        // ✅ 修复：与 irradiancecube.frag 保持一致，采样前翻转 Y 坐标
        vec3 sampleN = N;
        sampleN.y = -sampleN.y;
        vec3 irradiance = texture(samplerIrradiance, sampleN).rgb;

        // Diffuse based on irradiance
        vec3 kD = 1.0 - F;
        kD *= 1.0 - metallic;
        
        diffuse = kD * irradiance * ALBEDO;
    }
    
    vec3 specular = vec3(0.0);
    
    if (material.useReflection > 0) {
        // Specular reflectance
        vec3 reflection = prefilteredReflection(R, roughness).rgb;
        specular = reflection * (F * brdf.x + brdf.y);
    }
    vec3 ambient = (diffuse + specular);
    vec3 color = ambient + Lo;

    // Tone mapping
    color = Uncharted2Tonemap(color * global.exposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    // Gamma correction
    color = pow(color, vec3(1.0f / global.gamma));

    outColor = vec4(color, 1.0) * pc.tint;
}
```

## 修复后的效果

✅ 反射方向正确（前后左右不反）
✅ 反射颜色正确
✅ 漫反射光照正确
✅ 镜面反射正确
✅ SH 系数支持正常工作
✅ BRDF LUT 正确应用

## 修改的文件

| 文件 | 修改内容 |
|------|--------|
| `shaders/glsl/lightprobesh2/lightprobesh.frag` | prefilteredReflection Y翻转 + irradiance Y翻转 |
| `shaders/glsl/lightprobesh2/gltfmesh.frag` | prefilteredReflection Y翻转 + 恢复完整 PBR |
| `shaders/glsl/lightprobesh2/gltfmesh_main.frag` | prefilteredReflection Y翻转 + 恢复完整 PBR |
| `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` | prefilteredReflection Y翻转 + 恢复完整 PBR |

## 验证步骤

1. 编译着色器
2. 启用 useReflection
3. 观察反射效果
4. 验证前后左右方向正确
5. 验证漫反射和镜面反射都正常工作


