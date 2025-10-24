# ✅ 捕获的 gltfModel 是黑色的问题 - 完整修复

## 🐛 问题

捕获到的 gltfModel 没有光照信息，显示为黑色。

## 🔍 根本原因

### 原因 1: Fragment Shader 中使用了错误的相机位置

**文件**: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`

**问题**:
- 硬编码使用 `global.cameraPos[0]`（第一个面的相机位置）
- 在 multiview 渲染中，应该使用 `gl_ViewIndex` 来选择正确的相机位置
- 导致其他 5 个面使用错误的相机位置 → 光照计算错误 → 黑色

### 原因 2: CAPTURE_SCENE 中绘制了 4 个模型副本

**文件**: `examples/lightprobesh2/gltfload.cpp`

**问题**:
- 在 CAPTURE_SCENE 中，gltfModel 被绘制了 4 次，每次都有不同的偏移
- 这导致 cubemap 中有多个模型副本，光照计算混乱
- 应该只在原点绘制一次

## ✅ 修复方案

### 修复 1: 在 Fragment Shader 中使用 gl_ViewIndex

**文件**: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`

```glsl
#version 450
#extension GL_EXT_multiview : enable  // ✅ 添加 multiview 扩展

// ... 其他代码 ...

void main()
{
    vec3 N = normalize(inNormal);
    // ✅ 使用 gl_ViewIndex 选择正确的相机位置
    vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);
    vec3 R = reflect(-V, N);
    
    // ... 其他代码 ...
}
```

### 修复 2: 在 CAPTURE_SCENE 中只绘制一次

**文件**: `examples/lightprobesh2/gltfload.cpp`

```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
    // ... 安全检查 ...

    // ✅ 根据技术类型选择不同的绘制方式
    if (tech == ETechnique::CAPTURE_SCENE) {
        // CAPTURE_SCENE：只在原点绘制一次，使用 localData.transform
        pc.modelOffset = glm::mat4(1.0f);  // 不应用 push constant 偏移
        pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, 
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                          0, sizeof(PushConstantBlock), &pc);
        model->draw(cmd, vkglTF::RenderFlags::BindImages, 
                   techniques[techIdx].pipelineLayout, 1);
    } else {
        // MAIN：绘制 4 个不同位置的模型
        for (int i = 0; i < 4; ++i) {
            pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) * 
                            glm::scale(glm::mat4(1.0f), glm::vec3(scale));
            pc.tint = glm::vec4(colors[i % 3], 1.0f);
            vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, 
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                              0, sizeof(PushConstantBlock), &pc);
            model->draw(cmd);
        }
    }
}
```

## 📊 修复效果

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **相机位置** | ❌ 硬编码为 [0] | ✅ 使用 gl_ViewIndex |
| **绘制次数** | ❌ 4 次 | ✅ 1 次 |
| **光照计算** | ❌ 错误 | ✅ 正确 |
| **捕获结果** | ❌ 黑色 | ✅ 有光照 |

## 🔑 关键要点

1. **Multiview 渲染中必须使用 gl_ViewIndex**
   - 每个面有不同的视图矩阵和相机位置
   - 不能硬编码使用某一个面的数据

2. **CAPTURE_SCENE 和 MAIN 的绘制方式不同**
   - CAPTURE_SCENE：只绘制一次，在原点
   - MAIN：绘制多个副本，在不同位置

3. **Fragment Shader 中需要访问 gl_ViewIndex**
   - 需要声明 `#extension GL_EXT_multiview : enable`
   - 然后使用 `gl_ViewIndex` 来索引数组

## 🚀 修复步骤

1. ✅ 修改 `gltfmesh_mvr.frag` 中的相机位置计算
2. ✅ 重新编译着色器
3. ✅ 修改 `gltfload.cpp` 中的 Draw 方法
4. 重新编译 C++ 代码
5. 测试 CaptureCubemap 功能

## 📝 修改文件列表

1. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` - 使用 gl_ViewIndex
2. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag.spv` - 重新编译
3. ✅ `examples/lightprobesh2/gltfload.cpp` - 根据技术类型选择绘制方式

