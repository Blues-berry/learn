# PRT 综合分析与修复方案

## 你的问题重新表述

你想实现一个完整的PRT系统，包括：
1. **预计算阶段**：
   - LIGHT：光源的球谐系数
   - LIGHT TRANSPORT：场景的光传输系数
   - 光源旋转系数：预计算24个旋转角度的LIGHT系数

2. **运行时阶段**：
   - 根据光源旋转角度查询对应的LIGHT系数
   - 使用LT系数和LIGHT系数计算着色
   - 旋转光源时，Cornell模型应该显示环境光效果

3. **当前问题**：
   - 不开启light时，Cornell模型没有显示初始渲染结果

## 根本原因分析

### 原因1：编译错误（已修复）
- **错误**：`SphericalHarmonics::Project()` 方法不存在
- **修复**：添加了模板方法到SphericalHarmonics.h

### 原因2：着色公式错误（已修复）
- **错误**：albedo被乘了两次
- **位置**：SphericalHarmonics.cpp:501
- **修复**：移除重复的albedo乘法

### 原因3：编译环境问题（需要手动处理）
- **错误**：MSVC找不到vcruntime.h和cmath
- **原因**：编译器环境变量未正确设置
- **解决**：使用compile.bat脚本（已创建）

### 原因4：可能的数据流问题（需要验证）

#### 数据流：
```
预计算：
  1. 生成采样方向 (Fibonacci)
  2. 计算LIGHT系数 (ProjectLight)
  3. 计算LT系数 (Project with visibility)
  4. 预计算旋转 (24个角度)
  5. 导出数据 (txt文件)

运行时：
  1. 加载数据 (ImportLighting, ImportLightTransport)
  2. 初始化PRT数据
  3. 根据旋转角度查询LIGHT系数
  4. 计算着色：
     lighting = ReconstructLight(LIGHT_SH, normal)
     shading = albedo * lighting
```

#### 可能的问题点：
1. **LT数据加载**：ImportLightTransport只读第一行
   - 如果有多个顶点的LT，只有第一个被加载
   - 其他顶点使用默认的零系数

2. **着色时使用的LT**：
   - 当前使用 `prtData.lightTransport`（单个系数）
   - 应该使用每个顶点对应的LT系数

3. **数据导出格式**：
   - lighting文件：24行（每行一个旋转角度）
   - lt文件：多行（每行一个顶点的LT系数）

## 修复步骤

### 步骤1：编译（必须）
```bash
# 方法1：使用compile.bat
examples\lightprobesh2\compile.bat

# 方法2：手动使用VS开发者命令提示符
"C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
cd c:\Users\Bluesky\Desktop\SKY\Learn\Vulkan
cmake --build build --config Debug --target lightprobesh2 -j 4
```

### 步骤2：验证数据导出
运行程序并导出PRT数据，检查：
- `prt_output/prt_data_lighting.txt`：应该有24行
- `prt_output/prt_data_lt.txt`：应该有多行（顶点数）

### 步骤3：验证数据加载
添加日志确认数据正确加载

### 步骤4：验证着色
- 检查LIGHT系数是否非零
- 检查LT系数是否非零
- 检查最终着色结果

## 关键代码位置

| 文件 | 位置 | 功能 |
|------|------|------|
| SphericalHarmonics.h | 50-76 | Project模板方法 |
| SphericalHarmonics.cpp | 498-503 | ComputeShading修复 |
| main.cpp | 1700 | LT计算 |
| main.cpp | 1726 | LT导出 |
| SphericalHarmonics.cpp | 469-490 | 数据加载 |

## 下一步行动

1. **立即**：使用compile.bat编译
2. **验证**：检查导出的数据文件
3. **调试**：添加日志输出追踪数据流
4. **优化**：如果需要per-vertex LT，修改数据加载逻辑

