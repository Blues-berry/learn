# 着色器数据绑定详细分析

## 1. 描述符集布局定义

### 1.1 Set 0 - 全局数据（来自 MainPass）

**在 Pass.h 中定义的 GlobalUbo 结构：**

```cpp
struct GlobalUbo {
    glm::mat4 projection;  // 投影矩阵
    glm::mat4 view;        // 视图矩阵
    glm::vec4 light[4];    // 4 个光源位置
    glm::vec4 cameraPos;   // 相机位置
    float exposure;        // 曝光度
    float gamma;           // 伽马值
};
```

**绑定点：**
- Binding 0: GlobalUbo（UBO）
- Binding 1: SHCoefficients（球谐系数 UBO）
- Binding 2: samplerBRDFLUT（BRDF 查找表纹理）
- Binding 3: samplerIrradiance（辐照度立方体贴图）
- Binding 4: prefilteredMap（预过滤立方体贴图）

---

### 1.2 Set 1 - 模型数据（GltfModel 自己的）

**在 gltfload.h 中定义的结构：**

```cpp
struct LocalBuffer {
    glm::mat4 transform;  // 模型变换矩阵
};

struct MaterialBuffer {
    float roughness;      // 粗糙度 [0, 1]
    float metallic;       // 金属度 [0, 1]
    float specular;       // 镜面反射 [0, 1]
    float padding;        // 对齐填充
    glm::vec4 elbedo;     // 反照率（基础颜色）
    int32_t useSH;        // 是否使用球谐光照
    int32_t useReflection;// 是否使用反射
};
```

**绑定点：**
- Binding 0: LocalBuffer（模型变换 UBO）
- Binding 1: MaterialBuffer（材质参数 UBO）
- Binding 2: 模型纹理（COMBINED_IMAGE_SAMPLER）

---

## 2. 顶点着色器数据流

### 2.1 gltfmesh.vert 完整分析

```glsl
#version 450

// ============ 输入顶点属性 ============
layout (location = 0) in vec3 inPos;      // 顶点位置
layout (location = 1) in vec3 inNormal;   // 顶点法线
layout (location = 2) in vec2 inUV;       // 纹理坐标

// ============ Set 0 - 全局数据 ============
layout (set = 0, binding = 0) uniform Global {
    mat4 projection;    // 投影矩阵
    mat4 view;          // 视图矩阵
    vec4 lights[4];     // 光源位置
    vec4 cameraPos;     // 相机位置
    float exposure;     // 曝光度
    float gamma;        // 伽马值
} global;

// ============ Set 1 - 模型数据 ============
layout (set = 1, binding = 0) uniform Local {
    mat4 model;         // 模型矩阵
} ubo;

// ============ Push Constant ============
layout(push_constant) uniform PushConstant {
    mat4 modelOffset;   // 额外的模型变换（平移 + 缩放）
    vec4 tint;          // 颜色着色
} pc;

// ============ 输出到片段着色器 ============
layout (location = 0) out vec3 outWorldPos;  // 世界坐标
layout (location = 1) out vec3 outNormal;    // 世界法线
layout (location = 2) out vec2 outUV;        // 纹理坐标

void main() {
    // ============ 计算最终顶点位置 ============
    // 变换链：
    // 1. 模型矩阵：将顶点从模型空间变换到世界空间
    // 2. Push Constant 的 modelOffset：额外的世界空间变换
    // 3. 视图矩阵：从世界空间变换到相机空间
    // 4. 投影矩阵：从相机空间变换到裁剪空间
    
    vec4 worldPos = pc.modelOffset * ubo.model * vec4(inPos, 1.0);
    gl_Position = global.projection * global.view * worldPos;
    
    // ============ 计算世界坐标和法线 ============
    outWorldPos = worldPos.xyz;
    outNormal = mat3(ubo.model) * inNormal;  // 法线变换
    outUV = inUV;
}
```

---

## 3. 片段着色器数据流

### 3.1 gltfmesh.frag 完整分析

