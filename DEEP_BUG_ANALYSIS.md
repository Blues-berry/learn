# 深度Bug分析：捕获后模型变黑

## 🔴 发现的关键问题

### 问题1: MainPass::UpdateBindings() 中的描述符池大小不匹配

**位置**: `Pass.cpp` 第497-499行

```cpp
void MainPass::PreparePerPassResource()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },           // ❌ 只有2个
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },   // ❌ 只有4个
    };
```

**但在 UpdateBindings() 中绑定的是**:

```cpp
void MainPass::UpdateBindings()
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &environmemts.shCoeffs),  // ✅ 绑定1
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &environmemts.brdfView),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &environmemts.irradianceCube),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &environmemts.prefilteredCube),
    };
```

**问题**: 
- 绑定1是 `VkDescriptorBufferInfo`（UNIFORM_BUFFER）
- 但 `environmemts.shCoeffs` 初始化时可能为空
- 当 `UpdateBindings()` 被调用时，如果 `shCoeffs` 未初始化，会导致绑定失败

---

### 问题2: 捕获后 MainPass::UpdateBindings() 被调用，但 shCoeffs 可能未初始化

**流程**:
1. CaptureCubemap() 被调用
2. 生成 SH 系数
3. `mainPass->UpdateBindings()` 被调用 (第850行)
4. 但此时 `environmemts.shCoeffs` 可能未被正确初始化

**关键代码** (main.cpp 第841-850行):

```cpp
VkDescriptorBufferInfo shBufferInfo;
shGenPass->FeedSH(shBufferInfo);                    // 获取 SH 缓冲区信息
mainPass->environmemts.shCoeffs = shBufferInfo;     // 设置到 MainPass

genIBL->SetCubeMap(capturedCubemap);
genIBL->Generate(queue);
genIBL->FeedIrradianceMap(mainPass->environmemts.irradianceCube);
genIBL->FeedPrefilteredMap(mainPass->environmemts.prefilteredCube);

mainPass->UpdateBindings();  // ✅ 这里调用 UpdateBindings
```

---

### 问题3: 初始化时 environmemts 的成员未初始化

**位置**: `Pass.h` 第126-132行

```cpp
struct Evnironmemt
{
    VkDescriptorImageInfo brdfView;
    VkDescriptorImageInfo irradianceCube;
    VkDescriptorImageInfo prefilteredCube;
    VkDescriptorBufferInfo shCoeffs;  // ❌ 未初始化！
};
```

**问题**: 
- 这些成员在构造时未初始化
- 包含垃圾数据
- 当 UpdateBindings() 被调用时，可能绑定无效的描述符

---

### 问题4: 纹理绑定后 MainPass 描述符池不足

**位置**: `main.cpp` 第418-422行

```cpp
std::vector<VkDescriptorPoolSize> poolSizes = {
    vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
    vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32)
};
VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 33);
```

**但 MainPass 的池大小是**:

```cpp
std::vector<VkDescriptorPoolSize> poolSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
};
```

**问题**: 
- MainPass 的池太小
- 当启用纹理后，可能导致描述符分配失败

---

## 🎯 根本原因

**捕获后模型变黑的真正原因**:

1. 捕获时调用 `mainPass->UpdateBindings()`
2. 此时 `environmemts` 的成员可能未正确初始化
3. 绑定失败或绑定了无效的描述符
4. 着色器无法访问光照数据
5. 模型渲染为黑色

---

## ✅ 修复方案

### 修复1: 初始化 Evnironmemt 结构体

```cpp
struct Evnironmemt
{
    VkDescriptorImageInfo brdfView = {};
    VkDescriptorImageInfo irradianceCube = {};
    VkDescriptorImageInfo prefilteredCube = {};
    VkDescriptorBufferInfo shCoeffs = {};
};
```

### 修复2: 增加 MainPass 描述符池大小

```cpp
std::vector<VkDescriptorPoolSize> poolSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 },        // 增加到4
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 }, // 增加到8
};
```

### 修复3: 验证 UpdateBindings() 前的数据

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    // ... 捕获代码 ...
    
    // ✅ 验证数据有效性
    if (!mainPass->environmemts.shCoeffs.buffer) {
        std::cerr << "Error: shCoeffs buffer not initialized!" << std::endl;
        return;
    }
    
    mainPass->UpdateBindings();
}
```

---

## 📊 影响范围

- ✅ 捕获后 preview 变黑
- ✅ 捕获后 gltfmodel 变黑
- ✅ 光照信息丢失
- ✅ 纹理无法显示

---

## 总结

**根本原因**: 描述符绑定失败，导致着色器无法访问光照数据

**关键问题**:
1. Evnironmemt 结构体未初始化
2. MainPass 描述符池太小
3. UpdateBindings() 被调用时数据可能无效

**解决方案**: 初始化结构体、增加池大小、验证数据

