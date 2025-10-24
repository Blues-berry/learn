# 探针可视化 - 快速开始指南

## 🚀 编译和运行

### 编译
```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 运行
```bash
./build/bin/Release/lightprobesh2.exe
```

## 📖 使用指南

### 1. 单探针可视化

**步骤**:
1. 启动程序
2. 在 UI 中找到 "Probe Visualization" 部分
3. 选择 "Display Mode" = "Single"
4. 点击 "Capture Cubemap at Camera" 按钮
5. 观察红色球体显示在相机位置

**效果**: 显示最后捕获的单个探针

### 2. 多探针可视化

**步骤**:
1. 启用 "Use Multiple Probes" 复选框
2. 调整探针网格参数（可选）:
   - Probe Min/Max: 设置探针网格的边界
   - Probe Dim: 设置探针网格的维度
   - Probe Resolution: 设置探针的分辨率
3. 点击 "Generate Probes" 按钮
4. 点击 "Capture All Probes" 按钮
5. 选择 "Display Mode" = "All"
6. 观察所有探针显示为彩色球体

**效果**: 显示所有已捕获的探针

### 3. 插值模式

**步骤**:
1. 启用 "Use Multiple Probes" 复选框
2. 点击 "Generate Probes" 按钮
3. 点击 "Capture All Probes" 按钮
4. 选择 "Display Mode" = "Interpolated"
5. 移动相机观察探针网格

**效果**: 显示多探针网格中的所有探针，相机移动时可以看到光照的插值变化

### 4. 调整探针大小

使用 "Probe Scale" 滑块调整探针球体的大小：
- 最小值: 0.05 (很小的球体)
- 最大值: 1.0 (较大的球体)

## 🎨 显示模式说明

### None (不显示)
- 不显示任何探针
- 用于查看场景的正常渲染

### Single (单个探针)
- 显示最后捕获的探针
- 颜色: 红色 (1.0, 0.0, 0.0)
- 用于查看单个探针的位置

### All (所有探针)
- 显示所有已捕获的探针
- 颜色: 彩色（根据索引生成）
- 用于查看所有探针的分布

### Interpolated (插值)
- 显示多探针网格中的所有探针
- 颜色: 彩色（根据索引生成）
- 用于查看探针网格的结构和插值效果

## 💡 技巧

### 1. 查看探针网格结构
- 启用 "Use Multiple Probes"
- 生成探针网格
- 选择 "Display Mode" = "Interpolated"
- 移动相机查看网格的三维结构

### 2. 对比单个和多个探针
- 先选择 "Single" 模式查看单个探针
- 再选择 "All" 模式查看所有探针
- 观察光照的差异

### 3. 调整探针大小以便观察
- 如果探针太小，增加 "Probe Scale"
- 如果探针太大，减少 "Probe Scale"

### 4. 性能优化
- 如果帧率下降，减少探针数量
- 调整 "Probe Dim" 参数减少探针数量
- 或者选择 "Display Mode" = "None" 关闭可视化

## 🔍 常见问题

### Q: 为什么看不到探针？
A: 
1. 检查 "Display Mode" 是否设置为 "None"
2. 检查是否已经捕获探针
3. 尝试增加 "Probe Scale" 使探针更大

### Q: 为什么探针显示为黑色？
A: 这是正常的，探针使用简单的材质，不受光照影响

### Q: 如何隐藏探针？
A: 选择 "Display Mode" = "None"

### Q: 可以改变探针的颜色吗？
A: 目前不支持，但可以修改 ProbeVisualizer.cpp 中的颜色生成算法

## 📊 参数说明

### Probe Grid 参数

| 参数 | 范围 | 说明 |
|------|------|------|
| Probe Min X/Y/Z | -∞ ~ +∞ | 探针网格的最小边界 |
| Probe Max X/Y/Z | -∞ ~ +∞ | 探针网格的最大边界 |
| Probe Dim X/Y/Z | 1 ~ 20 | 各轴方向的探针数量 |
| Probe Resolution | 4 ~ 256 | 每个探针的立方体贴图分辨率 |

### Visualization 参数

| 参数 | 范围 | 说明 |
|------|------|------|
| Display Mode | None/Single/All/Interpolated | 显示模式 |
| Probe Scale | 0.05 ~ 1.0 | 探针球体的缩放因子 |

## 🎯 工作流程

```
1. 启动程序
   ↓
2. 选择显示模式 (Display Mode)
   ↓
3. 如果选择 Single:
   - 点击 "Capture Cubemap at Camera"
   - 观察红色球体
   ↓
4. 如果选择 All/Interpolated:
   - 启用 "Use Multiple Probes"
   - 点击 "Generate Probes"
   - 点击 "Capture All Probes"
   - 观察彩色球体
   ↓
5. 调整 "Probe Scale" 改变大小
   ↓
6. 移动相机观察效果
```

## 📝 注意事项

1. 探针可视化仅用于调试和观察，不影响最终的光照计算
2. 显示大量探针可能会影响性能
3. 探针的颜色是自动生成的，用于区分不同的探针
4. 探针的大小可以独立调整，不影响实际的光照计算

## 🔗 相关文档

- `PROBE_VISUALIZATION_IMPLEMENTATION.md` - 详细的实现说明
- `examples/lightprobesh2/ProbeVisualizer.h` - 头文件
- `examples/lightprobesh2/ProbeVisualizer.cpp` - 实现文件


