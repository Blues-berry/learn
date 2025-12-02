# PRT vs PBR 修复验证指南

## 编译步骤

### 方法 1: 使用编译脚本（推荐）

```powershell
cd c:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\examples\lightprobesh2
.\compile.ps1
```

### 方法 2: 手动编译

```bash
cd c:\Users\Bluesky\Desktop\SKY\Learn\Vulkan
mkdir build
cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
ninja
```

## 运行程序

```bash
c:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\bin\lightprobesh2.exe
```

## 测试场景

### 测试 1: PBR 光源颜色正确性

**步骤**：
1. 启动程序
2. 在 UI 中找到 "Light Source" 部分
3. 点击 "Light Color" 颜色选择器
4. 选择绿色 (R:0, G:1, B:0)
5. 观察 Cornell Box 的光照

**预期结果**：
- ✅ Cornell Box 应显示绿色光照
- ✅ 不应该显示红色或其他颜色
- ✅ 光照强度应该正确

**失败症状**：
- ❌ 显示红色或其他颜色
- ❌ 光照方向错误

### 测试 2: PRT 光源颜色响应

**步骤**：
1. 启用 PRT（UI 中的 "Use PRT" 复选框）
2. 等待 PRT 预计算完成（控制台会输出信息）
3. 设置光源颜色为绿色
4. 观察 Cornell Box 的光照

**预期结果**：
- ✅ PRT 应显示绿色光照
- ✅ 与 PBR 的颜色应该一致
- ✅ 着色应该可见（不是全黑或全白）

**失败症状**：
- ❌ 完全黑色或无着色
- ❌ 颜色与 PBR 不一致
- ❌ 改变颜色时无反应

### 测试 3: 光源旋转

**步骤**：
1. 在 "Light Source" 中启用 "Auto Rotate"
2. 观察 Cornell Box 的着色变化
3. 手动调整 "Light Rotation" 滑块

**预期结果**：
- ✅ PBR 模式下着色应该随旋转而改变
- ✅ PRT 模式下着色应该随旋转而改变
- ✅ 两种模式的变化应该相似

**失败症状**：
- ❌ PRT 模式下着色无变化
- ❌ 旋转时闪烁或跳跃
- ❌ PBR 和 PRT 的变化不一致

### 测试 4: 多种颜色组合

**步骤**：
1. 测试不同的光源颜色：
   - 红色 (1, 0, 0)
   - 蓝色 (0, 0, 1)
   - 黄色 (1, 1, 0)
   - 白色 (1, 1, 1)
2. 对每种颜色，同时观察 PBR 和 PRT 模式

**预期结果**：
- ✅ 所有颜色都应该正确显示
- ✅ PBR 和 PRT 应该显示相同的颜色
- ✅ 没有颜色反向或混淆

## 调试信息

### 控制台输出

程序启动时应该输出：

```
[Step 2] Precomputing Lighting (Light Source)...
  - Lighting SH coefficients computed (using unit light source)
  - Current Light Color: (1.00, 1.00, 1.00)
  - Current Light Intensity: 100.00
  - Note: Color and intensity will be applied at runtime in UpdatePRTLighting()
```

### 运行时调试

在 PRT 模式下，每当光源旋转时应该输出：

```
[DEBUG PRT] Updating lighting for angle: XXX degrees
[DEBUG PRT] Updated UBO for angle XXX deg. L0M0 (after color/intensity): (R, G, B)
```

## 常见问题

### Q: PRT 显示全黑
**A**: 检查 SH 系数是否被正确计算。查看控制台输出中的 L0M0 值。

### Q: PBR 和 PRT 颜色不一致
**A**: 检查 `UpdatePRTLighting()` 中的颜色应用逻辑。

### Q: 旋转时无变化
**A**: 检查 `QueryCoefficients()` 是否正确插值。

## 性能指标

- 帧率应该 > 60 FPS
- PRT 预计算应该在 < 5 秒内完成
- 颜色改变应该立即生效（< 1 帧延迟）

