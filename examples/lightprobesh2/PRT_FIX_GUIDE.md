# PRT 修复指南

## 核心问题诊断

### 问题1：着色公式错误（已修复）
**文件**：SphericalHarmonics.cpp:498-503
**问题**：albedo被乘了两次
**修复**：移除重复的albedo乘法

### 问题2：LT计算中的albedo处理
**当前状态**：
- main.cpp:1700 使用 `Project(visibilityFunc, directions)` 计算LT
- 这个LT只包含visibility，不包含albedo（正确！）
- 着色时在 `ComputeRelighting` 中应用albedo（正确！）

### 问题3：可能的根本原因 - 数据导出和加载

#### 导出阶段（main.cpp:1726）
```cpp
bool ok2 = DataExporter::ExportLightTransportBatch(base + "_lt.txt", ltCpuBatch);
```
这导出每个顶点的LT系数到 `prt_data_lt.txt`

#### 加载阶段（SphericalHarmonics.cpp:469-490）
```cpp
bool PRTRenderer::Initialize(PRTData& prtData,
                            const std::string& lightingFile,
                            const std::string& ltFile) {
    // 导入Lighting数据
    prtData.rotations = DataExporter::ImportLighting(lightingFile);
    // 导入Light Transport数据
    prtData.lightTransport = DataExporter::ImportLightTransport(ltFile);
    ...
}
```

**问题**：`ImportLightTransport` 只读取第一行！
- 这对于单个LT系数可以
- 但如果有多个顶点，需要不同的加载方式

### 问题4：着色时使用的LT系数

在运行时，着色使用的是 `prtData.lightTransport`（单个系数），而不是每个顶点的LT。

这可能是"不开启light时没有显示"的原因！

## 修复方案

### 方案A：简化版（适合测试）
1. 使用单个LT系数（整个场景共用）
2. 确保LT正确计算
3. 验证着色公式

### 方案B：完整版（生产级）
1. 为每个顶点存储LT系数
2. 在着色时查询对应顶点的LT
3. 支持per-vertex着色

## 建议的调试步骤

### 步骤1：验证编译
使用VS开发者命令提示符编译

### 步骤2：验证数据导出
检查 `prt_output/prt_data_lighting.txt` 和 `prt_data_lt.txt`
- lighting文件应该有24行（24个旋转角度）
- lt文件应该有多行（每个顶点一行）

### 步骤3：验证数据加载
添加日志输出确认数据正确加载

### 步骤4：验证着色
在着色前添加调试输出：
```cpp
std::cout << "LIGHT: " << prtData.lighting.coeffs[0] << std::endl;
std::cout << "LT: " << prtData.lightTransport.coeffs[0] << std::endl;
std::cout << "Shading result: " << result << std::endl;
```

## 关键代码修改

### 修改1：LT计算（已完成）
在main.cpp中使用visibility函数投影到SH基

### 修改2：着色公式（已完成）
移除重复的albedo乘法

### 修改3：数据加载（需要）
可能需要修改加载逻辑以支持per-vertex LT

