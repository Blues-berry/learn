# Cornell Box 着色器修复详细说明

## 修复目标

解决Preview Model颜色改变时，Cornell Box着色不对应的问题。

---

## 问题分析

### 原始问题
1. **Preview Model**: 颜色通过 `material.albedo` 正确显示
2. **Cornell Box**: 使用固定的 `lightDir = normalize(vec3(1.0, 1.0, 1.0))`
3. **结果**: 光源颜色改变时，Cornell Box着色不变

### 根本原因
- Global UBO在Pass.h中包含了 `lightColor` 和 `lightPosition`
- main.cpp中正确设置了这些值
- **但**: gltfmesh_mvr.frag着色器中的Global UBO结构缺少这些字段

---

## 修复步骤

### 1. 更新Global UBO结构 (gltfmesh_mvr.frag)

**修改前**:
```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 viewProject[6];
    vec4 cameraPos[6];
    vec4 mainLight;
    float exposure;
    float gamma;
} global;
```

**修改后**:
```glsl
layout (set = 0, binding = 0) uniform Global
{
    mat4 viewProject[6];
    vec4 cameraPos[6];
    vec4 mainLight;
    float exposure;
    float gamma;
    int useLightSource;
    float lightIntensity;
    vec3 lightPosition;
    vec3 lightColor;
} global;
```

### 2. 更新光照计算 (gltfmesh_mvr.frag)

**修改前**:
```glsl
vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
float NdotL = max(dot(N, lightDir), 0.0);
diffuse += albedo * NdotL * 0.5;

vec3 H = normalize(V + lightDir);
float NdotH = max(dot(N, H), 0.0);
vec3 specular = vec3(material.specular) * pow(NdotH, mix(8.0, 64.0, clamp(material.roughness, 0.0, 1.0)));
```

**修改后**:
```glsl
// 使用动态光源位置和颜色
vec3 lightDir = normalize(global.lightPosition - inWorldPos);
float NdotL = max(dot(N, lightDir), 0.0);

// 应用光源颜色和强度
vec3 lightContribution = global.lightColor * global.lightIntensity * NdotL;
diffuse += albedo * lightContribution * 0.5;

vec3 H = normalize(V + lightDir);
float NdotH = max(dot(N, H), 0.0);
vec3 specular = vec3(material.specular) * pow(NdotH, mix(8.0, 64.0, clamp(material.roughness, 0.0, 1.0))) * lightContribution;
```

---

## 修复的关键改进

### 1. 动态光源位置
- **原始**: 固定方向 `vec3(1.0, 1.0, 1.0)`
- **修复**: 使用 `global.lightPosition - inWorldPos` 计算动态方向
- **优势**: 支持光源旋转和移动

### 2. 光源颜色支持
- **原始**: 无颜色支持，只有白光
- **修复**: 使用 `global.lightColor` 调制光照
- **优势**: Preview Model颜色改变时，Cornell Box着色相应改变

### 3. 光源强度支持
- **原始**: 固定强度 `0.5`
- **修复**: 使用 `global.lightIntensity` 动态调整
- **优势**: 支持光源强度控制

### 4. 镜面反射改进
- **原始**: 镜面反射不受光源颜色影响
- **修复**: 镜面反射乘以 `lightContribution`
- **优势**: 镜面反射也会随光源颜色改变

---

## 数据流验证

### 数据流链路
```
main.cpp (prepareData)
    ↓
mainPassData.lightColor = lightColor
mainPassData.lightPosition = lightPosition
mainPassData.lightIntensity = lightIntensity
    ↓
mainPass->UpdateGlobal(mainPassData)
    ↓
Pass.cpp (UpdateGlobal)
    ↓
memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo))
    ↓
GPU Global UBO Buffer
    ↓
gltfmesh_mvr.frag
    ↓
global.lightColor
global.lightPosition
global.lightIntensity
```

### 验证点
1. ✅ Pass.h中GlobalUbo包含lightColor、lightPosition、lightIntensity
2. ✅ main.cpp中prepareData()设置这些值
3. ✅ Pass.cpp中UpdateGlobal()正确复制数据
4. ✅ gltfmesh_mvr.frag中Global UBO结构已更新
5. ✅ 着色器中使用了这些值

---

## 预期效果

修改后，当用户改变Preview Model颜色时：

1. **UI颜色选择** → lightColor更新
2. **PreviewModel::SetLightColor()** → 更新Preview Model材质
3. **prepareData()** → 更新mainPassData.lightColor
4. **UpdateGlobal()** → 传递到GPU
5. **gltfmesh_mvr.frag** → 使用新的lightColor计算光照
6. **结果** → Cornell Box着色相应改变

---

## 编译和测试

### 编译
- 着色器需要重新编译为SPIR-V
- 使用glslc或glslangValidator编译

### 测试步骤
1. 启动应用
2. 打开UI颜色选择器
3. 改变光源颜色
4. 观察Preview Model和Cornell Box的颜色是否对应

### 预期结果
- Preview Model显示新颜色
- Cornell Box的着色也相应改变
- 光照效果与光源颜色一致

