# PRT 快速参考指南

## 核心概念 (3句话)

1. **Lighting (L)**: 光源的球谐系数 - 描述光源如何照亮场景
2. **Light Transport (LT)**: 物体表面的球谐系数 - 描述物体如何响应光照
3. **Relighting**: L × LT 的点积 - 计算最终的着色结果

---

## 预计算流程 (5步)

```
采样方向 → Lighting预计算 → LT预计算 → 旋转预计算 → 导出文件
```

### 代码位置
- **采样**: `SphericalHarmonics::GenerateFibonacciSamples()`
- **Lighting**: `PRTPrecomputer::PrecomputeLighting()`
- **LT**: `PRTPrecomputer::PrecomputeLightTransport()`
- **旋转**: `PRTPrecomputer::PrecomputeRotations()`
- **导出**: `DataExporter::ExportPRTData()`

---

## 运行时流程 (3步)

```
导入文件 → 查询Lighting → 计算Relighting
```

### 代码位置
- **导入**: `DataExporter::ImportLighting()` + `ImportLightTransport()`
- **查询**: `Relighter::QueryCoefficients()`
- **计算**: `Σ(Lighting[i] × LT[i])`

---

## 关键公式

### Lighting投影
```
L[i] = (4π/N) × Σ(radiance × basis[i])
```

### Light Transport投影
```
LT[i] = (4π/N) × Σ(albedo × max(0, dot(N, dir)) × basis[i])
```

### Relighting
```
color = Σ(L[i] × LT[i])
```

---

## 文件格式

### prt_data_lighting.txt
```
# 注释行
0 <27个浮点数>      # 角度0°
15 <27个浮点数>     # 角度15°
30 <27个浮点数>     # 角度30°
...
```

### prt_data_lt.txt
```
# 注释行
<27个浮点数>        # 单个Light Transport系数集合
```

---

## 修复的问题

| 问题 | 原因 | 修复 |
|------|------|------|
| 只有Lighting | 没有预计算LT | 添加PrecomputeLightTransport() |
| 旋转不工作 | 直接复制系数 | 实现正确的旋转矩阵 |
| 数据混乱 | 只导出一个文件 | 分别导出Lighting和LT |
| 不清楚流程 | 代码注释不足 | 添加详细日志和文档 |

---

## 调试技巧

### 检查Lighting系数
```cpp
std::cout << "Lighting[0]: " << lightingCoeffs.coeffs[0].x << std::endl;
```

### 检查Light Transport系数
```cpp
std::cout << "LT[0]: " << ltCoeffs.coeffs[0].x << std::endl;
```

### 检查旋转
```cpp
for (auto& rc : prtData) {
    std::cout << "Angle: " << rc.angle << std::endl;
}
```

### 检查Relighting结果
```cpp
glm::vec3 result = glm::vec3(0.0f);
for (int i = 0; i < 9; i++) {
    result += currentLighting.coeffs[i] * ltCoeffs.coeffs[i];
}
std::cout << "Relighting: " << result.x << ", " << result.y << ", " << result.z << std::endl;
```

---

## 性能优化

### 预计算阶段
- 增加采样数 (16→64) 提高精度
- 增加旋转数 (24→36) 提高平滑度
- 使用GPU加速 (可选)

### 运行时阶段
- 使用GPU纹理存储系数
- 使用着色器计算Relighting
- 缓存查询结果

---

## 常见问题

**Q: 为什么需要分别预计算Lighting和LT?**
A: 因为Lighting随光源旋转变化，而LT是固定的。分离可以减少存储和计算。

**Q: 为什么要预计算旋转?**
A: 因为球谐旋转计算复杂，预计算可以加速运行时查询。

**Q: 为什么使用2阶球谐?**
A: 2阶 (9个系数) 是精度和性能的平衡。更高阶更精确但更慢。

**Q: 如何支持多个光源?**
A: 为每个光源预计算Lighting，运行时求和。

**Q: 如何支持动态物体?**
A: 为每个顶点预计算LT，运行时查询。

---

## 下一步

1. **编译代码**
   ```bash
   cmake --build . --config Release --target lightprobesh2
   ```

2. **运行程序**
   ```bash
   ./bin/lightprobesh2.exe
   ```

3. **检查输出文件**
   ```bash
   ls -la prt_data_*.txt
   ```

4. **验证数据**
   - 打开文件检查格式
   - 检查数值范围
   - 检查旋转变化

5. **集成着色器**
   - 创建UBO存储系数
   - 在片段着色器中计算Relighting
   - 编译着色器到SPIR-V

---

## 参考资源

- **论文**: Sloan et al. "Precomputed Radiance Transfer for Real-Time Rendering"
- **教程**: GAMES202 Assignment 2
- **代码**: SphericalHarmonics.h/cpp, main.cpp
- **文档**: PRT_CORRECT_IMPLEMENTATION.md

---

## 版本历史

| 版本 | 日期 | 改动 |
|------|------|------|
| 1.0 | 2025-11-27 | 初始实现 (仅Lighting) |
| 2.0 | 2025-11-27 | 添加LT预计算和旋转矩阵修复 |
| 当前 | 2025-11-27 | 完整的PRT系统 |

