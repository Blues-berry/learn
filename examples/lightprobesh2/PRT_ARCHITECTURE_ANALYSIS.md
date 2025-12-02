# PRT 架构分析与问题诊断

## 你的问题描述

1. **预计算阶段应该包含三项内容**：
   - LIGHT：光源的球谐系数
   - LIGHT TRANSPORT：光传输系数（每个顶点）
   - 光源旋转情况下的系数

2. **当前问题**：
   - 不开启 enable light 时，Cornell模型没有显示初始的渲染结果
   - 这表明基础的PRT渲染可能没有正确实现

## 架构分析

### 正确的PRT流程

```
预计算阶段：
  1. 生成采样方向 (Fibonacci球采样)
  2. 计算LIGHT系数 (光源投影到SH基)
  3. 计算LT系数 (每个顶点的光传输)
  4. 预计算旋转系数 (24个旋转角度)
  5. 导出数据 (lighting_*.txt, lt_*.txt)

运行时阶段：
  1. 加载预计算数据
  2. 根据当前旋转角度查询LIGHT系数
  3. 对每个顶点计算着色：
     shading = albedo * dot(LIGHT_SH, LT_SH)
```

### 当前实现问题

#### 问题1：缺少 `Project()` 模板方法
- **位置**：main.cpp:1700
- **原因**：需要投影任意函数到SH基
- **修复**：已添加模板方法到SphericalHarmonics.h

#### 问题2：编译环境问题
- **错误**：无法找到 vcruntime.h 和 cmath
- **原因**：MSVC编译器环境变量未正确设置
- **解决**：需要从VS开发者命令提示符编译

#### 问题3：PRT渲染逻辑问题
- **症状**：不开启light时没有显示
- **可能原因**：
  1. LT系数计算不正确
  2. 着色公式错误
  3. 数据加载失败

### 着色公式分析

当前实现（SphericalHarmonics.cpp:501）：
```cpp
glm::vec3 ComputeShading(const PRTData& prtData,
                         const glm::vec3& normal,
                         const glm::vec3& albedo) {
    return Relighter::ComputeRelighting(prtData.lighting, normal, albedo) * albedo;
}
```

这里有问题：
1. `ComputeRelighting` 已经乘以了 albedo（第416行）
2. 再乘一次 albedo 会导致 albedo^2，不正确

### 建议的修复方案

#### 1. 修复着色公式
```cpp
// 应该是：
glm::vec3 shading = Relighter::ComputeRelighting(prtData.lighting, normal, albedo);
// 不应该再乘 albedo
```

#### 2. 验证LT计算
LT应该是：
```
LT(dir) = max(0, dot(normal, dir)) * visibility(dir)
```

#### 3. 验证着色结果
最终着色应该是：
```
result = albedo * dot(LIGHT_SH, LT_SH)
```

## 编译问题解决步骤

1. 打开 VS 2022 开发者命令提示符
2. 导航到项目目录
3. 运行：`cmake --build build --config Debug --target lightprobesh2 -j 4`

或者在VS Code中配置任务使用正确的编译器环境。

