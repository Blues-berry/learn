# 基于PRT的Cornell Box Relighting项目路线图

## 📋 项目概述

**最终目标：** 实现使用Cornell Box场景的实时Relighting系统

**核心技术：**
- 预旋转光照传输 (Precomputed Radiance Transfer, PRT)
- 球谐函数 (Spherical Harmonics, SH)
- GPU并行计算 (Vulkan Compute Shader)
- 实时光照旋转交互

**项目结构：**
```
项目目录: C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\examples\lightprobesh2
着色器目录: C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2
```

---

## 🎯 项目分阶段目标

### 第一阶段：GPU端PRT预计算实现 (10个任务)

**目标：** 将CPU端PRT预计算转移到GPU端，实现100倍性能提升

**核心任务：**

#### 1.1 分析Cornell Box和PreviewModel的完整架构
- **目标：** 深入理解现有代码结构
- **关键文件：**
  - `main.cpp` - 主程序入口
  - `PreviewModel.h/cpp` - 模型渲染类
  - `GltfScene.h/cpp` - 场景管理
  - `Pass.h/cpp` - 渲染通道
  - 着色器：`gltfmesh_main.vert/frag` 等
- **交付物：** 架构分析文档

#### 1.2 完成PRTComputeShader的GPU Pipeline实现
- **目标：** 实现完整的Vulkan Compute Pipeline
- **需要实现的方法：**
  ```cpp
  bool LoadComputeShader();              // 加载SPIR-V着色器
  bool CreateDescriptorSetLayout();      // 创建描述符集布局
  bool CreateDescriptorPool();           // 创建描述符池
  bool CreateComputePipeline();          // 创建计算管道
  bool CreateBuffers();                  // 创建GPU缓冲区
  bool CreateDescriptorSet();            // 创建描述符集
  bool UpdateDescriptorSet(...);         // 更新描述符集
  bool ExecuteComputeShader(...);        // 执行计算着色器
  ```
- **关键参数：**
  - Descriptor Set Layout: 5个binding (SSBO x4, UBO x1)
  - Buffer大小计算和对齐
  - 工作组大小优化
- **交付物：** 完整的PRTComputeShader.cpp实现

#### 1.3 实现光照投影计算的GPU版本
- **目标：** 将环境光照投影到球谐空间
- **方法：** `ComputeLightingProjection()`
- **流程：**
  1. 采样方向和辐射度上传到GPU
  2. 执行 `sh_compute.comp` 着色器
  3. 计算光照的9个球谐系数
  4. 下载结果到CPU
- **输入：** 采样方向向量、辐射度值
- **输出：** SHCoefficients (9个vec3)
- **交付物：** 功能实现 + 单元测试

#### 1.4 实现Light Transport计算的GPU版本（逐顶点）
- **目标：** 计算Cornell Box每个顶点对入射光的响应
- **方法：** `ComputeLightTransportSingle()` 和 `ComputeLightTransportBatch()`
- **流程：**
  1. 顶点数据上传到GPU (位置、法向量、反射率)
  2. 采样方向上传到GPU
  3. 执行计算着色器，逐顶点计算
  4. 下载每个顶点的Light Transport系数
- **关键：** 使用GPU并行处理多个顶点
- **输入：** 顶点位置、法向量、反射率、采样方向
- **输出：** 每个顶点的SHCoefficients
- **交付物：** 功能实现 + 批量处理优化

#### 1.5 实现球谐旋转计算的GPU版本
- **目标：** 预计算不同旋转角度下的光照系数
- **方法：** `ComputeRotatedSHCoefficients()` 和 `ComputeMultipleRotations()`
- **流程：**
  1. 输入原始光照系数
  2. 指定旋转角度
  3. 执行旋转计算着色器
  4. 输出旋转后的系数
- **支持：** 批量计算多个旋转角度 (如24个45度旋转)
- **交付物：** 功能实现 + 批量处理

#### 1.6 创建GPU端数据上传和下载机制
- **目标：** 实现高效的CPU-GPU数据传输
- **需要处理：**
  - 采样方向数据上传
  - 顶点数据上传 (位置、法向量、反射率)
  - 计算结果下载
  - 内存对齐 (16字节对齐)
  - 同步问题 (等待GPU完成)
- **使用的缓冲区类型：**
  - SSBO (Storage Buffer Object) - 采样数据、顶点数据、结果
  - UBO (Uniform Buffer Object) - 参数
- **交付物：** 数据传输实现 + 性能优化

#### 1.7 集成GPU预计算到主程序（main.cpp）
- **目标：** 在主程序中使用GPU预计算
- **需要做的：**
  1. 创建PRTComputeShader实例
  2. 初始化GPU计算管道
  3. 在适当时机调用GPU预计算函数
  4. 处理结果
- **时机选择：**
  - 程序启动时预计算光照
  - 加载模型时预计算Light Transport
  - 用户交互时更新旋转
