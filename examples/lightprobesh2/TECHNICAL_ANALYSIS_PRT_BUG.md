# PRT Relighting 旋转消失问题 - 技术分析

## 问题现象

启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块时，Cornell 场景消失。

## 问题分析

### 代码流程（修复前）

```
drawFrame()
  ├─ 绑定 PRT 描述符集
  │  └─ vkCmdBindDescriptorSets(cmd, ..., pipelineLayoutPRT, 0, 2, prtDescriptorSets, ...)
  │
  └─ 调用 gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, pipelinePRT)
     └─ GltfModel::Draw()
        ├─ 重新绑定描述符集（使用错误的管线布局！）
        │  └─ vkCmdBindDescriptorSets(cmd, ..., techniques[techIdx].pipelineLayout, 0, 2, sets, ...)
        │
        ├─ 绑定管线
        │  └─ vkCmdBindPipeline(cmd, ..., pipelinePRT)
        │
        └─ 绘制模型
```

### 问题根源

**关键问题**：描述符集绑定使用了错误的管线布局

```cpp
// 在 drawFrame() 中
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       pipelineLayoutPRT,  // ✓ 正确的 PRT 管线布局
                       0, 2, prtDescriptorSets, ...);

// 然后调用 gltfModel->Draw()，它会执行：
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       techniques[techIdx].pipelineLayout,  // ✗ 错误！这是标准管线的布局
                       0, 2, sets, ...);
```

### 为什么会导致场景消失？

1. **描述符集布局不匹配**
   - PRT 管线布局期望：Set 0 (Global UBO) + Set 1 (PRT UBO with Lighting SH)
   - 标准管线布局期望：Set 0 (Global UBO) + Set 1 (Material UBO)
   - 用错误的布局绑定会导致着色器无法访问正确的数据

2. **Vulkan 验证层错误**
   - 验证层会检测到描述符集与管线布局不匹配
   - 可能导致渲染被跳过或崩溃

3. **着色器数据错误**
   - 着色器期望从 Set 1, Binding 0 读取 Lighting SH 系数
   - 但实际上读取的是 Material UBO 数据
   - 导致光照计算错误或无效

## 解决方案

### 修复策略

**不使用 `gltfModel->Draw()`，而是手动管理渲染**

```cpp
if (usePRTRelighting && pipelinePRT != VK_NULL_HANDLE) {
    // 1. 绑定 PRT 管线
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePRT);
    
    // 2. 绑定正确的描述符集（使用 PRT 管线布局）
    std::array<VkDescriptorSet, 2> prtDescriptorSets = { 
        mainPass->descriptorSet,      // Set 0
        descriptorSetPRT              // Set 1
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           pipelineLayoutPRT, 0, 2, 
                           prtDescriptorSets.data(), 0, nullptr);
    
    // 3. 绑定顶点缓冲区
    gltfModel->getModel()->bindBuffers(cmd);
    
    // 4. 手动遍历节点并绘制
    for (auto* node : gltfModel->getModel()->nodes) {
        drawNode(node);  // 使用 Push Constants 传递模型矩阵
    }
}
```

### 为什么这样修复有效？

1. **避免描述符集重新绑定**
   - 我们直接控制描述符集绑定过程
   - 确保使用正确的管线布局

2. **完全控制管线状态**
   - 不依赖 `gltfModel->Draw()` 的内部逻辑
   - 可以确保所有状态都与 PRT 管线一致

3. **数据流正确**
   ```
   Lighting SH (从 UpdatePRTLighting 更新)
        ↓
   lightingSHBuffer (UBO)
        ↓
   descriptorSetPRT (Set 1, Binding 0)
        ↓
   PRT 着色器 (prt_relight.vert/frag)
        ↓
   正确的光照计算
   ```

## 关键数据结构

### PRT 管线的描述符集布局

```cpp
// Set 0: 全局数据（与主管线相同）
layout(set = 0, binding = 0) uniform UBO {
    mat4 projection;
    mat4 view;
} uboMatrices;

// Set 1: PRT 专用数据
layout(set = 1, binding = 0) uniform LightingUBO {
    SHCoefficients lighting;  // 9 个 vec4
} ubo;
```

### Push Constants

```cpp
struct PushConstantBlock {
    glm::mat4 modelOffset;    // 模型矩阵
    glm::vec4 baseColor;      // 材质颜色
};
```

## 调试验证

### 验证修复是否有效

1. **检查描述符集绑定**
   ```
   [DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
   [DEBUG PRT]   - pipelinePRT: 0x...
   [DEBUG PRT]   - pipelineLayoutPRT: 0x...
   [DEBUG PRT]   - descriptorSetPRT: 0x...
   [DEBUG PRT]   - Drew X primitives
   ```

2. **检查光照更新**
   ```
   [DEBUG PRT] Updating lighting for angle: 0 degrees
   [DEBUG PRT] Updated UBO for angle 0 deg. L0M0: (32.9336, 32.9336, 32.9336)
   ```

3. **检查是否有 Vulkan 错误**
   - 运行时不应该有验证层错误
   - 场景应该平滑渲染

## 性能影响

- **正面**：避免了不必要的描述符集重新绑定
- **中立**：手动遍历节点与 `gltfModel->Draw()` 的性能相似
- **总体**：性能影响可忽略不计

## 相关改动

- `main.cpp` 第 714-780 行：修复了 PRT 渲染逻辑
- 无需修改着色器或其他文件

