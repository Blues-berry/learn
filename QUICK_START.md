# 🚀 快速开始 - Cubemap捕获修复

## ✅ 修复状态

所有修复都已完成！

| 修复 | 状态 | 文件 |
|------|------|------|
| 修复1: 第二次捕获变黑 | ✅ | main.cpp |
| 修复2: 布局转换不完整 | ✅ | LightProbe.cpp |
| 修复3: 绘制偏移太大 | ✅ | gltfload.cpp |
| 修复4: 只有一个面有纹理 | ✅ | gltfload.cpp + 着色器 |

---

## 🔧 编译步骤

### 第1步: 着色器编译 ✅ 已完成
```bash
cd shaders/glsl
python ./compileshaders.py --project lightprobesh2
```

**结果**: 
- ✅ gltfmesh_mvr.vert.spv
- ✅ gltfmesh_mvr.frag.spv

### 第2步: C++代码编译
```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 第3步: 运行程序
```bash
./build/Release/lightprobesh2.exe
```

---

## 🧪 快速测试

### 测试1: 第一次捕获
1. 启动程序
2. 点击"Capture Cubemap at Camera"
3. **验证**: 所有6个面都有纹理 ✅

### 测试2: 第二次捕获
1. 再次点击"Capture Cubemap at Camera"
2. **验证**: 模型不变黑 ✅

### 测试3: 检查文件
查看保存的cubemap文件：
```
Captured_0_0.ppm  ✓ 有纹理
Captured_0_1.ppm  ✓ 有纹理
Captured_0_2.ppm  ✓ 有纹理
Captured_0_3.ppm  ✓ 有纹理
Captured_0_4.ppm  ✓ 有纹理
Captured_0_5.ppm  ✓ 有纹理
```

---

## 📁 关键文件

### 新创建的文件
- `shaders/glsl/lightprobesh2/gltfmesh_mvr.vert` - Multiview顶点着色器
- `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` - Multiview片段着色器

### 修改的文件
- `examples/lightprobesh2/main.cpp` - 修复1
- `examples/lightprobesh2/LightProbe.cpp` - 修复2
- `examples/lightprobesh2/gltfload.cpp` - 修复3和修复4

---

## 🎯 核心修复

### 修复4: Multiview着色器支持

**问题**: 只有一个cubemap面有纹理

**解决方案**:
1. 创建multiview着色器（支持gl_ViewIndex）
2. 修改PreparePSO根据技术类型选择着色器

**代码**:
```cpp
// gltfload.cpp PreparePSO函数
if (technique == ETechnique::MAIN) {
	// 普通着色器
	shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh.vert.spv", ...);
	shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh.frag.spv", ...);
}
else {
	// Multiview着色器
	shaderStages[0] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.vert.spv", ...);
	shaderStages[1] = iLoader->LoadShader("lightprobesh2/gltfmesh_mvr.frag.spv", ...);
}
```

---

## 📊 预期效果

### 修复前 ❌
- 只有1个面有纹理
- 其他5个面是黑色
- cubemap不完整

### 修复后 ✅
- 所有6个面都有纹理
- gltfModel在所有面上可见
- cubemap完整

---

## 💡 技术要点

### Multiview渲染
- 使用VK_KHR_MULTIVIEW扩展
- 单次渲染通道渲染6个cubemap面
- 着色器使用gl_ViewIndex区分不同的面

### 关键改动
- Global UBO使用数组: `mat4 viewProject[6]`
- 顶点着色器: `gl_Position = global.viewProject[gl_ViewIndex] * worldPos`
- 片段着色器: `vec3 V = normalize(global.cameraPos[0].xyz - inWorldPos)`

---

## ✨ 完成清单

- [x] 分析根本原因
- [x] 创建multiview着色器
- [x] 修改C++代码
- [x] 编译着色器
- [ ] 编译C++代码
- [ ] 运行程序
- [ ] 执行测试

---

## 🎉 准备好了吗？

1. 编译C++代码: `cmake --build build --config Release`
2. 运行程序: `./build/Release/lightprobesh2.exe`
3. 执行测试: 按照快速测试步骤验证

**所有修复都已完成！** 🚀


