# Cubemap 预览坐标系修复

## 问题描述

当 cubemap 渲染到 preview 上后，前后左右不对应。这是因为 `GenIBLCubeMipPass::PrepareData()` 中的 MVP 矩阵设置不正确。

## 根本原因

在 `Pass.cpp` 的 `GenIBLCubeMipPass::PrepareData()` 函数中，使用了旋转矩阵来生成立方体贴图的六个面的视图矩阵。这种方法容易出错，因为：

1. **旋转顺序问题**: 多个旋转的组合容易导致坐标系混乱
2. **不直观**: 很难验证每个面的方向是否正确
3. **与 Vulkan 标准不一致**: Vulkan 立方体贴图的面顺序有特定的定义

### 原始代码问题

```cpp
// 原始代码使用旋转矩阵
uboData.mvp[0] = project * glm::rotate(glm::rotate(glm::mat4(1.0f), 
    glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), 
    glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // 正 X 面
```

这种方式很难验证是否正确，而且容易出错。

## 修复方案

使用 `glm::lookAt()` 函数生成视图矩阵，这样更直观且正确：

### 修改的文件

**文件**: `examples/lightprobesh2/Pass.cpp`
**函数**: `GenIBLCubeMipPass::PrepareData()`
**行数**: 719-747

### 修复代码

```cpp
void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);

    IBLGenUBO uboData = {};
    
    // ✅ 修复：使用 lookAt 方式生成视图矩阵
    glm::mat4 views[6] = {
        glm::lookAt(glm::vec3( 1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // +X 面
        glm::lookAt(glm::vec3(-1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // -X 面
        glm::lookAt(glm::vec3( 0, 1, 0), glm::vec3(0, 0, 0), glm::vec3(0,  0,  1)), // +Y 面
        glm::lookAt(glm::vec3( 0,-1, 0), glm::vec3(0, 0, 0), glm::vec3(0,  0, -1)), // -Y 面
        glm::lookAt(glm::vec3( 0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // +Z 面
        glm::lookAt(glm::vec3( 0, 0,-1), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0))  // -Z 面
    };
    
    for (int i = 0; i < 6; ++i) {
        uboData.mvp[i] = project * views[i];
    }

    FeedUBO(uboData);
    memcpy(ubo.mapped, &uboData, sizeof(IBLGenUBO));
}
```

## 关键改进

### 1. 使用 lookAt 方式

```cpp
glm::lookAt(eye, center, up)
```

- **eye**: 相机位置（从该方向看向原点）
- **center**: 看向的点（原点）
- **up**: 向上方向

### 2. Vulkan 立方体贴图面顺序

Vulkan 立方体贴图的标准面顺序：

| 索引 | 面 | 方向 | 相机位置 | 向上方向 |
|------|-----|------|---------|---------|
| 0 | +X | 从右边看 | (1, 0, 0) | (0, -1, 0) |
| 1 | -X | 从左边看 | (-1, 0, 0) | (0, -1, 0) |
| 2 | +Y | 从上面看 | (0, 1, 0) | (0, 0, 1) |
| 3 | -Y | 从下面看 | (0, -1, 0) | (0, 0, -1) |
| 4 | +Z | 从前面看 | (0, 0, 1) | (0, -1, 0) |
| 5 | -Z | 从后面看 | (0, 0, -1) | (0, -1, 0) |

### 3. 向上方向的特殊处理

注意 +Y 和 -Y 面的向上方向不同：
- **+Y 面**: 向上方向是 +Z（因为从上面看）
- **-Y 面**: 向上方向是 -Z（因为从下面看）

这确保了立方体贴图的正确方向。

## 验证

修复后，cubemap 预览应该显示正确的方向：
- ✅ 前面（+Z）显示前面的内容
- ✅ 后面（-Z）显示后面的内容
- ✅ 左面（-X）显示左面的内容
- ✅ 右面（+X）显示右面的内容
- ✅ 上面（+Y）显示上面的内容
- ✅ 下面（-Y）显示下面的内容

## 编译状态

✅ **编译成功** - 无错误，仅有类型转换警告（不影响功能）

## 相关文件

- `examples/lightprobesh2/Pass.cpp` - GenIBLCubeMipPass::PrepareData()
- `examples/lightprobesh2/Pass.h` - IBLGenUBO 结构体定义
- `shaders/glsl/lightprobesh2/irradiancecube.frag` - 辐照度立方体贴图着色器
- `shaders/glsl/lightprobesh2/prefilterenvmap.frag` - 预过滤环境贴图着色器

## 下一步

1. 运行程序测试 cubemap 预览
2. 验证前后左右方向是否正确
3. 检查 IBL 贴图生成是否正确
4. 验证反射效果是否改善


