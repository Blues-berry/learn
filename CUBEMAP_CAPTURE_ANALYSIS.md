# Cubemap捕获逻辑详细分析

## 概述
这个系统实现了在运行时动态捕获环境cubemap的功能，用于实时光照探针和IBL（Image-Based Lighting）计算。

---

## 核心流程

### 1. **入口点：CaptureCubemap()**
**位置**: `main.cpp:578-636`

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
```

**功能**:
- 在指定位置创建新的LightProbe对象
- 设置探针的场景引用（天空盒、模型）
- 执行cubemap捕获
- 生成SH系数和IBL贴图
- 保存捕获结果

**关键步骤**:
1. 创建1024×1024分辨率的LightProbe
2. 设置位置和场景对象
3. 调用`probe->CaptureCubeMap(queue)`执行捕获
4. 生成SH和IBL数据
5. 更新主渲染通道的绑定

---

## 2. **LightProbe类**
**位置**: `LightProbe.h/cpp`

### 构造函数
```cpp
LightProbe::LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, 
                       uint32_t width_, uint32_t height_)
```

**初始化**:
- 创建`CaptureScenePass`对象（用于渲染到cubemap）
- 存储设备指针和分辨率

### CaptureCubeMap()方法
**位置**: `LightProbe.cpp:50-141`

**核心逻辑**:

#### 步骤1: 准备UBO数据
```cpp
CaptureScenePass::GlobalUbo ubo = {};
glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 256.0f);
```

- 创建90°视角的投影矩阵（立方体贴图标准）
- 宽高比为1.0（正方形）
- 近裁剪面0.1，远裁剪面256.0

#### 步骤2: 生成6个面的视图矩阵
```cpp
std::array<glm::mat4, 6> viewMatrices = {
    glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0, -1,  0)), // +X
    glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1,  0)), // -X
    glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0,  0,  1)), // +Y
    glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0,  0, -1)), // -Y
    glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0, -1,  0)), // +Z
    glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0, -1,  0))  // -Z
};
```

**关键点**:
- 每个面都从probe位置看向不同方向
- Up向量根据面的方向调整（+Y面用Z作up，-Y面用-Z作up）
- 其他面都用-Y作up向量

#### 步骤3: 计算viewproj矩阵
```cpp
for (uint32_t face = 0; face < 6; ++face) {
    ubo.viewproj[face] = projection * viewMatrices[face];
    ubo.cameraPos[face] = glm::vec4(position, 1.0f);
}
```

#### 步骤4: 执行渲染
```cpp
drawScene(cmdBuf);
```

---

## 3. **CaptureScenePass类**
**位置**: `UpsampleCubeMapPass.h/cpp`

### 关键成员
```cpp
struct GlobalUbo {
    glm::mat4 viewproj[6];      // 6个面的视图投影矩阵
    glm::vec4 cameraPos[6];     // 6个面的相机位置
    glm::vec4 mainLight;        // 光源信息
    float exposure;             // 曝光值
    float gamma;                // 伽马值
};
```

### 渲染目标
- **RenderTargetCube**: 立方体贴图（R16G16B16A16_SFLOAT格式）
- **DepthStencil**: 深度缓冲（D32_SFLOAT，6层用于6个面）

### Draw()方法
```cpp
void CaptureScenePass::Draw(VkCommandBuffer cmd, 
                            std::function<void(VkCommandBuffer)>&& encoder)
```

**执行流程**:
1. 设置渲染区域和帧缓冲
2. 开始渲染通道：`vkCmdBeginRenderPass()`
3. 设置视口和裁剪矩形
4. 调用encoder回调函数绘制场景
5. 结束渲染通道：`vkCmdEndRenderPass()`

**Multiview支持**:
- 使用VK_KHR_MULTIVIEW扩展
- 单次渲染通道同时渲染到6个立方体面
- 着色器通过`gl_ViewIndex`区分不同面

---

## 4. **场景绘制**
**位置**: `LightProbe.cpp:23-36`

```cpp
void LightProbe::drawScene(VkCommandBuffer cmdBuf)
{
    capturePass->Draw(cmdBuf, [this](VkCommandBuffer cmd) {
        if (skybox) {
            skybox->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
        if (gltfModel) {
            gltfModel->Draw(cmd, capturePass->descriptorSet, ETechnique::CAPTURE_SCENE);
        }
    });
}
```

**绘制内容**:
- 天空盒（环境背景）
- glTF模型（场景几何体）

---

## 5. **后处理步骤**

### 步骤1: 布局转换
```cpp
// 从COLOR_ATTACHMENT_OPTIMAL转换到SHADER_READ_ONLY_OPTIMAL
VkImageMemoryBarrier barrier = {...};
barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
vkCmdPipelineBarrier(...);
```

### 步骤2: 生成SH系数
```cpp
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);
```

### 步骤3: 生成IBL贴图
```cpp
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
```

### 步骤4: 保存cubemap面
```cpp
probe->SaveCubeMapFaces(queue, basePath);
```

---

## 关键技术点

### 1. **Multiview渲染**
- 使用VK_KHR_MULTIVIEW扩展
- 单次渲染通道渲染6个视图
- 提高性能，减少CPU开销

### 2. **视图矩阵计算**
- 标准立方体贴图方向
- Up向量根据面调整
- 确保正确的纹理坐标映射

### 3. **内存同步**
- 渲染完成后等待队列空闲
- 执行布局转换
- 确保数据可被后续着色器读取

### 4. **资源管理**
- cubemap自动转换为TextureCubeMap
- 支持采样和后续处理
- 自动清理旧资源

---

## 数据流

```
CaptureCubemap()
    ↓
LightProbe::CaptureCubeMap()
    ↓
准备UBO (6个viewproj矩阵)
    ↓
CaptureScenePass::Draw()
    ↓
Multiview渲染 (6个面同时)
    ↓
布局转换 (COLOR_ATTACHMENT → SHADER_READ_ONLY)
    ↓
获取cubemap
    ↓
生成SH系数 (GenSHComputePass)
    ↓
生成IBL贴图 (GenIBLPass)
    ↓
更新MainPass绑定
    ↓
保存到文件
    ↓
添加到cubeMaps列表
```

---

## 性能特点

1. **高效的多面渲染**: Multiview一次渲染6个面
2. **高分辨率**: 1024×1024支持高质量捕获
3. **实时处理**: 支持运行时动态捕获
4. **完整的IBL流程**: 自动生成SH和预过滤贴图


