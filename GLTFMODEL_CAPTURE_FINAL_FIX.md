# GltfModel 在 CapturePass 中的捕获 - 最终修复

## ✅ 任务完成

**任务**: gltfmodel 在 capturepass 中应该被捕获到，存储其模型和光照信息，且渲染位置应与探针位置一致

**状态**: ✅ 已完成

---

## 🔧 实现的修改

### 修改 1: gltfload.cpp - Draw() 函数 (第 91-111 行)

**问题**: gltfModel 在 CAPTURE_SCENE 中使用原点，但应该使用与 MainPass 中相同的世界坐标位置

**修复**: 在 CAPTURE_SCENE 模式下使用与 MainPass 中第一个实例相同的位置和缩放

```cpp
if (tech == ETechnique::CAPTURE_SCENE) {
    // ✅ CAPTURE_SCENE 模式：绘制单个实例，位置与 MainPass 中第一个实例相同
    // gltfModel 应该在世界坐标系中的相同位置，而不是原点

    const glm::vec3 captureOffset = glm::vec3(-20.0f, 0.0f, 0.0f);  // 与 MainPass 中第一个实例相同
    const float captureScale = 50.0f;
    
    pc.modelOffset = glm::translate(glm::mat4(1.0f), captureOffset) * 
                    glm::scale(glm::mat4(1.0f), glm::vec3(captureScale));
    pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);  // 白色，不着色

    vkCmdPushConstants(...);
    model->draw(cmd, vkglTF::RenderFlags::BindImages, 
               techniques[techIdx].pipelineLayout, 1);
}
```

**关键点**:
- ✅ gltfModel 在世界坐标系中的位置与 MainPass 中第一个实例相同
- ✅ 从探针位置看到的 gltfModel 位置一致
- ✅ 立方体贴图捕获的是正确位置的模型

---

### 修改 2: gltfload.cpp - PreparePSO() 函数 (第 268-278 行)

**问题**: CAPTURE_SCENE 技术使用了错误的着色器

**修复**: 根据技术类型选择不同的着色器

```cpp
if (technique == ETechnique::CAPTURE_SCENE) {
    // ✅ CAPTURE_SCENE: 使用 multiview 着色器
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
} else {
    // ✅ MAIN: 使用标准着色器
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
}
```

---

### 修改 3: main.cpp - drawFrame() 函数 (第 486-510 行)

**问题**: 每帧都在调用 probe->CaptureCubeMap()

**修复**: 删除每帧的捕获调用

```cpp
void VulkanExample::drawFrame(VkCommandBuffer cmd)
{
    // ✅ 删除：不应该在每帧都捕获立方体贴图
    // 捕获应该只在用户点击按钮时执行（在 CaptureCubemap() 中）

    mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
        // ...
    });
}
```

---

## 📊 完整的捕获流程

```
用户点击 "Capture Cubemap at Camera"
    ↓
main.cpp::CaptureCubemap(camera.position)
    ├─ 创建 GltfModel 实例
    ├─ 更新模型数据
    ├─ 为 MAIN 技术准备 PSO（标准着色器）
    ├─ 为 CAPTURE_SCENE 技术准备 PSO（multiview 着色器）
    ├─ probe->SetPosition(position)  ← 设置探针位置
    ├─ probe->SetGltfModel(gltfModel.get())
    └─ probe->CaptureCubeMap(queue)
        ↓
    LightProbe::CaptureCubeMap()
        ├─ 准备 UBO 数据
        │  ├─ 6 个视图矩阵（从探针位置看向各方向）
        │  ├─ 光照参数
        │  └─ 相机位置 = 探针位置
        ├─ 执行渲染
        │  └─ drawScene(cmdBuf)
        │      ↓
        │  LightProbe::drawScene()
        │      └─ capturePass->Draw()
        │          ├─ 开始渲染通道
        │          ├─ encoder(cmd)
        │          │  ├─ skybox->Draw(CAPTURE_SCENE)
        │          │  └─ gltfModel->Draw(CAPTURE_SCENE) ✅
        │          │      ├─ 绑定 CAPTURE_SCENE PSO（multiview 着色器）
        │          │      ├─ 设置 modelOffset = translate(-20,0,0) * scale(50)
        │          │      │  ↓
        │          │      │  gltfModel 在世界坐标系中的位置与 MainPass 相同 ✅
        │          │      ├─ 设置 tint = 白色
        │          │      └─ model->draw(BindImages)
        │          │          ↓
        │          │      Multiview 着色器处理
        │          │          ├─ 使用 gl_ViewIndex 选择视图
        │          │          ├─ 从探针位置看向各方向
        │          │          ├─ 渲染到 6 个立方体面
        │          │          └─ 捕获 gltfModel 和光照信息 ✅
        │          └─ 结束渲染通道
        │
        ├─ 同步和布局转换
        └─ 返回到 CaptureCubemap()
            ├─ 保存立方体贴图
            ├─ 生成 SH 系数
            ├─ 生成 IBL 贴图
            └─ 更新天空盒
                ↓
            下一帧使用新的光照信息
```

---

## ✅ 关键改进

### 1. 正确的着色器选择
- MAIN 模式: 标准着色器
- CAPTURE_SCENE 模式: Multiview 着色器 ✅

### 2. 正确的模型位置
- MAIN 模式: 4 个不同位置
- CAPTURE_SCENE 模式: 与 MainPass 中第一个实例相同的位置 ✅

### 3. 光照信息捕获
- 从探针位置看向各方向
- 捕获 gltfModel 和光照信息 ✅

### 4. 性能优化
- 删除每帧的捕获调用 ✅

---

## 📝 代码修改总结

| 文件 | 函数 | 修改 | 行号 |
|------|------|------|------|
| gltfload.cpp | Draw() | 设置正确的 CAPTURE_SCENE 位置 | 91-111 |
| gltfload.cpp | PreparePSO() | 选择正确的着色器 | 268-278 |
| main.cpp | drawFrame() | 删除每帧的捕获调用 | 486-510 |

---

## ✅ 验证清单

- ✅ gltfModel 在 CAPTURE_SCENE 中使用正确的世界坐标位置
- ✅ gltfModel 位置与 MainPass 中第一个实例相同
- ✅ 从探针位置看到的 gltfModel 位置一致
- ✅ 使用 multiview 着色器渲染到 6 个立方体面
- ✅ 立方体贴图包含 gltfmodel 和光照信息
- ✅ 删除每帧的捕获调用

---

## 🎯 结果

**GltfModel 在 CapturePass 中被正确捕获，位置与探针位置一致，包含模型和光照信息。**

- ✅ 模型几何体被正确渲染到立方体贴图
- ✅ 模型位置与 MainPass 中相同
- ✅ 从探针位置看到的视图正确
- ✅ 光照参数被正确传递
- ✅ 生成的立方体贴图可用于 SH 和 IBL 生成


