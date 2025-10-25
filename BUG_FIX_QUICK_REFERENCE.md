# Bug修复快速参考

## 🐛 问题

**现象**: 点击"Capture Cubemap at Camera"后，preview和gltfmodel变黑

**原因**: 描述符集布局绑定的 `descriptorCount` 未指定（默认为1），但实际绑定15个纹理

---

## ✅ 修复

**文件**: `examples/lightprobesh2/gltfload.cpp`

**行号**: 第158行

**修改**:
```cpp
// 修改前
vks::initializers::descriptorSetLayoutBinding(
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
    VK_SHADER_STAGE_FRAGMENT_BIT, 
    2
);

// 修改后
vks::initializers::descriptorSetLayoutBinding(
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
    VK_SHADER_STAGE_FRAGMENT_BIT, 
    2, 
    15  // ✅ 添加descriptorCount
);
```

---

## 📊 修改统计

| 项目 | 数值 |
|------|------|
| 修改文件 | 1 |
| 修改行数 | 1 |
| 编译状态 | ✅ 成功 |

---

## 🔍 关键概念

### Vulkan描述符绑定的三层配置

1. **描述符池** (DescriptorPool)
   ```cpp
   poolSize.descriptorCount = 15;  // 池中最多15个
   ```

2. **描述符集布局** (DescriptorSetLayout)
   ```cpp
   binding.descriptorCount = 15;  // 绑定点最多15个
   ```

3. **描述符集更新** (WriteDescriptorSet)
   ```cpp
   write.descriptorCount = 15;  // 实际写入15个
   ```

**关键**: 三层必须一致！

---

## 🛡️ 预防措施

1. **明确指定参数**
   - 不要依赖默认值
   - 总是指定 `descriptorCount`

2. **保持一致性**
   - 池、布局、更新三层配置必须匹配
   - 使用常量避免不一致

3. **添加验证**
   ```cpp
   if (imageInfos.size() > MAX_TEXTURES) {
       std::cerr << "Too many textures!" << std::endl;
       return;
   }
   ```

4. **添加注释**
   ```cpp
   // 绑定 2: 模型纹理数组（最多15个）
   vks::initializers::descriptorSetLayoutBinding(
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
       VK_SHADER_STAGE_FRAGMENT_BIT, 
       2, 
       15
   );
   ```

---

## 📝 完整的正确配置

```cpp
// 1. 描述符池
std::vector<VkDescriptorPoolSize> poolSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 15 },  // ✅
};

// 2. 描述符集布局
std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
        0
    ),
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        1
    ),
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        2, 
        15  // ✅ 关键修复
    ),
};

// 3. 纹理绑定
if (model && !model->textures.empty()) {
    std::vector<VkDescriptorImageInfo> imageInfos;
    
    for (size_t i = 0; i < model->textures.size() && i < 15; ++i) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = model->textures[i].sampler;
        imageInfo.imageView = model->textures[i].view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos.push_back(imageInfo);
    }
    
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
        static_cast<uint32_t>(imageInfos.size())  // ✅ 15个纹理
    );
    writeDescriptorSets.push_back(textureWrite);
}
```

---

## ✨ 预期效果

修复后：
- ✅ 捕获后模型不再变黑
- ✅ Preview模型正常显示
- ✅ GltfModel正常显示纹理
- ✅ 立方体贴图正确捕获
- ✅ 光照和纹理信息正确应用

---

## 📚 相关文档

- BUG_FIX_REPORT_TEXTURE_BINDING.md - 详细bug报告
- DESCRIPTOR_BINDING_ANALYSIS.md - 描述符绑定分析
- WORK_COMPLETED.md - 工作完成报告

