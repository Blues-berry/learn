# 统一PRT-PBR测试和验证指南

## 编译状态
✅ **编译成功** - 所有代码改动已集成

## 修改总结

### 1. 预计算阶段改动 (ExportPRTDataGPU)
- **文件**: main.cpp (行 1528-1552)
- **改动**: 将聚光锥模型替换为Lambert余弦项
- **效果**: 预计算光照分布与PBR直射漫反射一致

### 2. 运行时更新改动 (UpdatePRTLighting)
- **文件**: main.cpp (行 2000-2074)
- **改动**: 应用lightColor和lightIntensity到SH系数
- **效果**: PRT使用与PBR相同的光源参数

### 3. UI改动 (OnUpdateUIOverlay)
- **文件**: main.cpp (行 1095-1107)
- **改动**: 删除spotInnerDeg和spotOuterDeg滑条
- **效果**: 简化UI，移除不再使用的参数

### 4. 成员变量改动
- **文件**: main.cpp (行 239-250)
- **改动**: 注释掉spotInnerDeg和spotOuterDeg声明
- **效果**: 清理代码，明确新策略

## 测试步骤

### 第1步：启动应用
```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\build\bin
.\lightprobesh2.exe
```

### 第2步：导出PRT数据（使用新的Lambert模型）
1. 在UI中找到 **"PRT GPU Export"** 部分
2. 验证显示 **"Mode: PBR-unified (Lambert cosine)"**
3. 点击 **"Export PRT (GPU)"** 按钮
4. 等待导出完成（控制台输出 "Done"）

**预期控制台输出**：
```
[ExportPRTDataGPU] Using Lambert cosine term (PBR-unified mode)
[ExportPRTDataGPU] Lighting L00 after irradiance: (x, y, z)
[ExportPRTDataGPU] Export lighting rotations => OK: ...
[ExportPRTDataGPU] Export light transport => OK: ...
```

### 第3步：启用PRT Relighting
1. 在UI中找到 **"PRT Relighting"** 部分
2. 勾选 **"Enable PRT Relighting"** 复选框
3. 等待资源加载完成

**预期控制台输出**：
```
[DEBUG PRT] --- Preparing PRT Relighting Resources --- 
[DEBUG PRT] LT count matches vertex count: 64
[DEBUG PRT] Created LT Coefficients SSBO: Handle=...
[DEBUG PRT] Created Lighting SH UBO: Handle=...
[DEBUG PRT] PRT Relighting resources prepared successfully.
```

### 第4步：验证着色效果
1. **启用直射光**：在"Light Source"中勾选"Enable Light"
2. **观察颜色**：模型应显示**黄色着色**（与PBR相同）
3. **对比效果**：
   - PBR (标准渲染): 黄色着色
   - PRT (预计算): **应该也是黄色着色**

**如果PRT仍然是灰色/无着色**：
- 检查lightIntensity值（应该在50-100范围）
- 检查lightColor是否为黄色
- 查看控制台是否有错误信息

### 第5步：验证光源旋转同步
1. 在"Light Source"中勾选"Auto Rotate"
2. **观察颜色变化**：
   - PBR和PRT的颜色应该**同时变化**
   - 当光源旋转时，黄色应该逐渐变淡或变深

**预期行为**：
- 光源旋转 → 两种渲染方式的颜色同步变化
- 说明PRT正确应用了lightColor和lightIntensity

### 第6步：验证性能提升
1. 打开性能监视器（如果有）
2. 对比PBR vs PRT的帧率
3. PRT应该有更好的性能（预计算查表 vs 逐像素计算）

## 验证检查清单

### 编译检查
- [x] 代码编译无错误
- [x] 仅有警告（类型转换），无阻塞性错误

### 功能检查
- [ ] PRT导出时显示"PBR-unified (Lambert cosine)"
- [ ] PRT导出成功完成
- [ ] PRT Relighting启用成功
- [ ] 模型显示黄色着色（与PBR一致）
- [ ] 光源旋转时颜色同步变化

### 性能检查
- [ ] PRT帧率 >= PBR帧率
- [ ] 无明显卡顿或闪烁

### 调试信息检查
- [ ] 控制台无ERROR或FAILED消息
- [ ] LT count与model vertices匹配
- [ ] SH系数有效（非NaN）

## 常见问题排查

### 问题1：PRT仍然是灰色
**原因**：lightIntensity可能为0或lightColor为黑色

**解决**：
1. 检查lightIntensity > 0
2. 检查lightColor不是黑色
3. 重新导出PRT数据

### 问题2：PRT和PBR颜色不同步
**原因**：UpdatePRTLighting可能未被调用

**解决**：
1. 检查usePRTRelighting是否为true
2. 检查prtReady是否为true
3. 查看控制台是否有相关错误

### 问题3：导出PRT失败
**原因**：模型顶点数与LT数据不匹配

**解决**：
1. 确保使用相同的模型
2. 删除旧的prt_output目录
3. 重新导出

### 问题4：性能没有提升
**原因**：可能仍在使用PBR渲染

**解决**：
1. 验证usePRTRelighting为true
2. 验证pipelinePRT != VK_NULL_HANDLE
3. 查看drawFrame中是否进入PRT分支

## 代码改动验证

### 验证改动1：Lambert余弦项
```cpp
// 应该看到这样的代码（main.cpp ~1545）：
float cosTerm = glm::max(0.0f, glm::dot(-w, lightDir));
radiances.push_back(glm::vec3(cosTerm));
```

### 验证改动2：应用lightColor/Intensity
```cpp
// 应该看到这样的代码（main.cpp ~2040）：
float intensityScale = lightIntensity / 100.0f;
for (int i = 0; i < 9; ++i) {
    currentSHCoefficients.coeffs[i] *= lightColor * intensityScale;
}
```

### 验证改动3：UI简化
```cpp
// 应该看到这样的代码（main.cpp ~1098）：
overlay->text("Mode: PBR-unified (Lambert cosine)");
// 不应该看到spotInnerDeg和spotOuterDeg滑条
```

## 预期结果对比

| 方面 | 修改前 | 修改后 |
|------|--------|--------|
| **PBR颜色** | 黄色 | 黄色 |
| **PRT颜色** | 灰色/无着色 | **黄色** |
| **光源旋转** | PBR变色，PRT不变 | **两者同步变色** |
| **UI复杂度** | 有spot参数 | **简化** |
| **预计算模型** | 聚光锥 | **Lambert余弦** |
| **性能** | PBR快，PRT慢 | **PRT >= PBR** |

## 后续优化建议

1. **亮度标定**：如果PRT仍然偏暗，调整intensityScale系数
2. **多光源**：预计算多个光源方向的SH系数
3. **IBL混合**：将IBL与PRT SH系数混合
4. **动态光源**：实时更新SH而非预计算旋转

## 文件清单

### 修改的文件
- `examples/lightprobesh2/main.cpp` (4处改动)

### 新增文档
- `examples/lightprobesh2/PRT_PBR_UNIFIED_STRATEGY.md` (策略文档)
- `examples/lightprobesh2/UNIFIED_PRT_PBR_TESTING_GUIDE.md` (本文件)

### 生成的PRT数据
- `prt_output/prt_data_lighting.txt` (旋转后的光照SH)
- `prt_output/prt_data_lt.txt` (光传输系数)
- `prt_output/prt_data_lighting_original.txt` (原始光照SH)


