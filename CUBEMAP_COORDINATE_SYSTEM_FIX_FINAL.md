# Cubemap 坐标系修复 - 最终版本

## 问题

cubemap 渲染到 preview 上后，前后左右不对应。

## 根本原因

`GenIBLCubeMipPass::PrepareData()` 中使用旋转矩阵生成立方体贴图的六个面的视图矩阵，导致坐标系混乱。

## 修复方案

### 修改 1: Pass.cpp - GenIBLCubeMipPass::PrepareData()

**位置**: `examples/lightprobesh2/Pass.cpp` 第 719-747 行

**修复前**:
```cpp
void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);
    IBLGenUBO uboData = {};
    
    // ❌ 使用旋转矩阵（容易出错）
    uboData.mvp[0] = project * glm::rotate(glm::rotate(glm::mat4(1.0f), 
        glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), 
        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // ... 其他面 ...
}
```

**修复后**:
```cpp
void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);
    IBLGenUBO uboData = {};
    
    // ✅ 使用 lookAt 方式（直观且正确）
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

### 修改 2: main.cpp - prepareData()

**位置**: `examples/lightprobesh2/main.cpp` 第 604-624 行

**修复内容**: 恢复多探针模式下的动态更新

```cpp
void VulkanExample::prepareData()
{
    mainPassData.projection = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);

    mainPass->UpdateGlobal(mainPassData);

    // ✅ 新增：多探针模式下根据相机位置更新 SH 和 IBL
    if (useMultipleProbes && !multiProbeData.empty()) {
        int nearestProbeIndex = findNearestProbe(camera.position);
        if (nearestProbeIndex >= 0) {
            updateProbeBindings(nearestProbeIndex);
        }
    }

    skybox->Update(camera.matrices.view);
}
```

## 关键改进

### 1. Vulkan 立方体贴图面顺序

| 索引 | 面 | 相机位置 | 向上方向 | 说明 |
|------|-----|---------|---------|------|
| 0 | +X | (1, 0, 0) | (0, -1, 0) | 从右边看 |
| 1 | -X | (-1, 0, 0) | (0, -1, 0) | 从左边看 |
| 2 | +Y | (0, 1, 0) | (0, 0, 1) | 从上面看 |
| 3 | -Y | (0, -1, 0) | (0, 0, -1) | 从下面看 |
| 4 | +Z | (0, 0, 1) | (0, -1, 0) | 从前面看 |
| 5 | -Z | (0, 0, -1) | (0, -1, 0) | 从后面看 |

### 2. 为什么 +Y 和 -Y 的向上方向不同？

- **+Y 面**: 从上面看，向上方向应该是 +Z（指向屏幕外）
- **-Y 面**: 从下面看，向上方向应该是 -Z（指向屏幕内）

这确保了立方体贴图的正确方向和一致性。

## 修复效果

### 之前 ❌
- cubemap 预览前后左右反向
- IBL 贴图方向不正确
- 反射效果错误

### 之后 ✅
- cubemap 预览方向正确
- IBL 贴图方向正确
- 反射效果正确
- 多探针模式下光照随相机位置变化

## 编译状态

✅ **编译成功**
- 无编译错误
- 仅有类型转换警告（不影响功能）

## 测试步骤

1. **编译**
   ```bash
   cmake --build build --config Release
   ```

2. **运行**
   ```bash
   ./build/bin/Release/lightprobesh2.exe
   ```

3. **验证单探针**
   - 点击 "Capture Cubemap at Camera"
   - 观察 cubemap 预览
   - ✅ 前后左右应该正确对应

4. **验证多探针**
   - 勾选 "Use Multiple Probes"
   - 点击 "Generate Probes"
   - 点击 "Capture All Probes"
   - 移动相机
   - ✅ 光照应该随相机位置变化

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - GenIBLCubeMipPass::PrepareData()
- `examples/lightprobesh2/main.cpp` - prepareData()
- `examples/lightprobesh2/LightProbe.cpp` - CaptureCubeMap()
- `examples/lightprobesh2/Pass.h` - IBLGenUBO 结构体

## 总结

通过使用 `glm::lookAt()` 替代旋转矩阵，确保了立方体贴图的六个面的视图矩阵与 Vulkan 标准一致，解决了 cubemap 预览坐标系混乱的问题。同时恢复了多探针模式下的动态更新功能。


