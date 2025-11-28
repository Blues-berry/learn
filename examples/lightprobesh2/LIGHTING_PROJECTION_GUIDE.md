# GPU端光照投影计算详细指南

## 1. 光照投影理论

### 1.1 基本概念

光照投影是将环境光照投影到球谐基函数空间的过程。

**数学公式：**
```
L[i] = (4π / N) * Σ(j=0 to N-1) radiance[j] * Y_i(direction[j])
```

其中：
- `L[i]` : 第i个球谐系数
- `N` : 采样点数量
- `radiance[j]` : 第j个采样方向的辐射度
- `Y_i` : 第i个球谐基函数
- `direction[j]` : 第j个采样方向

### 1.2 物理意义

- **球面积分：** 将球面上的光照函数分解为基函数的线性组合
- **压缩表示：** 用9个系数表示整个光照环境
- **快速重建：** 可以快速重建任意方向的光照值

### 1.3 采样策略

**Fibonacci球采样：**
- 均匀分布在球面上
- 避免极点聚集
- 计算高效

**采样数量建议：**
- 最小：16个采样
- 推荐：32个采样
- 高质量：64个采样

## 2. GPU端实现

### 2.1 当前实现（prt_compute_optimized.glsl）

**函数：** `ComputeLightingProjection()`

**工作流程：**
1. 初始化9个输出系数为0
2. 遍历所有采样方向
3. 对每个方向计算9个基函数值
4. 累加到对应的系数中
5. 应用归一化因子

**代码分析：**
```glsl
void ComputeLightingProjection()
{
    // 1. 获取工作项索引（只有1个输出）
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 1) return;
    
    // 2. 计算归一化因子
    float normalization = (4.0 * PI) / float(computeParams.numSamples);
    
    // 3. 初始化输出系数
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i] = vec4(0.0);
    }
    
    // 4. 遍历所有采样方向
    for(uint s = 0; s < computeParams.numSamples; s++) {
        // 5. 获取采样方向和辐射度
        vec3 direction = samples[s].direction.xyz;
        vec3 radiance = samples[s].radiance.xyz;
        
        // 6. 计算所有基函数值
        float basis[9];
        EvaluateAllBasisOptimized(direction, basis);
        
        // 7. 累加到系数中
        for(int i = 0; i < 9; i++) {
            outputCoeffs[idx].coeffs[i].xyz += radiance * basis[i];
        }
    }
    
    // 8. 应用归一化因子
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i].xyz *= normalization;
    }
}
```

### 2.2 性能特性

**计算复杂度：**
- 时间复杂度：O(N * 9) = O(N)
- 空间复杂度：O(1)（不计输入输出）

**内存访问：**
- 读取：N个采样点 + 参数
- 写入：1个输出系数
- 带宽：低（计算密集）

**并行性：**
- 高度并行
- 无线程间依赖
- 可充分利用GPU

## 3. 数据流程

### 3.1 CPU端准备

**步骤1：生成采样方向**
```cpp
std::vector<glm::vec3> directions = 
    SphericalHarmonics::GenerateFibonacciSamples(32);
```

**步骤2：准备辐射度**
```cpp
std::vector<glm::vec3> radiances;
for(int i = 0; i < 32; i++) {
    radiances.push_back(lightColor * lightIntensity);
}
```

**步骤3：创建GPU采样缓冲区**
```cpp
std::vector<GPUSample> gpuSamples(32);
for(int i = 0; i < 32; i++) {
    gpuSamples[i].direction = glm::vec4(directions[i], 0.0f);
    gpuSamples[i].radiance = glm::vec4(radiances[i], 0.0f);
}
```

### 3.2 GPU端计算

**步骤1：上传采样数据**
```cpp
// 创建并上传采样缓冲区
vks::Buffer samplesBuffer;
samplesBuffer.create(vulkanDevice, gpuSamples.data(), 
                     gpuSamples.size() * sizeof(GPUSample),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
```

**步骤2：创建输出缓冲区**
```cpp
// 创建输出系数缓冲区
vks::Buffer outputBuffer;
outputBuffer.create(vulkanDevice, nullptr,
                    sizeof(GPUSHCoefficients),
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT);
```

