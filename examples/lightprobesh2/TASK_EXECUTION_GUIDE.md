# 任务执行指南

## 📋 任务列表总览

### 第一阶段：GPU端PRT预计算实现 (10个任务)

```
[1.1] 分析Cornell Box和PreviewModel的完整架构
      ↓
[1.2] 完成PRTComputeShader的GPU Pipeline实现
      ↓
[1.3] 实现光照投影计算的GPU版本
      ↓
[1.4] 实现Light Transport计算的GPU版本（逐顶点）
      ↓
[1.5] 实现球谐旋转计算的GPU版本
      ↓
[1.6] 创建GPU端数据上传和下载机制
      ↓
[1.7] 集成GPU预计算到主程序（main.cpp）
      ↓
[1.8] 实现预计算数据导出为TXT文件
      ↓
[1.9] 测试GPU预计算结果的正确性
      ↓
[1.10] 编写GPU端预计算实现总结文档
```

### 第二阶段：UI读取和应用预计算数据进行Relighting (7个任务)

```
[2.1] 分析现有UI系统和PreviewModel的渲染管道
      ↓
[2.2] 实现TXT文件读取功能
      ↓
[2.3] 创建Relighting着色器（prt_relighting.frag）
      ↓
[2.4] 实现Relighting数据应用管道
      ↓
[2.5] 实现光照旋转交互UI
      ↓
[2.6] 测试完整的relighting流程
      ↓
[2.7] 编写第二阶段总结文档和用户指南
```

### 第三阶段：优化和验证 (3个任务)

```
[3.1] 性能优化和基准测试
      ↓
[3.2] 数值精度验证
      ↓
[3.3] 完整的项目文档和示例
```

---

## 🔍 任务详细说明

### 任务 1.1：分析Cornell Box和PreviewModel的完整架构

**目标：** 深入理解现有代码结构，为后续实现奠定基础

**需要分析的文件：**

1. **主程序和场景管理**
   - `main.cpp` - 查看Cornell Box的初始化和渲染流程
   - `GltfScene.h/cpp` - 场景管理
   - `gltfload.h/cpp` - glTF模型加载

2. **模型渲染**
   - `PreviewModel.h/cpp` - 模型渲染类
   - `Pass.h/cpp` - 渲染通道定义
   - 着色器：`gltfmesh_main.vert/frag`

3. **光照系统**
   - `LightProbe.h/cpp` - 光照探针
   - `Skybox.h/cpp` - 天空盒

4. **着色器**
   - `prt_relighting.vert/frag` - Relighting着色器
   - `gltfmesh_main.vert/frag` - 主着色器

**分析要点：**
- Cornell Box模型的顶点结构 (位置、法向量、纹理坐标)
- 光照设置方式
- 渲染管道的执行流程
- PreviewModel如何管理材质和渲染状态
- 如何添加新的Technique

**交付物：**
- 创建 `ARCHITECTURE_ANALYSIS.md` 文档
- 记录关键类和函数
- 绘制数据流程图

**时间估计：** 2-3小时

---

### 任务 1.2：完成PRTComputeShader的GPU Pipeline实现

**目标：** 实现完整的Vulkan Compute Pipeline

**需要实现的方法：**

```cpp
// 1. 着色器加载
bool LoadComputeShader();

// 2. 描述符集布局
bool CreateDescriptorSetLayout();

// 3. 描述符池
bool CreateDescriptorPool();

// 4. 计算管道
bool CreateComputePipeline();

// 5. 缓冲区
bool CreateBuffers();

// 6. 描述符集
bool CreateDescriptorSet();
bool UpdateDescriptorSet(...);

// 7. 执行
bool ExecuteComputeShader(...);
```

**关键实现细节：**

1. **LoadComputeShader()**
   - 读取 `sh_compute.spv` 文件
   - 创建 `VkShaderModule`

2. **CreateDescriptorSetLayout()**
   - 5个binding:
     - Binding 0: Samples Buffer (SSBO)
     - Binding 1: Input Coefficients (SSBO)
     - Binding 2: Output Coefficients (SSBO)
     - Binding 3: LT Input Buffer (SSBO)
     - Binding 4: Rotation Params (UBO)

