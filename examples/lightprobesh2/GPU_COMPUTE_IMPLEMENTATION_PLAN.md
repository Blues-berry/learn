# GPU端PRT计算实现计划

## 1. 概述

本文档详细说明GPU端PRT计算的实现计划，包括Vulkan compute pipeline的创建、shader的编写、以及数据传输机制。

## 2. 已创建的文件

### 2.1 头文件：PRTComputeShader.h
- 定义了GPU端数据结构
- 定义了PRTComputeShader类接口
- 包含所有必要的公共API

### 2.2 实现文件：PRTComputeShader.cpp
- 框架实现，TODO标记了需要完成的部分
- 包含初始化、清理等基础功能
- 公共API的框架

### 2.3 Compute Shader：prt_compute.glsl
- 完整的GLSL compute shader代码
- 实现了球谐基函数计算
- 实现了光照投影计算
- 实现了Light Transport计算
- 实现了球谐旋转计算

## 3. 实现步骤

### 3.1 第一步：Shader编译和加载
**文件：** PRTComputeShader.cpp - LoadComputeShader()

**需要做的：**
1. 使用glslangValidator或glslc编译prt_compute.glsl为SPIR-V
2. 从SPIR-V文件加载二进制数据
3. 创建VkShaderModule

**代码框架：**
```cpp
bool PRTComputeShader::LoadComputeShader()
{
    // 1. 读取SPIR-V文件
    std::ifstream file("prt_compute.spv", std::ios::binary | std::ios::ate);
    
    // 2. 创建VkShaderModule
    VkShaderModuleCreateInfo moduleCreateInfo = {};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    // ... 设置其他参数
    
    return vkCreateShaderModule(...) == VK_SUCCESS;
}
```

### 3.2 第二步：Descriptor Set Layout创建
**文件：** PRTComputeShader.cpp - CreateDescriptorSetLayout()

**需要做的：**
1. 创建5个descriptor set layout binding：
   - Binding 0: Samples Buffer (SSBO)
   - Binding 1: Input Coefficients Buffer (SSBO)
   - Binding 2: Output Coefficients Buffer (SSBO)
   - Binding 3: LT Input Buffer (SSBO)
   - Binding 4: Rotation Params (UBO)
   - Binding 5: Compute Params (UBO)

**关键参数：**
```cpp
VkDescriptorSetLayoutBinding binding;
binding.binding = 0;
binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
binding.descriptorCount = 1;
binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
```

### 3.3 第三步：Descriptor Pool创建
**文件：** PRTComputeShader.cpp - CreateDescriptorPool()

**需要做的：**
1. 创建descriptor pool，包含足够的SSBO和UBO资源
2. 设置maxSets为1

### 3.4 第四步：Compute Pipeline创建
**文件：** PRTComputeShader.cpp - CreateComputePipeline()

**需要做的：**
1. 创建VkPipelineLayout
2. 创建VkComputePipelineCreateInfo
3. 创建VkPipeline

**关键参数：**
```cpp
VkComputePipelineCreateInfo pipelineInfo = {};
pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
pipelineInfo.stage.module = computeShaderModule;
pipelineInfo.stage.pName = "main";
pipelineInfo.layout = pipelineLayout;
```

### 3.5 第五步：Buffer创建
**文件：** PRTComputeShader.cpp - CreateBuffers()

**需要做的：**
1. 创建采样方向buffer (SSBO)
2. 创建输入系数buffer (SSBO)
3. 创建输出系数buffer (SSBO)
4. 创建Light Transport输入buffer (SSBO)
5. 创建旋转参数buffer (UBO)
6. 创建计算参数buffer (UBO)

**Buffer大小计算：**
```cpp
// 采样方向buffer: numSamples * sizeof(GPUSample)
size_t samplesBufferSize = numSamples * sizeof(GPUSample);

// 系数buffer: sizeof(GPUSHCoefficients)
size_t coeffsBufferSize = sizeof(GPUSHCoefficients);

// Light Transport输入: numVertices * sizeof(GPULTInput)
size_t ltInputBufferSize = numVertices * sizeof(GPULTInput);
```

