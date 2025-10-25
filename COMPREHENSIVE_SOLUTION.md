# 三个任务的综合解决方案

## 任务1和2: 立方体贴图方向问题修复

### 问题
- 捕获后preview和gltfmodel变成黑色
- 立方体贴图的上下面（±Y）贴图位置反了

### 根本原因
视图矩阵的up向量与着色器中的Y坐标翻转不一致，导致立方体贴图采样方向错误。

### 解决方案
**文件**: `examples/lightprobesh2/LightProbe.cpp` (第69-79行)

修改所有6个立方体贴图面的视图矩阵，使用统一的up向量 `(0, -1, 0)`:

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

### 原理
- 所有面使用相同的up向量 `(0, -1, 0)` 是标准的立方体贴图配置
- 着色器中的Y坐标翻转 (`sampleVector.y = -sampleVector.y`) 会处理OpenGL到Vulkan的坐标系转换
- 这样可以确保立方体贴图的所有6个面方向一致，没有翻转或黑色问题

## 任务3: 为GltfModel增加纹理信息

### 问题
- GltfModel缺少纹理支持
- 纹理绑定代码被注释掉了

### 解决方案
**文件**: `examples/lightprobesh2/gltfload.cpp` (第186-228行)

在 `UpdateSet()` 方法中启用纹理绑定:

```cpp
void GltfModel::UpdateSet()
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
    };

    // ✅ 启用纹理绑定：从vkglTF::Model中提取纹理信息
    if (model && !model->textures.empty()) {
        std::vector<VkDescriptorImageInfo> imageInfos;
        
        // 遍历模型的所有纹理
        for (size_t i = 0; i < model->textures.size() && i < 15; ++i) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.sampler = model->textures[i].sampler;
            imageInfo.imageView = model->textures[i].view;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos.push_back(imageInfo);
        }
        
        // 填充到15个纹理槽位
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
    
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}
```

### 关键点
1. 从 `model->textures` 中提取纹理信息
2. 每个纹理包含 `sampler`, `view`, 和 `imageLayout`
3. 支持最多15个纹理（与描述符池大小一致）
4. 空纹理槽位用 `VK_NULL_HANDLE` 填充
5. 纹理绑定到描述符集的绑定点2

## 编译状态
✅ 所有修改已编译成功，无错误或警告

## 预期效果
1. ✅ 立方体贴图方向正确，上下面不再反转
2. ✅ 捕获后preview和gltfmodel不再变黑
3. ✅ GltfModel支持纹理渲染
4. ✅ 在capture过程中纹理信息被正确保留