3. **CreateBuffers()**
   - 计算缓冲区大小
   - 创建SSBO和UBO
   - 设置内存对齐

4. **ExecuteComputeShader()**
   - 创建命令缓冲区
   - 绑定Pipeline和DescriptorSet
   - 分发工作组
   - 等待完成

**参考资源：**
- `GPU_COMPUTE_IMPLEMENTATION_PLAN.md` - 详细步骤
- Vulkan官方文档 - Compute Pipeline

**时间估计：** 4-6小时

---

### 任务 1.3：实现光照投影计算的GPU版本

**目标：** 将环境光照投影到球谐空间

**方法签名：**
```cpp
bool ComputeLightingProjection(
    const std::vector<glm::vec3>& directions,
    const std::vector<glm::vec3>& radiances,
    GPUSHCoefficients& outputCoeffs
);
```

**实现步骤：**

1. 创建采样数据缓冲区
   ```cpp
   std::vector<GPUSample> samples;
   for (int i = 0; i < directions.size(); i++) {
       samples[i].direction = glm::vec4(directions[i], 0.0f);
       samples[i].radiance = glm::vec4(radiances[i], 0.0f);
   }
   ```

2. 上传到GPU
   ```cpp
   UploadToGPU(samplesBuffer, samples);
   ```

3. 执行计算着色器
   ```cpp
   ExecuteComputeShader(1, 1, 1);  // 单个工作组
   ```

4. 下载结果
   ```cpp
   DownloadFromGPU(outputCoefficientsBuffer, outputCoeffs);
   ```

**着色器代码参考：** `sh_compute.comp`

**时间估计：** 2-3小时

---

### 任务 1.4：实现Light Transport计算的GPU版本（逐顶点）

**目标：** 计算Cornell Box每个顶点对入射光的响应

**方法签名：**
```cpp
bool ComputeLightTransportSingle(
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& albedo,
    const std::vector<glm::vec3>& directions,
    GPUSHCoefficients& outputCoeffs
);

bool ComputeLightTransportBatch(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec3>& albedos,
    const std::vector<glm::vec3>& directions,
    std::vector<GPUSHCoefficients>& outputCoeffsBatch
);
```

**实现步骤：**

1. 准备顶点数据
   ```cpp
   std::vector<GPULTInput> ltInputs;
   for (int i = 0; i < positions.size(); i++) {
       ltInputs[i].position = glm::vec4(positions[i], 0.0f);
       ltInputs[i].normal = glm::vec4(normals[i], 0.0f);
       ltInputs[i].albedo = glm::vec4(albedos[i], 0.0f);
   }
   ```

2. 上传数据
   ```cpp
   UploadToGPU(ltInputBuffer, ltInputs);
   UploadToGPU(samplesBuffer, samples);
   ```

3. 执行计算着色器
   ```cpp
   // 每个顶点一个工作组
   ExecuteComputeShader(positions.size(), 1, 1);
   ```

4. 下载结果
   ```cpp
   DownloadFromGPU(outputCoefficientsBuffer, outputCoeffsBatch);
   ```

**关键优化：**
- 批量处理多个顶点
- 工作组大小优化
- 内存访问模式优化

**时间估计：** 3-4小时

---

### 任务 1.5：实现球谐旋转计算的GPU版本

**目标：** 预计算不同旋转角度下的光照系数

**方法签名：**
```cpp
bool ComputeRotatedSHCoefficients(
    const GPUSHCoefficients& inputCoeffs,
    float angleRadians,
    GPUSHCoefficients& outputCoeffs
);

bool ComputeMultipleRotations(
    const GPUSHCoefficients& inputCoeffs,
    int numRotations,
    float maxAngleDegrees,
    std::vector<GPUSHCoefficients>& outputCoeffsBatch
);
```

**实现步骤：**

1. 上传输入系数
   ```cpp
   UploadToGPU(inputCoefficientsBuffer, inputCoeffs);
   ```

2. 设置旋转参数
   ```cpp
   GPURotationParam param;
   param.angleRadians = angleRadians;
   UploadToGPU(rotationParamBuffer, param);
   ```

