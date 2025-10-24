# 🔍 捕获的 gltfModel 问题 - 真正的原因分析

## 🐛 问题

修改后的代码捕捉不到 gltfModel 的信息了。

## 🔍 根本原因

### 错误的假设

我之前假设 CAPTURE_SCENE 中应该只绘制一次，但这是错误的。

**实际情况**:
- Skybox 在 CAPTURE_SCENE 中只绘制一次（因为它是背景）
- **gltfModel 在 CAPTURE_SCENE 中也应该绘制 4 个副本**（就像在 MAIN 中一样）
- 这样才能捕捉到所有的模型信息

### 真正的问题

**问题 1: 绘制次数错误**
- 修改后只绘制一次，导致捕捉不到完整的模型信息
- 应该绘制 4 个副本

**问题 2: model->draw() 的参数不同**
- CAPTURE_SCENE 中需要传递 `pipelineLayout` 和 `bindImageSet` 参数
- MAIN 中不需要

## ✅ 正确的修复方案

**文件**: `examples/lightprobesh2/gltfload.cpp`

```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
    // ... 安全检查 ...

    // ✅ 绘制 4 个不同位置的模型（MAIN 和 CAPTURE_SCENE 都一样）
    for (int i = 0; i < 4; ++i) {
        pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) * 
                        glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        pc.tint = glm::vec4(colors[i % 3], 1.0f);
        vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, 
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                          0, sizeof(PushConstantBlock), &pc);
        
        // ✅ CAPTURE_SCENE 需要传递额外参数
        if (tech == ETechnique::CAPTURE_SCENE) {
            model->draw(cmd, vkglTF::RenderFlags::BindImages, 
                       techniques[techIdx].pipelineLayout, 1);
        } else {
            model->draw(cmd);
        }
    }
}
```

## 📊 修复效果

| 方面 | 错误的修复 | 正确的修复 |
|------|----------|----------|
| **绘制次数** | ❌ 1 次 | ✅ 4 次 |
| **捕捉信息** | ❌ 不完整 | ✅ 完整 |
| **draw() 参数** | ❌ 不同 | ✅ 根据技术类型调整 |

## 🔑 关键要点

1. **CAPTURE_SCENE 和 MAIN 的绘制逻辑基本相同**
   - 都绘制 4 个副本
   - 都使用相同的偏移和颜色

2. **唯一的区别是 model->draw() 的参数**
   - CAPTURE_SCENE：需要 `BindImages` 标志和 `pipelineLayout`
   - MAIN：不需要这些参数

3. **gl_ViewIndex 的修复仍然有效**
   - Fragment Shader 中使用 `gl_ViewIndex` 选择正确的相机位置
   - 这确保了光照计算正确

## 🚀 修复步骤

1. ✅ 修改 `gltfmesh_mvr.frag` 使用 gl_ViewIndex
2. ✅ 重新编译着色器
3. ✅ 修改 `gltfload.cpp` 恢复 4 个副本的绘制
4. 重新编译 C++ 代码
5. 测试 CaptureCubemap 功能

## 📝 修改文件列表

1. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` - 使用 gl_ViewIndex
2. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag.spv` - 重新编译
3. ✅ `examples/lightprobesh2/gltfload.cpp` - 恢复 4 个副本，但根据技术类型调整 draw() 参数

