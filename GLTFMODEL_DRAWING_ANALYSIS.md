# GltfModel 绘制逻辑详细分析

## 1. 核心绘制流程

### 1.1 Draw() 函数入口 (gltfload.cpp:49-103)

```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
```

**关键步骤：**

1. **安全检查** (L58-63)
   - 验证 PSO (Pipeline State Object) 和 pipelineLayout 是否已准备
   - 如果未准备，打印错误并返回

2. **描述符集绑定** (L65-69)
   - 创建描述符集数组：`{globalSet, descriptorSet}`
   - globalSet：来自 MainPass 的全局数据（投影、视图、光照等）
   - descriptorSet：模型自己的数据（变换、材质）
   - 绑定到管线布局的绑定点 0 和 1

3. **管线绑定** (L70)
   - 绑定对应技术的图形管线

4. **多实例绘制** (L91-102)
   - 循环 4 次，在不同位置绘制模型
   - 每次迭代：
     - 计算 Push Constant（模型偏移矩阵 + 颜色）
     - 根据技术类型调用不同的 model->draw()

---

## 2. 描述符集布局设计

### 2.1 两层描述符集架构

**Set 0 (GlobalSet - 来自 MainPass)**
- Binding 0: 全局 UBO（投影、视图、光照、相机位置等）
- Binding 1: SH 系数（球谐）
- Binding 2: BRDF LUT 纹理
- Binding 3: 辐照度立方体贴图
- Binding 4: 预过滤立方体贴图

**Set 1 (GltfModel 自己的 descriptorSet)**
- Binding 0: 局部变换矩阵 UBO
- Binding 1: 材质参数 UBO（粗糙度、金属度、镜面反射等）
- Binding 2: 模型纹理（COMBINED_IMAGE_SAMPLER）

### 2.2 管线布局创建 (PreparePSO L213-227)

```cpp
std::vector<VkDescriptorSetLayout> setLayouts = {
    passLayout,              // Set 0: 全局数据
    descriptorSetLayout      // Set 1: 模型数据
};
```

---

## 3. 着色器数据流

### 3.1 顶点着色器 (gltfmesh.vert)

**输入：**
- 顶点属性：位置、法线、UV
- Set 0 Binding 0: Global UBO（投影、视图矩阵）
- Set 1 Binding 0: Local UBO（模型矩阵）
- Push Constant: modelOffset（额外的模型变换）

**计算：**
```glsl
gl_Position = global.projection * global.view * pc.modelOffset * ubo.model * vec4(inPos, 1.0);
```

**输出：**
- 世界坐标、法线、UV

### 3.2 片段着色器 (gltfmesh.frag)

**输入：**
- Set 0: 全局数据（SH、IBL 贴图、BRDF LUT）
- Set 1 Binding 1: 材质参数
- Push Constant: tint 颜色

**计算：**
- PBR 光照计算
- 使用 SH 或 IBL 进行环境光照
- 应用材质参数

---

## 4. Push Constant 机制

### 4.1 Push Constant 定义 (PreparePSO L219-222)

```cpp
VkPushConstantRange pushConstantRange{};
pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
pushConstantRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);  // 80 字节
```

### 4.2 Push Constant 使用 (Draw L72-94)

```cpp
struct PushConstantBlock {
    glm::mat4 modelOffset;  // 模型变换矩阵
    glm::vec4 tint;         // 颜色着色
} pc;

// 为 4 个实例设置不同的 Push Constant
for (int i = 0; i < 4; ++i) {
    pc.modelOffset = glm::translate(...) * glm::scale(...);
    pc.tint = glm::vec4(colors[i % 3], 1.0f);
    vkCmdPushConstants(...);
    model->draw(...);
}
```

---

## 5. 技术类型处理

### 5.1 两种技术模式

**ETechnique::MAIN**
- 标准渲染模式
- 调用 `model->draw(cmd)` 使用默认绑定

**ETechnique::CAPTURE_SCENE**
- 场景捕获模式（用于光照探针）
- 调用 `model->draw(cmd, vkglTF::RenderFlags::BindImages, pipelineLayout, 1)`
- 传递额外参数以绑定图像资源

---

## 6. 资源准备流程

### 6.1 初始化 (PreparePerBatchResource L105-149)

1. **创建描述符池** - 支持 UBO 和纹理采样器
2. **创建描述符集布局** - 定义 3 个绑定
3. **分配描述符集**
4. **创建 UBO 缓冲区**
   - localBuffer: 模型变换矩阵
   - materialBuffer: 材质参数

### 6.2 更新绑定 (UpdateSet L151-167)

- 将 UBO 缓冲区写入描述符集
- 纹理绑定被注释掉（可能在 model->draw() 中处理）

---

## 7. 关键问题与修复

### 7.1 已修复的问题

✅ **PSO 安全检查** (L58-63)
- 防止使用未初始化的管线

✅ **CAPTURE_SCENE 参数传递** (L97-98)
- 正确传递 pipelineLayout 和 bindImageSet 标志

✅ **多实例绘制** (L91-102)
- 为每个实例设置不同的 Push Constant

### 7.2 潜在问题

⚠️ **纹理绑定** (UpdateSet L158-165)
- 纹理绑定代码被注释
- 需要确认纹理是否在 model->draw() 中处理

⚠️ **描述符集同步**
- 需要确保 globalSet 和 descriptorSet 的生命周期一致

---

## 8. 调用链

```
main.cpp::drawFrame()
  └─ mainPass->Draw()
      └─ gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN)
          ├─ vkCmdBindDescriptorSets()  // 绑定 Set 0 和 Set 1
          ├─ vkCmdBindPipeline()        // 绑定管线
          ├─ for i in 0..3:
          │   ├─ vkCmdPushConstants()   // 设置 Push Constant
          │   └─ model->draw()          // 提交绘制命令
```

---

## 9. 性能优化建议

1. **批量绘制** - 使用 VkCmdDrawIndexedIndirect 减少 CPU 开销
2. **Push Constant 缓存** - 避免重复计算变换矩阵
3. **描述符集重用** - 共享相同的全局描述符集
4. **动态渲染** - 考虑使用 VK_KHR_dynamic_rendering 简化渲染通道