**步骤3：执行Compute Shader**
```cpp
// 设置计算参数
ComputeParams params;
params.numSamples = 32;
params.computeMode = 0;  // Lighting Projection

// 执行计算
ExecuteComputeShader(1, 1, 1);  // 1个工作组
```

### 3.3 CPU端回收

**步骤1：下载结果**
```cpp
// 从GPU下载结果
GPUSHCoefficients result;
outputBuffer.copyTo(&result, sizeof(GPUSHCoefficients));
```

**步骤2：转换为CPU格式**
```cpp
SHCoefficients cpuCoeffs;
for(int i = 0; i < 9; i++) {
    cpuCoeffs.coeffs[i] = glm::vec3(result.coeffs[i]);
}
```

**步骤3：保存到文件**
```cpp
DataExporter::ExportLighting("prt_lighting.txt", rotations);
```

## 4. 验证方法

### 4.1 数值验证

**CPU端参考实现：**
```cpp
SHCoefficients CPULightingProjection(
    const std::vector<glm::vec3>& directions,
    const std::vector<glm::vec3>& radiances)
{
    return SphericalHarmonics::ProjectLight(directions, radiances);
}
```

**GPU端验证：**
```cpp
// 计算GPU结果
auto gpuResult = prtCompute->ComputeLightingProjection(
    directions, radiances);

// 计算CPU结果
auto cpuResult = CPULightingProjection(directions, radiances);

// 比较
for(int i = 0; i < 9; i++) {
    float diff = glm::length(gpuResult.coeffs[i] - cpuResult.coeffs[i]);
    assert(diff < 1e-4);  // 允许浮点误差
}
```

### 4.2 性能验证

**测试代码：**
```cpp
// 测试不同采样数量
for(int numSamples : {16, 32, 64, 128}) {
    auto directions = GenerateFibonacciSamples(numSamples);
    auto radiances = GenerateRadiances(numSamples);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = prtCompute->ComputeLightingProjection(
        directions, radiances);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<
        std::chrono::microseconds>(end - start);
    
    std::cout << "Samples: " << numSamples 
              << ", Time: " << duration.count() << " μs" << std::endl;
}
```

**性能预期：**
- 16个采样：< 10 μs
- 32个采样：< 20 μs
- 64个采样：< 40 μs
- 128个采样：< 80 μs

## 5. 优化建议

### 5.1 采样优化

**问题：** 采样数量影响质量和性能

**解决方案：**
- 使用自适应采样
- 根据光照复杂度调整采样数量
- 预计算常用光照的采样

### 5.2 内存优化

**问题：** 采样缓冲区占用内存

**解决方案：**
- 使用共享内存缓存采样方向
- 压缩采样方向（使用oct编码）
- 动态生成采样方向

### 5.3 计算优化

**问题：** 基函数计算重复

**解决方案：**
- 使用向量化计算
- 预计算常数
- 使用SIMD指令

## 6. 常见问题

### 6.1 Q: 为什么需要归一化因子 (4π / N)？

**A:** 
- 球面积分的离散近似
- 4π是球面总面积
- N是采样点数量
- 确保投影的能量守恒

### 6.2 Q: 采样方向必须归一化吗？

**A:**
- 是的，必须归一化
- 球谐基函数定义在单位球面上
- 未归一化的方向会导致错误结果

### 6.3 Q: 辐射度可以是负数吗？

**A:**
- 理论上可以
- 实际应用中通常为非负
- 负值可能表示光的吸收

### 6.4 Q: 如何处理非均匀采样？

**A:**
- 需要使用重要性采样
- 每个采样点乘以对应的权重
- 权重 = 1 / (采样概率 * N)

## 7. 总结

### 7.1 关键点
- ✅ 光照投影是将光照投影到球谐空间
- ✅ GPU端实现高效且并行
- ✅ 需要正确的采样和归一化
- ✅ 验证很重要

### 7.2 下一步
1. 编译shader为SPIR-V
2. 实现CPU-GPU数据传输
3. 集成到主程序
4. 进行性能测试

### 7.3 性能目标
- 单次投影：< 50 μs
- 24次旋转：< 1.2 ms
- 完整预计算：< 10 ms

---

**文档创建时间：** 2025-11-28
**状态：** 完成

