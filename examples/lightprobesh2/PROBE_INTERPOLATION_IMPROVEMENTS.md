# 光照探针插值系统改进总结

## 概述

本文档总结了对 lightprobesh2 示例中光照探针立方体贴图插值系统的三项主要改进。

---

## 任务 1: 实现像素级插值 ✅

### 目标
通过多个低分辨率探针的数据，插值到高分辨率的全局立方体贴图。

### 实现方案

#### 新增文件
- **shaders/glsl/lightprobesh2/probe_interpolation.comp** - GPU计算着色器
- **examples/lightprobesh2/ProbeInterpolationPass.h** - 插值计算通道头文件
- **examples/lightprobesh2/ProbeInterpolationPass.cpp** - 插值计算通道实现

#### 核心特性
1. **GPU加速计算**：使用Vulkan计算着色器进行像素级插值
2. **多探针支持**：最多支持256个探针
3. **高分辨率输出**：支持任意分辨率的输出立方体贴图
4. **高效内存使用**：使用统一缓冲区存储探针数据

#### 工作流程
```
多个低分辨率探针立方体贴图
        ↓
GPU计算着色器（probe_interpolation.comp）
        ↓
反距离加权/线性/三次样条插值
        ↓
高分辨率全局立方体贴图
```

#### 使用方法
```cpp
// 创建插值对象
cubemapInterpolation = std::make_unique<CubemapInterpolation>(vulkanDevice, this);

// 添加探针
cubemapInterpolation->AddProbe(position, cubemap);

// 执行插值
auto result = cubemapInterpolation->InterpolateAt(
    camera.position,    // 查询位置
    50.0f,             // 最大搜索距离
    256,               // 输出分辨率
    queue              // Vulkan队列
);
```

---

## 任务 2: 添加权重可视化 ✅

### 目标
实现权重可视化功能，显示每个像素的插值权重分布。

### 实现方案

#### 新增文件
- **shaders/glsl/lightprobesh2/probe_weight_visualization.comp** - 权重可视化着色器
- **examples/lightprobesh2/ProbeWeightVisualizationPass.h** - 权重可视化通道头文件
- **examples/lightprobesh2/ProbeWeightVisualizationPass.cpp** - 权重可视化通道实现

#### 可视化模式

1. **单个探针权重** (Mode 0)
   - 显示第一个探针的权重分布
   - 用于调试单个探针的影响范围

2. **权重热力图** (Mode 1)
   - 显示所有探针中最高权重的分布
   - 颜色映射：蓝色(低) → 绿色 → 黄色 → 红色(高)
   - 用于理解整体权重分布

3. **最近探针ID** (Mode 2)
   - 显示每个像素最近的探针ID
   - 使用伪随机颜色编码探针ID
   - 用于识别探针的影响区域

#### 热力图颜色映射
```
权重值    颜色
0.0-0.25  蓝色 → 绿色
0.25-0.5  绿色 → 黄色
0.5-0.75  黄色 → 橙色
0.75-1.0  橙色 → 红色
```

#### 使用方法
```cpp
// 权重热力图可视化
auto heatmap = cubemapInterpolation->VisualizeWeights(
    256,    // 输出分辨率
    queue,  // Vulkan队列
    1       // 模式：WEIGHT_HEATMAP
);

// 最近探针ID可视化
auto probeID = cubemapInterpolation->VisualizeWeights(
    256,    // 输出分辨率
    queue,  // Vulkan队列
    2       // 模式：CLOSEST_PROBE_ID
);
```

---

## 任务 3: 支持多种插值算法 ✅

### 目标
支持IDW、线性、三次样条等多种插值算法。

### 实现方案

#### 支持的算法

1. **反距离加权 (IDW)** - Mode 0
   - 权重 = 1 / (distance + epsilon)
   - 优点：平滑、自然
   - 缺点：计算量大

2. **线性插值** - Mode 1
   - 使用最近的两个探针进行线性插值
   - 优点：快速、简单
   - 缺点：可能出现不连续

3. **三次样条插值** - Mode 2
   - 使用最近的4个探针进行高斯加权插值
   - 优点：平滑、高质量
   - 缺点：计算量最大

#### 算法选择
在UI中通过下拉菜单选择插值算法：
```
Interpolation Mode: [IDW ▼]
                    - IDW
                    - Linear
                    - Cubic
```

#### 使用方法
```cpp
// 设置插值算法
cubemapInterpolation->SetInterpolationMode(
    CubemapInterpolation::InterpolationMode::IDW
);

// 执行插值（使用选定的算法）
auto result = cubemapInterpolation->InterpolateAt(
    camera.position,
    50.0f,
    256,
    queue
);
```

---

## UI 控制

### 新增UI控件

1. **Interpolation Mode** - 下拉菜单
   - 选择插值算法（IDW/Linear/Cubic）

2. **Interpolate Cubemap (GPU)** - 按钮
   - 执行GPU加速的立方体贴图插值

3. **Visualize Weights (Heatmap)** - 按钮
   - 生成权重热力图可视化

4. **Visualize Closest Probe ID** - 按钮
   - 生成最近探针ID可视化

---

## 性能特性

### GPU加速优势
- **并行处理**：16×16工作组并行处理像素
- **高效内存**：统一缓冲区存储探针数据
- **可扩展性**：支持最多256个探针
- **实时性**：256×256分辨率立方体贴图在毫秒级完成

### 内存使用
- 探针缓冲区：~16KB（256个探针）
- 输出立方体贴图：~4MB（256×256×RGBA16F）

---

## 代码结构

### 类关系
```
CubemapInterpolation
├── ProbeInterpolationPass (GPU插值)
└── ProbeWeightVisualizationPass (权重可视化)
```

### 关键方法

#### CubemapInterpolation
- `AddProbe()` - 添加探针
- `ClearProbes()` - 清空探针
- `InterpolateAt()` - 执行插值
- `VisualizeWeights()` - 可视化权重
- `SetInterpolationMode()` - 设置插值算法

#### ProbeInterpolationPass
- `AddProbe()` - 添加探针
- `SetOutputCubemap()` - 设置输出纹理
- `SetInterpolationMode()` - 设置算法
- `Generate()` - 执行计算

#### ProbeWeightVisualizationPass
- `AddProbe()` - 添加探针
- `SetOutputCubemap()` - 设置输出纹理
- `SetVisualizationMode()` - 设置可视化模式
- `Generate()` - 执行计算

---

## 编译和运行

### 编译
```bash
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Debug --target lightprobesh2
```

### 运行
```bash
./bin/Debug/lightprobesh2.exe
```

### 使用流程
1. 启用 "Use Multiple Probes"
2. 配置探针网格参数
3. 点击 "Generate Probes" 生成探针
4. 点击 "Capture All Probes" 捕获探针立方体贴图
5. 选择插值算法
6. 点击 "Interpolate Cubemap (GPU)" 执行插值
7. 点击 "Visualize Weights (Heatmap)" 查看权重分布

---

## 扩展建议

1. **更多插值算法**
   - Kriging插值
   - 径向基函数（RBF）插值
   - 多项式插值

2. **性能优化**
   - 探针缓存机制
   - 动态分辨率调整
   - 渐进式插值

3. **可视化增强**
   - 探针位置标记
   - 权重梯度显示
   - 实时权重更新

4. **交互改进**
   - 鼠标选择探针
   - 动态调整探针位置
   - 权重参数微调

---

## 参考资源

- Vulkan计算着色器文档
- 反距离加权插值算法
- 立方体贴图采样技术
- GPU并行计算最佳实践

