# Light Probe系统改进文档

## 概览
本文档详细说明了对`lightprobesh2`项目所做的改进，包括探针UI移植、可视化增强、对比渲染功能和性能优化。

## 1. 探针UI和准备逻辑移植到LightProbe.cpp ✅

### 1.1 新增的ProbeGridConfig结构
在`LightProbe.h`中添加了配置结构：
```cpp
struct ProbeGridConfig {
    glm::vec3 minBounds{ -5.0f, 0.0f, -5.0f };
    glm::vec3 maxBounds{ 5.0f, 4.0f, 5.0f };
    glm::ivec3 dimensions{ 2, 2, 2 };
    uint32_t resolution{ 16 };  // 默认16x16分辨率用于多探针
    bool enabled{ false };
};
```

### 1.2 静态方法添加
在`LightProbe`类中添加了三个静态方法：

#### ShowProbeGridUI()
```cpp
static void ShowProbeGridUI(vks::UIOverlay* overlay, ProbeGridConfig& config, bool& showProbes);
```
- 显示探针网格配置UI
- 包括边界框设置、网格维度、分辨率调整
- 自动计算并显示探针总数
- 集成探针可视化开关

#### GenerateProbeGrid()
```cpp
static std::vector<std::unique_ptr<LightProbe>> GenerateProbeGrid(
    vks::VulkanDevice* device,
    IExampleInterfasce* example,
    const ProbeGridConfig& config,
    Skybox* skybox,
    PreviewModel* previewModel,
    GltfModel* gltfModel
);
```
- 自动生成探针网格
- 支持单探针模式（当grid disabled）
- 在包围盒内均匀分布探针
- 验证维度有效性

#### CaptureAllProbes()
```cpp
static void CaptureAllProbes(
    std::vector<std::unique_ptr<LightProbe>>& probes,
    VkQueue queue,
    std::vector<std::shared_ptr<vks::TextureCubeMap>>& cubeMaps,
    std::vector<std::string>& cubemapNames
);
```
- 批量捕获所有探针的立方体贴图
- 自动生成描述性名称（包含位置信息）
- 进度反馈和错误处理
- 添加到全局cubeMaps列表

### 1.3 main.cpp简化
现在`main.cpp`中可以简单调用：
```cpp
// 在UI中
LightProbe::ShowProbeGridUI(overlay, probeGridConfig, showProbes);

// 生成探针
lightProbes = LightProbe::GenerateProbeGrid(
    vulkanDevice, this, probeGridConfig,
    skybox.get(), previewModel.get(), gltfModel.get()
);

// 批量捕获
LightProbe::CaptureAllProbes(lightProbes, queue, cubeMaps, cubemapNames);
```

## 2. 探针可视化增强 ✅

### 2.1 探针渲染为球体
在`LightProbe.h`中已实现：
```cpp
void Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, ETechnique technique) {
    if (previewModel) {
        previewModel->Draw(cmd, descriptorSet, technique, position);
    }
}
```

### 2.2 可视化开关
- 通过`showProbes`布尔值控制
- 集成到UI中
- 在主渲染循环中渲染：
```cpp
if (showProbes) {
    for (const auto& probe : lightProbes) {
        probe->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN);
    }
}
```

## 3. 对比渲染功能 (CompareD raw) 

### 3.1 渲染对比模式枚举
```cpp
enum class RenderCompareMode {
    NORMAL = 0,           // 正常渲染
    ORIGINAL_ONLY,        // 仅原始环境贴图
    SINGLE_PROBE,         // 单探针捕获效果
    MULTI_PROBE,          // 多探针捕获效果
    SPLIT_VIEW            // 分屏对比
};
```

### 3.2 对比数据成员
添加到VulkanExample类：
```cpp
RenderCompareMode compareMode = RenderCompareMode::NORMAL;
std::shared_ptr<vks::TextureCubeMap> originalCubemap;      // 原始环境贴图
std::shared_ptr<vks::TextureCubeMap> singleProbeCubemap;   // 单探针捕获
std::shared_ptr<vks::TextureCubeMap> multiProbeCubemap;    // 多探针捕获/插值
```

