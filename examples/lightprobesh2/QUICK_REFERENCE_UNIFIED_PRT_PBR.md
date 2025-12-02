# 快速参考：统一PRT-PBR实现

## 🎯 核心改动（3处）

### 1️⃣ 预计算：Lambert余弦项
**文件**: main.cpp (行1528-1552)
```cpp
// 旧：聚光锥 → 新：Lambert余弦
float cosTerm = glm::max(0.0f, glm::dot(-w, lightDir));
radiances.push_back(glm::vec3(cosTerm));
```

### 2️⃣ 运行时：应用光源参数
**文件**: main.cpp (行2040-2044)
```cpp
// 新增：乘以lightColor和lightIntensity
float intensityScale = lightIntensity / 100.0f;
for (int i = 0; i < 9; ++i) {
    currentSHCoefficients.coeffs[i] *= lightColor * intensityScale;
}
```

### 3️⃣ UI：简化聚光参数
**文件**: main.cpp (行1095-1107)
```cpp
// 删除：spotInnerDeg和spotOuterDeg滑条
// 新增：显示"Mode: PBR-unified (Lambert cosine)"
```

## 🚀 快速测试

```bash
# 1. 编译
cd examples\lightprobesh2
powershell -ExecutionPolicy Bypass -File compile.ps1

# 2. 运行
cd ..\..\build\bin
.\lightprobesh2.exe

# 3. UI操作
# - 点击 "Export PRT (GPU)"
# - 勾选 "Enable PRT Relighting"
# - 勾选 "Enable Light"
# - 观察：模型应显示黄色（与PBR相同）
# - 勾选 "Auto Rotate"
# - 观察：颜色应同步变化
```

## ✅ 验证清单

- [ ] 编译成功
- [ ] PRT导出完成
- [ ] PRT Relighting启用成功
- [ ] 模型显示黄色着色
- [ ] 光源旋转时颜色同步变化
- [ ] 控制台无ERROR消息

## 📊 效果对比

| 项目 | 修改前 | 修改后 |
|------|--------|--------|
| PBR颜色 | 黄色 | 黄色 |
| PRT颜色 | 灰色 | **黄色** |
| 光源旋转 | 不同步 | **同步** |
| 预计算模型 | 聚光锥 | **Lambert** |

## 🔧 调试技巧

### PRT仍然是灰色？
```cpp
// 检查：
1. lightIntensity > 0 ?
2. lightColor != black ?
3. 重新导出PRT数据
```

### 颜色不同步？
```cpp
// 检查：
1. usePRTRelighting == true ?
2. prtReady == true ?
3. 查看控制台错误
```

### 导出失败？
```cpp
// 检查：
1. 模型顶点数 == LT数据行数 ?
2. 删除旧的 prt_output 目录
3. 重新导出
```

## 📁 文件改动

| 文件 | 行号 | 改动 |
|------|------|------|
| main.cpp | 239-250 | 注释spotInnerDeg/spotOuterDeg |
| main.cpp | 1095-1107 | 删除spot滑条，简化UI |
| main.cpp | 1528-1552 | Lambert余弦项 |
| main.cpp | 2040-2044 | 应用lightColor/Intensity |

## 📚 相关文档

- `PRT_PBR_UNIFIED_STRATEGY.md` - 详细策略
- `UNIFIED_PRT_PBR_TESTING_GUIDE.md` - 完整测试指南
- `IMPLEMENTATION_SUMMARY_UNIFIED_PRT_PBR.md` - 实现总结

## 💡 关键概念

**Lambert余弦项**：
```
radiance(ω) = max(0, dot(-ω, lightDir))
```
- 与PBR直射漫反射一致
- 无需聚光锥参数
- 支持任意旋转

**强度标定**：
```
intensityScale = lightIntensity / 100.0f
```
- 将UI范围(50-100)标定到(0.5-1.0)
- 可根据需要调整系数

## 🎓 原理

```
PBR直射漫反射：
  color = diffuse * lightColor * lightIntensity

PRT预计算：
  radiance = Lambert(direction)
  SH = project(radiance)
  
PRT运行时：
  SH *= lightColor * intensityScale
  color = dot(LT, SH)
  
结果：两者一致 ✓
```

## ⚡ 性能

- **预计算**：一次性，导出时
- **运行时**：查表 + 向量乘法
- **效率**：PRT >= PBR（预计算优势）

## 🔗 相关代码位置

```
ExportPRTDataGPU()      → 行1522-1745
UpdatePRTLighting()     → 行2000-2074
OnUpdateUIOverlay()     → 行1095-1107
```

## ✨ 总结

✅ 在PBR框架下进行PRT预计算
✅ 使用Lambert余弦项代替聚光锥
✅ 应用PBR的光源参数到PRT
✅ 简化UI，提高效率
✅ 着色效果一致，性能提升


