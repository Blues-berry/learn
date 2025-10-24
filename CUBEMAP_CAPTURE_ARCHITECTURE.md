# Cubemap捕获系统架构

## 类关系图

```
VulkanExample (主应用)
    ├── LightProbe (光照探针)
    │   ├── CaptureScenePass (捕获渲染通道)
    │   │   ├── RenderTargetCube (立方体贴图目标)
    │   │   ├── DepthStencil (深度缓冲)
    │   │   ├── ResourceView (图像视图)
    │   │   └── GlobalUbo (UBO数据)
    │   ├── Skybox (天空盒)
    │   └── GltfModel (glTF模型)
    │
    ├── GenSHComputePass (SH计算)
    ├── GenIBLPass (IBL生成)
    └── MainPass (主渲染通道)
```

---

## 核心类详解

### 1. VulkanExample
**职责**: 应用主类，管理整个捕获流程

**关键方法**:
- `CaptureCubemap(position)`: 入口点
- `PreparePasses()`: 初始化所有渲染通道
- `PrepareProbes()`: 初始化光照探针网格

**关键成员**:
```cpp
std::unique_ptr<CaptureScenePass> capturePass;  // 全局捕获通道
std::vector<std::unique_ptr<LightProbe>> lightProbes;  // 探针列表
std::vector<std::shared_ptr<vks::TextureCubeMap>> cubeMaps;  // 捕获的cubemap
```

---

### 2. LightProbe
**职责**: 单个光照探针，负责在指定位置捕获cubemap

**关键方法**:
```cpp
void SetPosition(const glm::vec3& position);
void setSkybox(Skybox* skybox);
void setPreviewModel(PreviewModel* previewModel);
void SetGltfModel(GltfModel* model);
void CaptureCubeMap(VkQueue queue, VkCommandBuffer cmd);
std::shared_ptr<vks::TextureCubeMap> GetCubemap() const;
void SaveCubeMapFaces(VkQueue queue, const std::string& basePath);
```

**内部流程**:
1. 创建CaptureScenePass
2. 准备UBO数据（6个viewproj矩阵）
3. 执行drawScene()
4. 等待渲染完成
5. 执行布局转换
6. 返回cubemap

---

### 3. CaptureScenePass
**职责**: 管理cubemap的渲染过程

**关键成员**:
```cpp
std::shared_ptr<RenderTargetCube> cube;        // 立方体贴图
std::shared_ptr<DepthStencil> depthStencil;    // 深度缓冲
std::shared_ptr<ResourceView> colorView;       // 颜色视图
std::shared_ptr<ResourceView> dsView;          // 深度视图
VkRenderPass renderPass;                       // 渲染通道
VkFramebuffer framebuffer;                     // 帧缓冲
vks::Buffer globalBuffer;                      // UBO缓冲
```

**关键方法**:
```cpp
void UpdateGlobal(const GlobalUbo& ubo);
void Draw(VkCommandBuffer cmd, std::function<void(VkCommandBuffer)>&& encoder);
std::shared_ptr<vks::TextureCubeMap> GetCubeMap() const;
void FeedCubeDescriptor(VkDescriptorImageInfo& descriptor);
```

**渲染通道特点**:
- 使用Multiview扩展
- 单次渲染通道渲染6个面
- 最终布局为SHADER_READ_ONLY_OPTIMAL

---

### 4. RenderTargetCube
**职责**: 创建和管理立方体贴图

**继承关系**:
```
RenderAttachment
    └── RenderTarget2D
            └── RenderTargetCube
```

**关键方法**:
```cpp
std::shared_ptr<vks::TextureCubeMap> GetTextureCubeMap();
```

**特点**:
- 6层数组纹理（每层对应一个面）
- 支持高精度格式（R16G16B16A16_SFLOAT）
- 自动转换为TextureCubeMap

---

### 5. ResourceView
**职责**: 创建图像视图用于渲染

**构造函数**:
```cpp
ResourceView(const std::shared_ptr<RenderAttachment>& attachment,
             VkImageViewType type,
             uint32_t firstSlice,
             uint32_t sliceCount,
             VkImageAspectFlags flags);
```

**用途**:
- 颜色视图：VK_IMAGE_VIEW_TYPE_2D_ARRAY（6层）
- 深度视图：VK_IMAGE_VIEW_TYPE_2D_ARRAY（6层）

