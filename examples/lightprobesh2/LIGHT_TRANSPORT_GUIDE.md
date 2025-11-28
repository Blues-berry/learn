# GPU端Light Transport逐顶点计算详细指南

## 1. Light Transport理论

### 1.1 基本概念

Light Transport（光传输）是物体表面对入射光的响应函数。

**数学公式：**
```
T[i] = (4π / N) * Σ(j=0 to N-1) albedo * max(0, dot(normal, direction[j])) * Y_i(direction[j])
```

其中：
- `T[i]` : 第i个Light Transport系数
- `albedo` : 表面反射率
- `normal` : 表面法向量
- `direction[j]` : 第j个采样方向
- `Y_i` : 第i个球谐基函数

### 1.2 物理意义

- **Lambert反射：** max(0, dot(normal, direction))项
- **能量守恒：** albedo项确保反射率不超过1
- **方向相关性：** 不同方向的响应不同
- **预计算优势：** 可以离线计算，运行时快速查询

### 1.3 与Lighting的区别

| 特性 | Lighting | Light Transport |
|------|----------|-----------------|
| 输入 | 光源方向和强度 | 表面位置、法向量、反射率 |
| 输出 | 光源的球谐系数 | 表面对光的响应系数 |
| 计算时机 | 预计算一次 | 对每个顶点预计算 |
| 依赖关系 | 与表面无关 | 与表面相关 |
| 最终着色 | Lighting · LT | 点积 |

## 2. GPU端实现

### 2.1 当前实现（prt_compute_optimized.glsl）

**函数：** `ComputeLightTransport()`

**工作流程：**
1. 获取顶点的位置、法向量、反射率
2. 初始化9个输出系数为0
3. 遍历所有采样方向
4. 对每个方向计算Lambert项和基函数值
5. 累加到对应的系数中
6. 应用归一化因子

**代码分析：**
```glsl
void ComputeLightTransport()
{
    // 1. 获取工作项索引（对应顶点）
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= computeParams.numVertices) return;
    
    // 2. 计算归一化因子
    float normalization = (4.0 * PI) / float(computeParams.numSamples);
    
    // 3. 获取顶点数据
    LTInput ltInput = ltInputs[idx];
    vec3 normal = normalize(ltInput.normal.xyz);
    vec3 albedo = ltInput.albedo.xyz;
    
    // 4. 初始化输出系数
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i] = vec4(0.0);
    }
    
    // 5. 遍历所有采样方向
    for(uint s = 0; s < computeParams.numSamples; s++) {
        // 6. 获取采样方向
        vec3 direction = normalize(samples[s].direction.xyz);
        
        // 7. 计算Lambert项
        float cosTheta = max(0.0, dot(normal, direction));
        
        // 8. 计算所有基函数值
        float basis[9];
        EvaluateAllBasisOptimized(direction, basis);
        
        // 9. 累加到系数中
        vec3 contribution = albedo * cosTheta;
        for(int i = 0; i < 9; i++) {
            outputCoeffs[idx].coeffs[i].xyz += contribution * basis[i];
        }
    }
    
    // 10. 应用归一化因子
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i].xyz *= normalization;
    }
}
```

### 2.2 性能特性

**计算复杂度：**
- 时间复杂度：O(V * N * 9) = O(V * N)
- 空间复杂度：O(V)（输出缓冲区）

其中V是顶点数，N是采样数

**内存访问：**
- 读取：V个顶点数据 + N个采样点
- 写入：V个输出系数
- 带宽：中等（计算密集）

**并行性：**
- 高度并行
- 每个工作项处理一个顶点
- 无线程间依赖

**工作组配置：**
```
local_size_x = 256
numWorkGroups = ceil(numVertices / 256)
```

## 3. Cornell Box模型处理

### 3.1 模型加载

**步骤1：加载模型**
```cpp
// 使用glTF加载Cornell Box模型
auto cornellModel = vkglTF::Model::load(
    "cornell_box.gltf", vulkanDevice, queue);
```

**步骤2：提取顶点数据**
```cpp
std::vector<glm::vec3> positions;
std::vector<glm::vec3> normals;
std::vector<glm::vec3> albedos;

for(const auto& mesh : cornellModel->meshes) {
    for(const auto& vertex : mesh->vertices) {
        positions.push_back(vertex.position);
        normals.push_back(vertex.normal);
        albedos.push_back(vertex.color);  // 或从材质获取
    }
}
```

### 3.2 数据准备

**步骤1：创建GPU输入缓冲区**
```cpp
std::vector<GPULTInput> ltInputs(positions.size());
for(size_t i = 0; i < positions.size(); i++) {
    ltInputs[i].position = glm::vec4(positions[i], 0.0f);
    ltInputs[i].normal = glm::vec4(normals[i], 0.0f);
    ltInputs[i].albedo = glm::vec4(albedos[i], 0.0f);
}
```

**步骤2：上传到GPU**
```cpp
vks::Buffer ltInputBuffer;
ltInputBuffer.create(vulkanDevice, ltInputs.data(),
                     ltInputs.size() * sizeof(GPULTInput),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
```

### 3.3 批量计算

**步骤1：计算工作组数量**
```cpp
uint32_t numVertices = positions.size();
uint32_t workGroupSize = 256;
uint32_t numWorkGroups = (numVertices + workGroupSize - 1) / workGroupSize;
```

