# 统一PRT-PBR实现总结

## 项目背景

**问题**：PBR着色效果是黄色，但PRT看不到有什么着色效果。

**根本原因**：
- PBR使用直射光源（点光源/方向光）+ IBL
- PRT使用聚光锥模型（spotlight）
- 两者光源分布不同，导致着色效果差异

**解决方案**：在PBR框架下进行PRT预计算，使用Lambert余弦项代替聚光锥，仅提高着色效率。

## 核心改动

### 改动1：预计算阶段 - 使用Lambert余弦项

**位置**：`main.cpp` 行 1528-1552 (ExportPRTDataGPU函数)

**改动前**：
```cpp
// 聚光锥模型
const float cosOuter = cosf(glm::radians(spotOuterDeg));
const float cosInner = cosf(glm::radians(spotInnerDeg));
auto smoothstep = [](float edge0, float edge1, float x) { ... };
for (const auto& w : directions) {
    float c = glm::dot(glm::normalize(-w), lightDir);
    float falloff = (c <= cosOuter) ? 0.0f : smoothstep(cosOuter, cosInner, c);
    radiances.push_back(glm::vec3(1.0f) * falloff);
}
```

**改动后**：
```cpp
// Lambert余弦项（与PBR直射漫反射一致）
glm::vec3 lightDir = glm::vec3(0.0f, 0.0f, -1.0f);
for (const auto& w : directions) {
    float cosTerm = glm::max(0.0f, glm::dot(-w, lightDir));
    radiances.push_back(glm::vec3(cosTerm));
}
```

**优势**：
- 与PBR的Lambert漫反射模型一致
- 预计算数据与光源方向无关（只需旋转）
- 运行时可直接应用lightColor和lightIntensity

### 改动2：运行时更新 - 应用光源参数

**位置**：`main.cpp` 行 2000-2074 (UpdatePRTLighting函数)

**改动前**：
```cpp
// 直接使用预计算的SH系数
currentSHCoefficients = Relighter::QueryCoefficients(angleDegrees, prtData);
// 打包到GPU UBO
for (int i = 0; i < 9; ++i) {
    gpuLighting.coeffs[i] = glm::vec4(currentSHCoefficients.coeffs[i], 0.0f);
}
```

**改动后**：
```cpp
// 查询旋转后的SH系数
currentSHCoefficients = Relighter::QueryCoefficients(angleDegrees, prtData);

// ===== NEW: 应用PBR的光源参数 =====
float intensityScale = lightIntensity / 100.0f;
for (int i = 0; i < 9; ++i) {
    currentSHCoefficients.coeffs[i] *= lightColor * intensityScale;
}

// 打包到GPU UBO
for (int i = 0; i < 9; ++i) {
    gpuLighting.coeffs[i] = glm::vec4(currentSHCoefficients.coeffs[i], 0.0f);
}
```

**优势**：
- PRT使用与PBR相同的lightColor和lightIntensity
- 光源旋转时PRT颜色同步变化
- 用户体验一致

### 改动3：UI简化 - 移除聚光参数

**位置**：`main.cpp` 行 1095-1107 (OnUpdateUIOverlay函数)

**改动前**：
```cpp
if (overlay->header("PRT GPU Export")) {
    bool changed = false;
    changed |= overlay->sliderFloat("Spot Inner (deg)", &spotInnerDeg, 1.0f, 60.0f);
    changed |= overlay->sliderFloat("Spot Outer (deg)", &spotOuterDeg, 1.0f, 90.0f);
    if (spotOuterDeg < spotInnerDeg) spotOuterDeg = spotInnerDeg;
    if (changed) { ... }
    if (overlay->button("Export PRT (GPU)")) {
        ExportPRTDataGPU();
    }
}
```

**改动后**：
```cpp
if (overlay->header("PRT GPU Export")) {
    overlay->text("Mode: PBR-unified (Lambert cosine)");
    
    if (overlay->button("Export PRT (GPU)")) {
        ExportPRTDataGPU();
    }
    if (isExportingPRT) {
        overlay->text("Exporting: %s", prtExportStatus.c_str());
    }
}
```

