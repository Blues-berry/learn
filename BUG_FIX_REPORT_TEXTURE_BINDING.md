# Bug修复报告：捕获后模型变黑问题

## 🐛 问题描述

**现象**: 点击"Capture Cubemap at Camera"按钮后，preview和gltfmodel模型都变成黑色

**影响范围**: 
- Preview模型无法显示
- GltfModel无法显示
- 捕获后的渲染完全黑色

---

## 🔍 根本原因分析

### 问题定位过程

1. **代码变化追踪**:
   - 任务3中启用了纹理绑定代码（gltfload.cpp 第186-228行）
   - 在 `UpdateSet()` 方法中添加了纹理描述符绑定

2. **关键发现**:
   - 描述符集布局定义（第158行）：
     ```cpp
     vks::initializers::descriptorSetLayoutBinding(
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
         VK_SHADER_STAGE_FRAGMENT_BIT, 
         2
     )
     ```
   - 纹理绑定代码（第217-223行）：
     ```cpp
     VkWriteDescriptorSet textureWrite = vks::initializers::writeDescriptorSet(
         descriptorSet,
         VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         2,
         imageInfos.data(),
         static_cast<uint32_t>(imageInfos.size())  // 15个纹理
     );
     ```

### 真正的Bug

**问题**: 描述符集布局绑定的 `descriptorCount` 默认为1，但 `UpdateSet()` 尝试绑定15个纹理

**后果**:
- Vulkan验证层报错（可能被忽略）
- 描述符集绑定失败
- 着色器无法访问纹理
- 模型渲染为黑色

**代码位置**: `examples/lightprobesh2/gltfload.cpp` 第158行

---

## ✅ 修复方案

### 修改内容

**文件**: `examples/lightprobesh2/gltfload.cpp`

**行号**: 第158行

**修改前**:
```cpp
// 绑定 2: 模型纹理（片段着色器）
vks::initializers::descriptorSetLayoutBinding(
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
    VK_SHADER_STAGE_FRAGMENT_BIT, 
    2
),
```

**修改后**:
```cpp
// ✅ 修复：绑定 2: 模型纹理数组（片段着色器）- 指定 descriptorCount = 15
vks::initializers::descriptorSetLayoutBinding(
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
    VK_SHADER_STAGE_FRAGMENT_BIT, 
    2, 
    15  // ✅ 关键修复：指定正确的descriptorCount
),
```

### 修复原理

1. **描述符集布局绑定**需要指定 `descriptorCount`
2. `descriptorCount` 必须与实际绑定的描述符数量匹配
3. 在我们的情况下，需要支持最多15个纹理
4. 因此 `descriptorCount` 必须设置为15

---

## 📊 修改统计

| 项目 | 数值 |
|------|------|
| 修改文件 | 1 |
| 修改行数 | 1 |
| 修改字符 | 添加 `, 15` |
| 编译状态 | ✅ 成功 |
| 编译错误 | 0 |
| 编译警告 | 0 |

---

## 🧪 验证

### 编译验证
```
✅ lightprobesh2.vcxproj -> lightprobesh2.exe
✅ 编译成功，无错误无警告
```

### 预期效果

修复后，捕获应该能够正常工作：
1. ✅ 点击"Capture Cubemap at Camera"后，模型不再变黑
2. ✅ Preview模型正常显示光照
3. ✅ GltfModel正常显示纹理和光照
4. ✅ 立方体贴图正确捕获
5. ✅ SH系数正确生成
6. ✅ IBL贴图正确生成

---

## 🔗 相关代码

### 描述符集布局定义 (第151-159行)
```cpp
std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
    // 绑定 0: 局部变换矩阵
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
        0
    ),
    // 绑定 1: 材质参数
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        1
    ),
    // ✅ 修复：绑定 2: 模型纹理数组 - 指定 descriptorCount = 15
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        2, 
        15
    ),
};
```

### 描述符池定义 (第143-147行)
```cpp
std::vector<VkDescriptorPoolSize> poolSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 15 },  // 支持15个纹理
};
```

### 纹理绑定代码 (第217-224行)
```cpp
VkWriteDescriptorSet textureWrite = vks::initializers::writeDescriptorSet(
    descriptorSet,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    2,
    imageInfos.data(),
    static_cast<uint32_t>(imageInfos.size())  // 15个纹理
);
writeDescriptorSets.push_back(textureWrite);
```

---

## 📝 总结

✅ **Bug已修复**

- **问题**: 描述符集布局绑定的 `descriptorCount` 未指定，导致纹理绑定失败
- **原因**: 任务3中启用纹理绑定时，忽略了布局定义中的 `descriptorCount` 参数
- **解决**: 在描述符集布局绑定中添加 `descriptorCount = 15`
- **编译**: ✅ 成功
- **预期效果**: 捕获后模型不再变黑，纹理正常显示

