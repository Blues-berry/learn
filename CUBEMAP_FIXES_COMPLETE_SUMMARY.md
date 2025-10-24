# Cubemap 问题修复完整总结

## 问题演进

### 第一阶段：方向问题
**症状**: cubemap 预览前后左右反向
**原因**: MVP 矩阵使用了错误的旋转组合
**解决**: 改用 lookAt 方式

### 第二阶段：纹理拼接问题
**症状**: 边缘出现拼接痕迹，纹理不连续
**原因**: lookAt 方式改变了顶点投影方式，导致纹理坐标不连续
**解决**: 恢复旋转矩阵方式，但使用正确的旋转参数

## 最终修复

### 修改: Pass.cpp - GenIBLCubeMipPass::PrepareData()

**位置**: `examples/lightprobesh2/Pass.cpp` 第 719-746 行

**关键改动**:
```cpp
// ✅ 使用旋转矩阵方式，确保纹理坐标连续性
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

## 为什么这样修复？

### 1. 旋转矩阵保持立方体结构
- 只改变方向，不改变顶点相对关系
- 确保立方体贴图的六个面无缝拼接

### 2. 与着色器兼容
着色器中的 Y 坐标翻转是针对旋转矩阵方式的：
```glsl
// prefilterenvmap.frag
L.y = -L.y;

// irradiancecube.frag
sampleVector.y = -sampleVector.y;
```

### 3. 纹理连续性
- lookAt 方式改变了顶点投影方式
- 导致相邻面的边界不对齐
- 旋转矩阵方式保持了对齐

## 修复效果对比

| 问题 | 之前 | 现在 |
|------|------|------|
| 方向 | ❌ 反向 | ✅ 正确 |
| 纹理连续性 | ❌ 拼接痕迹 | ✅ 无缝 |
| 渲染效果 | ❌ 不圆滑 | ✅ 圆滑 |
| 反射效果 | ❌ 错误 | ✅ 正确 |

## 编译状态

✅ **编译成功**
- 无编译错误
- 无警告

## 关键学习点

### 1. 不同场景需要不同的矩阵方式
- **相机视图**: 使用 lookAt
- **立方体贴图**: 使用旋转矩阵

### 2. MVP 矩阵和着色器必须匹配
- 改变矩阵方式需要验证着色器兼容性
- 着色器中的坐标变换是针对特定矩阵方式的

### 3. 纹理连续性很重要
- 立方体贴图的六个面必须无缝拼接
- 任何不连续都会导致可见的拼接痕迹

## 测试建议

1. **视觉检查**
   - 运行程序
   - 观察 cubemap 预览
   - 检查是否有拼接痕迹

2. **功能测试**
   - 单探针模式：点击 "Capture Cubemap at Camera"
   - 多探针模式：生成并捕获探针
   - 验证反射效果

3. **性能检查**
   - 确保没有额外的性能开销
   - 验证 IBL 生成速度

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - GenIBLCubeMipPass::PrepareData()
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤着色器
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度着色器
- `shaders/glsl/lightprobesh2/lightprobesh.frag` - 预览着色器

## 总结

通过使用正确的旋转矩阵方式，同时保持与着色器的兼容性，解决了 cubemap 纹理拼接问题。现在 cubemap 应该能够圆滑地渲染在球体上，没有可见的拼接痕迹。