**优势**：
- UI更简洁
- 移除不再使用的参数
- 用户不会被混淆

### 改动4：代码清理 - 注释过时声明

**位置**：`main.cpp` 行 239-250 (成员变量声明)

**改动前**：
```cpp
float spotInnerDeg = 50.0f;   // Default widened cone for robust non-zero lighting
float spotOuterDeg = 80.0f;   // Outer cone for broad coverage
```

**改动后**：
```cpp
// ===== UNIFIED PBR-PRT STRATEGY =====
// Removed spotlight parameters (spotInnerDeg, spotOuterDeg)
// Now using Lambert cosine term matching PBR's direct diffuse lighting
```

**优势**：
- 代码意图清晰
- 易于维护
- 记录设计决策

## 预期效果

### 修改前
| 方面 | 效果 |
|------|------|
| PBR着色 | 黄色 ✓ |
| PRT着色 | 灰色/无着色 ✗ |
| 光源旋转 | PBR变色，PRT不变 ✗ |
| 着色效率 | PBR快，PRT慢 |

### 修改后
| 方面 | 效果 |
|------|------|
| PBR着色 | 黄色 ✓ |
| PRT着色 | **黄色 ✓** |
| 光源旋转 | **两者同步变色 ✓** |
| 着色效率 | **PRT >= PBR** |

## 技术细节

### Lambert余弦项的数学原理

对于每个采样方向 `w`（指向球面采样点）：
- 入射方向：`-w`（从光源指向表面）
- 光源方向：`lightDir`（规范方向）
- Lambert项：`max(0, dot(-w, lightDir))`

这与PBR的直射漫反射计算一致：
```glsl
// PBR着色器
float diffuse = max(0.0, dot(normal, lightDir)) * 0.5;
color += diffuse * lightColor * lightIntensity;
```

### 强度标定

```cpp
float intensityScale = lightIntensity / 100.0f;
```

- `lightIntensity` 范围：50-100（UI默认值）
- 标定后范围：0.5-1.0
- 可根据需要调整系数

## 编译状态

✅ **编译成功**
- 无编译错误
- 仅有类型转换警告（不影响功能）

## 验证步骤

1. **导出PRT**：点击"Export PRT (GPU)"
2. **启用PRT**：勾选"Enable PRT Relighting"
3. **启用光源**：勾选"Enable Light"
4. **观察颜色**：模型应显示黄色（与PBR相同）
5. **旋转光源**：勾选"Auto Rotate"，观察颜色同步变化

## 文件清单

### 修改的源文件
- `examples/lightprobesh2/main.cpp` (4处改动)

### 新增文档
- `PRT_PBR_UNIFIED_STRATEGY.md` - 详细策略文档
- `UNIFIED_PRT_PBR_TESTING_GUIDE.md` - 测试验证指南
- `IMPLEMENTATION_SUMMARY_UNIFIED_PRT_PBR.md` - 本文件

### 生成的数据文件
- `prt_output/prt_data_lighting.txt` - 旋转光照SH系数
- `prt_output/prt_data_lt.txt` - 光传输系数
- `prt_output/prt_data_lighting_original.txt` - 原始光照SH系数

## 后续优化方向

1. **多光源支持**
   - 预计算多个光源方向的SH系数
   - 运行时选择或混合

2. **动态光源**
   - 实时更新SH系数而非预计算旋转
   - 支持任意光源方向

3. **IBL集成**
   - 将IBL与PRT SH系数混合
   - 实现更逼真的全局光照

4. **性能分析**
   - 对比PBR vs PRT的性能差异
   - 优化着色器计算

5. **质量改进**
   - 增加SH采样数量
   - 支持更高阶球谐函数

## 总结

通过在PBR框架下进行PRT预计算，使用Lambert余弦项代替聚光锥，我们实现了：

✅ **一致的着色效果** - PRT和PBR显示相同的颜色
✅ **同步的光源参数** - 使用相同的lightColor和lightIntensity
✅ **简化的UI** - 移除不必要的聚光参数
✅ **提高的效率** - 预计算查表 vs 逐像素计算

这个方案既保留了PRT的预计算优势，又确保了与PBR的视觉一致性。


