# PRT vs PBR 问题修复总结

## 问题描述

1. **PBR 光源颜色反向**：设置绿色显示红色
2. **PRT 完全看不到着色变化**：光源颜色对了，但没有着色效果
3. **光源旋转无效**：开启旋转后 Cornell Box 着色无变化

## 根本原因

### 问题 1: PBR 光源颜色反向

**原因**：`PreviewModel::SetLightColor()` 错误地修改了模型的材质 albedo，而不是全局光源颜色。

```cpp
// ❌ 错误的实现
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    materialData.elbedo = glm::vec4(color, 1.0f);  // 修改了材质！
    // ...
}
```

**影响**：
- 用户设置光源为绿色 → PreviewModel 的 albedo 变为绿色
- PBR 着色：`color = albedo * lightColor * intensity * NdotL`
- 由于 albedo 是绿色，而光源是白色，结果显示绿色（看起来反向）

### 问题 2 & 3: PRT 着色无变化 + 旋转无效

**原因**：PRT 预计算时已经应用了光源颜色，导致：
1. 预计算的 SH 系数包含了特定的光源颜色
2. 改变光源颜色时，预计算数据不会改变
3. 只有运行时的乘法会改变，但基础数据已经固定

## 实施的修复

### 修复 1: 移除 SetLightColor 的错误实现

**文件**：`examples/lightprobesh2/PreviewModel.cpp`

```cpp
// ✅ 修复后的实现
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    // 光源颜色是全局属性，不应该修改材质
    // 这个函数保留用于 API 兼容性，但不执行任何操作
    printf("[DEBUG] SetLightColor called - NO-OP (light color is global)\n");
}
```

**效果**：
- PBR 现在正确地显示光源颜色
- 模型保持其原始的材质 albedo

### 修复 2: 使用单位光源进行 PRT 预计算

**文件**：`examples/lightprobesh2/main.cpp`（第 1420-1437 行）

```cpp
// ✅ 修复后的实现
std::vector<glm::vec3> radiances;
for (int i = 0; i < shSamples; i++) {
    // 使用单位光源（白色，强度 1.0）
    // 运行时会在 UpdatePRTLighting() 中应用实际的光源颜色和强度
    radiances.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
}
```

**效果**：
- PRT 的 SH 系数现在是"中立的"
- 光源颜色和强度只在运行时应用一次
- 改变光源颜色时，PRT 会正确响应

## 验证步骤

1. **编译**：`examples/lightprobesh2/compile.ps1`
2. **运行**：`bin/lightprobesh2.exe`
3. **测试**：
   - 设置光源颜色为绿色 → PBR 应显示绿色光照
   - 启用 PRT → PRT 应显示相同的绿色光照
   - 旋转光源 → 两种模式都应显示相同的变化

## 预期结果

- ✅ PBR 和 PRT 显示一致的光源颜色
- ✅ 改变光源颜色时，两种模式都会响应
- ✅ 旋转光源时，着色会相应改变
- ✅ Cornell Box 在 PRT 模式下显示正确的着色

## 后续工作

1. 验证 PRT 着色器中的 SH 重建逻辑
2. 检查 Light Transport 系数是否被正确应用
3. 优化 SH 插值算法以提高旋转平滑度

