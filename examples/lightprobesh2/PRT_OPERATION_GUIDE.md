# PRT 动态光照预计算渲染 - 操作指南

## 📋 完整操作流程

### 第一阶段：启动程序

```bash
# 1. 编译（如果还没编译）
powershell -ExecutionPolicy Bypass -File "examples\lightprobesh2\compile.ps1"

# 2. 运行程序
bin\lightprobesh2.exe
```

**预期**: 程序启动，显示 Cornell Box 场景

---

## 🎬 第二阶段：预计算 PRT 数据

### 步骤 1：打开 UI 菜单

在程序窗口中，你应该看到一个 ImGui 界面（通常在左上角或右侧）

### 步骤 2：找到 PRT 相关选项

在 UI 中查找以下选项：
- **"Export PRT Data"** 按钮
- **"PRT Samples"** 滑块（用于调整采样数量）
- **"Light Intensity"** 滑块（光源强度）
- **"Light Color"** 颜色选择器

### 步骤 3：调整参数（可选）

```
PRT Samples: 32-64 (默认值即可)
  - 越高越精确，但计算时间越长
  - 推荐: 32-64

Light Intensity: 1.0-2.0
  - 控制光源亮度
  - 推荐: 1.0

Light Color: 白色 (1.0, 1.0, 1.0)
  - 控制光源颜色
  - 推荐: 白色以获得最佳效果
```

### 步骤 4：点击 "Export PRT Data" 按钮

**操作**: 在 UI 中找到 "Export PRT Data" 按钮，点击它

**预期输出**:
```
[Step 1] Generating sample directions...
  - Generated 32 sample directions

[Step 2] Precomputing Lighting (Light Source)...
  - Lighting SH coefficients computed
  - Light Color: (1.0, 1.0, 1.0)
  - Light Intensity: 1.0

[Step 3] Precomputing Light Transport...
  - LT CPU computation complete.

[ExportPRTDataGPU] Precomputing rotations
[ExportPRTDataGPU] Export lighting rotations => OK
[ExportPRTDataGPU] Export light transport => OK
```

### 步骤 5：验证导出的数据

打开文件管理器，检查 `prt_output/` 目录：

```
prt_output/
├── prt_data_lighting.txt    (应该有 24 行)
└── prt_data_lt.txt          (应该有多行，每行对应一个顶点)
```

**检查文件内容**:
```bash
# 查看 lighting 文件（应该有 24 行）
type prt_output\prt_data_lighting.txt

# 输出示例：
# 0 0.1 0.2 0.3 ... (24 行，每行代表一个旋转角度)

# 查看 LT 文件（应该有多行）
type prt_output\prt_data_lt.txt

# 输出示例：
# 0.5 0.6 0.7 ... (多行，每行代表一个顶点的 LT 系数)
```

---

## 🎨 第三阶段：启用 PRT 渲染

### 步骤 1：找到 PRT 启用选项

在 UI 中查找：
- **"Enable PRT"** 复选框
- **"PRT Rotation Angle"** 滑块

### 步骤 2：勾选 "Enable PRT"

**操作**: 点击 "Enable PRT" 复选框

**预期**:
- Cornell Box 会被照亮
- 你应该看到环境光效果
- 如果之前是全黑，现在应该看到颜色

### 步骤 3：观察初始效果

此时，Cornell Box 应该显示：
- 红色墙（右侧）
- 绿色墙（左侧）
- 白色天花板和地板
- 环境光照亮整个场景

---

## 🔄 第四阶段：动态旋转光源

### 步骤 1：找到旋转控制

在 UI 中查找：
- **"PRT Rotation Angle"** 滑块（0-360°）
- 或 **"Light Rotation"** 滑块

### 步骤 2：拖动滑块旋转光源

**操作**: 拖动 "PRT Rotation Angle" 滑块

**预期效果**:
```
旋转 0°:   Cornell Box 显示初始光照
旋转 90°:  光源从右侧旋转到前方，颜色变化
旋转 180°: 光源旋转到背面，颜色继续变化
旋转 270°: 光源从左侧旋转，颜色再次变化
旋转 360°: 回到初始状态
```

### 步骤 3：观察动态效果

当你旋转光源时，你应该看到：
- ✅ Cornell Box 的颜色平滑变化
- ✅ 环境光效果随光源旋转而改变
- ✅ 红墙和绿墙的亮度随之变化
- ✅ 效果接近 PBR 渲染

---

## 📊 第五阶段：对比 PBR 和 PRT

### 步骤 1：启用 PBR 渲染

在 UI 中查找：
- **"Enable Lighting"** 或 **"Enable PBR"** 复选框

### 步骤 2：同时启用两种渲染

```
启用 PBR:   ✓
启用 PRT:   ✓
```

### 步骤 3：对比效果

