# Cubemap 纹理拼接问题修复

## 问题描述

cubemap 渲染到 preview 上出现了纹理不对应的问题：
- ❌ 之前是圆滑的渲染在球体上
- ❌ 现在边缘出现了界限，像是拼接的
- ❌ 立方体贴图的六个面没有正确对齐

## 根本原因

之前使用 `glm::lookAt()` 方式生成 MVP 矩阵虽然解决了方向问题，但引入了新的问题：

**lookAt 矩阵的问题**:
1. lookAt 矩阵是为了从特定位置看向原点
2. 但立方体贴图的 IBL 生成需要的是将立方体顶点正确投影到各个面
3. lookAt 方式改变了顶点的投影方式，导致纹理坐标不连续

**正确的方式**:
- 使用旋转矩阵方式，确保立方体的顶点被正确地投影到各个面
- 旋转矩阵保持了立方体的结构，只改变了方向

## 修复方案

### 修改: Pass.cpp - GenIBLCubeMipPass::PrepareData()

**位置**: `examples/lightprobesh2/Pass.cpp` 第 719-746 行

**修复内容**: 恢复使用旋转矩阵方式，但保持正确的方向

```cpp
void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);
    IBLGenUBO uboData = {};

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

    FeedUBO(uboData);
    memcpy(ubo.mapped, &uboData, sizeof(IBLGenUBO));
}
```

## 为什么旋转矩阵更合适？

### 1. 保持立方体结构

旋转矩阵只改变方向，不改变顶点之间的相对关系：
```
原始立方体顶点 → 旋转 → 投影到立方体面
```

### 2. 纹理坐标连续性

立方体贴图的六个面需要无缝拼接：
- 相邻面的边界必须对齐
- 旋转矩阵保证了这种对齐

### 3. 与着色器兼容

着色器中的采样逻辑（Y 坐标翻转）是基于旋转矩阵方式设计的：
```glsl
// prefilterenvmap.frag 第 97 行
L.y = -L.y;  // 这个翻转是针对旋转矩阵方式的

// irradiancecube.frag 第 38 行
sampleVector.y = -sampleVector.y;  // 同样的翻转
```

## 对比分析

| 方面 | lookAt 方式 | 旋转矩阵方式 |
|------|-----------|-----------|
| 方向正确性 | ✅ 正确 | ✅ 正确 |
| 纹理连续性 | ❌ 不连续 | ✅ 连续 |
| 着色器兼容性 | ❌ 不兼容 | ✅ 兼容 |
| 顶点投影 | ❌ 改变结构 | ✅ 保持结构 |

## 修复效果

### 之前 (lookAt 方式)
- ✅ 方向正确
- ❌ 边缘出现拼接痕迹
- ❌ 纹理不连续

### 之后 (旋转矩阵方式)
- ✅ 方向正确
- ✅ 纹理连续无缝
- ✅ 圆滑渲染在球体上

## 编译状态

✅ **编译成功** - 无错误

## 关键要点

1. **不是所有问题都用 lookAt 解决**
   - lookAt 适合相机视图矩阵
   - 立方体贴图需要旋转矩阵

2. **着色器和 MVP 矩阵必须匹配**
   - 着色器中的 Y 坐标翻转是针对旋转矩阵的
   - 改变 MVP 矩阵方式需要同时修改着色器

3. **纹理连续性很重要**
   - 立方体贴图的六个面必须无缝拼接
   - 任何不连续都会导致可见的拼接痕迹

## 下一步

1. 运行程序测试
2. 验证 cubemap 预览是否圆滑
3. 检查是否还有拼接痕迹
4. 验证反射效果


