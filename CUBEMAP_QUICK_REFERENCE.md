# Cubemap 修复快速参考

## 问题和解决方案

### 问题 1: 方向反向
- **症状**: 前后左右反向
- **原因**: MVP 矩阵旋转参数错误
- **解决**: 使用正确的旋转参数

### 问题 2: 纹理拼接
- **症状**: 边缘出现拼接痕迹
- **原因**: lookAt 方式改变了顶点投影
- **解决**: 恢复旋转矩阵方式

## 修复代码

**文件**: `examples/lightprobesh2/Pass.cpp`
**函数**: `GenIBLCubeMipPass::PrepareData()`
**行数**: 719-746

```cpp
// ✅ 使用旋转矩阵方式
glm::mat4 views[6] = {
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), 
                glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // +X
    glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), 
                glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // -X
    glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // +Y
    glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // -Y
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // +Z
    glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f))  // -Z
};

for (int i = 0; i < 6; ++i) {
    uboData.mvp[i] = project * views[i];
}
```

## 关键点

### 为什么用旋转矩阵？

1. **保持立方体结构**
   - 顶点相对关系不变
   - 六个面无缝拼接

2. **与着色器兼容**
   - 着色器中的 Y 坐标翻转是针对旋转矩阵的
   - 改变矩阵方式需要修改着色器

3. **纹理连续性**
   - lookAt 方式导致纹理坐标不连续
   - 旋转矩阵保持连续性

### 立方体贴图面顺序

| 索引 | 面 | 旋转方式 |
|------|-----|---------|
| 0 | +X | Y轴90° + X轴180° |
| 1 | -X | Y轴-90° + X轴180° |
| 2 | +Y | X轴-90° |
| 3 | -Y | X轴90° |
| 4 | +Z | X轴180° |
| 5 | -Z | Z轴180° |

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

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - MVP 矩阵生成
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤着色器
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度着色器
- `shaders/glsl/lightprobesh2/lightprobesh.frag` - 预览着色器

## 常见问题

### Q: 为什么不用 lookAt？
A: lookAt 改变了顶点投影方式，导致纹理坐标不连续。旋转矩阵保持了立方体的结构。

### Q: 着色器需要修改吗？
A: 不需要。着色器中的 Y 坐标翻转已经是针对旋转矩阵方式的。

### Q: 还是有拼接怎么办？
A: 检查是否编译了最新的代码，或者检查着色器是否被正确加载。

## 编译状态

✅ **成功** - 无错误