- **交付物：** 集成代码 + 调用示例

#### 1.8 实现预计算数据导出为TXT文件
- **目标：** 将GPU计算结果导出为可读的文本文件
- **导出内容：**
  - 光照系数 (Lighting SH coefficients)
  - Light Transport系数 (LT SH coefficients)
  - 旋转信息 (Rotation angles and coefficients)
- **文件格式：** 与CPU版本兼容的TXT格式
- **文件位置：** 项目目录或指定输出目录
- **交付物：** DataExporter GPU版本实现

#### 1.9 测试GPU预计算结果的正确性
- **目标：** 验证GPU计算的准确性和性能
- **测试内容：**
  1. 对比GPU和CPU计算结果
  2. 验证数值精度 (允许浮点误差)
  3. 测试TXT文件读写
  4. 性能基准测试
- **验收标准：**
  - 数值误差 < 0.1%
  - GPU加速比 > 50x
  - 文件格式正确
- **交付物：** 测试报告 + 性能数据

#### 1.10 编写GPU端预计算实现总结文档
- **目标：** 记录实现细节供后续参考
- **文档内容：**
  - 实现概述
  - 遇到的问题和解决方案
  - 性能优化技巧
  - 性能数据对比
  - 使用指南
- **交付物：** GPU_IMPLEMENTATION_SUMMARY.md

---

### 第二阶段：UI读取和应用预计算数据进行Relighting (7个任务)

**目标：** 实现实时Relighting系统，支持光照旋转交互

**核心任务：**

#### 2.1 分析现有UI系统和PreviewModel的渲染管道
- **目标：** 理解如何集成relighting功能
- **关键类：**
  - `VulkanUIOverlay` - UI系统
  - `Pass` - 渲染通道
  - `ETechnique` - 渲染技术枚举
  - `PreviewModel` - 模型渲染
- **需要理解：**
  - 如何添加新的Technique
  - 如何在UI中添加控件
  - 如何切换渲染模式
- **交付物：** 架构分析文档

#### 2.2 实现TXT文件读取功能
- **目标：** 从导出的TXT文件读取预计算数据
- **方法：** `ImportLighting()` 和 `ImportLightTransport()`
- **流程：**
  1. 打开TXT文件
  2. 解析球谐系数
  3. 解析旋转信息
  4. 返回数据结构
- **错误处理：** 文件不存在、格式错误等
- **交付物：** 文件读取实现

#### 2.3 创建Relighting着色器（prt_relighting.frag）
- **目标：** 编写片段着色器进行实时relighting
- **着色器功能：**
  - 接收球谐系数
  - 接收顶点法向量
  - 接收材质参数 (反射率、粗糙度等)
  - 计算最终颜色
- **公式：** 
  ```glsl
  color = albedo * dot(SHCoefficients, SHBasis(normal))
  ```
- **支持特性：**
  - 光照旋转
  - 材质参数调整
  - 多种着色模式
- **交付物：** prt_relighting.frag 着色器

#### 2.4 实现Relighting数据应用管道
- **目标：** 创建Pass或Technique应用relighting着色器
- **需要做的：**
  1. 创建新的Technique (如 `RELIGHTING`)
  2. 创建对应的Pass
  3. 集成到PreviewModel的Draw()方法
  4. 支持动态切换模式
- **关键参数：**
  - SH系数缓冲区
  - 当前旋转角度
  - 材质参数
- **交付物：** Pass实现 + 集成代码

#### 2.5 实现光照旋转交互UI
- **目标：** 允许用户通过UI控制光照旋转
- **UI控件：**
  - 滑块 (Slider) - 旋转角度 0-360度
  - 或输入框 (Input Box) - 直接输入角度
  - 实时显示当前角度
- **交互流程：**
  1. 用户调整滑块
  2. 更新旋转角度
  3. 查询对应的SH系数
  4. 实时更新渲染结果
- **交付物：** UI代码 + 交互实现

#### 2.6 测试完整的relighting流程
- **目标：** 验证端到端的relighting系统
- **测试流程：**
  1. 读取TXT文件
  2. 应用到Cornell Box
  3. 调整光照旋转
  4. 验证渲染结果
- **验收标准：**
  - 文件读取成功
  - 渲染结果正确
  - 旋转交互流畅
  - 性能满足要求
- **交付物：** 测试报告

#### 2.7 编写第二阶段总结文档和用户指南
- **目标：** 提供完整的使用文档
- **文档内容：**
  - 功能概述
  - 使用指南
  - UI控制说明
  - 性能数据
  - 常见问题
- **交付物：** RELIGHTING_USER_GUIDE.md

---

### 第三阶段：优化和验证 (3个任务)

**目标：** 性能优化、数值验证、文档完善

#### 3.1 性能优化和基准测试
- **优化方向：**
  - 工作组大小优化
  - 内存访问模式优化
  - 缓冲区复用
  - 同步点优化
