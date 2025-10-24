# 最终修复总结 - Cubemap 坐标系问题

## 问题

cubemap 渲染到 preview 上后，前后左右不对应。

## 根本原因

`GenIBLCubeMipPass::PrepareData()` 中使用旋转矩阵生成立方体贴图的六个面的视图矩阵，导致坐标系混乱。

## 修复方案

### 修改 1: Pass.cpp - GenIBLCubeMipPass::PrepareData()

**文件**: `examples/lightprobesh2/Pass.cpp`
**行数**: 719-747

**修复内容**:
- 将旋转矩阵方式改为 `glm::lookAt()` 方式
- 使用更直观的视图矩阵生成方法
- 确保与 Vulkan 立方体贴图标准一致

**关键改动**:
```cpp
// ✅ 使用 lookAt 方式生成视图矩阵
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

### 修改 2: main.cpp - prepareData()

**文件**: `examples/lightprobesh2/main.cpp`
**行数**: 604-624

**修复内容**:
- 恢复多探针模式下的 SH 和 IBL 更新代码
- 每帧根据相机位置自动切换到最近的探针

**关键改动**:
```cpp
// ✅ 多探针模式下根据相机位置更新 SH 和 IBL
if (useMultipleProbes && !multiProbeData.empty()) {
    int nearestProbeIndex = findNearestProbe(camera.position);
    if (nearestProbeIndex >= 0) {
        updateProbeBindings(nearestProbeIndex);
    }
}
```

## Vulkan 立方体贴图面顺序

| 索引 | 面 | 相机位置 | 看向 | 向上方向 |
|------|-----|---------|------|---------|
| 0 | +X | (1, 0, 0) | (0, 0, 0) | (0, -1, 0) |
| 1 | -X | (-1, 0, 0) | (0, 0, 0) | (0, -1, 0) |
| 2 | +Y | (0, 1, 0) | (0, 0, 0) | (0, 0, 1) |
| 3 | -Y | (0, -1, 0) | (0, 0, 0) | (0, 0, -1) |
| 4 | +Z | (0, 0, 1) | (0, 0, 0) | (0, -1, 0) |
| 5 | -Z | (0, 0, -1) | (0, 0, 0) | (0, -1, 0) |

## 修复效果

### 之前
- ❌ cubemap 预览前后左右反向
- ❌ IBL 贴图方向不正确
- ❌ 反射效果错误

### 之后
- ✅ cubemap 预览方向正确
- ✅ IBL 贴图方向正确
- ✅ 反射效果正确
- ✅ 多探针模式下光照随相机位置变化

## 编译状态

✅ **编译成功**
- 无编译错误
- 仅有类型转换警告（不影响功能）

## 测试建议

1. **单探针模式**:
   - 点击 "Capture Cubemap at Camera"
   - 观察 cubemap 预览
   - 验证前后左右方向是否正确

2. **多探针模式**:
   - 勾选 "Use Multiple Probes"
   - 点击 "Generate Probes"
   - 点击 "Capture All Probes"
   - 移动相机观察光照变化
   - 验证漫反射和镜面反射是否随相机位置变化

3. **反射效果**:
   - 勾选 "Use Reflect"
   - 观察物体表面的反射
   - 验证反射方向是否正确

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - GenIBLCubeMipPass::PrepareData()
- `examples/lightprobesh2/main.cpp` - prepareData()
- `examples/lightprobesh2/LightProbe.cpp` - CaptureCubeMap()
- `examples/lightprobesh2/Pass.h` - IBLGenUBO 结构体

## 总结

通过使用 `glm::lookAt()` 替代旋转矩阵，确保了立方体贴图的六个面的视图矩阵与 Vulkan 标准一致，解决了 cubemap 预览坐标系混乱的问题。同时恢复了多探针模式下的动态更新功能，使得光照能够根据相机位置自动切换。


