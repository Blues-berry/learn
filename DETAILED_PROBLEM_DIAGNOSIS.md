# 详细问题诊断

## 问题1和2的根本原因分析

### 立方体贴图坐标系统

**Vulkan标准立方体贴图面顺序**:
- 面0: +X (右)
- 面1: -X (左)
- 面2: +Y (上)
- 面3: -Y (下)
- 面4: +Z (前)
- 面5: -Z (后)

### 当前LightProbe.cpp中的视图矩阵 (第69-76行)

```cpp
glm::lookAt(position, position + glm::vec3( 1, 0, 0), glm::vec3(0, -1,  0)), // +X
glm::lookAt(position, position + glm::vec3(-1, 0, 0), glm::vec3(0, -1,  0)), // -X
glm::lookAt(position, position + glm::vec3( 0, 1, 0), glm::vec3(0,  0,  1)), // +Y
glm::lookAt(position, position + glm::vec3( 0,-1, 0), glm::vec3(0,  0, -1)), // -Y
glm::lookAt(position, position + glm::vec3( 0, 0, 1), glm::vec3(0, -1,  0)), // +Z
glm::lookAt(position, position + glm::vec3( 0, 0,-1), glm::vec3(0, -1,  0))  // -Z
```

### 着色器中的Y坐标翻转

**irradiancecube.frag 第38行**:
```glsl
sampleVector.y = -sampleVector.y;
```

**prefilterenvmap.frag 第97行**:
```glsl
L.y = -L.y;
```

**lightprobesh.frag 第84, 178行**:
```glsl
sampleR.y = -sampleR.y;
sampleN.y = -sampleN.y;
```

### 问题分析

1. **Y坐标翻转的目的**: 将OpenGL坐标系转换为Vulkan坐标系
   - OpenGL: Y轴向上
   - Vulkan: Y轴向下

2. **视图矩阵up向量的作用**:
   - 定义了相机的"上"方向
   - 影响立方体贴图的方向

3. **当前问题**:
   - +Y面: up向量为(0, 0, 1) - 指向+Z
   - -Y面: up向量为(0, 0, -1) - 指向-Z
   - 这与着色器的Y翻转可能产生冲突

### 黑色问题的原因

当SH系数或IBL贴图全为黑色时，通常表示:
1. 采样方向完全错误
2. 立方体贴图本身为空
3. 描述符绑定失败

### 上下贴图反转的原因

±Y面的贴图反转表示:
1. 视图矩阵的up向量方向不对
2. 导致立方体贴图的上下面被交换

