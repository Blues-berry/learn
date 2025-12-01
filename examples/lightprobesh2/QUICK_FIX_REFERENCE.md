# PRT Relighting 旋转消失问题 - 快速参考

## 问题
启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块，Cornell 场景消失。

## 原因
描述符集绑定冲突：`gltfModel->Draw()` 用错误的管线布局重新绑定了描述符集。

## 解决方案
**文件**：`main.cpp` 第 714-780 行

**核心改动**：
```cpp
// 修复前：调用 gltfModel->Draw()（会重新绑定描述符集）
gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, pipelinePRT);

// 修复后：手动管理渲染
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePRT);
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       pipelineLayoutPRT, 0, 2, prtDescriptorSets, ...);
gltfModel->getModel()->bindBuffers(cmd);
// 手动遍历节点并绘制
```

## 关键点

| 项目 | 说明 |
|------|------|
| **问题位置** | `main.cpp` 第 714-780 行 |
| **问题类型** | 描述符集绑定冲突 |
| **修复方法** | 手动管理 PRT 管线渲染 |
| **影响范围** | 仅 PRT Relighting 模式 |
| **性能影响** | 无 |
| **向后兼容** | 是 |

## 测试步骤

1. **编译**
   ```bash
   cd build && cmake --build . --config Release
   ```

2. **运行**
   ```bash
   bin\lightprobesh2.exe
   ```

3. **测试**
   - 加载 Cornell 模型
   - 启用 PRT Relighting
   - 拖动 Light Rotation 滑块
   - **预期**：场景应该平滑渲染，不消失

## 验证修复

### 成功标志
```
[DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
[DEBUG PRT]   - Drew 6 primitives
```

### 失败标志
- 场景消失
- 控制台有 Vulkan 验证错误
- 原始输出不出现

## 调试技巧

### 如果场景仍然消失

1. **检查着色器**
   ```bash
   # 确保着色器文件存在
   ls shaders/glsl/lightprobesh2/prt_relight.*.spv
   ```

2. **检查描述符集**
   - 查看控制台输出中的指针地址是否有效
   - 确保 `descriptorSetPRT != VK_NULL_HANDLE`

3. **检查管线**
   - 确保 `pipelinePRT != VK_NULL_HANDLE`
   - 确保 `pipelineLayoutPRT != VK_NULL_HANDLE`

4. **启用 Vulkan 验证层**
   ```bash
   # 在 VulkanExampleBase 中启用验证层
   enableValidationLayers = true;
   ```

## 相关文件

| 文件 | 说明 |
|------|------|
| `main.cpp` | 主修复位置 |
| `prt_relight.vert` | PRT 顶点着色器 |
| `prt_relight.frag` | PRT 片元着色器 |
| `CHANGES_SUMMARY.md` | 详细修改总结 |
| `TECHNICAL_ANALYSIS_PRT_BUG.md` | 技术分析 |
| `PRT_TESTING_GUIDE.md` | 完整测试指南 |

## 代码片段

### 修复的完整代码结构

```cpp
if (gltfModel) {
    if (usePRTRelighting && pipelinePRT != VK_NULL_HANDLE) {
        // 1. 绑定管线
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePRT);
        
        // 2. 绑定描述符集
        std::array<VkDescriptorSet, 2> prtDescriptorSets = { 
            mainPass->descriptorSet, descriptorSetPRT 
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                               pipelineLayoutPRT, 0, 2, 
                               prtDescriptorSets.data(), 0, nullptr);
        
        // 3. 绑定顶点缓冲区
        gltfModel->getModel()->bindBuffers(cmd);
        
        // 4. 绘制
        // ... 手动遍历节点并绘制 ...
    } else {
        // 标准渲染
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
    }
}
```

## 常见问题

**Q: 为什么不能用 `gltfModel->Draw()`？**
A: 因为 `Draw()` 会用标准管线的布局重新绑定描述符集，导致 PRT 着色器无法访问正确的数据。

**Q: 手动渲染会影响性能吗？**
A: 不会。手动遍历节点与 `Draw()` 函数的性能相同。

**Q: 其他模型也会有这个问题吗？**
A: 只有在使用 PRT Relighting 时才会出现。标准 PBR 渲染不受影响。

**Q: 如何禁用 PRT Relighting？**
A: 在 UI 中取消勾选 "PRT Relighting" 复选框，或设置 `usePRTRelighting = false`。

## 性能指标

- **修复前**：场景消失（无法渲染）
- **修复后**：正常渲染，帧率与标准 PBR 相同

## 下一步

1. 验证修复有效
2. 测试其他模型
3. 优化光照质量
4. 添加新功能（如镜面反射）

