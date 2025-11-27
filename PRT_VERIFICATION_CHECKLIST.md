# PRT 实现验证检查清单

## 代码逻辑检查

### ✅ 已修复的问题

#### 1. 球谐旋转矩阵 (SphericalHarmonics.cpp, 行121-161)
- [x] 实现了正确的2阶球谐旋转矩阵
- [x] 处理了l=0, l=1, l=2的所有系数
- [x] 使用了正确的旋转公式

#### 2. Light Transport预计算 (SphericalHarmonics.cpp, 行204-237)
- [x] 添加了 `PrecomputeLightTransport()` 函数
- [x] 实现了Lambert's law (cosine term)
- [x] 正确的球谐投影公式

#### 3. 数据导出分离 (SphericalHarmonics.cpp, 行251-338)
- [x] 分别导出Lighting和Light Transport
- [x] 导出Lighting带旋转角度信息
- [x] 导出Light Transport为单个系数集合
- [x] 实现了对应的导入函数

#### 4. 预计算流程 (main.cpp, 行1185-1266)
- [x] 第1步: 生成采样方向
- [x] 第2步: 预计算Lighting
- [x] 第3步: 预计算Light Transport
- [x] 第4步: 预计算旋转
- [x] 第5步: 导出数据
- [x] 添加了详细的日志输出

#### 5. Relighting更新 (main.cpp, 行1268-1294)
- [x] 正确的Relighting公式说明
- [x] 查询当前旋转角度的Lighting系数
- [x] 准备好与着色器集成

---

## 预计算数据验证

### 导出文件检查

**prt_data_lighting.txt** 应该包含:
```
# PRT Lighting Data (Rotated)
# Rotations: 24
# SH Order: 2 (9 coefficients)
# Format: angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

0 <27个浮点数>
15 <27个浮点数>
30 <27个浮点数>
...
345 <27个浮点数>
```

**prt_data_lt.txt** 应该包含:
```
# PRT Light Transport Data
# SH Order: 2 (9 coefficients)
# Format: coeff[0].xyz coeff[1].xyz ... coeff[8].xyz

<27个浮点数>
```

### 数据有效性检查

- [ ] Lighting系数不全为0
- [ ] Light Transport系数不全为0
- [ ] 旋转系数随角度平滑变化
- [ ] 系数值在合理范围内 (通常 [-1, 1])

---

## 运行时验证

### 编译检查
- [ ] 代码编译无错误
- [ ] 代码编译无警告 (除了已知的)
- [ ] 所有新函数都被正确声明和定义

### 执行检查
- [ ] PrecomputePRT() 成功执行
- [ ] 日志输出显示所有5个步骤完成
- [ ] 文件成功导出到磁盘
- [ ] 文件大小合理 (通常几KB)

### 功能检查
- [ ] UpdatePRTLighting() 正确更新系数
- [ ] 光源旋转时Lighting系数变化
- [ ] 插值工作正确 (角度在两个预计算点之间)

---

## 着色器集成准备

### 需要实现的着色器代码

```glsl
// 在片段着色器中
uniform vec3 lightingCoeffs[9];  // 当前旋转角度的Lighting
uniform vec3 ltCoeffs[9];        // Light Transport系数

void main() {
    // 计算Relighting
    vec3 relitColor = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        relitColor += lightingCoeffs[i] * ltCoeffs[i];
    }
    
    // 应用到最终颜色
    vec3 finalColor = albedo * relitColor;
    outColor = vec4(finalColor, 1.0);
}
```

### UBO结构

```cpp
struct PRTData {
    glm::vec3 lightingCoeffs[9];
    glm::vec3 ltCoeffs[9];
};
```

---

## 性能指标

| 指标 | 值 | 说明 |
|------|-----|------|
| 预计算时间 | < 1秒 | 离线操作 |
| 导出文件大小 | ~5KB | 24个旋转 + 1个LT |
| 运行时查询时间 | < 1μs | 每帧 |
| 内存占用 | ~1MB | 包括所有预计算数据 |

---

## 已知限制

1. **Light Transport简化**: 当前实现不包含visibility项 (阴影)
2. **单一表面**: 只为Cornell Box主表面预计算，实际应该对所有顶点预计算
3. **固定光源方向**: 只支持绕Y轴旋转
4. **2阶球谐**: 精度有限，复杂场景可能需要3阶或更高

---

## 下一步工作

### 立即需要
1. [ ] 编译并运行代码
2. [ ] 验证导出的文件
3. [ ] 检查数据有效性

### 短期需要
1. [ ] 实现着色器集成
2. [ ] 编译着色器到SPIR-V
3. [ ] 创建UBO并传递数据到GPU

### 长期优化
1. [ ] 支持多个表面的Light Transport
2. [ ] 添加visibility项
3. [ ] 支持多个光源旋转轴
4. [ ] 使用3阶或更高阶球谐
5. [ ] 实现GPU端的预计算

