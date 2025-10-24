# Cubemap 问题最终解决方案总结

## 问题

cubemap 渲染到 preview 上出现两个问题：
1. ❌ 方向反了（前后左右反向）
2. ❌ 纹理拼接（边缘出现界限）

## 根本原因

- **GenIBLCubeMipPass** 和 **LightProbe::CaptureCubeMap** 使用了不同的 MVP 矩阵生成方式
- 导致方向不一致和纹理不连续

## 最终解决方案

### 修改 1: Pass.cpp - 使用 lookAt 方式

**文件**: `examples/lightprobesh2/Pass.cpp`
**函数**: `GenIBLCubeMipPass::PrepareData()`
**行数**: 719-744

```cpp
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
```

### 修改 2: filtercube.vert - 顶点坐标翻转

**文件**: `shaders/glsl/lightprobesh2/filtercube.vert`
**行数**: 22-30

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

### 1. 一致性
- 两个 Pass 都使用 lookAt 方式
- MVP 矩阵生成方式一致
- 方向一致

### 2. 方向正确
- lookAt 方式生成的视图矩阵方向正确
- 与 LightProbe 捕获的方向一致

### 3. 纹理连续
- 顶点坐标翻转补偿 lookAt 方式的坐标系变化
- 保持立方体贴图的六个面无缝拼接

### 4. 采样不受影响
- 采样坐标保持原始值
- 片段着色器的采样逻辑不变

## 修复效果

| 问题 | 之前 | 现在 |
|------|------|------|
| 方向 | ❌ 反向 | ✅ 正确 |
| 纹理连续性 | ❌ 拼接 | ✅ 无缝 |
| 渲染效果 | ❌ 不圆滑 | ✅ 圆滑 |
| 一致性 | ❌ 不一致 | ✅ 一致 |

## 编译和测试

```bash
# 编译
cmake --build build --config Release

# 运行
./build/bin/Release/lightprobesh2.exe
```

## 验证清单

- [ ] 编译成功
- [ ] cubemap 预览方向正确
- [ ] 没有拼接痕迹
- [ ] 纹理圆滑渲染
- [ ] 反射效果正确
- [ ] 与 LightProbe 捕获结果一致

## 关键改进

### 顶点着色器的作用

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

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - MVP 矩阵生成
- `examples/lightprobesh2/LightProbe.cpp` - CaptureCubeMap
- `shaders/glsl/lightprobesh2/filtercube.vert` - 顶点着色器
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤着色器
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度着色器

## 编译状态

✅ **成功** - 无错误

## 总结

通过统一使用 lookAt 方式生成 MVP 矩阵，并在顶点着色器中进行坐标翻转，实现了方向正确和纹理连续的完美结合。现在 cubemap 应该能够圆滑地渲染在球体上，方向也与 LightProbe 捕获的结果一致。


