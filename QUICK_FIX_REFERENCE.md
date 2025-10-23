# 🚀 快速修复参考

## 修复1: gltfModel初始为黑色

**文件**: `examples/lightprobesh2/gltfload.h`

**改动**:
```cpp
// 第19行: roughness = 1.f → roughness = 0.5f
// 第25行: useSH = 1 → useSH = 0
```

**原因**: 太粗糙 + SH还没生成

---

## 修复2: gltfModel跟随视角移动

**文件**: `examples/lightprobesh2/gltfload.cpp`

**改动** (第82-98行):
```cpp
// MAIN技术中：
pc.modelOffset = glm::mat4(1.0f);  // 不应用push constant偏移
```

**原因**: 使用SetTransform()设置的localData.transform

---

## 修复3: 捕获的图像只有一张

**文件**: `examples/lightprobesh2/UpsampleCubeMapPass.cpp`

**改动** (第133-145行):
```cpp
// 从数组改为单个值
uint32_t viewMask = 0x3F;  // 0b111111 = 6个面
uint32_t correlationMask = 0x3F;

renderPassMultiviewCI.pViewMasks = &viewMask;  // 指向单个值
renderPassMultiviewCI.correlationMaskCount = 1;
renderPassMultiviewCI.pCorrelationMasks = &correlationMask;
```

**原因**: Multiview配置错误

---

## 编译和测试

```bash
# 编译
cmake --build build --config Release

# 运行
./build/Release/lightprobesh2.exe
```

---

## 验证清单

- [ ] gltfModel初始显示为灰白色（不是黑色）
- [ ] 移动鼠标时gltfModel保持固定位置
- [ ] 点击"Capture Cubemap"后生成6张图片
- [ ] 所有6张图片都有内容


