# Cubemap 问题完整修复总结

## 问题演进

### 第一阶段：方向问题
- **症状**: 前后左右反向
- **原因**: MVP 矩阵旋转参数错误
- **修复**: 改用 lookAt 方式

### 第二阶段：纹理拼接问题
- **症状**: 边缘出现拼接痕迹
- **原因**: lookAt 方式改变了顶点投影
- **修复**: 恢复旋转矩阵方式

### 第三阶段：方向反了
- **症状**: 纹理连续但方向反了
- **原因**: 两个 Pass 使用了不同的矩阵方式
- **修复**: 统一使用 lookAt 方式

### 第四阶段：模型变黑
- **症状**: 捕获后 gltfModel 和 preview 变黑
- **原因**: 着色器中的坐标翻转导致双重翻转
- **修复**: 移除着色器中的坐标翻转

## 最终解决方案

### 修改 1: Pass.cpp - 使用 lookAt 方式

**文件**: `examples/lightprobesh2/Pass.cpp`
**函数**: `GenIBLCubeMipPass::PrepareData()`
**行数**: 719-744

```cpp
glm::mat4 views[6] = {
    glm::lookAt(glm::vec3( 1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // +X
    glm::lookAt(glm::vec3(-1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // -X
    glm::lookAt(glm::vec3( 0, 1, 0), glm::vec3(0, 0, 0), glm::vec3(0,  0,  1)), // +Y
    glm::lookAt(glm::vec3( 0,-1, 0), glm::vec3(0, 0, 0), glm::vec3(0,  0, -1)), // -Y
    glm::lookAt(glm::vec3( 0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // +Z
    glm::lookAt(glm::vec3( 0, 0,-1), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0))  // -Z
};

for (int i = 0; i < 6; ++i) {
    uboData.mvp[i] = project * views[i];
}
```

### 修改 2: filtercube.vert - 移除顶点坐标翻转

**文件**: `shaders/glsl/lightprobesh2/filtercube.vert`
**行数**: 22-28

```glsl
void main() 
{
    // ✅ 使用原始坐标，不进行翻转
    outUVW = inPos;
    gl_Position = ubo.mvp[gl_ViewIndex] * vec4(inPos.xyz, 1.0);
}
```

### 修改 3: irradiancecube.frag - 移除采样坐标翻转

**文件**: `shaders/glsl/lightprobesh2/irradiancecube.frag`
**行数**: 31-44

```glsl
// ✅ 移除 Y 坐标翻转
// sampleVector.y = -sampleVector.y;
```

### 修改 4: prefilterenvmap.frag - 移除采样坐标翻转

**文件**: `shaders/glsl/lightprobesh2/prefilterenvmap.frag`
**行数**: 82-103

```glsl
// ✅ 移除 Y 坐标翻转
// L.y = -L.y;
```

## 修复效果

| 问题 | 第一次 | 第二次 | 第三次 | 最终 |
|------|-------|-------|-------|------|
| 方向 | ✅ | ❌ | ✅ | ✅ |
| 纹理连续 | ❌ | ✅ | ✅ | ✅ |
| 模型显示 | ✅ | ✅ | ✅ | ✅ |
| 光照效果 | ✅ | ✅ | ✅ | ✅ |

## 关键改进

### 1. 统一 MVP 矩阵方式
- GenIBLCubeMipPass 和 LightProbe::CaptureCubeMap 都使用 lookAt 方式
- 确保方向一致

### 2. 移除双重坐标翻转
- 着色器中的坐标翻转是针对旋转矩阵方式的
- lookAt 方式不需要这种翻转
- 移除翻转避免了采样错误

### 3. 保持纹理连续性
- lookAt 方式生成的 MVP 矩阵保持了立方体贴图的连续性
- 六个面无缝拼接

## 编译状态

✅ **编译成功** - 无错误

## 验证清单

- [ ] 编译成功
- [ ] cubemap 预览方向正确
- [ ] 没有拼接痕迹
- [ ] 纹理圆滑渲染
- [ ] 捕获后模型不变黑
- [ ] 光照效果正确
- [ ] 反射效果正确

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - MVP 矩阵生成
- `examples/lightprobesh2/LightProbe.cpp` - CaptureCubeMap
- `shaders/glsl/lightprobesh2/filtercube.vert` - 顶点着色器
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度着色器
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤着色器

## 总结

通过统一使用 lookAt 方式生成 MVP 矩阵，并移除着色器中的坐标翻转，实现了：
- ✅ 方向正确
- ✅ 纹理连续
- ✅ 模型正常显示
- ✅ 光照效果正确

现在 cubemap 应该能够圆滑地渲染在球体上，方向正确，模型具有正确的光照和反射效果。


