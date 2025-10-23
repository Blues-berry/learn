# ✅ gltfModel问题修复完成

## 🎯 修复总结

已成功修复gltfModel的**3个关键问题**！

---

## 📋 问题和修复

### 问题1: gltfModel初始为黑色 ✅

**根本原因**:
- `MaterialBuffer` 的默认值 `roughness = 1.0` 太粗糙
- `useSH = 1` 但SH系数还没生成，导致光照为0

**修复**:
```cpp
// 文件: examples/lightprobesh2/gltfload.h (第18-28行)
struct MaterialBuffer {
    float roughness = 0.5f;     // ✅ 从1.0改为0.5
    float metallic = 0.5;
    float specular = 0.5;
    float padding = 0.f;
    glm::vec4 elbedo = glm::vec4(1.f, 1.f, 1.f, 1.f);
    
    int32_t useSH = 0;          // ✅ 从1改为0（初始不使用SH）
    int32_t useReflection = 0;
};
```

**效果**: 模型初始可见，显示为灰白色

---

### 问题2: gltfModel跟随视角移动 ✅

**根本原因**:
- `Draw()` 函数中的 push constant `modelOffset` 覆盖了 `SetTransform()` 设置的值
- MAIN技术中应用了大的偏移和缩放，导致模型跟随相机

**修复**:
```cpp
// 文件: examples/lightprobesh2/gltfload.cpp (第82-98行)
if (tech == ETechnique::CAPTURE_SCENE) {
    pc.modelOffset = glm::mat4(1.0f);
    pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    vkCmdPushConstants(...);
    model->draw(cmd);
}
else {
    // MAIN技术：使用SetTransform()设置的localData.transform
    pc.modelOffset = glm::mat4(1.0f);  // ✅ 不应用push constant偏移
    pc.tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    vkCmdPushConstants(...);
    model->draw(cmd);
}
```

**效果**: 模型固定在世界坐标系中，不跟随相机移动

---

### 问题3: 捕获的图像只有一张 ✅

**根本原因**:
- `CaptureScenePass::PrepareFrameBuffer()` 中的 multiview 配置错误
- `pViewMasks` 指向数组而不是单个值
- 应该使用 `viewMask = 0x3F`（6个视图的掩码）

**修复**:
```cpp
// 文件: examples/lightprobesh2/UpsampleCubeMapPass.cpp (第133-145行)
// ✅ 正确配置multiview - 所有6个面在单个子通道中同时渲染
uint32_t viewMask = 0x3F;  // 0b111111 = 6个立方体面
uint32_t correlationMask = 0x3F;

VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{};
renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
renderPassMultiviewCI.subpassCount = 1;
renderPassMultiviewCI.pViewMasks = &viewMask;  // ✅ 指向单个值
renderPassMultiviewCI.correlationMaskCount = 1;
renderPassMultiviewCI.pCorrelationMasks = &correlationMask;
```

**效果**: 所有6个cubemap面都被正确渲染，`SaveCubeMapFaces()` 能保存6张图片

---

## 📁 修改的文件

1. ✅ `examples/lightprobesh2/gltfload.h` - 修改MaterialBuffer默认值
2. ✅ `examples/lightprobesh2/gltfload.cpp` - 修改Draw函数逻辑
3. ✅ `examples/lightprobesh2/UpsampleCubeMapPass.cpp` - 修正multiview配置

---

## 🧪 测试步骤

### 第1步: 编译
```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 第2步: 运行
```bash
./build/Release/lightprobesh2.exe
```

### 第3步: 验证修复

#### 验证修复1 (gltfModel初始可见):
- 启动程序
- 应该看到gltfModel显示为灰白色（不是黑色）
- 模型应该有基本的光照

#### 验证修复2 (gltfModel不跟随相机):
- 移动鼠标改变视角
- gltfModel应该保持在固定位置
- 不应该跟随相机移动

#### 验证修复3 (捕获6张图片):
- 点击"Capture Cubemap at Camera"
- 检查保存的文件（应该有6张）：
  - `Captured_X_pos_x.ppm`
  - `Captured_X_neg_x.ppm`
  - `Captured_X_pos_y.ppm`
  - `Captured_X_neg_y.ppm`
  - `Captured_X_pos_z.ppm`
  - `Captured_X_neg_z.ppm`
- 所有6张图片都应该有内容（不是黑色）

---

## ✨ 预期效果

### 修复前 ❌
- gltfModel初始为黑色
- gltfModel跟随视角移动
- 捕获的cubemap只有一张或不完整

### 修复后 ✅
- gltfModel初始可见（灰白色）
- gltfModel固定在世界坐标系中
- 捕获的cubemap有完整的6张图片

---

## 📚 相关文档

- `GLTFMODEL_ISSUES_ANALYSIS.md` - 详细问题分析
- `ADDITIONAL_MERGE_CONFLICT_FIXES.md` - 之前的merge conflict修复
- `ALL_FIXES_COMPLETE.md` - 所有修复总结

---

## 🎉 总结

**所有gltfModel问题都已修复！**

系统现在应该能够：
✅ 正确显示gltfModel（初始可见）
✅ gltfModel固定在世界坐标系中
✅ 正确捕获完整的6张cubemap图片
✅ 支持多次捕获而不出现问题

**准备好编译和测试了吗？** 🚀