**步骤2：执行计算**
```cpp
// 设置计算参数
ComputeParams params;
params.numVertices = numVertices;
params.numSamples = 32;
params.computeMode = 1;  // Light Transport

// 执行计算
ExecuteComputeShader(numWorkGroups, 1, 1);
```

**步骤3：下载结果**
```cpp
std::vector<GPUSHCoefficients> ltCoefficients(numVertices);
outputBuffer.copyTo(ltCoefficients.data(),
                    ltCoefficients.size() * sizeof(GPUSHCoefficients));
```

## 4. 数据流程

### 4.1 完整流程

```
加载Cornell Box模型
    ↓
提取顶点数据（位置、法向量、反射率）
    ↓
生成采样方向（Fibonacci采样）
    ↓
创建GPU缓冲区
    ↓
上传数据到GPU
    ↓
执行Compute Shader
    ↓
下载结果到CPU
    ↓
保存到文件
    ↓
用于Relighting着色
```

### 4.2 内存布局

**GPU端缓冲区：**
```
SamplesBuffer (N * 32字节)
    ├─ direction[0] (vec4)
    ├─ radiance[0] (vec4)
    ├─ direction[1] (vec4)
    ├─ radiance[1] (vec4)
    └─ ...

LTInputBuffer (V * 48字节)
    ├─ position[0] (vec4)
    ├─ normal[0] (vec4)
    ├─ albedo[0] (vec4)
    ├─ position[1] (vec4)
    ├─ normal[1] (vec4)
    ├─ albedo[1] (vec4)
    └─ ...

OutputBuffer (V * 144字节)
    ├─ coeffs[0][0..8] (vec4 * 9)
    ├─ coeffs[1][0..8] (vec4 * 9)
    └─ ...
```

## 5. 验证方法

### 5.1 数值验证

**CPU端参考实现：**
```cpp
SHCoefficients CPULightTransport(
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& albedo,
    const std::vector<glm::vec3>& directions)
{
    return PRTPrecomputer::PrecomputeLightTransport(
        position, normal, albedo, directions);
}
```

**GPU端验证：**
```cpp
// 计算GPU结果
auto gpuResults = prtCompute->ComputeLightTransportBatch(
    positions, normals, albedos, directions);

// 计算CPU结果
std::vector<SHCoefficients> cpuResults;
for(size_t i = 0; i < positions.size(); i++) {
    cpuResults.push_back(CPULightTransport(
        positions[i], normals[i], albedos[i], directions));
}

// 比较
for(size_t i = 0; i < positions.size(); i++) {
    for(int j = 0; j < 9; j++) {
        float diff = glm::length(
            glm::vec3(gpuResults[i].coeffs[j]) -
            cpuResults[i].coeffs[j]);
        assert(diff < 1e-4);
    }
}
```

### 5.2 性能验证

**测试代码：**
```cpp
// 测试不同顶点数量
for(int numVertices : {100, 1000, 10000}) {
    auto positions = GenerateRandomPositions(numVertices);
    auto normals = GenerateRandomNormals(numVertices);
    auto albedos = GenerateRandomAlbedos(numVertices);
    auto directions = GenerateFibonacciSamples(32);
    
    auto start = std::chrono::high_resolution_clock::now();
    auto results = prtCompute->ComputeLightTransportBatch(
        positions, normals, albedos, directions);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<
        std::chrono::milliseconds>(end - start);
    
    std::cout << "Vertices: " << numVertices
              << ", Time: " << duration.count() << " ms" << std::endl;
}
```

**性能预期：**
- 100个顶点：< 1 ms
- 1000个顶点：< 5 ms
- 10000个顶点：< 50 ms

## 6. 优化建议

### 6.1 法向量优化

**问题：** 法向量可能未归一化

**解决方案：**
```glsl
vec3 normal = normalize(ltInput.normal.xyz);
```

### 6.2 反射率优化

**问题：** 反射率可能超过1

**解决方案：**
```glsl
vec3 albedo = clamp(ltInput.albedo.xyz, 0.0, 1.0);
```

### 6.3 采样优化

**问题：** 采样数量影响质量

**解决方案：**
- 使用自适应采样
- 根据法向量方向调整采样
- 使用重要性采样

## 7. 常见问题

### 7.1 Q: 为什么需要normalize(normal)？

**A:**
- 模型加载时法向量可能未归一化
- 球谐基函数定义在单位球面上
- 未归一化导致错误的Lambert项计算

### 7.2 Q: 为什么使用max(0, dot(normal, direction))？

**A:**
- Lambert反射模型
- 背面光照应该为0
- max(0, ...)确保非负

### 7.3 Q: 如何处理背面顶点？

**A:**
- 自动处理（Lambert项为0）
- 或在预处理时过滤
- 或使用双面法向量

### 7.4 Q: 能否使用不同的反射率？

**A:**
- 可以，每个顶点独立
- 支持纹理映射的反射率
- 需要上传纹理坐标和采样

## 8. 总结

### 8.1 关键点
- ✅ Light Transport是表面对光的响应
- ✅ GPU端实现支持批量顶点处理
- ✅ 需要正确的法向量和反射率
- ✅ 性能随顶点数线性增长

### 8.2 下一步
1. 编译shader为SPIR-V
2. 实现CPU-GPU数据传输
3. 集成到主程序
4. 进行性能测试

### 8.3 性能目标
- 单个顶点：< 100 μs
- 1000个顶点：< 100 ms
- 完整Cornell Box：< 50 ms

---

**文档创建时间：** 2025-11-28
**状态：** 完成