3. 执行计算着色器
   ```cpp
   ExecuteComputeShader(1, 1, 1);
   ```

4. 下载结果
   ```cpp
   DownloadFromGPU(outputCoefficientsBuffer, outputCoeffs);
   ```

**批量处理：**
- 计算多个旋转角度 (如24个45度旋转)
- 使用工作组并行处理

**时间估计：** 2-3小时

---

### 任务 1.6：创建GPU端数据上传和下载机制

**目标：** 实现高效的CPU-GPU数据传输

**需要实现的函数：**

```cpp
// 上传数据到GPU
template<typename T>
bool UploadToGPU(vks::Buffer& buffer, const std::vector<T>& data);

// 从GPU下载数据
template<typename T>
bool DownloadFromGPU(const vks::Buffer& buffer, std::vector<T>& data);

// 单个数据上传
template<typename T>
bool UploadToGPU(vks::Buffer& buffer, const T& data);

// 单个数据下载
template<typename T>
bool DownloadFromGPU(const vks::Buffer& buffer, T& data);
```

**关键考虑：**

1. **内存对齐**
   - 16字节对齐 (vec4)
   - 使用 `std::aligned_storage`

2. **同步问题**
   - 等待GPU完成
   - 使用 `vkDeviceWaitIdle()` 或 fence

3. **缓冲区类型**
   - SSBO: `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT`
   - UBO: `VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT`
   - 暂存缓冲区: `VK_BUFFER_USAGE_TRANSFER_SRC_BIT`

**时间估计：** 2-3小时

---

### 任务 1.7：集成GPU预计算到主程序（main.cpp）

**目标：** 在主程序中使用GPU预计算

**实现步骤：**

1. 创建PRTComputeShader实例
   ```cpp
   PRTComputeShader* prtCompute = new PRTComputeShader(vulkanDevice);
   prtCompute->Initialize();
   ```

2. 在适当时机调用预计算
   ```cpp
   // 程序启动时
   ComputeLightingProjection(...);
   
   // 加载模型时
   ComputeLightTransportBatch(...);
   
   // 预计算旋转
   ComputeMultipleRotations(...);
   ```

3. 处理结果
   ```cpp
   // 导出结果
   ExportLighting(...);
   ExportLightTransport(...);
   ```

**时间估计：** 1-2小时

---

### 任务 1.8：实现预计算数据导出为TXT文件

**目标：** 将GPU计算结果导出为可读的文本文件

**导出格式示例：**

```
# Lighting Coefficients
LIGHTING_COEFFICIENTS
9
0.5 0.5 0.5
0.3 0.3 0.3
...

# Light Transport Coefficients
LIGHT_TRANSPORT_COEFFICIENTS
9
0.8 0.8 0.8
...

# Rotations
ROTATIONS
24
0.0 0.5 0.5 0.5 ...
45.0 0.48 0.48 0.48 ...
...
```

**实现步骤：**

1. 创建导出函数
   ```cpp
   bool ExportLighting(const std::string& filename,
                      const std::vector<GPUSHCoefficients>& data);
   ```

2. 格式化数据
   ```cpp
   std::stringstream ss;
   ss << "LIGHTING_COEFFICIENTS\n";
   ss << "9\n";
   for (int i = 0; i < 9; i++) {
       ss << coeffs[i].x << " " << coeffs[i].y << " " << coeffs[i].z << "\n";
   }
   ```

3. 写入文件
   ```cpp
   std::ofstream file(filename);
   file << ss.str();
   file.close();
   ```

**时间估计：** 1-2小时

---

### 任务 1.9：测试GPU预计算结果的正确性

**目标：** 验证GPU计算的准确性和性能

**测试内容：**

1. **数值对比**
   ```cpp
   // 对比GPU和CPU结果
   float maxError = 0.0f;
   for (int i = 0; i < 9; i++) {
       float error = glm::distance(gpuResult[i], cpuResult[i]);
       maxError = std::max(maxError, error);
   }
   assert(maxError < 0.001f);  // 允许0.1%误差
   ```

