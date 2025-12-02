# PRT vs PBR 问题分析

## 问题现象

1. **PBR 光源颜色反向**
   - 设置光源颜色为绿色，但 PBR 显示红色
   - 着色逻辑是对的，只是颜色反向

2. **PRT 完全看不到着色变化**
   - 光源颜色对了（绿色显示绿色）
   - 但完全看不到着色逻辑的效果
   - 开启光源旋转后，Cornell Box 着色无变化

## 根本原因分析

### 问题 1: PBR 光源颜色反向

**根源**：`PreviewModel::SetLightColor()` 的实现错误

```cpp
// PreviewModel.cpp:330-339
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    // ❌ 错误：修改了材质的 albedo，而不是光源颜色
    materialData.elbedo = glm::vec4(color, 1.0f);
    materialData.roughness = 0.1f;
    materialData.metallic = 0.0f;
    materialData.specular = 1.0f;
    materialData.useLighting = 1;
    
    if (materialBuffer.mapped) {
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
        // ...
    }
}
```

**影响**：
- 当用户设置光源颜色为绿色时，PreviewModel 的材质 albedo 被设为绿色
- PBR 着色器计算：`color = albedo * lightColor * intensity * NdotL`
- 由于 albedo 被改为绿色，而实际光源还是白色，所以显示的是绿色
- 但用户期望的是光源是绿色，所以看起来是"反向"的

### 问题 2: PRT 完全看不到着色变化

**根源**：多个因素叠加

1. **SH 系数的初始值问题**
   - 在 `PrecomputePRT()` 中，光照 SH 系数被预计算为：
   ```cpp
   // main.cpp:1426
   for (int i = 0; i < shSamples; i++) {
       radiances.push_back(lightColor * lightIntensity);
   }
   SHCoefficients lightingCoeffs = PRTPrecomputer::PrecomputeLighting(directions, radiances);
   ```
   - 这些系数被存储在 `prtData` 中

2. **UpdatePRTLighting() 中的颜色应用**
   ```cpp
   // main.cpp:2040-2043
   float intensityScale = (lightIntensity / 100.0f) * 0.1f;
   for (int i = 0; i < 9; ++i) {
       currentSHCoefficients.coeffs[i] *= lightColor * intensityScale;
   }
   ```
   - 这里再次应用了光源颜色和强度

3. **问题**：
   - 预计算时已经应用了光源颜色
   - 运行时又应用了一次光源颜色
   - 这导致颜色被应用了两次（平方效应）
   - 而且当光源颜色改变时，预计算的数据不会改变
   - 只有运行时的乘法会改变，但这是在已经预计算的数据基础上

### 问题 3: 光源旋转无效

**根源**：
- `QueryCoefficients()` 从 `prtData` 中插值 SH 系数
- 但 `prtData` 是预计算的，只有 24 个离散旋转角度
- 当光源旋转时，`UpdatePRTLighting()` 确实会更新 UBO
- 但由于 SH 系数本身没有正确反映光照信息，所以看不到变化

## 解决方案

### 修复 1: 移除 SetLightColor 对 PreviewModel 的修改

不应该修改 PreviewModel 的材质。光源颜色应该只影响全局光照，而不是模型材质。

### 修复 2: 分离 PRT 的预计算和运行时更新

- **预计算阶段**：使用单位光源（白色，强度 1.0）预计算 SH 系数
- **运行时阶段**：只在 UpdatePRTLighting() 中应用光源颜色和强度

### 修复 3: 验证 PRT 着色器的 SH 重建

检查 `prt_relighting.frag` 中的 `ReconstructLighting()` 是否正确

## 实现步骤

1. 移除 `PreviewModel::SetLightColor()` 的实现
2. 修改 `PrecomputePRT()` 使用单位光源
3. 验证 `UpdatePRTLighting()` 的颜色应用逻辑
4. 测试 PRT 和 PBR 的一致性

