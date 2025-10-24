# Cubemap捕获系统 - 完整分析文档

## 📚 文档导航

本分析包含5个详细文档，从不同角度解析cubemap捕获系统：

### 1. **快速参考** 📖
**文件**: `CUBEMAP_CAPTURE_SUMMARY.md`

- ⚡ 一句话总结
- 🔄 5步核心流程
- 📋 关键概念速查
- ❓ 常见问题解答
- 💡 性能优化建议

**适合**: 快速了解系统、查找特定概念

---

### 2. **详细流程分析** 🔍
**文件**: `CUBEMAP_CAPTURE_ANALYSIS.md`

- 📍 完整流程概述
- 🎯 核心流程详解（5个主要步骤）
- 🏗️ 关键类详解
- 🔗 数据流图
- ⚙️ 关键技术点

**适合**: 深入理解系统设计、学习实现细节

---

### 3. **代码细节解析** 💻
**文件**: `CUBEMAP_CAPTURE_CODE_DETAILS.md`

- 🧮 视图矩阵的6个方向（含数学解释）
- 📐 投影矩阵配置
- 📦 UBO数据结构
- 🎨 Multiview渲染机制
- 🔄 布局转换详解
- 💾 保存cubemap面的完整流程
- 🌟 SH系数生成
- 🎭 IBL贴图生成

**适合**: 理解具体代码实现、调试问题

---

### 4. **系统架构设计** 🏛️
**文件**: `CUBEMAP_CAPTURE_ARCHITECTURE.md`

- 📊 类关系图
- 🔧 核心类详解
- 🌊 数据流图
- 🎯 关键设计决策
- 💾 内存管理
- 🚀 扩展点
- 📈 性能指标

**适合**: 理解系统架构、进行扩展开发

---

### 5. **流程图解** 📈
**文件**: `CUBEMAP_CAPTURE_FLOWCHART.md`

- 🔀 完整流程图（ASCII艺术）
- 🎯 关键决策点
- 💾 内存流向图
- 📊 Multiview vs 传统方案对比

**适合**: 可视化理解流程、演示讲解

---

### 6. **代码片段** 📝
**文件**: `CUBEMAP_CAPTURE_CODE_SNIPPETS.md`

- 🎬 入口点代码
- 🔧 核心捕获逻辑
- 🎨 场景绘制
- 📊 关键数据结构

**适合**: 快速查找代码、复制参考

---

## 🎯 快速导航

### 我想...

#### 快速了解系统
→ 阅读 `CUBEMAP_CAPTURE_SUMMARY.md`

#### 理解完整流程
→ 阅读 `CUBEMAP_CAPTURE_ANALYSIS.md` + `CUBEMAP_CAPTURE_FLOWCHART.md`

#### 学习代码实现
→ 阅读 `CUBEMAP_CAPTURE_CODE_DETAILS.md` + `CUBEMAP_CAPTURE_CODE_SNIPPETS.md`

#### 理解系统架构
→ 阅读 `CUBEMAP_CAPTURE_ARCHITECTURE.md`

#### 调试问题
→ 查看 `CUBEMAP_CAPTURE_CODE_DETAILS.md` 的调试技巧部分

#### 进行扩展开发
→ 阅读 `CUBEMAP_CAPTURE_ARCHITECTURE.md` 的扩展点部分

---

## 🔑 核心概念速查

### Multiview渲染
- **定义**: 单次渲染通道同时渲染到6个立方体面
- **优势**: 性能提升，减少CPU开销
- **实现**: VK_KHR_MULTIVIEW扩展
- **详见**: `CUBEMAP_CAPTURE_CODE_DETAILS.md` 第4节

### 6个视图矩阵
- **+X/-X**: 看向左右，Up=(0,-1,0)
- **+Y/-Y**: 看向上下，Up=(0,±0,±1) ← 特殊！
- **+Z/-Z**: 看向前后，Up=(0,-1,0)
- **详见**: `CUBEMAP_CAPTURE_CODE_DETAILS.md` 第1节

### 布局转换
- **从**: COLOR_ATTACHMENT_OPTIMAL（渲染）
- **到**: SHADER_READ_ONLY_OPTIMAL（采样）
- **为什么**: 确保GPU内存可见性
- **详见**: `CUBEMAP_CAPTURE_CODE_DETAILS.md` 第5节

