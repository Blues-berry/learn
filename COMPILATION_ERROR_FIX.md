# ✅ 编译错误修复

## 🐛 编译错误

```
error C2369: "offsets": 重定义；不同的下标
  C:\Users\Bluesky\Desktop\graphic\learn\examples\lightprobesh2\gltfload.cpp(69,18):
  参见"offsets"的声明
```

---

## 🔍 问题原因

在 `gltfload.cpp` 中，`offsets` 数组被定义了两次：

1. **第69行**: `const glm::vec3 offsets[4]` - 用于模型位置偏移
2. **第83行**: `VkDeviceSize offsets[1]` - 用于顶点缓冲偏移

这导致变量名冲突。

---

## ✅ 修复方案

### 修改文件: `examples/lightprobesh2/gltfload.cpp`

**改动**: 将第83行的 `offsets` 重命名为 `vertexOffsets`

```cpp
// ✅ 修复: 绑定顶点和索引缓冲
VkDeviceSize vertexOffsets[1] = { 0 };  // ✅ 改名为 vertexOffsets
vkCmdBindVertexBuffers(cmd, 0, 1, &model->vertices.buffer, vertexOffsets);
vkCmdBindIndexBuffer(cmd, model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
```

---

## 📊 修改统计

| 文件 | 改动 | 行数 | 状态 |
|------|------|------|------|
| gltfload.cpp | 重命名offsets为vertexOffsets | 83 | ✅ |

---

## 🧪 编译步骤

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

---

## 🎉 总结

**问题**: 变量名冲突 - `offsets` 被定义了两次

**解决**: 将顶点缓冲偏移数组重命名为 `vertexOffsets`

**结果**: 编译错误已解决