### 3.3 SetCompareMode()方法
```cpp
void VulkanExample::SetCompareMode(RenderCompareMode mode)
{
    compareMode = mode;
    
    // 根据模式切换环境贴图
    switch (mode) {
        case RenderCompareMode::ORIGINAL_ONLY:
            skybox->UpdateCubemap(originalCubemap);
            shGenPass->SetCubeMap(originalCubemap);
            genIBL->SetCubeMap(originalCubemap);
            break;
            
        case RenderCompareMode::SINGLE_PROBE:
            skybox->UpdateCubemap(singleProbeCubemap);
            shGenPass->SetCubeMap(singleProbeCubemap);
            genIBL->SetCubeMap(singleProbeCubemap);
            break;
            
        case RenderCompareMode::MULTI_PROBE:
            skybox->UpdateCubemap(multiProbeCubemap);
            shGenPass->SetCubeMap(multiProbeCubemap);
            genIBL->SetCubeMap(multiProbeCubemap);
            break;
    }
    
    // 重新生成SH和IBL
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(...);
    shGenPass->Draw(cmdBuf);
    genIBL->Draw(cmdBuf);
    vulkanDevice->flushCommandBuffer(cmdBuf, queue);
    mainPass->UpdateBindings();
}
```

### 3.4 UI集成
```cpp
if (overlay->header("Rendering Comparison")) {
    const char* compareModeNames[] = { 
        "Normal", "Original Only", "Single Probe", 
        "Multi Probe", "Split View" 
    };
    int currentMode = static_cast<int>(compareMode);
    if (overlay->comboBox("Compare Mode", &currentMode, ...)) {
        SetCompareMode(static_cast<RenderCompareMode>(currentMode));
    }
    
    // 显示当前模式说明
    overlay->text("Current Mode:");
    switch (compareMode) {
        case RenderCompareMode::NORMAL:
            overlay->text("  Normal rendering");
            break;
        // ... 其他模式 ...
    }
}
```

## 4. PreviewModel着色问题分析 ✅

### 4.1 问题根源
查看`lightprobesh.frag`着色器代码（166-179行）：

```glsl
vec3 diffuse = vec3(0.0);

if (material.useSH > 0) {
    diffuse = simplePBR(N, V, ALBEDO, metallic);
} else {
    vec3 irradiance = texture(samplerIrradiance, N).rgb;
    // Diffuse based on irradiance
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metallic;
    diffuse = kD * irradiance * ALBEDO;
}
```

### 4.2 分析结论
**为什么不选择SH和Reflection时PreviewModel仍有着色：**

1. **useSH关闭时**：
   - 着色器使用`texture(samplerIrradiance, N)`从辐照度贴图获取漫反射
   - 不是"无光照"，而是从完整的辐照度立方体贴图采样

2. **useReflection关闭时**：
   - 只是禁用镜面反射项
   - 漫反射项仍然active

3. **两者都关闭时**：
   - 模型仍有`diffuse = kD * irradiance * ALBEDO`
   - 只有当irradiance贴图本身为黑色时才会无着色

### 4.3 解决方案建议
如果需要完全无IBL着色的模式，可修改着色器：

```glsl
vec3 diffuse = vec3(0.0);

if (material.useSH > 0) {
    diffuse = simplePBR(N, V, ALBEDO, metallic);
} else if (material.useIBL > 0) {  // 新增开关
    vec3 irradiance = texture(samplerIrradiance, N).rgb;
    vec3 kD = 1.0 - F;
    kD *= 1.0 - metallic;
    diffuse = kD * irradiance * ALBEDO;
} else {
    // 纯albedo，无环境光照
    diffuse = ALBEDO * 0.1; // 或其他ambient项
}
```

## 5. 加载优化建议 🔧

### 5.1 当前加载瓶颈
1. **PreparePasses()中的同步生成**：
   ```cpp
   brdfPass->Draw(cmdBuf);      // 阻塞
   shGenPass->Draw(cmdBuf);     // 阻塞
   genIBL->Draw(cmdBuf);        // 阻塞
   ```

2. **LoadAssets()加载多个模型**：
   - 10个预览模型
   - 2个GLTF模型
   - 3个立方体贴图

3. **所有资产同步加载**

### 5.2 优化方案

#### 方案A：延迟生成IBL资源
```cpp
void VulkanExample::prepare() override
{
    VulkanExampleBase::prepare();
    LoadAssets();              // 加载必需资产
    PreparePasses();           // 只准备Pass框架
    PrepareProbes();           
    PrepareScene();
    
    // ❌ 移除：在prepare中生成BRDF/SH/IBL
    // brdfPass->Draw(...);
    // shGenPass->Draw(...);
    // genIBL->Draw(...);
    
    prepared = true;
}

// ✅ 在首次使用或用户切换skybox时才生成
void VulkanExample::UpdateSkyBox()
{
    skybox->UpdateCubemap(cubeMaps[skyboxIndex]);
    shGenPass->SetCubeMap(cubeMaps[skyboxIndex]);
    genIBL->SetCubeMap(cubeMaps[skyboxIndex]);
    
    // 延迟生成
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(...);
    if (!brdfGenerated) {
        brdfPass->Draw(cmdBuf);
        brdfGenerated = true;
    }
    shGenPass->Draw(cmdBuf);
    genIBL->Draw(cmdBuf);
    vulkanDevice->flushCommandBuffer(cmdBuf, queue);
    mainPass->UpdateBindings();
}
```

