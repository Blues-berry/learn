# PRT Relighting 旋转消失问题 - 修复总结

## 问题

启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块，Cornell 场景消失。

## 根本原因

在 `main.cpp` 的 `drawFrame()` 函数中，当使用 PRT 管线渲染 Cornell 模型时，发生了**描述符集绑定冲突**：

1. 代码先用 `pipelineLayoutPRT` 绑定 PRT 描述符集
2. 然后调用 `gltfModel->Draw()`，该函数用 `techniques[techIdx].pipelineLayout` 重新绑定描述符集
3. 两个管线布局不同，导致着色器无法访问正确的数据
4. 最终导致场景消失

## 修复方案

**文件**：`main.cpp`

**位置**：第 714-780 行（`drawFrame()` 函数中的 gltfModel 渲染部分）

**改动**：
- 当启用 PRT Relighting 时，不再使用 `gltfModel->Draw()` 函数
- 改为手动管理整个渲染过程：
  1. 绑定 PRT 管线
  2. 绑定 PRT 描述符集（使用正确的管线布局）
  3. 绑定顶点缓冲区
  4. 手动遍历模型节点并绘制

## 代码变更

### 修复前
```cpp
if (gltfModel) {
    if (usePRTRelighting && pipelinePRT != VK_NULL_HANDLE) {
        std::array<VkDescriptorSet, 2> prtDescriptorSets = { 
            mainPass->descriptorSet, descriptorSetPRT 
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                               pipelineLayoutPRT, 0, 2, 
                               prtDescriptorSets.data(), 0, nullptr);
        
        // ✗ 这会导致描述符集重新绑定！
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN, pipelinePRT);
    } else {
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
    }
}
```

### 修复后
```cpp
if (gltfModel) {
    if (usePRTRelighting && pipelinePRT != VK_NULL_HANDLE) {
        // ✓ 绑定 PRT 管线
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinePRT);
        
        // ✓ 绑定正确的描述符集
        std::array<VkDescriptorSet, 2> prtDescriptorSets = { 
            mainPass->descriptorSet, descriptorSetPRT 
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                               pipelineLayoutPRT, 0, 2, 
                               prtDescriptorSets.data(), 0, nullptr);
        
        // ✓ 绑定顶点缓冲区
        gltfModel->getModel()->bindBuffers(cmd);
        
        // ✓ 手动遍历节点并绘制
        struct PushConstantBlock {
            glm::mat4 modelOffset;
            glm::vec4 baseColor;
        } pc;
        
        std::function<void(vkglTF::Node*)> drawNode = [&](vkglTF::Node* node) {
            if (!node) return;
            glm::mat4 nodeMatrix = node->getMatrix();
            if (node->mesh) {
                for (auto* primitive : node->mesh->primitives) {
                    pc.modelOffset = nodeMatrix;
                    pc.baseColor = primitive->material.baseColorFactor;
                    vkCmdPushConstants(cmd, pipelineLayoutPRT,
                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                     0, sizeof(PushConstantBlock), &pc);
                    vkCmdDrawIndexed(cmd, primitive->indexCount, 1, 
                                    primitive->firstIndex, 0, 0);
                }
            }
            for (auto* child : node->children) {
                drawNode(child);
            }
        };
        
        for (auto* node : gltfModel->getModel()->nodes) {
            drawNode(node);
        }
    } else {
        gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
    }
}
```

## 添加的调试信息

为了便于问题诊断，添加了详细的调试输出（每 60 帧输出一次）：

```cpp
[DEBUG PRT] Rendering Cornell model with PRT pipeline (frame X)
[DEBUG PRT]   - pipelinePRT: 0x...
[DEBUG PRT]   - pipelineLayoutPRT: 0x...
[DEBUG PRT]   - descriptorSetPRT: 0x...
[DEBUG PRT]   - mainPass->descriptorSet: 0x...
[DEBUG PRT]   - Drew Y primitives
```

## 验证修复

### 编译
```bash
cd build
cmake --build . --config Release
```

### 运行测试
1. 启动程序
2. 加载 Cornell 模型
3. 启用 PRT Relighting
4. 拖动 Light Rotation 滑块
5. **预期结果**：场景应该平滑渲染，不应该消失

### 预期输出
```
[DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
[DEBUG PRT]   - pipelinePRT: 0x...
[DEBUG PRT]   - pipelineLayoutPRT: 0x...
[DEBUG PRT]   - descriptorSetPRT: 0x...
[DEBUG PRT]   - mainPass->descriptorSet: 0x...
[DEBUG PRT]   - Drew 6 primitives
```

## 相关文档

- `PRT_ROTATION_BUG_FIX.md`：详细的技术分析
- `TECHNICAL_ANALYSIS_PRT_BUG.md`：深入的问题分析
- `PRT_TESTING_GUIDE.md`：完整的测试指南

## 影响范围

- **修改文件**：`main.cpp`（第 714-780 行）
- **影响功能**：PRT Relighting 渲染
- **向后兼容**：完全兼容，不影响其他渲染模式
- **性能**：无性能下降

## 后续工作

1. 测试其他模型是否也能正确渲染
2. 验证光照效果是否符合预期
3. 优化光照质量（增加 SH 采样数）
4. 添加镜面反射支持