---

## 数据流

### 捕获流程
```
CaptureCubemap(position)
    ↓
LightProbe::LightProbe()
    ├─ CaptureScenePass::CaptureScenePass()
    │   ├─ RenderTargetCube::RenderTargetCube()
    │   ├─ DepthStencil::DepthStencil()
    │   ├─ ResourceView (colorView)
    │   ├─ ResourceView (dsView)
    │   └─ PrepareFrameBuffer()
    │
    ↓
LightProbe::CaptureCubeMap()
    ├─ 准备UBO (6个viewproj矩阵)
    ├─ drawScene()
    │   └─ CaptureScenePass::Draw()
    │       ├─ vkCmdBeginRenderPass()
    │       ├─ encoder() [绘制场景]
    │       └─ vkCmdEndRenderPass()
    ├─ 等待队列空闲
    ├─ 布局转换 (COLOR_ATTACHMENT → SHADER_READ_ONLY)
    └─ 返回cubemap
    
    ↓
后处理
    ├─ GenSHComputePass::Generate()
    ├─ GenIBLPass::Generate()
    ├─ MainPass::UpdateBindings()
    └─ SaveCubeMapFaces()
```

---

## 关键设计决策

### 1. 为什么使用Multiview？
- **性能**: 单次渲染通道渲染6个面，减少CPU开销
- **效率**: 避免6次独立的渲染通道调用
- **简洁**: 着色器通过gl_ViewIndex自动处理

### 2. 为什么分离CaptureScenePass？
- **复用性**: 可用于多个LightProbe
- **灵活性**: 支持不同分辨率和格式
- **清晰性**: 渲染逻辑独立

### 3. 为什么使用RenderTargetCube？
- **抽象**: 隐藏Vulkan细节
- **转换**: 自动转换为TextureCubeMap
- **管理**: 统一的资源生命周期

### 4. 为什么需要布局转换？
- **渲染**: 需要COLOR_ATTACHMENT_OPTIMAL
- **采样**: 需要SHADER_READ_ONLY_OPTIMAL
- **同步**: 确保数据可见性

---

## 内存管理

### 资源生命周期

```
创建
├─ LightProbe构造
│   └─ CaptureScenePass构造
│       ├─ RenderTargetCube分配GPU内存
│       ├─ DepthStencil分配GPU内存
│       └─ GlobalBuffer分配GPU内存
│
使用
├─ CaptureCubeMap()
│   ├─ 更新GlobalBuffer
│   ├─ 执行渲染
│   └─ 布局转换
│
销毁
└─ LightProbe析构
    └─ CaptureScenePass析构
        ├─ 销毁RenderPass
        ├─ 销毁Framebuffer
        ├─ 释放GPU内存
        └─ 销毁采样器
```

---

## 扩展点

### 1. 支持多个探针
```cpp
std::vector<std::unique_ptr<LightProbe>> lightProbes;
// 每个探针独立捕获
for (auto& probe : lightProbes) {
    probe->CaptureCubeMap(queue);
}
```

### 2. 支持不同分辨率
```cpp
// 低分辨率快速捕获
LightProbe lowRes(device, example, 256, 256);

// 高分辨率精细捕获
LightProbe highRes(device, example, 2048, 2048);
```

### 3. 支持不同格式
```cpp
// 在CaptureScenePass构造中指定
CaptureScenePass(device, example, VK_FORMAT_R32G32B32A32_SFLOAT, 1024, 1024);
```

### 4. 支持异步捕获
```cpp
// 使用VkFence等待完成
VkFence captureFence = ...;
probe->CaptureCubeMap(queue, cmd);
vkWaitForFences(device, 1, &captureFence, VK_TRUE, UINT64_MAX);
```

---

## 性能指标

| 操作 | 时间 | 备注 |
|------|------|------|
| 创建LightProbe | ~1ms | 分配GPU内存 |
| 捕获1024×1024 | ~5-10ms | 取决于场景复杂度 |
| 生成SH系数 | ~2-3ms | 计算着色器 |
| 生成IBL贴图 | ~10-20ms | 多个Mipmap级别 |
| 保存6个面 | ~50-100ms | 磁盘I/O |


