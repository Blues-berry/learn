# PRT Relighting Rotation Bug Fix

## 问题描述

启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块时，Cornell 场景消失。

## 根本原因

问题出在 `main.cpp` 的 `drawFrame()` 函数中，当使用 PRT 管线渲染 Cornell 模型时：

1. **描述符集绑定冲突**：代码在调用 `gltfModel->Draw()` 之前绑定了 PRT 专用的描述符集，但 `Draw()` 函数会用标准管线的描述符集重新绑定，导致 PRT 管线使用了错误的描述符集。

2. **管线布局不匹配**：
   - PRT 管线的布局：`pipelineLayoutPRT`（包含 2 个描述符集布局）
   - 标准管线的布局：`techniques[techIdx].pipelineLayout`（不同的结构）
   - 当 `Draw()` 函数用标准管线的布局重新绑定时，会导致验证错误或渲染失败

3. **顶点属性不匹配**：PRT 管线期望的顶点属性与标准 glTF 模型不同

## 解决方案

### 修改位置：`main.cpp` 第 714-766 行

**关键改变**：当启用 PRT Relighting 时，不再使用 `gltfModel->Draw()` 函数，而是手动管理整个渲染过程：

```cpp
if (usePRTRelighting && pipelinePRT != VK_NULL_HANDLE) {
    // 1. 绑定 PRT 管线
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePRT);
    
    // 2. 绑定正确的描述符集（使用 PRT 管线布局）
    std::array<VkDescriptorSet, 2> prtDescriptorSets = { 
        mainPass->descriptorSet,      // Set 0: 全局 UBO
        descriptorSetPRT              // Set 1: PRT 特定 UBO（光照 SH 系数）
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           pipelineLayoutPRT, 0, 2, 
                           prtDescriptorSets.data(), 0, nullptr);
    
    // 3. 绑定顶点缓冲区
    gltfModel->getModel()->bindBuffers(cmd);
    
    // 4. 手动遍历模型节点并绘制
    // 使用 Push Constants 传递模型矩阵和材质颜色
}
```

## 技术细节

### 描述符集布局

**PRT 管线的描述符集布局**：
- Set 0: 全局 UBO（投影矩阵、视图矩阵、光源数据）
- Set 1: PRT 专用 UBO（旋转后的光照 SH 系数）+ SSBO（每顶点 LT 系数）

### Push Constants

PRT 管线使用 Push Constants 传递：
```cpp
struct PushConstantBlock {
    glm::mat4 modelOffset;    // 模型矩阵
    glm::vec4 baseColor;      // 材质颜色
};
```

### 着色器

使用 `prt_relight.vert` 和 `prt_relight.frag` 着色器：
- 顶点着色器：从 Push Constants 获取模型矩阵，从顶点属性获取 LT 系数
- 片元着色器：简单输出顶点着色器计算的颜色

## 测试步骤

1. 编译项目
2. 运行程序
3. 加载 Cornell Box 模型
4. 启用 "PRT Relighting" 选项
5. 拖动 "Light" -> "Rotation" 滑块
6. **预期结果**：场景应该平滑地随光源旋转而改变光照，不应该消失

## 相关代码文件

- `main.cpp`：主渲染循环（已修复）
- `gltfload.cpp`：glTF 模型加载和渲染
- `shaders/glsl/lightprobesh2/prt_relight.vert`：PRT 顶点着色器
- `shaders/glsl/lightprobesh2/prt_relight.frag`：PRT 片元着色器

## 调试信息

修复后会输出：
```
[DEBUG] Rendering with PRT pipeline
```

这表示 PRT 管线被正确激活。

