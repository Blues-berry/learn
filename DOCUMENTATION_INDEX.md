# GltfModel 在 CapturePass 中的实现 - 文档索引

## 📚 文档总览

本文档集合提供了关于如何在 CapturePass 中绘制 GltfModel 并捕获光照信息的完整指南。

---

## 🚀 快速开始

### 新手入门
**推荐阅读顺序**: 
1. **GLTFMODEL_CAPTURE_QUICK_START.md** ⭐ 从这里开始
   - 4 个核心修改位置
   - 执行流程
   - 常见问题

2. **CAPTURE_GLTFMODEL_IMPLEMENTATION_GUIDE.md**
   - 详细的实现步骤
   - 数据流向说明
   - 验证清单

---

## 📖 详细分析文档

### 1. 绘制逻辑分析
**文件**: `GLTFMODEL_DRAWING_ANALYSIS.md`
- 核心绘制流程
- 描述符集布局设计
- 着色器数据流
- Push Constant 机制
- 技术类型处理
- 资源准备流程

**适用场景**: 理解 GltfModel 的绘制原理

---

### 2. 代码注释分析
**文件**: `GLTFMODEL_CODE_ANALYSIS.md`
- Draw() 函数完整分析
- 关键概念解释
- 描述符集的两层架构
- Push Constant vs UBO
- 矩阵变换顺序
- 性能分析

**适用场景**: 深入理解代码实现

---

### 3. 着色器数据绑定
**文件**: `SHADER_BINDING_ANALYSIS.md`
- 描述符集布局定义
- Set 0 和 Set 1 的详细说明
- 顶点着色器数据流
- 片段着色器数据流
- 数据绑定流程
- 数据更新流程
- 绑定点总结表

**适用场景**: 理解着色器资源绑定

---

### 4. MainPass vs CapturePass 对比
**文件**: `MAINPASS_VS_CAPTUREPASS_COMPARISON.md`
- 整体架构对比
- GltfModel 在两个 Pass 中的行为
- 详细参数对比
- 数据流对比
- 关键连接点
- 性能对比
- 调试技巧

**适用场景**: 理解两个 Pass 的区别

---

### 5. 具体修改方案
**文件**: `GLTFMODEL_CAPTURE_MODIFICATIONS.md`
- 修改 1: gltfload.cpp - Draw() 函数
- 修改 2: main.cpp - CaptureCubemap() 函数
- 修改 3: LightProbe.cpp - drawScene() 函数
- 修改 4: main.cpp - drawFrame() 函数
- 测试步骤
- 数据对比
- 注意事项

**适用场景**: 实施具体代码修改

---

### 6. 实现总结
**文件**: `CAPTURE_IMPLEMENTATION_SUMMARY.md`
- 目标说明
- 已完成的工作
- 核心修改方案
- 执行流程
- 验证步骤
- 性能考虑
- 常见问题
- 相关文件

**适用场景**: 整体了解实现方案

---

## 🎨 可视化流程图

### 1. GltfModel 绘制流程图
- 安全检查
- 描述符集绑定
- 管线绑定
- 多实例绘制循环
- 技术类型判断

### 2. GltfModel 数据流向图
- CPU 端数据
- GPU 管线
- 着色器资源
- 顶点处理
- 片段处理

### 3. GltfModel 捕获流程完整图
- 用户交互
- LightProbe 创建
- PSO 准备
- 场景绘制
- Multiview 处理
- SH/IBL 生成
- 光照应用

---

## 🔍 按用途查找

### 我想...

#### 快速了解如何实现
→ **GLTFMODEL_CAPTURE_QUICK_START.md**

#### 理解绘制原理
→ **GLTFMODEL_DRAWING_ANALYSIS.md**

#### 理解代码细节
→ **GLTFMODEL_CODE_ANALYSIS.md**

#### 理解着色器绑定
→ **SHADER_BINDING_ANALYSIS.md**

#### 对比两个 Pass
→ **MAINPASS_VS_CAPTUREPASS_COMPARISON.md**

#### 获取具体修改代码
→ **GLTFMODEL_CAPTURE_MODIFICATIONS.md**

#### 了解完整实现方案
→ **CAPTURE_IMPLEMENTATION_SUMMARY.md**

#### 查看流程图
→ 查看 Mermaid 图表

---

## 📋 核心概念速查

