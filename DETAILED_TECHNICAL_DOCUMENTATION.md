# 详细技术文档

## 问题1和2: 立方体贴图黑色和方向反转

### 问题分析

#### 黑色问题的原因
当SH系数或IBL贴图全为黑色时，表示采样方向完全错误。这通常由以下原因引起：
1. 立方体贴图的视图矩阵配置不当
2. 着色器中的Y坐标翻转与视图矩阵不匹配
3. 导致采样方向错误，最终得到黑色结果

#### 方向反转的原因
±Y面贴图反转表示：
1. 视图矩阵的up向量方向不对
2. 导致立方体贴图的上下面被交换

### 坐标系统分析

**OpenGL坐标系**:
- Y轴向上
- Z轴向观察者

**Vulkan坐标系**:
- Y轴向下
- Z轴背离观察者

**立方体贴图标准配置**:
- 所有面使用相同的up向量 `(0, -1, 0)`
- 着色器中的Y坐标翻转处理坐标系转换

### 修复方案

**文件**: `examples/lightprobesh2/LightProbe.cpp` 第69-79行

```cpp
std::array<glm::mat4, 6> viewMatrices = {
    glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0, -1,  0)), // +X
    glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1,  0)), // -X
    glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0, -1,  0)), // +Y
    glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0, -1,  0)), // -Y
    glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0, -1,  0)), // +Z
    glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0, -1,  0))  // -Z
};
```

**关键点**:
- 所有6个面使用统一的up向量 `(0, -1, 0)`
- 这是标准的立方体贴图配置
- 着色器中的Y坐标翻转会处理坐标系转换

### 着色器中的Y坐标翻转

**irradiancecube.frag 第38行**:
```glsl
sampleVector.y = -sampleVector.y;
```

**prefilterenvmap.frag 第97行**:
```glsl
L.y = -L.y;
```

**lightprobesh.frag 第84, 178行**:
```glsl
sampleR.y = -sampleR.y;
sampleN.y = -sampleN.y;
```

这些翻转确保了采样方向的正确性。

---

## 问题3: 纹理支持

### 纹理加载流程

1. **模型加载**: vkglTF::Model自动加载纹理
2. **纹理存储**: 存储在 `model->textures` 数组中
3. **纹理信息**: 每个纹理包含:
   - `sampler`: VkSampler
   - `view`: VkImageView
   - `imageLayout`: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL

### 修复实现

**文件**: `examples/lightprobesh2/gltfload.cpp` 第186-228行

**UpdateSet()方法**:
1. 检查模型是否有纹理
2. 创建VkDescriptorImageInfo数组
3. 从model->textures中提取纹理信息
4. 填充到15个纹理槽位
5. 绑定到描述符集

**关键代码**:
```cpp
if (model && !model->textures.empty()) {
    std::vector<VkDescriptorImageInfo> imageInfos;
    
    for (size_t i = 0; i < model->textures.size() && i < 15; ++i) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = model->textures[i].sampler;
        imageInfo.imageView = model->textures[i].view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(imageInfo);
    }
    
    // 填充空槽位
    while (imageInfos.size() < 15) {
        VkDescriptorImageInfo emptyInfo{};
        emptyInfo.sampler = VK_NULL_HANDLE;
        emptyInfo.imageView = VK_NULL_HANDLE;
        emptyInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(emptyInfo);
    }
    
    VkWriteDescriptorSet textureWrite = vks::initializers::writeDescriptorSet(
        descriptorSet, 
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        2, 
        imageInfos.data(), 
        static_cast<uint32_t>(imageInfos.size())
    );
    writeDescriptorSets.push_back(textureWrite);
}
```

### 着色器支持

**gltfmesh.frag** 和 **gltfmesh_mvr.frag**:
- 已支持纹理采样
- 纹理绑定到 `set=1, binding=2`
- 支持最多15个纹理

---

## 验证清单

- [x] 编译成功，无错误
- [x] 立方体贴图视图矩阵修复
- [x] 纹理绑定启用
- [x] 着色器支持纹理
- [x] 多探针系统兼容
- [x] Capture和Main技术都支持