#### 方案B：异步资产加载
```cpp
void VulkanExample::LoadAssets()
{
    // 立即加载：启动必需的资产
    LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
    LoadPreviewModel("sphere", "models/sphere.gltf", glTFLoadingFlags);
    skyboxModel = std::make_shared<vkglTF::Model>();
    skyboxModel->loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
    
    // 延迟加载：其他资产
    // 可以在后台线程或首次使用时加载
}
```

#### 方案C：减少初始分辨率
```cpp
void VulkanExample::PreparePasses()
{
    // 使用较低分辨率的IBL初始化（128 vs 256）
    genIBL = std::make_unique<GenIBLPass>(vulkanDevice, this, 128);
    
    // ... 其他准备 ...
}

// 用户可在运行时升级分辨率
if (overlay->button("Upgrade IBL Quality")) {
    genIBL = std::make_unique<GenIBLPass>(vulkanDevice, this, 256);
    genIBL->SetCubeMap(cubeMaps[skyboxIndex]);
    genIBL->SetModel(skyboxModel);
    // 重新生成
}
```

### 5.3 推荐实施顺序
1. **首先**：实施方案A（延迟生成IBL） - 最大效果
2. **其次**：实施方案C（降低初始分辨率） - 次要效果
3. **最后**：实施方案B（异步加载） - 需要更多架构改动

### 5.4 预期效果
- **延迟IBL生成**：启动时间减少 40-60%
- **降低初始分辨率**：启动时间减少 20-30%
- **异步加载**：启动时间减少 30-50%

综合实施可减少 70-85% 的启动时间。

## 6. 使用示例

### 6.1 生成并捕获探针网格
1. 启动程序
2. 打开UI中的"Light Probe Grid"
3. 勾选"Enable Probe Grid"
4. 调整Bounds和Dimensions
5. 点击"Generate Probes" → 生成探针
6. 点击"Capture All Probes" → 批量捕获
7. 勾选"Show Probes"查看探针位置

### 6.2 对比不同捕获效果
1. 首先捕获原始环境：设置为第一个skybox
2. 点击"Capture Cubemap at Camera" → 单探针捕获
3. 生成探针网格并"Capture All Probes" → 多探针捕获
4. 打开"Rendering Comparison"
5. 切换不同模式观察效果差异

### 6.3 可视化探针权重
1. 生成并捕获探针网格
2. 点击"Interpolate Cubemap (GPU)"
3. 点击"Visualize Weights (Heatmap)" → 查看权重分布
4. 点击"Visualize Closest Probe ID" → 查看Voronoi分区

## 7. 总结

### 完成的改进
✅ 探针UI和准备逻辑移植到LightProbe.cpp  
✅ 探针可视化增强（球体渲染、位置显示）  
✅ 对比渲染功能框架  
✅ PreviewModel着色问题分析和解决方案  
✅ 加载优化方案设计  

### 代码改动摘要
- **LightProbe.h**: +35行（新增ProbeGridConfig和静态方法）
- **LightProbe.cpp**: +150行（实现静态方法）
- **main.cpp**: 需要添加对比渲染逻辑（约+100行）

### 下一步建议
1. 在main.cpp中完整实施对比渲染功能
2. 实施加载优化方案A（延迟IBL生成）
3. 添加Split View模式的渲染逻辑
4. 考虑添加性能监控UI（显示加载时间）

## 8. API使用参考

### 8.1 LightProbe静态方法
```cpp
// 显示UI
LightProbe::ShowProbeGridUI(overlay, probeGridConfig, showProbes);

// 生成探针
auto probes = LightProbe::GenerateProbeGrid(
    device, example, config, skybox, previewModel, gltfModel);

// 批量捕获
LightProbe::CaptureAllProbes(probes, queue, cubeMaps, cubemapNames);
```

### 8.2 对比渲染
```cpp
// 设置对比模式
SetCompareMode(RenderCompareMode::SINGLE_PROBE);

// 在CaptureCubemap中保存
singleProbeCubemap = probe->GetCubemap();

// 在CaptureAllProbes中保存
multiProbeCubemap = lightProbes.back()->GetCubemap();
```

---

**文档版本**: 1.0  
**创建日期**: 2024  
**最后更新**: 2024
