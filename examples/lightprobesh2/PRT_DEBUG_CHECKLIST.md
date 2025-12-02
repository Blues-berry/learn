# PRT 调试检查清单

## 编译阶段

- [ ] 使用compile.bat或VS开发者命令提示符编译
- [ ] 确保没有编译错误
- [ ] 检查是否有警告

## 预计算阶段

### 数据生成
- [ ] 采样方向生成成功（应该看到日志："Generated X sample directions"）
- [ ] LIGHT系数计算成功（应该看到："Lighting SH coefficients computed"）
- [ ] LT系数计算成功（应该看到："LT CPU computation complete"）
- [ ] 旋转系数计算成功（应该看到："Precomputing rotations"）

### 数据导出
- [ ] 检查 `prt_output/` 目录是否创建
- [ ] 检查 `prt_data_lighting.txt` 是否存在
  - 应该有24行（24个旋转角度）
  - 每行格式：`angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz`
- [ ] 检查 `prt_data_lt.txt` 是否存在
  - 应该有多行（每个顶点一行）
  - 每行格式：`coeff[0].xyz coeff[1].xyz ... coeff[8].xyz`

### 数据验证
- [ ] LIGHT系数不全为零
  - 第一个系数（l=0）应该最大
  - 其他系数应该相对较小
- [ ] LT系数不全为零
  - 应该反映表面法线和可见性
- [ ] 旋转系数随角度变化
  - 不同角度的系数应该不同

## 运行时阶段

### 数据加载
- [ ] PRTRenderer::Initialize 成功
  - 应该看到："Initialized successfully"
  - 应该看到："Loaded X rotations"
- [ ] 数据加载没有错误
  - 检查文件路径是否正确
  - 检查文件格式是否正确

### 着色计算
- [ ] 启用PRT渲染
- [ ] 检查着色结果
  - 应该看到Cornell模型被照亮
  - 颜色应该反映光源颜色和表面法线

### 旋转测试
- [ ] 旋转光源
  - 着色结果应该随之变化
  - 应该看到环境光效果
- [ ] 检查旋转是否平滑
  - 相邻角度的着色应该相似

## 调试输出位置

### 预计算阶段
```
[Step 1] Generating sample directions...
[Step 2] Precomputing Lighting...
[Step 3] Precomputing Light Transport...
[ExportPRTDataGPU] Precomputing rotations
[ExportPRTDataGPU] Export lighting rotations => OK
[ExportPRTDataGPU] Export light transport => OK
```

### 运行时阶段
```
[PRTRenderer] Initialized successfully
[PRTRenderer] Loaded X rotations
```

## 常见问题排查

### 问题：没有看到预计算日志
- [ ] 检查是否点击了"Export PRT Data"按钮
- [ ] 检查是否有其他错误信息

### 问题：导出文件为空或格式错误
- [ ] 检查数据是否正确计算
- [ ] 检查导出函数是否正确
- [ ] 查看文件内容是否有数据

### 问题：加载失败
- [ ] 检查文件路径是否正确
- [ ] 检查文件是否存在
- [ ] 检查文件格式是否正确

### 问题：着色结果全黑
- [ ] 检查LIGHT系数是否非零
- [ ] 检查LT系数是否非零
- [ ] 检查着色公式是否正确
- [ ] 检查albedo是否为零

### 问题：着色结果不变（旋转无效）
- [ ] 检查旋转系数是否正确计算
- [ ] 检查旋转角度是否正确更新
- [ ] 检查着色是否使用了当前的LIGHT系数

## 添加调试日志的位置

### SphericalHarmonics.cpp - ComputeShading
```cpp
std::cout << "[DEBUG] LIGHT coeff[0]: " << prtData.lighting.coeffs[0] << std::endl;
std::cout << "[DEBUG] LT coeff[0]: " << prtData.lightTransport.coeffs[0] << std::endl;
std::cout << "[DEBUG] Normal: " << normal << std::endl;
std::cout << "[DEBUG] Albedo: " << albedo << std::endl;
std::cout << "[DEBUG] Shading result: " << result << std::endl;
```

### main.cpp - 导出阶段
```cpp
std::cout << "[DEBUG] lightingSH.coeffs[0]: " << lightingSH.coeffs[0] << std::endl;
std::cout << "[DEBUG] ltCpuBatch.size(): " << ltCpuBatch.size() << std::endl;
for (int i = 0; i < std::min(3, (int)ltCpuBatch.size()); i++) {
    std::cout << "[DEBUG] ltCpuBatch[" << i << "].coeffs[0]: " 
              << ltCpuBatch[i].coeffs[0] << std::endl;
}
```

## 验证清单

完成以下检查以确保PRT系统正常工作：

- [ ] 编译成功
- [ ] 预计算数据生成
- [ ] 数据文件导出
- [ ] 数据文件格式正确
- [ ] 数据加载成功
- [ ] 着色结果非零
- [ ] 旋转有效果
- [ ] 结果与PBR相似

