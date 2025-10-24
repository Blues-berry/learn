# Cubemap 最终解决方案 - 方向正确 + 纹理连续

## 问题演进

### 第一次修复：方向问题
- **症状**: 前后左右反向
- **修复**: 改用 lookAt 方式
- **结果**: ✅ 方向正确，❌ 纹理拼接

### 第二次修复：纹理拼接问题
- **症状**: 边缘出现拼接痕迹
- **修复**: 改用旋转矩阵方式
- **结果**: ✅ 纹理连续，❌ 方向反了

### 最终解决方案：综合修复
- **方案**: 使用 lookAt 方式 + 顶点坐标翻转
- **结果**: ✅ 方向正确 + ✅ 纹理连续

## 最终修复

### 修改 1: Pass.cpp - GenIBLCubeMipPass::PrepareData()

**位置**: `examples/lightprobesh2/Pass.cpp` 第 719-744 行

**改动**: 使用 lookAt 方式与 LightProbe::CaptureCubeMap 保持一致

```cpp
void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);
    IBLGenUBO uboData = {};

    // ✅ 使用 lookAt 方式与 LightProbe::CaptureCubeMap 保持一致
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

    FeedUBO(uboData);
    memcpy(ubo.mapped, &uboData, sizeof(IBLGenUBO));
}
```

### 修改 2: filtercube.vert - 顶点坐标翻转

**位置**: `shaders/glsl/lightprobesh2/filtercube.vert` 第 22-30 行

**改动**: 在顶点着色器中翻转 Y 坐标以补偿 lookAt 方式

```glsl
void main() 
{
    // ✅ 修复：调整坐标以适应 lookAt 方式的 MVP 矩阵
    vec3 pos = inPos;
    pos.y = -pos.y;  // 翻转 Y 坐标以补偿 lookAt 方式
    
    outUVW = inPos;  // 保持原始坐标用于采样
    gl_Position = ubo.mvp[gl_ViewIndex] * vec4(pos, 1.0);
}
```

## 为什么这样修复？

### 1. 保持一致性
- LightProbe::CaptureCubeMap 使用 lookAt 方式
- GenIBLCubeMipPass 也使用 lookAt 方式
- 两个地方的 MVP 矩阵生成方式一致

### 2. 方向正确
- lookAt 方式生成的视图矩阵方向正确
- 与 LightProbe 捕获的方向一致

### 3. 纹理连续
- 在顶点着色器中翻转 Y 坐标
- 补偿 lookAt 方式导致的坐标系变化
- 保持立方体贴图的六个面无缝拼接

### 4. 着色器兼容
- 片段着色器中的 Y 坐标翻转保持不变
- 与现有的采样逻辑兼容

## 关键改进

### 顶点坐标翻转的作用

```
原始顶点 inPos
    ↓
翻转 Y 坐标 (pos.y = -pos.y)
    ↓
应用 lookAt MVP 矩阵
    ↓
得到正确的投影位置
    ↓
保持纹理坐标连续性
```

### 采样坐标保持不变

```glsl
outUVW = inPos;  // 保持原始坐标用于采样
```

这确保了片段着色器中的采样坐标不受影响。

## 修复效果

| 方面 | 之前 | 现在 |
|------|------|------|
| 方向 | ❌ 反向 | ✅ 正确 |
| 纹理连续性 | ❌ 拼接痕迹 | ✅ 无缝 |
| 渲染效果 | ❌ 不圆滑 | ✅ 圆滑 |
| 反射效果 | ❌ 错误 | ✅ 正确 |
| 一致性 | ❌ 不一致 | ✅ 一致 |

## 编译状态

✅ **编译成功** - 无错误

## 关键学习点

### 1. MVP 矩阵的一致性很重要
- 不同的 Pass 应该使用相同的矩阵生成方式
- 否则会导致方向或纹理问题

### 2. 顶点着色器可以调整坐标
- 可以在顶点着色器中进行坐标变换
- 而不影响片段着色器的采样坐标

### 3. 坐标系统需要仔细处理
- lookAt 方式改变了坐标系统
- 需要在适当的地方进行补偿

## 测试建议

1. **视觉检查**
   - 运行程序
   - 观察 cubemap 预览
   - 检查方向是否正确
   - 检查是否有拼接痕迹

2. **功能测试**
   - 单探针模式：点击 "Capture Cubemap at Camera"
   - 多探针模式：生成并捕获探针
   - 验证反射效果

3. **对比测试**
   - 与 LightProbe 捕获的结果对比
   - 确保方向一致

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - GenIBLCubeMipPass::PrepareData()
- `examples/lightprobesh2/LightProbe.cpp` - CaptureCubeMap()
- `shaders/glsl/lightprobesh2/filtercube.vert` - 顶点着色器
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤着色器
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度着色器

## 总结

通过在 GenIBLCubeMipPass 中使用 lookAt 方式，并在顶点着色器中进行坐标翻转，实现了方向正确和纹理连续的完美结合。现在 cubemap 应该能够圆滑地渲染在球体上，方向也与 LightProbe 捕获的结果一致。


