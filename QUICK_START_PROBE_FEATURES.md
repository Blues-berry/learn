# 快速开始 - 探针可视化和插值功能

## 🎯 5 分钟快速上手

### 编译

```bash
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release --target lightprobesh2
```

### 运行

```bash
cd bin/Release
lightprobesh2.exe
```

---

## 📖 使用指南

### 场景 1: 查看单个探针

1. **加载模型**
   - 在 UI 中选择 "PreviewModel" 下拉框，选择 "rock01" 或其他模型
   - 选择 "Skybox" 下拉框，选择 "pisa" 或其他环境贴图

2. **捕获单个探针**
   - 点击 "Capture Cubemap at Camera" 按钮
   - 系统会在相机位置捕获立方体贴图

3. **查看结果**
   - 新的立方体贴图会被添加到 "Skybox" 列表中
   - 天空盒会自动更新为最新捕获的立方体贴图

---

### 场景 2: 多探针网格和可视化

1. **启用多探针模式**
   - 勾选 "Use Multiple Probes" 复选框

2. **配置探针网格**
   - 调整 "Probe Min/Max" 参数设置包围盒
   - 调整 "Probe Dim X/Y/Z" 设置网格维度
   - 例如：3×2×3 的网格会创建 18 个探针

3. **生成探针**
   - 点击 "Generate Probes" 按钮
   - 探针会被放置在网格的每个单元中心

4. **可视化探针**
   - 勾选 "Show Probes" 复选框
   - 场景中会显示所有探针的位置（球体）

---

### 场景 3: 捕获所有探针并进行插值

1. **准备工作**
   - 完成"场景 2"中的步骤 1-3

2. **捕获所有探针**
   - 点击 "Capture All Probes" 按钮
   - 系统会自动捕获所有探针的立方体贴图
   - 每个探针的立方体贴图会被添加到列表中

3. **进行插值**
   - 移动相机到任意位置
   - 点击 "Interpolate Cubemap" 按钮
   - 系统会在当前相机位置进行立方体贴图插值

4. **查看插值结果**
   - 新的插值立方体贴图会被添加到 "Skybox" 列表中
   - 名称为 "Interpolated_X"
   - 天空盒会自动更新为最新的插值结果

---

## 🎮 UI 控制说明

### 基本设置
- **Exposure** - 曝光度调节
- **Gamma** - 伽马值调节
- **Skybox** - 选择当前天空盒
- **PreviewModel** - 选择预览模型

### 单探针捕获
- **Capture Cubemap at Camera** - 在相机位置捕获立方体贴图

### 多探针模式
- **Use Multiple Probes** - 启用/禁用多探针模式
- **Probe Min X/Y/Z** - 探针网格最小边界
- **Probe Max X/Y/Z** - 探针网格最大边界
- **Probe Dim X/Y/Z** - 探针网格维度
- **Probe Resolution** - 单个探针的立方体贴图分辨率
- **Generate Probes** - 生成探针网格
- **Capture All Probes** - 捕获所有探针
- **Interpolate Cubemap** - 进行立方体贴图插值
- **Show Probes** - 显示/隐藏探针可视化

---

## 💡 提示

### 性能优化
- 减少探针数量以提高性能
- 使用较低的分辨率（16×16）进行快速测试
- 使用较高的分辨率（256×256）获得更好的质量

### 最佳实践
- 先用少量探针（2×2×2）测试功能
- 逐步增加探针数量
- 在不同位置进行多次插值测试

### 调试
- 启用 "Show Probes" 查看探针位置
- 在 UI 中查看当前选择的立方体贴图
- 检查控制台输出了解操作进度

---

## 🔧 常见问题

### Q: 插值后的立方体贴图看起来不对？
**A**: 
- 确保已经捕获了足够的探针
- 检查探针网格是否覆盖了目标区域
- 尝试调整 maxDistance 参数

### Q: 探针可视化不显示？
**A**:
- 确保已经生成了探针
- 勾选 "Show Probes" 复选框
- 检查相机位置是否能看到探针

### Q: 捕获速度很慢？
**A**:
- 减少探针数量
- 降低立方体贴图分辨率
- 使用较小的包围盒

---

## 📊 推荐配置

### 快速测试
```
Probe Dim: 2×2×2 (8 个探针)
Probe Resolution: 16×16
Bounds: [-10, 10] × [0, 4] × [-10, 10]
```

### 标准配置
```
Probe Dim: 4×2×4 (32 个探针)
Probe Resolution: 64×64
Bounds: [-10, 10] × [0, 4] × [-10, 10]
```

### 高质量配置
```
Probe Dim: 6×3×6 (108 个探针)
Probe Resolution: 256×256
Bounds: [-15, 15] × [0, 6] × [-15, 15]
```

---

## 📝 输出文件

捕获的立方体贴图会保存为 PPM 格式：
- `Captured_X_pos_x.ppm` - +X 面
- `Captured_X_neg_x.ppm` - -X 面
- `Captured_X_pos_y.ppm` - +Y 面
- `Captured_X_neg_y.ppm` - -Y 面
- `Captured_X_pos_z.ppm` - +Z 面
- `Captured_X_neg_z.ppm` - -Z 面