### 3.6 第六步：Descriptor Set创建和更新
**文件：** PRTComputeShader.cpp - CreateDescriptorSet() 和 UpdateDescriptorSet()

**需要做的：**
1. 分配descriptor set
2. 更新descriptor set中的buffer绑定
3. 使用VkWriteDescriptorSet更新绑定

### 3.7 第七步：Compute Shader执行
**文件：** PRTComputeShader.cpp - ExecuteComputeShader()

**需要做的：**
1. 创建command buffer
2. 绑定pipeline和descriptor set
3. 分发compute shader工作组
4. 提交命令缓冲区
5. 等待完成

**代码框架：**
```cpp
bool PRTComputeShader::ExecuteComputeShader(
    uint32_t groupCountX,
    uint32_t groupCountY,
    uint32_t groupCountZ)
{
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(...);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, 
                           pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmdBuf, groupCountX, groupCountY, groupCountZ);
    vulkanDevice->flushCommandBuffer(cmdBuf, queue);
    return true;
}
```

## 4. 数据流程

### 4.1 光照投影计算流程
```
CPU端数据准备
    ↓
上传采样方向和辐射度到GPU
    ↓
执行Compute Shader (mode=0)
    ↓
下载输出系数到CPU
    ↓
保存到文件
```

### 4.2 Light Transport计算流程
```
CPU端数据准备 (位置、法向量、反射率)
    ↓
上传LT输入数据和采样方向到GPU
    ↓
执行Compute Shader (mode=1)
    ↓
下载输出系数到CPU
    ↓
保存到文件
```

### 4.3 旋转计算流程
```
上传输入系数到GPU
    ↓
循环执行Compute Shader (mode=2)
    ↓
下载所有旋转后的系数到CPU
    ↓
保存到文件
```

## 5. 工作组大小

**当前设置：** local_size_x = 256

**计算方式：**
- 光照投影：1个工作组（输出1个系数）
- Light Transport：ceil(numVertices / 256)个工作组
- 旋转计算：1个工作组（输出1个系数）

## 6. 内存对齐注意事项

### 6.1 std430布局（SSBO）
- 标量：4字节对齐
- vec2：8字节对齐
- vec3/vec4：16字节对齐
- 数组：按最大元素对齐

### 6.2 std140布局（UBO）
- 更严格的对齐要求
- 所有成员16字节对齐

### 6.3 C++端对应
```cpp
// 确保结构体大小正确
static_assert(sizeof(GPUSample) == 32, "GPUSample size mismatch");
static_assert(sizeof(GPUSHCoefficients) == 144, "GPUSHCoefficients size mismatch");
```

## 7. 调试建议

### 7.1 验证步骤
1. 验证shader编译是否成功
2. 验证buffer创建是否成功
3. 验证descriptor set绑定是否正确
4. 验证compute shader执行是否成功
5. 验证输出数据是否正确

### 7.2 常见问题
- Buffer大小不匹配
- Descriptor set绑定错误
- 内存对齐问题
- 工作组大小设置不当

## 8. 性能优化建议

### 8.1 工作组大小优化
- 当前256可能过大，可尝试64-128
- 根据GPU特性调整

### 8.2 内存优化
- 使用共享内存缓存采样方向
- 减少全局内存访问

### 8.3 并行优化
- 批量处理多个顶点
- 使用多个command buffer并行执行

## 9. 下一步

1. 编译prt_compute.glsl为SPIR-V
2. 实现LoadComputeShader()
3. 实现CreateDescriptorSetLayout()
4. 实现CreateDescriptorPool()
5. 实现CreateComputePipeline()
6. 实现CreateBuffers()
7. 实现CreateDescriptorSet()
8. 实现ExecuteComputeShader()
9. 实现公共API
10. 集成到主程序

---

**创建时间：** 2025-11-28
**状态：** 框架完成，待实现