**PBR 渲染**:
- 实时计算光照
- 更精确但计算量大
- 光源位置实时变化

**PRT 渲染**:
- 预计算光照
- 快速但精度受限
- 光源旋转使用预计算数据

### 步骤 4：观察差异

- 两种渲染应该显示相似的效果
- PRT 应该比 PBR 快得多
- 颜色应该大致相同

---

## 🎮 完整操作示例

### 场景 1：基础 PRT 渲染

```
1. 启动程序
   bin\lightprobesh2.exe

2. 导出 PRT 数据
   - 点击 "Export PRT Data"
   - 等待完成

3. 启用 PRT
   - 勾选 "Enable PRT"
   - 观察 Cornell Box 被照亮

4. 旋转光源
   - 拖动 "PRT Rotation Angle" 滑块
   - 观察颜色变化
```

### 场景 2：对比 PBR 和 PRT

```
1. 启用 PBR
   - 勾选 "Enable Lighting"
   - 观察实时光照

2. 启用 PRT
   - 勾选 "Enable PRT"
   - 观察预计算光照

3. 旋转光源
   - 两种渲染都应该显示相似效果
   - PRT 应该更快
```

### 场景 3：调整参数

```
1. 修改 PRT Samples
   - 增加采样数量以提高精度
   - 减少采样数量以加快计算

2. 修改 Light Intensity
   - 增加强度使场景更亮
   - 减少强度使场景更暗

3. 修改 Light Color
   - 改变光源颜色
   - 重新导出 PRT 数据
```

---

## 🔍 调试和验证

### 验证 1：检查导出数据

```bash
# 查看 lighting 文件大小
dir prt_output\prt_data_lighting.txt

# 应该显示非零大小（通常 1-5 KB）

# 查看 LT 文件大小
dir prt_output\prt_data_lt.txt

# 应该显示非零大小（通常 10-100 KB）
```

### 验证 2：检查数据内容

```bash
# 查看前几行
type prt_output\prt_data_lighting.txt | head -5

# 输出应该类似：
# 0 0.123 0.456 0.789 ...
# 15 0.234 0.567 0.890 ...
# 30 0.345 0.678 0.901 ...
# ...
```

### 验证 3：观察渲染效果

- ✅ Cornell Box 应该被照亮（不是全黑）
- ✅ 红墙应该显示红色
- ✅ 绿墙应该显示绿色
- ✅ 旋转时颜色应该平滑变化

### 验证 4：性能对比

- ✅ PRT 渲染应该比 PBR 快
- ✅ 旋转应该流畅（60+ FPS）
- ✅ 没有明显的卡顿

---

## ⚙️ 高级操作

### 修改采样数量

```
PRT Samples 滑块:
- 最小: 4 (快速但低精度)
- 推荐: 32-64 (平衡)
- 最大: 256 (高精度但慢)
```

### 修改光源参数

```
Light Intensity:
- 0.5: 暗光
- 1.0: 标准光
- 2.0: 亮光

Light Color:
- 白色 (1, 1, 1): 中性
- 红色 (1, 0, 0): 红光
- 蓝色 (0, 0, 1): 蓝光
```

### 预计算多个光源配置

```
1. 设置光源参数 1
2. 导出 PRT 数据 1
3. 设置光源参数 2
4. 导出 PRT 数据 2
5. 在运行时切换使用不同的数据
```

---

## 🎯 常见问题

### Q: 导出后看不到效果
A: 
1. 检查 "Enable PRT" 是否勾选
2. 检查 prt_output/ 目录中的文件是否存在
3. 查看控制台输出是否有错误

### Q: 颜色不对
A:
1. 检查 Light Color 设置
2. 重新导出 PRT 数据
3. 确保 Light Intensity 不为 0

### Q: 旋转没有效果
A:
1. 检查 "Enable PRT" 是否启用
2. 拖动 "PRT Rotation Angle" 滑块
3. 检查数据文件是否正确导出

### Q: 程序很慢
A:
1. 减少 PRT Samples 数量
2. 关闭其他应用
3. 检查 GPU 是否被充分利用

---

## 📈 预期结果总结

| 步骤 | 预期结果 |
|------|---------|
| 启动程序 | 看到 Cornell Box |
| 导出数据 | 看到日志输出，文件生成 |
| 启用 PRT | Cornell Box 被照亮 |
| 旋转光源 | 颜色平滑变化 |
| 对比 PBR | 效果相似，PRT 更快 |

---

## ✅ 操作完成检查清单

- [ ] 程序启动成功
- [ ] 导出 PRT 数据成功
- [ ] 文件生成在 prt_output/ 目录
- [ ] 启用 PRT 后 Cornell Box 被照亮
- [ ] 旋转光源时颜色变化
- [ ] 效果与 PBR 相似
- [ ] 性能满足要求

---

**现在你已经准备好使用 PRT 系统了！** 🚀