2. **文件读写测试**
   ```cpp
   // 导出文件
   ExportLighting("test_lighting.txt", gpuData);
   
   // 读取文件
   auto importedData = ImportLighting("test_lighting.txt");
   
   // 验证数据
   assert(importedData.size() == gpuData.size());
   ```

3. **性能测试**
   ```cpp
   auto start = std::chrono::high_resolution_clock::now();
   ComputeLightingProjection(...);
   auto end = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
   std::cout << "GPU time: " << duration.count() << " ms\n";
   ```

**时间估计：** 2-3小时

---

### 任务 1.10：编写GPU端预计算实现总结文档

**目标：** 记录实现细节供后续参考

**文档内容：**

1. **实现概述**
   - 系统架构
   - 关键组件
   - 数据流程

2. **遇到的问题和解决方案**
   - 内存对齐问题
   - 同步问题
   - 性能瓶颈

3. **性能优化技巧**
   - 工作组大小选择
   - 内存访问优化
   - 缓冲区复用

4. **性能数据对比**
   - CPU vs GPU时间
   - 加速比
   - 内存使用

5. **使用指南**
   - API文档
   - 使用示例
   - 常见问题

**时间估计：** 1-2小时

---

## 📊 任务优先级和依赖关系

### 优先级
- 🔴 **关键** (必须完成): 1.1-1.9
- 🟡 **重要** (应该完成): 1.10, 2.1-2.7
- 🟢 **优化** (可选): 3.1-3.3

### 依赖关系
```
1.1 (分析) → 1.2 (Pipeline) → 1.3-1.5 (计算) → 1.6 (数据传输) 
           → 1.7 (集成) → 1.8 (导出) → 1.9 (测试) → 1.10 (文档)

1.9 (测试) → 2.1 (分析UI) → 2.2 (读取) → 2.3 (着色器) 
           → 2.4 (应用) → 2.5 (交互) → 2.6 (测试) → 2.7 (文档)

2.6 (测试) → 3.1 (优化) → 3.2 (验证) → 3.3 (文档)
```

---

## ⏱️ 时间估计

| 任务 | 估计时间 | 实际时间 | 状态 |
|------|---------|---------|------|
| 1.1 | 2-3h | - | ⏳ |
| 1.2 | 4-6h | - | ⏳ |
| 1.3 | 2-3h | - | ⏳ |
| 1.4 | 3-4h | - | ⏳ |
| 1.5 | 2-3h | - | ⏳ |
| 1.6 | 2-3h | - | ⏳ |
| 1.7 | 1-2h | - | ⏳ |
| 1.8 | 1-2h | - | ⏳ |
| 1.9 | 2-3h | - | ⏳ |
| 1.10 | 1-2h | - | ⏳ |
| **第一阶段总计** | **23-33h** | - | - |
| 2.1 | 2-3h | - | ⏳ |
| 2.2 | 1-2h | - | ⏳ |
| 2.3 | 2-3h | - | ⏳ |
| 2.4 | 2-3h | - | ⏳ |
| 2.5 | 1-2h | - | ⏳ |
| 2.6 | 2-3h | - | ⏳ |
| 2.7 | 1-2h | - | ⏳ |
| **第二阶段总计** | **12-18h** | - | - |
| 3.1 | 2-3h | - | ⏳ |
| 3.2 | 1-2h | - | ⏳ |
| 3.3 | 2-3h | - | ⏳ |
| **第三阶段总计** | **5-8h** | - | - |
| **总计** | **40-59h** | - | - |

---

## 🎯 成功标准

### 第一阶段
- [ ] 所有10个任务完成
- [ ] GPU Pipeline正常工作
- [ ] TXT文件导出成功
- [ ] 数值精度验证通过
- [ ] 性能提升 > 50x

### 第二阶段
- [ ] 所有7个任务完成
- [ ] TXT文件读取成功
- [ ] Relighting着色器工作正常
- [ ] UI交互流畅
- [ ] 帧率 > 60 FPS

### 第三阶段
- [ ] 所有3个任务完成
- [ ] 性能优化完成
- [ ] 数值精度确认
- [ ] 文档完整

---

**最后更新：** 2025-11-28

