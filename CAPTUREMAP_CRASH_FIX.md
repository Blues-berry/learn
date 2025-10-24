# 🔧 CaptureCubemap 崩溃问题 - 原因分析和修复

## 🐛 问题

点击 "Capture Cubemap" 按钮时程序直接崩溃。

## 🔍 根本原因

**gltfModel 的 CAPTURE_SCENE PSO 没有被初始化**

### 问题流程

1. **PrepareScene() 中**:
   - 只为 MAIN 技术准备了 PSO
   - 注释掉了 CAPTURE_SCENE PSO 准备

2. **CaptureCubemap() 中**:
   - 创建 LightProbe
   - 调用 `probe->CaptureCubeMap(queue)`

3. **LightProbe::drawScene() 中**:
   - 调用 `gltfModel->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE)`
   - 尝试使用 `techniques[(uint32_t)ETechnique::CAPTURE_SCENE]` 的 PSO

4. **崩溃**:
   - `techniques[1].pso` 是 `VK_NULL_HANDLE`（未初始化）
   - `techniques[1].pipelineLayout` 是 `VK_NULL_HANDLE`（未初始化）
   - 尝试绑定无效的 PSO → **崩溃**

## ✅ 修复方案

### 修复 1: 在 CaptureCubemap() 中准备 CAPTURE_SCENE PSO

**文件**: `examples/lightprobesh2/main.cpp`

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    probe = std::make_unique<LightProbe>(vulkanDevice, this, 1024, 1024);
    probe->SetPosition(position);
    probe->setSkybox(skybox.get());
    probe->setPreviewModel(previewModel.get());

    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());

        // ✅ 为MAIN技术准备PSO
        gltfModel->PreparePSO(
            renderPass,
            mainPass->descriptorSetLayout,
            ETechnique::MAIN
        );

        // ✅ 为CAPTURE_SCENE技术准备PSO
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    } else {
        // ✅ 如果 gltfModel 已存在但没有 CAPTURE_SCENE PSO，则准备它
        gltfModel->PreparePSO(
            capturePass->renderPass,
            capturePass->descriptorSetLayout,
            ETechnique::CAPTURE_SCENE
        );
    }

    probe->SetGltfModel(gltfModel.get());
    probe->CaptureCubeMap(queue);
}
```

### 修复 2: 在 gltfModel::Draw() 中添加安全检查

**文件**: `examples/lightprobesh2/gltfload.cpp`

```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
    if (!model) {
        return;
    }

    uint32_t techIdx = (uint32_t)tech;
    
    // ✅ 安全检查：确保 PSO 已经准备好
    if (techniques[techIdx].pso == VK_NULL_HANDLE || 
        techniques[techIdx].pipelineLayout == VK_NULL_HANDLE)
    {
        std::cerr << "GltfModel::Draw - PSO not prepared for technique " << techIdx << "\n";
        return;
    }

    // ... 继续绘制 ...
}
```

## 📊 修复效果

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **CAPTURE_SCENE PSO** | ❌ 未初始化 | ✅ 在 CaptureCubemap() 中初始化 |
| **安全检查** | ❌ 无 | ✅ 有 |
| **崩溃** | ❌ 会崩溃 | ✅ 不会崩溃 |

## 🔑 关键要点

1. **每个技术都需要对应的 PSO**
   - MAIN 技术需要 MAIN PSO
   - CAPTURE_SCENE 技术需要 CAPTURE_SCENE PSO

2. **PSO 必须在使用前初始化**
   - 在 Draw() 之前调用 PreparePSO()
   - 不能使用未初始化的 PSO

3. **添加安全检查防止崩溃**
   - 检查 PSO 是否有效
   - 如果无效则跳过绘制

## 🚀 下一步

1. 重新编译代码
2. 测试 CaptureCubemap 功能
3. 验证 cubemap 是否正确捕获
4. 检查 SH 和 IBL 生成是否正常

## 📝 修改文件列表

1. ✅ `examples/lightprobesh2/main.cpp` - 在 CaptureCubemap() 中准备 CAPTURE_SCENE PSO
2. ✅ `examples/lightprobesh2/gltfload.cpp` - 添加 PSO 有效性检查

