# 🔍 gltfModel问题分析

## 问题1: gltfModel初始为黑色

### 根本原因
在 `gltfload.h` 中，`MaterialBuffer` 的默认值：
```cpp
struct MaterialBuffer {
    float roughness = 1.f;      // ❌ 太粗糙
    float metallic = 0.5;
    float specular = 0.5;
    float padding = 0.f;
    glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);
    
    int32_t useSH = 1;          // ❌ 使用SH（但SH还没生成）
    int32_t useReflection = 0;  // ❌ 不使用反射
};
```

**问题**:
- `roughness = 1.0` 太粗糙，导致镜面反射很弱
- `useSH = 1` 但SH系数还没生成，导致光照为0
- `useReflection = 0` 不使用IBL反射

### 解决方案
修改默认值：
```cpp
struct MaterialBuffer {
    float roughness = 0.5f;     // ✅ 适中的粗糙度
    float metallic = 0.5;
    float specular = 0.5;
    float padding = 0.f;
    glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);
    
    int32_t useSH = 0;          // ✅ 初始不使用SH
    int32_t useReflection = 0;  // ✅ 初始不使用反射
};
```

---

## 问题2: gltfModel跟随视角移动

### 根本原因
在 `main.cpp` 的 `PrepareScene()` 中（第310-314行）：
```cpp
gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);

// ❌ 设置了变换，但可能在着色器中被覆盖
glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(-10.0f, 0.0f, 0.0f));
glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
gltfModel->SetTransform(t * s);
```

**问题**:
- 模型变换被设置了，但在着色器中可能被相机矩阵覆盖
- 或者 `Draw()` 函数中的 push constant 覆盖了变换

### 解决方案
检查 `gltfload.cpp` 的 `Draw()` 函数，确保在 MAIN 技术中正确应用模型变换。

---

## 问题3: 捕获的图像只有一张

### 根本原因
可能是以下几个原因之一：

1. **Multiview渲染没有正确工作**
   - 着色器没有正确使用 `gl_ViewIndex`
   - UBO 数组没有正确初始化

2. **SaveCubeMapFaces() 只保存了一张**
   - 循环可能有问题
   - 文件名生成有问题

3. **Cubemap 本身只有一个面**
   - RenderPass 的 multiview 配置有问题
   - 渲染目标只有一个面

### 解决方案
需要检查：
1. `LightProbe::CaptureCubeMap()` 中的 UBO 初始化
2. `LightProbe::SaveCubeMapFaces()` 中的循环逻辑
3. `CaptureScenePass` 的 multiview 配置

---

## 修复优先级

1. **高优先级**: 修改 `MaterialBuffer` 默认值（问题1）
2. **高优先级**: 检查 `Draw()` 函数中的变换应用（问题2）
3. **中优先级**: 检查 cubemap 保存逻辑（问题3）


