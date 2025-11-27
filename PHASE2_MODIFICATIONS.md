# 阶段2 修改总结：修复Preview Model颜色对应问题

## 修改概述

成功修改了Cornell Box着色器，使其支持动态光源颜色、位置和强度。

---

## 修改的文件

### 1. shaders/glsl/lightprobesh2/gltfmesh_mvr.frag

**修改内容**:
- 扩展Global UBO结构，添加4个新字段
- 更新光照计算，使用动态光源位置和颜色

**具体改动**:

#### 修改1: Global UBO结构
```glsl
// 添加字段
int useLightSource;
float lightIntensity;
vec3 lightPosition;
vec3 lightColor;
```

#### 修改2: 光照计算
```glsl
// 原始: 固定方向光
vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));

// 修改后: 动态光源位置
vec3 lightDir = normalize(global.lightPosition - inWorldPos);

// 添加光源颜色和强度支持
vec3 lightContribution = global.lightColor * global.lightIntensity * NdotL;
diffuse += albedo * lightContribution * 0.5;
specular *= lightContribution;
```

---

### 2. shaders/glsl/lightprobesh2/gltfmesh_main.frag

**修改内容**:
- 扩展Global UBO结构，添加4个新字段
- 更新光照计算，使用动态光源位置和颜色

**具体改动**:

#### 修改1: Global UBO结构
```glsl
// 添加字段
int useLightSource;
float lightIntensity;
vec3 lightPosition;
vec3 lightColor;
```

#### 修改2: 光照计算
```glsl
// 原始: 固定方向光
vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));

// 修改后: 动态光源位置
vec3 lightDir = normalize(global.lightPosition - inWorldPos);

// 添加光源颜色和强度支持
vec3 lightContribution = global.lightColor * global.lightIntensity * NdotL;
diffuse += albedo * lightContribution * 0.5;
specular *= length(lightContribution);
```

---

## 未修改的文件

### light_source.frag
- ✅ 已包含正确的Global UBO结构
- ✅ 已支持lightColor和lightPosition
- ✅ 无需修改

### Pass.h
- ✅ GlobalUbo结构已包含所有必要字段
- ✅ 无需修改

### main.cpp
- ✅ prepareData()已正确设置lightColor和lightPosition
- ✅ 无需修改

### Pass.cpp
- ✅ UpdateGlobal()已正确复制UBO数据
- ✅ 无需修改

---

## 修改的关键改进

### 1. 动态光源位置
- **支持**: 光源可以旋转和移动
- **计算**: `lightDir = normalize(global.lightPosition - inWorldPos)`
- **优势**: 更真实的光照效果

### 2. 光源颜色支持
- **支持**: 光源颜色可以动态改变
- **计算**: `lightContribution = global.lightColor * global.lightIntensity * NdotL`
- **优势**: Preview Model和Cornell Box颜色对应

### 3. 光源强度支持
- **支持**: 光源强度可以动态调整
- **计算**: 通过`global.lightIntensity`参数
- **优势**: 灵活的光照控制

### 4. 镜面反射改进
- **支持**: 镜面反射随光源颜色改变
- **计算**: `specular *= lightContribution`
- **优势**: 更一致的光照效果

---

## 数据流验证

### 完整的数据流链路

```
UI颜色选择器
    ↓
main.cpp: lightColor = glm::vec3(color[0], color[1], color[2])
    ↓
main.cpp: previewModel->SetLightColor(lightColor)
    ↓
PreviewModel: materialData.elbedo = glm::vec4(color, 1.0f)
    ↓
main.cpp: prepareData()
    ↓
mainPassData.lightColor = lightColor
mainPassData.lightPosition = lightPosition
mainPassData.lightIntensity = lightIntensity
    ↓
mainPass->UpdateGlobal(mainPassData)
    ↓
Pass.cpp: memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo))
    ↓
GPU Global UBO Buffer
    ↓
gltfmesh_mvr.frag / gltfmesh_main.frag
    ↓
global.lightColor
global.lightPosition
global.lightIntensity
    ↓
光照计算
    ↓
Cornell Box着色
```

---

## 编译说明

### 着色器编译
需要重新编译以下着色器为SPIR-V格式：

1. `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`
   - 输出: `gltfmesh_mvr.frag.spv`

2. `shaders/glsl/lightprobesh2/gltfmesh_main.frag`
   - 输出: `gltfmesh_main.frag.spv`

### 编译命令
```bash
glslc -fshader-stage=fragment gltfmesh_mvr.frag -o gltfmesh_mvr.frag.spv
glslc -fshader-stage=fragment gltfmesh_main.frag -o gltfmesh_main.frag.spv
```

---

## 测试步骤

### 1. 编译着色器
- 使用glslc或glslangValidator编译修改的着色器

### 2. 启动应用
- 运行lightprobesh2示例

### 3. 测试颜色对应
- 打开UI颜色选择器
- 改变光源颜色
- 观察Preview Model和Cornell Box的颜色是否对应

### 4. 验证光照效果
- 改变光源强度
- 观察光照强度是否相应改变
- 验证镜面反射是否随光源颜色改变

---

## 预期结果

修改后，当用户改变Preview Model颜色时：

1. ✅ Preview Model显示新颜色
2. ✅ Cornell Box的着色相应改变
3. ✅ 光照效果与光源颜色一致
4. ✅ 镜面反射也随光源颜色改变
5. ✅ 光源强度可以动态调整

---

## 下一步

### 阶段3: 实现基于PRT的relighting
- 研究PRT和球谐函数理论
- 设计预计算系统架构
- 实现球谐函数库
- 预计算光照信息
- 导出为txt文件

### 阶段4: 应用预计算信息
- 实现数据导入功能
- 实现relighting着色器
- 集成到主渲染管线

