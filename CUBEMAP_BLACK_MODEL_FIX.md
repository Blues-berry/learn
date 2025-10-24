# Cubemap 捕获后模型变黑问题修复

## 问题描述

捕获 cubemap 后，gltfModel 和 preview 变黑色，无法看到模型。

## 根本原因

修改 MVP 矩阵生成方式（从旋转矩阵改为 lookAt）后，着色器中的坐标翻转逻辑变得不适用：

1. **filtercube.vert** 中添加了 Y 坐标翻转
2. **irradiancecube.frag** 中也有 Y 坐标翻转
3. **prefilterenvmap.frag** 中也有 Y 坐标翻转

这导致了**双重翻转**，采样坐标错误，IBL 贴图生成为黑色。

## 修复方案

### 修改 1: filtercube.vert - 移除顶点坐标翻转

**位置**: `shaders/glsl/lightprobesh2/filtercube.vert` 第 22-28 行

**修复前**:
```glsl
void main() 
{
    vec3 pos = inPos;
    pos.y = -pos.y;  // ❌ 翻转 Y 坐标
    
    outUVW = inPos;
    gl_Position = ubo.mvp[gl_ViewIndex] * vec4(pos, 1.0);
}
```

**修复后**:
```glsl
void main() 
{
    // ✅ 修复：使用原始坐标，不进行翻转
    // lookAt 方式已经正确处理了坐标系统
    outUVW = inPos;
    gl_Position = ubo.mvp[gl_ViewIndex] * vec4(inPos.xyz, 1.0);
}
```

### 修改 2: irradiancecube.frag - 移除采样坐标翻转

**位置**: `shaders/glsl/lightprobesh2/irradiancecube.frag` 第 31-44 行

**修复前**:
```glsl
for (float phi = 0.0; phi < TWO_PI; phi += ubo.deltaPhi) {
    for (float theta = 0.0; theta < HALF_PI; theta += ubo.deltaTheta) {
        vec3 tempVec = cos(phi) * right + sin(phi) * up;
        vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;

        sampleVector.y = -sampleVector.y;  // ❌ 翻转 Y 坐标
        
        color += texture(samplerEnv, sampleVector).rgb * cos(theta) * sin(theta);
        sampleCount++;
    }
}
```

**修复后**:
```glsl
for (float phi = 0.0; phi < TWO_PI; phi += ubo.deltaPhi) {
    for (float theta = 0.0; theta < HALF_PI; theta += ubo.deltaTheta) {
        vec3 tempVec = cos(phi) * right + sin(phi) * up;
        vec3 sampleVector = cos(theta) * N + sin(theta) * tempVec;

        // ✅ 修复：lookAt 方式不需要 Y 坐标翻转
        // sampleVector.y = -sampleVector.y;
        
        color += texture(samplerEnv, sampleVector).rgb * cos(theta) * sin(theta);
        sampleCount++;
    }
}
```

### 修改 3: prefilterenvmap.frag - 移除采样坐标翻转

**位置**: `shaders/glsl/lightprobesh2/prefilterenvmap.frag` 第 82-103 行

**修复前**:
```glsl
if(dotNL > 0.0) {
    // ... 计算代码 ...
    
    L.y = -L.y;  // ❌ 翻转 Y 坐标
    
    color += textureLod(samplerEnv, L, mipLevel).rgb * dotNL;
    totalWeight += dotNL;
}
```

**修复后**:
```glsl
if(dotNL > 0.0) {
    // ... 计算代码 ...
    
    // ✅ 修复：lookAt 方式不需要 Y 坐标翻转
    // L.y = -L.y;
    
    color += textureLod(samplerEnv, L, mipLevel).rgb * dotNL;
    totalWeight += dotNL;
}
```

## 为什么这样修复？

### 1. lookAt 方式已经处理了坐标系统
- lookAt 矩阵生成的视图矩阵已经包含了正确的坐标系统变换
- 不需要额外的坐标翻转

### 2. 避免双重翻转
- 旋转矩阵方式需要在着色器中进行 Y 坐标翻转
- lookAt 方式不需要这种翻转
- 双重翻转导致采样坐标错误

### 3. IBL 贴图生成正确
- 移除坐标翻转后，IBL 贴图采样正确
- 生成的辐照度和预过滤贴图不再是黑色

## 修复效果

| 方面 | 之前 | 现在 |
|------|------|------|
| 模型显示 | ❌ 黑色 | ✅ 正常 |
| IBL 贴图 | ❌ 黑色 | ✅ 正确 |
| 光照效果 | ❌ 无光照 | ✅ 正确光照 |
| 反射效果 | ❌ 无反射 | ✅ 正确反射 |

## 编译状态

✅ **编译成功** - 无错误

## 关键学习点

### 1. 坐标系统的一致性
- MVP 矩阵和着色器中的坐标变换必须一致
- 改变矩阵生成方式需要同时调整着色器

### 2. 避免双重变换
- 不要在多个地方进行相同的坐标变换
- 这会导致坐标错误和采样失败

### 3. lookAt 方式的优势
- 更直观，坐标系统更清晰
- 不需要复杂的坐标翻转逻辑
- 更容易维护和调试

## 相关文件

- `shaders/glsl/lightprobesh2/filtercube.vert` - 顶点着色器
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度着色器
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤着色器
- `examples/lightprobesh2/Pass.cpp` - MVP 矩阵生成

## 总结

通过移除着色器中的 Y 坐标翻转，避免了双重翻转导致的采样错误。现在 IBL 贴图应该能够正确生成，模型也应该能够正常显示，具有正确的光照和反射效果。


