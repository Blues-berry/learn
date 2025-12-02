# PRT在PBR框架下的统一策略

## 核心问题分析

当前架构存在的问题：
1. **PBR和PRT使用不同的光源模型**
   - PBR：直射光源（点光源/方向光）+ IBL
   - PRT：聚光锥模型（spotlight）+ 旋转SH系数
   
2. **着色效果差异**
   - PBR：黄色着色（因为启用了直射光）
   - PRT：看不出着色效果（因为用的是聚光锥，与场景不匹配）

3. **预计算与运行时不一致**
   - 预计算时用聚光锥
   - 运行时用直射光源
   - 导致最终效果不匹配

## 改进方案：PRT在PBR框架下的统一实现

### 方案概述

**核心思想**：在PBR的直射光源框架下进行PRT预计算，使用**Lambert余弦项**代替聚光锥，这样：
- 预计算的光照分布与PBR的直射漫反射一致
- 运行时直接使用PBR的光源参数（lightColor, lightIntensity）
- 仅提高着色效率（从逐像素计算改为预计算查表）

### 具体实现步骤

#### 第1步：修改预计算阶段（ExportPRTDataGPU）

**目标**：用Lambert余弦项代替聚光锥

```cpp
// 旧代码（聚光锥）：
const float cosOuter = cosf(glm::radians(spotOuterDeg));
const float cosInner = cosf(glm::radians(spotInnerDeg));
auto smoothstep = [](float edge0, float edge1, float x) { ... };
for (const auto& w : directions) {
    float c = glm::dot(glm::normalize(-w), lightDir);
    float falloff = (c <= cosOuter) ? 0.0f : smoothstep(cosOuter, cosInner, c);
    radiances.push_back(glm::vec3(1.0f) * falloff);
}

// 新代码（Lambert余弦项）：
glm::vec3 lightDir = glm::vec3(0.0f, 0.0f, -1.0f); // canonical direction
for (const auto& w : directions) {
    // w是指向球面采样点的方向，取反得到入射方向
    float cosTerm = glm::max(0.0f, glm::dot(-w, lightDir));
    radiances.push_back(glm::vec3(cosTerm)); // 白光，颜色在运行时乘
}
```

**优势**：
- 与PBR的Lambert漫反射模型一致
- 预计算数据与光源方向无关（只需旋转）
- 运行时可直接应用lightColor和lightIntensity

#### 第2步：修改运行时更新（UpdatePRTLighting）

**目标**：应用PBR的光源参数到PRT SH系数

```cpp
void VulkanExample::UpdatePRTLighting()
{
    if (usePRTRelighting && prtReady && !prtData.empty() && lightingSHBuffer.mapped)
    {
        float angleDegrees = lightRotationAngle * 180.0f / PI;
        while (angleDegrees < 0.0f) angleDegrees += 360.0f;
        while (angleDegrees >= 360.0f) angleDegrees -= 360.0f;

        // 查询旋转后的SH系数
        currentSHCoefficients = Relighter::QueryCoefficients(angleDegrees, prtData);

        // ===== NEW: 应用PBR的光源参数 =====
        // 这样PRT和PBR使用相同的光源颜色和强度
        for (int i = 0; i < 9; ++i) {
            // 乘以lightColor和lightIntensity
            // 注意：lightIntensity通常是50-100范围，需要标定
            float intensityScale = lightIntensity / 100.0f; // 标定到0-1范围
            currentSHCoefficients.coeffs[i] *= lightColor * intensityScale;
        }

        // 打包到GPU UBO
        PRT::GPUSHCoefficients gpuLighting{};
        for (int i = 0; i < 9; ++i) {
            gpuLighting.coeffs[i] = glm::vec4(currentSHCoefficients.coeffs[i], 0.0f);
        }
        memcpy(lightingSHBuffer.mapped, &gpuLighting, sizeof(PRT::GPUSHCoefficients));
    }
}
```

#### 第3步：删除UI中的聚光参数

**目标**：简化UI，移除不再使用的spotInnerDeg和spotOuterDeg

在OnUpdateUIOverlay()中找到"PRT GPU Export"部分，删除：
```cpp
// 删除这两行：
overlay->sliderFloat("Spot Inner (deg)", &spotInnerDeg, 0.0f, 90.0f);
overlay->sliderFloat("Spot Outer (deg)", &spotOuterDeg, 0.0f, 90.0f);
```

#### 第4步：修改PRT着色器（可选优化）

**目标**：确保着色器正确应用SH系数

在prt_relight.frag中，确保：
```glsl
// 从UBO读取lighting SH系数（已包含颜色和强度）
vec3 color = vec3(0.0);
for (int i = 0; i < 9; ++i) {
    color += ltCoeffs[i].rgb * lightingSH.coeffs[i].rgb;
}
// 应用基础颜色
color *= baseColor.rgb;
// 输出（不需要再乘lightColor/lightIntensity）
outColor = vec4(color, 1.0);
```

### 预期效果

**修改前**：
- PBR：黄色着色（启用直射光）
- PRT：灰色/无着色（聚光锥与场景不匹配）

**修改后**：
- PBR：黄色着色（启用直射光）
- PRT：**黄色着色**（与PBR一致）
- 旋转光源时，PRT颜色随之变化（与PBR同步）

### 性能优势

1. **预计算阶段**：
   - 无需计算聚光锥的smoothstep
   - 简单的dot product和max操作

2. **运行时**：
   - 从逐像素Lambert计算 → 预计算查表
   - 节省大量着色器计算

3. **内存**：
   - 无变化（仍是9个vec4 SH系数）

### 实现检查清单

- [ ] 修改ExportPRTDataGPU()中的radiance计算（Lambert代替spotlight）
- [ ] 修改UpdatePRTLighting()应用lightColor和lightIntensity
- [ ] 删除UI中的spotInnerDeg和spotOuterDeg滑条
- [ ] 删除相关的成员变量声明（如果有）
- [ ] 重新导出PRT数据（Export PRT (GPU)按钮）
- [ ] 启用PRT Relighting并验证效果
- [ ] 旋转光源验证颜色同步

### 代码改动位置

| 文件 | 函数 | 行号范围 | 改动 |
|------|------|---------|------|
| main.cpp | ExportPRTDataGPU() | 1530-1565 | 修改radiance计算 |
| main.cpp | UpdatePRTLighting() | 2017-2083 | 应用lightColor/Intensity |
| main.cpp | OnUpdateUIOverlay() | ~1000-1100 | 删除spot滑条 |

### 验证步骤

1. **编译**：`powershell -ExecutionPolicy Bypass -File examples\lightprobesh2\compile.ps1`

2. **导出PRT**：
   - 点击"Export PRT (GPU)"
   - 控制台应显示："Lighting L00 after irradiance: (x, y, z)"

3. **启用PRT**：
   - 勾选"Enable PRT Relighting"
   - 模型应显示黄色着色（与PBR相同）

4. **旋转光源**：
   - 勾选"Auto Rotate"
   - PRT颜色应随光源旋转而变化

### 后续优化方向

1. **多光源支持**：预计算多个光源方向的SH系数
2. **动态光源**：实时更新SH系数而非预计算旋转
3. **IBL集成**：将IBL与PRT SH系数混合
4. **性能分析**：对比PBR vs PRT的性能差异