### MAIN 模式
- **用途**: 主渲染，显示在屏幕上
- **实例**: 4 个
- **位置**: (-20,0,0), (20,0,0), (0,0,-20), (0,0,20)
- **颜色**: 红、绿、蓝、红
- **频率**: 每帧
- **文件**: gltfload.cpp - Draw() 函数

### CAPTURE_SCENE 模式
- **用途**: 捕获光照信息
- **实例**: 1 个
- **位置**: (0,0,0)
- **颜色**: 白色
- **频率**: 按需
- **文件**: gltfload.cpp - Draw() 函数

---

## 🔧 修改位置速查

| 修改 | 文件 | 函数 | 行号 |
|------|------|------|------|
| 1 | gltfload.cpp | Draw() | 49-103 |
| 2 | main.cpp | CaptureCubemap() | 566-637 |
| 3 | main.cpp | drawFrame() | 486-511 |
| 4 | LightProbe.cpp | drawScene() | 23-36 |

---

## 🧪 测试清单

- [ ] 编译成功
- [ ] 加载 glTF 模型
- [ ] 点击 "Capture Cubemap" 按钮
- [ ] 立方体贴图被捕获
- [ ] 生成 SH 系数
- [ ] 生成 IBL 贴图
- [ ] 模型显示新的光照效果
- [ ] 检查 Captured_*.ppm 文件

---

## 🐛 故障排除

### 问题: 黑色立方体贴图
**查看**: GLTFMODEL_CAPTURE_MODIFICATIONS.md - 常见问题

### 问题: 模型不显示
**查看**: GLTFMODEL_CAPTURE_QUICK_START.md - 常见问题

### 问题: 光照没有更新
**查看**: CAPTURE_IMPLEMENTATION_SUMMARY.md - 常见问题

### 问题: 性能下降
**查看**: MAINPASS_VS_CAPTUREPASS_COMPARISON.md - 性能对比

---

## 📞 文档使用建议

### 对于初学者
1. 从 **GLTFMODEL_CAPTURE_QUICK_START.md** 开始
2. 查看 4 个核心修改位置
3. 参考 **CAPTURE_GLTFMODEL_IMPLEMENTATION_GUIDE.md** 进行实施
4. 使用 **GLTFMODEL_CAPTURE_MODIFICATIONS.md** 验证代码

### 对于有经验的开发者
1. 快速浏览 **GLTFMODEL_CAPTURE_QUICK_START.md**
2. 查看 **MAINPASS_VS_CAPTUREPASS_COMPARISON.md** 理解架构
3. 参考 **GLTFMODEL_CAPTURE_MODIFICATIONS.md** 进行修改
4. 使用流程图进行验证

### 对于调试
1. 查看 **MAINPASS_VS_CAPTUREPASS_COMPARISON.md** - 调试技巧
2. 参考 **GLTFMODEL_CAPTURE_QUICK_START.md** - 常见问题
3. 使用 **CAPTURE_IMPLEMENTATION_SUMMARY.md** - 故障排除

---

## 📊 文档统计

| 文档 | 行数 | 主题 |
|------|------|------|
| GLTFMODEL_DRAWING_ANALYSIS.md | ~270 | 绘制逻辑 |
| GLTFMODEL_CODE_ANALYSIS.md | ~300 | 代码分析 |
| SHADER_BINDING_ANALYSIS.md | ~300 | 着色器绑定 |
| CAPTURE_GLTFMODEL_IMPLEMENTATION_GUIDE.md | ~300 | 实现指南 |
| GLTFMODEL_CAPTURE_MODIFICATIONS.md | ~300 | 修改方案 |
| MAINPASS_VS_CAPTUREPASS_COMPARISON.md | ~300 | 对比分析 |
| CAPTURE_IMPLEMENTATION_SUMMARY.md | ~300 | 实现总结 |
| GLTFMODEL_CAPTURE_QUICK_START.md | ~250 | 快速开始 |

---

## 🎯 总体目标

通过这些文档，你将能够：
1. ✅ 理解 GltfModel 的绘制原理
2. ✅ 理解 MainPass 和 CapturePass 的区别
3. ✅ 实施 4 个核心修改
4. ✅ 在 CapturePass 中绘制 GltfModel
5. ✅ 捕获包含光照信息的立方体贴图
6. ✅ 生成 SH 系数和 IBL 贴图
7. ✅ 在主视图中应用新的光照效果

---

## 📝 版本信息

- **创建日期**: 2025-10-24
- **文档版本**: 1.0
- **适用项目**: lightprobesh2
- **相关技术**: Vulkan, Multiview, PBR, IBL