### SH系数
- **用途**: 存储环境光的低频信息
- **数量**: 9个vec4（3阶球谐）
- **用处**: 快速漫反射光照计算
- **详见**: `CUBEMAP_CAPTURE_CODE_DETAILS.md` 第7节

### IBL贴图
- **Irradiance Map**: 漫反射环境光
- **Prefiltered Map**: 镜面反射环境光（多粗糙度）
- **详见**: `CUBEMAP_CAPTURE_CODE_DETAILS.md` 第8节

---

## 📊 系统概览

```
用户界面
    ↓
CaptureCubemap() [入口]
    ↓
LightProbe [探针对象]
    ├─ CaptureScenePass [渲染管理]
    │   ├─ RenderTargetCube [立方体贴图]
    │   ├─ DepthStencil [深度缓冲]
    │   └─ Multiview [6个面同时渲染]
    │
    ├─ drawScene() [绘制场景]
    │   ├─ Skybox [天空盒]
    │   └─ GltfModel [模型]
    │
    └─ 后处理
        ├─ GenSHComputePass [SH计算]
        ├─ GenIBLPass [IBL生成]
        └─ MainPass [更新渲染]
```

---

## 🎓 学习路径

### 初级（理解基本概念）
1. 阅读 `CUBEMAP_CAPTURE_SUMMARY.md`
2. 查看 `CUBEMAP_CAPTURE_FLOWCHART.md` 的流程图
3. 理解6个视图矩阵的方向

### 中级（理解实现细节）
1. 阅读 `CUBEMAP_CAPTURE_ANALYSIS.md`
2. 学习 `CUBEMAP_CAPTURE_CODE_DETAILS.md`
3. 查看 `CUBEMAP_CAPTURE_CODE_SNIPPETS.md` 的代码

### 高级（理解架构设计）
1. 研究 `CUBEMAP_CAPTURE_ARCHITECTURE.md`
2. 分析类关系和数据流
3. 理解扩展点和性能优化

---

## 🔧 关键文件位置

| 文件 | 位置 | 功能 |
|------|------|------|
| main.cpp | 578-636 | CaptureCubemap()入口 |
| LightProbe.h | - | LightProbe类定义 |
| LightProbe.cpp | 50-141 | CaptureCubeMap()实现 |
| UpsampleCubeMapPass.h | - | CaptureScenePass定义 |
| UpsampleCubeMapPass.cpp | 218-242 | Draw()实现 |
| Pass.h | - | 其他渲染通道定义 |
| Pass.cpp | - | 其他渲染通道实现 |

---

## 💡 关键洞察

### 1. 为什么使用Multiview？
- 传统方案: 6次独立渲染通道 = 6倍CPU开销
- Multiview方案: 1次渲染通道 = 1倍CPU开销
- **性能提升**: 5-6倍

### 2. 为什么需要布局转换？
- Vulkan要求显式管理内存布局
- 渲染需要COLOR_ATTACHMENT_OPTIMAL
- 采样需要SHADER_READ_ONLY_OPTIMAL
- 必须显式转换

### 3. 为什么+Y/-Y的Up向量特殊？
- 立方体贴图的标准约定
- 确保纹理坐标正确映射
- 其他引擎也采用相同方式

### 4. 为什么分离CaptureScenePass？
- 复用性: 多个LightProbe共享
- 灵活性: 支持不同分辨率和格式
- 清晰性: 渲染逻辑独立

---

## 📈 性能指标

| 操作 | 时间 | 备注 |
|------|------|------|
| 创建LightProbe | ~1ms | 分配GPU内存 |
| 捕获1024×1024 | ~5-10ms | 取决于场景复杂度 |
| 生成SH系数 | ~2-3ms | 计算着色器 |
| 生成IBL贴图 | ~10-20ms | 多个Mipmap级别 |
| 保存6个面 | ~50-100ms | 磁盘I/O |
| **总计** | **~70-140ms** | 完整流程 |

---

## 🚀 下一步

- 📖 选择合适的文档开始阅读
- 💻 查看代码片段理解实现
- 🔧 尝试修改参数进行实验
- 🎯 基于架构进行扩展开发

---

## 📞 文档信息

- **创建日期**: 2025-10-24
- **系统**: Vulkan光照探针系统
- **项目**: lightprobesh2示例
- **语言**: C++, GLSL


