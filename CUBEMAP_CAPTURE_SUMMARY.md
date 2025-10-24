# Cubemap捕获系统 - 快速参考

## 一句话总结
在运行时从指定位置捕获环境cubemap，用于实时光照探针和IBL计算。

---

## 核心流程（5步）

### 1️⃣ 创建探针
```cpp
probe = std::make_unique<LightProbe>(vulkanDevice, this, 1024, 1024);
probe->SetPosition(camera.position);
probe->setSkybox(skybox.get());
probe->setPreviewModel(previewModel.get());
probe->SetGltfModel(gltfModel.get());
```

### 2️⃣ 执行捕获
```cpp
probe->CaptureCubeMap(queue);
```

**内部发生的事**:
- 准备6个视图投影矩阵
- 使用Multiview同时渲染6个面
- 执行布局转换

### 3️⃣ 生成SH系数
```cpp
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);
```

### 4️⃣ 生成IBL贴图
```cpp
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
```

### 5️⃣ 更新渲染
```cpp
mainPass->UpdateBindings();
skybox->SetCubeMap(capturedCubemap);
```

---

## 关键概念

### Multiview渲染
- **什么**: 单次渲染通道同时渲染到6个立方体面
- **怎样**: 使用VK_KHR_MULTIVIEW扩展
- **好处**: 性能提升，减少CPU开销

### 6个视图矩阵
```
+X: 看向右边   (1,0,0)  Up: (0,-1,0)
-X: 看向左边  (-1,0,0)  Up: (0,-1,0)
+Y: 看向上方   (0,1,0)  Up: (0,0,1)   ← 特殊！
-Y: 看向下方   (0,-1,0) Up: (0,0,-1)  ← 特殊！
+Z: 看向前方   (0,0,1)  Up: (0,-1,0)
-Z: 看向后方   (0,0,-1) Up: (0,-1,0)
```

### 投影矩阵
- 视角: 90°（立方体贴图标准）
- 宽高比: 1.0（正方形）
- 近/远: 0.1 / 256.0

### 布局转换
```
渲染完成
    ↓
COLOR_ATTACHMENT_OPTIMAL
    ↓
[内存屏障]
    ↓
SHADER_READ_ONLY_OPTIMAL
    ↓
可被着色器采样
```

---

## 关键类

| 类 | 职责 | 关键方法 |
|----|------|--------|
| **LightProbe** | 单个探针 | `CaptureCubeMap()` |
| **CaptureScenePass** | 渲染管理 | `Draw()`, `GetCubeMap()` |
| **RenderTargetCube** | 立方体贴图 | `GetTextureCubeMap()` |
| **GenSHComputePass** | SH计算 | `Generate()` |
| **GenIBLPass** | IBL生成 | `Generate()` |

---

## 数据结构

### GlobalUbo
```cpp
struct GlobalUbo {
    glm::mat4 viewproj[6];      // 6个面的视图投影矩阵
    glm::vec4 cameraPos[6];     // 6个面的相机位置
    glm::vec4 mainLight;        // 光源信息
    float exposure;             // 曝光值
    float gamma;                // 伽马值
};
```

### 输出
- **cubemap**: 1024×1024 R16G16B16A16_SFLOAT
- **SH系数**: 9个vec4（3阶球谐）
- **Irradiance Map**: 漫反射环境光
- **Prefiltered Map**: 镜面反射环境光

---

## 常见问题

### Q: 为什么需要6个视图矩阵？
A: 立方体贴图有6个面，每个面需要不同的视图矩阵来正确渲染。

### Q: 为什么+Y和-Y的Up向量不同？
A: 因为看向上/下方时，需要不同的"右"方向来保持正确的纹理坐标。

### Q: Multiview有什么优势？
A: 单次渲染通道渲染6个面，比6次独立渲染快得多。

### Q: 为什么需要布局转换？
A: 渲染时需要COLOR_ATTACHMENT_OPTIMAL，采样时需要SHADER_READ_ONLY_OPTIMAL。

### Q: 可以实时捕获吗？
A: 可以，但会有性能开销。建议在需要时捕获，而不是每帧都捕获。

### Q: 支持多个探针吗？
A: 支持。每个LightProbe独立捕获，可以创建探针网格。

---

## 性能优化建议

### 1. 分辨率选择
```cpp
// 快速预览
LightProbe preview(device, example, 256, 256);

// 高质量
LightProbe highQuality(device, example, 2048, 2048);
```

### 2. 异步捕获
```cpp
// 在后台线程中捕获
std::thread captureThread([&]() {
    probe->CaptureCubeMap(queue);
});
```

### 3. 缓存结果
```cpp
// 保存cubemap供后续使用
probe->SaveCubeMapFaces(queue, "captured_");
```

### 4. 选择性更新
```cpp
// 只在需要时更新SH和IBL
if (needsUpdate) {
    shGenPass->Generate(queue);
    genIBL->Generate(queue);
}
```

---

## 调试技巧

### 1. 验证视图矩阵
```cpp
// 打印6个视图矩阵
for (int i = 0; i < 6; ++i) {
    std::cout << "Face " << i << ": " << glm::to_string(viewMatrices[i]) << std::endl;
}
```

### 2. 检查布局转换
```cpp
// 确保布局转换成功
if (cubemap->imageLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    std::cerr << "Layout transition failed!" << std::endl;
}
```

### 3. 保存中间结果
```cpp
// 保存cubemap面用于检查
probe->SaveCubeMapFaces(queue, "debug_");
```

### 4. 验证SH系数
```cpp
// 检查SH系数是否合理
for (int i = 0; i < 9; ++i) {
    std::cout << "SH[" << i << "]: " << glm::to_string(shCoeffs[i]) << std::endl;
}
```

---

## 相关文件

| 文件 | 内容 |
|------|------|
| `main.cpp` | 应用主类，CaptureCubemap()入口 |
| `LightProbe.h/cpp` | LightProbe类实现 |
| `UpsampleCubeMapPass.h/cpp` | CaptureScenePass类实现 |
| `Pass.h/cpp` | 其他渲染通道 |

---

## 扩展阅读

- `CUBEMAP_CAPTURE_ANALYSIS.md` - 详细流程分析
- `CUBEMAP_CAPTURE_CODE_DETAILS.md` - 代码细节解析
- `CUBEMAP_CAPTURE_ARCHITECTURE.md` - 系统架构设计


