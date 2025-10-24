# Cubemap捕获 - 代码细节解析

## 1. 视图矩阵的6个方向

### 标准立方体贴图方向
```cpp
// +X 面：看向右边
glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0, -1,  0))
// 目标点: position + (1,0,0)  →  看向+X方向
// Up向量: (0,-1,0)            →  Y轴向下

// -X 面：看向左边
glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1,  0))
// 目标点: position + (-1,0,0) →  看向-X方向
// Up向量: (0,-1,0)            →  Y轴向下

// +Y 面：看向上方
glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0,  0,  1))
// 目标点: position + (0,1,0)  →  看向+Y方向
// Up向量: (0,0,1)             →  Z轴向上（特殊！）

// -Y 面：看向下方
glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0,  0, -1))
// 目标点: position + (0,-1,0) →  看向-Y方向
// Up向量: (0,0,-1)            →  Z轴向下（特殊！）

// +Z 面：看向前方
glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0, -1,  0))
// 目标点: position + (0,0,1)  →  看向+Z方向
// Up向量: (0,-1,0)            →  Y轴向下

// -Z 面：看向后方
glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0, -1,  0))
// 目标点: position + (0,0,-1) →  看向-Z方向
// Up向量: (0,-1,0)            →  Y轴向下
```

### 为什么+Y和-Y面的Up向量不同？
- **+Y面**: 看向上方，需要用Z轴作为"右"方向，所以Up是Z
- **-Y面**: 看向下方，需要用-Z轴作为"右"方向，所以Up是-Z
- **其他面**: 都用-Y作Up，保持一致性

---

## 2. 投影矩阵配置

```cpp
glm::mat4 projection = glm::perspective(
    glm::radians(90.0f),  // 视角：90度（立方体贴图标准）
    1.0f,                 // 宽高比：1.0（正方形）
    0.1f,                 // 近裁剪面
    256.0f                // 远裁剪面
);
```

**为什么是90度？**
- 立方体贴图的每个面覆盖90°的视角
- 6个面组成360°的完整环境

**为什么宽高比是1.0？**
- 立方体贴图的每个面都是正方形
- 1024×1024的分辨率

---

## 3. UBO数据结构

```cpp
struct GlobalUbo {
    glm::mat4 viewproj[6];      // 6个面的视图投影矩阵
    glm::vec4 cameraPos[6];     // 6个面的相机位置
    glm::vec4 mainLight;        // 光源信息
    float exposure = 4.5f;      // 曝光值
    float gamma = 2.2f;         // 伽马值
};
```

**内存布局**:
- `viewproj[6]`: 6 × 64字节 = 384字节
- `cameraPos[6]`: 6 × 16字节 = 96字节
- `mainLight`: 16字节
- `exposure`: 4字节
- `gamma`: 4字节
- **总计**: 504字节

---

## 4. Multiview渲染机制

### 着色器中的使用
```glsl
// 顶点着色器
layout(location = 0) out VS_OUT {
    vec3 normal;
    vec3 fragPos;
} vs_out;

void main() {
    // gl_ViewIndex 自动由Multiview扩展提供
    // 值范围: 0-5（对应6个立方体面）
    
    mat4 viewproj = viewproj[gl_ViewIndex];
    gl_Position = viewproj * vec4(position, 1.0);
}
```

### 渲染通道配置
```cpp
// 在RenderPass中启用Multiview
VkRenderPassMultiviewCreateInfoKHR multiviewInfo{};
multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO_KHR;
multiviewInfo.subpassCount = 1;
multiviewInfo.pViewMasks = &viewMask;           // 0x3F = 0b111111 (6个视图)
multiviewInfo.pCorrelationMasks = &correlationMask;
```

---

## 5. 布局转换详解

```cpp
VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.image = cubemap->image;
barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
barrier.subresourceRange.baseMipLevel = 0;
barrier.subresourceRange.levelCount = 1;
barrier.subresourceRange.baseArrayLayer = 0;
barrier.subresourceRange.layerCount = 6;  // 所有6个面
barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

vkCmdPipelineBarrier(
    transitionCmd,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &barrier);
```

**关键点**:
- `layerCount = 6`: 转换所有6个立方体面
- `srcAccessMask`: 写入完成
- `dstAccessMask`: 准备读取
- 管道阶段: 从COLOR_ATTACHMENT_OUTPUT到FRAGMENT/COMPUTE_SHADER

---

## 6. 保存cubemap面

```cpp
void LightProbe::SaveCubeMapFaces(VkQueue queue, const std::string& basePath)
{
    // 对每个面执行以下操作：
    for (uint32_t face = 0; face < 6; ++face) {
        // 1. 创建线性图像（用于CPU读取）
        VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
        imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;  // 转换为8位
        imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;    // 线性排列
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        // 2. 从cubemap复制到线性图像
        VkImageCopy imageCopyRegion = {};
        imageCopyRegion.srcSubresource.baseArrayLayer = face;  // 指定面
        imageCopyRegion.srcSubresource.layerCount = 1;
        vkCmdCopyImage(...);
        
        // 3. 映射内存并读取数据
        void* data;
        vkMapMemory(device->logicalDevice, dstImageMemory, 0, VK_WHOLE_SIZE, 0, &data);
        
        // 4. 保存为PPM格式
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        file << "P6\n" << width << "\n" << height << "\n" << 255 << "\n";
        
        // 逐行写入RGB数据
        for (uint32_t y = 0; y < height; ++y) {
            const uint8_t* row = (const uint8_t*)(imageData + y * rowPitch);
            for (uint32_t x = 0; x < width; ++x) {
                file.write((const char*)row, 3);  // 写入RGB
                row += 4;  // 跳过Alpha
            }
        }
    }
}
```

---

## 7. SH系数生成

```cpp
// 在CaptureCubemap中
shGenPass->SetCubeMap(capturedCubemap);
shGenPass->Generate(queue);

VkDescriptorBufferInfo shBufferInfo;
shGenPass->FeedSH(shBufferInfo);
mainPass->environmemts.shCoeffs = shBufferInfo;
```

**SH系数用途**:
- 存储环境光的低频信息
- 用于快速的漫反射光照计算
- 9个vec4系数（3阶球谐）

---

## 8. IBL贴图生成

```cpp
genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);
```

**生成的贴图**:
- **Irradiance Map**: 漫反射环境光
- **Prefiltered Map**: 镜面反射环境光（多个粗糙度级别）