```glsl
#version 450

// ============ 来自顶点着色器的输入 ============
layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

// ============ Set 0 - 全局数据 ============
layout (set = 0, binding = 0) uniform Global {
    mat4 projection;
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;

// ============ Set 0 - 球谐系数 ============
layout (set = 0, binding = 1) uniform SHCoefficients {
    vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
} sh;

// ============ Set 0 - IBL 纹理 ============
layout (set = 0, binding = 2) uniform sampler2D samplerBRDFLUT;
layout (set = 0, binding = 3) uniform samplerCube samplerIrradiance;
layout (set = 0, binding = 4) uniform samplerCube prefilteredMap;

// ============ Set 1 - 材质参数 ============
layout (set = 1, binding = 1) uniform Material {
    float roughness;
    float metallic;
    float specular;
    float padding;
    vec4 elbedo;
    int useSH;
    int useReflection;
} material;

// ============ Push Constant ============
layout(push_constant) uniform PushConstant {
    mat4 modelOffset;
    vec4 tint;  // 颜色着色
} pc;

layout (location = 0) out vec4 outColor;

void main() {
    // ============ 获取基础颜色 ============
    vec3 albedo = material.elbedo.rgb * pc.tint.rgb;
    
    // ============ 准备 PBR 参数 ============
    float roughness = material.roughness;
    float metallic = material.metallic;
    float specular = material.specular;
    
    // ============ 计算法线和视向 ============
    vec3 N = normalize(inNormal);
    vec3 V = normalize(global.cameraPos.xyz - inWorldPos);
    
    // ============ 选择光照模式 ============
    vec3 lighting = vec3(0.0);
    
    if (material.useSH == 1) {
        // 使用球谐光照
        lighting = EvaluateSH(N, sh);
    }
    
    if (material.useReflection == 1) {
        // 使用 IBL（基于图像的光照）
        lighting += EvaluateIBL(N, V, albedo, roughness, metallic);
    }
    
    // ============ 应用曝光和伽马校正 ============
    vec3 color = lighting * albedo;
    color = Uncharted2Tonemap(color * global.exposure);
    color = pow(color, vec3(1.0 / global.gamma));
    
    outColor = vec4(color, 1.0);
}
```

---

## 4. 数据绑定流程

### 4.1 CPU 端绑定（gltfload.cpp）

```cpp
// 步骤 1：创建描述符集布局
std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
    // Set 1, Binding 0: 局部变换矩阵
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
        0),
    // Set 1, Binding 1: 材质参数
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        1),
    // Set 1, Binding 2: 模型纹理
    vks::initializers::descriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
        VK_SHADER_STAGE_FRAGMENT_BIT, 
        2),
};

// 步骤 2：创建描述符集
VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = 
    vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, 
                           nullptr, &descriptorSetLayout);

// 步骤 3：分配描述符集
VkDescriptorSetAllocateInfo allocInfo = 
    vks::initializers::descriptorSetAllocateInfo(descriptorPool, 
                                                &descriptorSetLayout, 1);
vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet);

// 步骤 4：更新描述符集（绑定 UBO）
std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    vks::initializers::writeDescriptorSet(descriptorSet, 
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &localBuffer.descriptor),
    vks::initializers::writeDescriptorSet(descriptorSet, 
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &materialBuffer.descriptor)
};
vkUpdateDescriptorSets(device->logicalDevice, 
                      static_cast<uint32_t>(writeDescriptorSets.size()), 
                      writeDescriptorSets.data(), 0, NULL);
```

### 4.2 GPU 端绑定（Draw 函数）

```cpp
// 绑定两个描述符集
std::vector<VkDescriptorSet> sets = {
    globalSet,      // Set 0: 来自 MainPass
    descriptorSet   // Set 1: 模型自己的
};

vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       pipelineLayout, 0, sets.size(), sets.data(), 0, NULL);
```

---

## 5. 数据更新流程

### 5.1 每帧更新

```cpp
// 在 main.cpp::prepareData() 中
mainPassData.projection = camera.matrices.perspective;
mainPassData.view = camera.matrices.view;
mainPassData.cameraPos = glm::vec4(camera.position, 1.0f);
mainPass->UpdateGlobal(mainPassData);  // 更新 Set 0

// 在 gltfload.cpp::ShowUI() 中
if (materialDirty) {
    memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
    materialDirty = false;  // 更新 Set 1
}
```

### 5.2 每实例更新

```cpp
// 在 Draw() 中，每次循环更新 Push Constant
pc.modelOffset = glm::translate(...) * glm::scale(...);
pc.tint = glm::vec4(colors[i % 3], 1.0f);
vkCmdPushConstants(cmd, pipelineLayout, 
                  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                  0, sizeof(PushConstantBlock), &pc);
```

---

## 6. 绑定点总结表

| Set | Binding | 类型 | 着色器阶段 | 内容 |
|-----|---------|------|----------|------|
| 0 | 0 | UBO | V, F | 全局数据（投影、视图等） |
| 0 | 1 | UBO | F | 球谐系数 |
| 0 | 2 | Sampler2D | F | BRDF LUT |
| 0 | 3 | SamplerCube | F | 辐照度贴图 |
| 0 | 4 | SamplerCube | F | 预过滤贴图 |
| 1 | 0 | UBO | V, F | 模型变换矩阵 |
| 1 | 1 | UBO | F | 材质参数 |
| 1 | 2 | Sampler | F | 模型纹理 |


