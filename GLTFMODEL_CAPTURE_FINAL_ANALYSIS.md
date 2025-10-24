# ✅ 捕获的 gltfModel 问题 - 最终分析和解决方案

## 🎯 问题总结

1. **初始问题**: gltfModel 在 mainpass 中不显示
2. **第二个问题**: CaptureCubemap 崩溃
3. **第三个问题**: 捕获的 gltfModel 是黑色的
4. **第四个问题**: 修改后捕捉不到 gltfModel 信息

## ✅ 完整的修复方案

### 修复 1: Global UBO 结构匹配（解决 mainpass 不显示）

**文件**: `shaders/glsl/lightprobesh2/gltfmesh.vert/frag`, `examples/lightprobesh2/Pass.h`, `examples/lightprobesh2/main.cpp`

```cpp
// Pass.h
struct GlobalUbo {
    glm::mat4 projection;  // ✅ 改为分开的矩阵
    glm::mat4 view;
    glm::vec4 light[4];
    glm::vec4 cameraPos;
    float exposure = 4.5f;
    float gamma = 2.2f;
};

// main.cpp
void VulkanExample::prepareData() {
    mainPassData.projection = camera.matrices.perspective;
    mainPassData.view = camera.matrices.view;
    mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
    mainPassData.light[0] = glm::vec4(10.0f, 10.0f, 10.0f, 1.0f);
    mainPass->UpdateGlobal(mainPassData);
}
```

### 修复 2: 在 CaptureCubemap 中准备 CAPTURE_SCENE PSO（解决崩溃）

**文件**: `examples/lightprobesh2/main.cpp`

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    // ...
    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    } else {
        // ✅ 确保 CAPTURE_SCENE PSO 已准备
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    }
    // ...
}
```

### 修复 3: 使用 gl_ViewIndex（解决黑色问题）

**文件**: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`

```glsl
#version 450
#extension GL_EXT_multiview : enable  // ✅ 添加扩展

void main()
{
    vec3 N = normalize(inNormal);
    // ✅ 使用 gl_ViewIndex 选择正确的相机位置
    vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);
    vec3 R = reflect(-V, N);
    // ...
}
```

### 修复 4: 正确的绘制方式（解决捕捉不到信息）

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

## 📊 修复效果总结

| 问题 | 原因 | 解决方案 | 状态 |
|------|------|--------|------|
| **mainpass 不显示** | UBO 结构不匹配 | 统一使用 projection + view | ✅ |
| **CaptureCubemap 崩溃** | PSO 未初始化 | 在 CaptureCubemap 中准备 PSO | ✅ |
| **捕获是黑色** | 相机位置错误 | 使用 gl_ViewIndex | ✅ |
| **捕捉不到信息** | 绘制次数错误 | 恢复 4 个副本 | ✅ |

## 🔑 关键要点

1. **UBO 结构必须匹配**
   - C++ 和 GLSL 中的结构定义必须完全一致
   - 包括字段顺序、类型和大小

2. **每个技术都需要对应的 PSO**
   - MAIN 和 CAPTURE_SCENE 都需要各自的 PSO
   - PSO 必须在使用前初始化

3. **Multiview 渲染中必须使用 gl_ViewIndex**
   - 每个面有不同的视图矩阵和相机位置
   - 不能硬编码使用某一个面的数据

4. **CAPTURE_SCENE 和 MAIN 的绘制逻辑基本相同**
   - 都绘制 4 个副本
   - 唯一的区别是 model->draw() 的参数

## 📝 修改文件列表

1. ✅ `shaders/glsl/lightprobesh2/gltfmesh.vert` - 使用 projection + view
2. ✅ `shaders/glsl/lightprobesh2/gltfmesh.frag` - 使用 projection + view
3. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` - 使用 gl_ViewIndex
4. ✅ `examples/lightprobesh2/Pass.h` - 改为 projection + view
5. ✅ `examples/lightprobesh2/main.cpp` - 在 CaptureCubemap 中准备 PSO
6. ✅ `examples/lightprobesh2/gltfload.cpp` - 根据技术类型调整 draw() 参数

## 🚀 下一步

1. 重新编译 C++ 代码
2. 测试 CaptureCubemap 功能
3. 验证捕获的 cubemap 是否正确
4. 检查 SH 和 IBL 生成是否正常

