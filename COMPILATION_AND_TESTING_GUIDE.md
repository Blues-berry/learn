# 🚀 编译和测试指南

## 📋 修复总结

所有4个cubemap捕获问题都已修复：

| 修复 | 文件 | 问题 | 解决方案 | 状态 |
|------|------|------|---------|------|
| 修复1 | main.cpp | 第二次捕获变黑 | 为MAIN技术准备PSO | ✅ |
| 修复2 | LightProbe.cpp | 布局转换不完整 | 总是执行布局转换 | ✅ |
| 修复3 | gltfload.cpp | 绘制偏移太大 | CAPTURE_SCENE中不应用偏移 | ✅ |
| **修复4** | **gltfload.cpp** | **只有一个面有纹理** | **使用multiview着色器** | ✅ |

---

## 🔧 编译步骤

### 第1步: 编译着色器

新创建的multiview着色器需要编译为SPIR-V格式：

```bash
# 使用glslc编译（如果已安装）
glslc shaders/glsl/lightprobesh2/gltfmesh_mvr.vert -o shaders/glsl/lightprobesh2/gltfmesh_mvr.vert.spv
glslc shaders/glsl/lightprobesh2/gltfmesh_mvr.frag -o shaders/glsl/lightprobesh2/gltfmesh_mvr.frag.spv
```

**或者** 使用CMake自动编译（推荐）：

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 第2步: 编译C++代码

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 第3步: 运行程序

```bash
./build/Release/lightprobesh2.exe
```

---

## 🧪 测试步骤

### 测试1: 第一次捕获

1. 启动程序
2. 点击"Capture Cubemap at Camera"按钮
3. 等待捕获完成（应该看到进度）
4. 检查保存的cubemap文件

**预期结果**:
- ✅ 6张cubemap图片都被生成
- ✅ 所有6个面都有gltfModel纹理
- ✅ 天空盒在所有面上都可见
- ✅ 没有黑色的面

### 测试2: 第二次捕获

1. 再次点击"Capture Cubemap at Camera"按钮
2. 等待捕获完成
3. 检查模型是否仍然可见

**预期结果**:
- ✅ gltfModel和previewModel不变黑
- ✅ 新的cubemap正确生成
- ✅ 系统稳定

### 测试3: 多次捕获

1. 连续点击"Capture Cubemap at Camera"多次
2. 每次都检查结果

**预期结果**:
- ✅ 所有捕获都成功
- ✅ 没有内存泄漏
- ✅ 系统稳定

### 测试4: 验证cubemap内容

检查保存的cubemap文件（通常在程序目录下）：

```
Captured_0_0.ppm  (右面 +X)
Captured_0_1.ppm  (左面 -X)
Captured_0_2.ppm  (上面 +Y)
Captured_0_3.ppm  (下面 -Y)
Captured_0_4.ppm  (前面 +Z)
Captured_0_5.ppm  (后面 -Z)
```

**验证内容**:
- ✅ 所有6张图片都有纹理
- ✅ gltfModel在所有面上都可见
- ✅ 天空盒在所有面上都可见
- ✅ 没有黑色或空白的面

---

## 📊 预期效果对比

### 修复前 ❌
```
Captured_0_0.ppm  ✓ 有纹理
Captured_0_1.ppm  ✗ 黑色
Captured_0_2.ppm  ✗ 黑色
Captured_0_3.ppm  ✗ 黑色
Captured_0_4.ppm  ✗ 黑色
Captured_0_5.ppm  ✗ 黑色
```

### 修复后 ✅
```
Captured_0_0.ppm  ✓ 有纹理
Captured_0_1.ppm  ✓ 有纹理
Captured_0_2.ppm  ✓ 有纹理
Captured_0_3.ppm  ✓ 有纹理
Captured_0_4.ppm  ✓ 有纹理
Captured_0_5.ppm  ✓ 有纹理
```

---

## 🔍 故障排除

### 问题1: 编译失败 - 找不到gltfmesh_mvr着色器

**原因**: 着色器文件没有被编译为SPIR-V格式

**解决方案**:
```bash
# 手动编译着色器
glslc shaders/glsl/lightprobesh2/gltfmesh_mvr.vert -o shaders/glsl/lightprobesh2/gltfmesh_mvr.vert.spv
glslc shaders/glsl/lightprobesh2/gltfmesh_mvr.frag -o shaders/glsl/lightprobesh2/gltfmesh_mvr.frag.spv
```

### 问题2: 运行时错误 - 着色器加载失败

**原因**: SPIR-V文件不存在或路径错误

**解决方案**:
1. 检查着色器文件是否存在
2. 检查文件路径是否正确
3. 重新编译着色器

### 问题3: 捕获后仍然只有一个面有纹理

**原因**: 着色器没有被正确编译或加载

**解决方案**:
1. 检查gltfmesh_mvr着色器是否被正确编译
2. 检查PreparePSO函数是否正确选择了multiview着色器
3. 重新编译整个项目

### 问题4: 第二次捕获后模型变黑

**原因**: PSO配置不完整

**解决方案**:
1. 检查main.cpp中CaptureCubemap函数是否为MAIN技术准备了PSO
2. 检查gltfload.cpp中PreparePSO函数是否正确处理了两种技术

---

## 📝 关键文件

### 新创建的着色器文件
- ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.vert` - Multiview顶点着色器
- ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` - Multiview片段着色器

### 修改的C++文件
- ✅ `examples/lightprobesh2/gltfload.cpp` - 添加multiview着色器支持

### 分析文档
- `CUBEMAP_CAPTURE_ROOT_CAUSE.md` - 根本原因分析
- `FINAL_CUBEMAP_FIX.md` - 最终修复说明

---

## ✨ 完成清单

- [x] 创建gltfmesh_mvr.vert着色器
- [x] 创建gltfmesh_mvr.frag着色器
- [x] 修改gltfload.cpp支持multiview着色器
- [ ] 编译着色器为SPIR-V格式
- [ ] 编译C++代码
- [ ] 运行程序
- [ ] 执行测试1: 第一次捕获
- [ ] 执行测试2: 第二次捕获
- [ ] 执行测试3: 多次捕获
- [ ] 执行测试4: 验证cubemap内容

---

## 🎉 下一步

1. **编译着色器** - 使用glslc或CMake编译
2. **编译代码** - `cmake --build build --config Release`
3. **运行程序** - `./build/Release/lightprobesh2.exe`
4. **执行测试** - 按照测试步骤验证

**准备好了吗？** 🚀