- **基准测试：**
  - 预计算时间 (CPU vs GPU)
  - 内存使用量
  - 帧率 (relighting渲染)
- **目标：** GPU加速比 > 100x

#### 3.2 数值精度验证
- **验证内容：**
  - GPU和CPU计算结果对比
  - 浮点精度分析
  - 误差范围确认
- **可接受范围：** < 0.1% 相对误差

#### 3.3 完整的项目文档和示例
- **文档：**
  - 项目架构文档
  - API参考
  - 使用示例
  - 常见问题解答
- **示例代码：**
  - 基本使用示例
  - 高级功能示例
  - 性能优化示例

---

## 📊 项目统计

| 阶段 | 任务数 | 预计工作量 | 优先级 |
|------|--------|----------|--------|
| 第一阶段 | 10 | 高 | 🔴 关键 |
| 第二阶段 | 7 | 中 | 🟡 重要 |
| 第三阶段 | 3 | 低 | 🟢 优化 |
| **总计** | **20** | - | - |

---

## 🔑 关键文件清单

### 核心代码文件
```
✅ PRTComputeShader.h/cpp      - GPU计算管理类
✅ SphericalHarmonics.h/cpp    - 球谐函数实现
✅ PreviewModel.h/cpp          - 模型渲染类
✅ Pass.h/cpp                  - 渲染通道
✅ main.cpp                    - 主程序
```

### 着色器文件
```
✅ sh_compute.comp             - 光照投影计算
✅ prt_relighting.vert/frag    - Relighting着色器 (待创建)
✅ gltfmesh_main.vert/frag     - 模型渲染着色器
```

### 文档文件
```
✅ START_HERE.md               - 项目入门指南
✅ GPU_PRT_README.md           - GPU实现概述
✅ GPU_COMPUTE_IMPLEMENTATION_PLAN.md - 实现计划
✅ PROJECT_ROADMAP.md          - 本文件
```

---

## 🚀 快速开始

### 第一步：理解项目结构
1. 阅读 `START_HERE.md`
2. 浏览 `PROJECT_ROADMAP.md` (本文件)
3. 查看 `GPU_PRT_README.md`

### 第二步：分析现有代码
1. 研究 `main.cpp` 中的Cornell Box实现
2. 理解 `PreviewModel` 的渲染流程
3. 分析 `SphericalHarmonics.h` 的CPU实现

### 第三步：实现GPU端预计算
1. 完成任务 1.1 - 1.10
2. 参考 `GPU_COMPUTE_IMPLEMENTATION_PLAN.md`
3. 逐步实现各个方法

### 第四步：实现Relighting功能
1. 完成任务 2.1 - 2.7
2. 集成到UI系统
3. 进行测试验证

### 第五步：优化和验证
1. 完成任务 3.1 - 3.3
2. 性能基准测试
3. 编写最终文档

---

## 📈 预期成果

### 性能提升
- **预计算速度：** CPU: 12s → GPU: 120ms (100x加速)
- **内存使用：** 优化缓冲区复用
- **实时渲染：** 60+ FPS

### 功能完整性
- ✅ GPU端PRT预计算
- ✅ TXT文件导出/导入
- ✅ 实时Relighting
- ✅ 光照旋转交互
- ✅ 完整的UI系统

### 代码质量
- ✅ 详尽的文档
- ✅ 清晰的架构
- ✅ 优化的实现
- ✅ 完整的测试

---

## 🎯 成功标准

### 第一阶段完成标准
- [ ] GPU Pipeline完全实现
- [ ] 所有计算函数可用
- [ ] TXT文件导出成功
- [ ] 数值精度验证通过
- [ ] 性能提升 > 50x

### 第二阶段完成标准
- [ ] TXT文件读取成功
- [ ] Relighting着色器工作正常
- [ ] UI交互流畅
- [ ] 光照旋转效果正确
- [ ] 帧率 > 60 FPS

### 第三阶段完成标准
- [ ] 性能优化完成
- [ ] 数值精度确认
- [ ] 文档完整
- [ ] 示例代码可用

---

## 📞 支持和参考

### 相关文档
- `GPU_COMPUTE_IMPLEMENTATION_PLAN.md` - 详细实现步骤
- `LIGHTING_PROJECTION_GUIDE.md` - 光照投影详解
- `LIGHT_TRANSPORT_GUIDE.md` - Light Transport详解
- `SH_BASIS_OPTIMIZATION.md` - 球谐函数优化

### 关键概念
- **PRT (Precomputed Radiance Transfer)** - 预计算光照传输
- **SH (Spherical Harmonics)** - 球谐函数
- **Light Transport** - 光照传输，描述表面对光的响应
- **Relighting** - 实时重新着色

---

**项目开始时间：** 2025-11-28
**预计完成时间：** 5-7天
**最后更新：** 2025-11-28

祝你开发愉快！🚀

