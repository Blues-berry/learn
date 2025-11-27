# PRT预计算系统架构设计

## 1. 系统概述

### 架构图
```
┌─────────────────────────────────────────────────────────┐
│                    PRT系统架构                           │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────────────────────────────────────────┐   │
│  │         离线预计算阶段 (Offline)                 │   │
│  ├──────────────────────────────────────────────────┤   │
│  │ 1. 光照采样 (Light Sampling)                     │   │
│  │ 2. 球谐投影 (SH Projection)                      │   │
│  │ 3. 旋转预计算 (Rotation Precomputation)          │   │
│  │ 4. 数据导出 (Data Export)                        │   │
│  └──────────────────────────────────────────────────┘   │
│                        ↓                                  │
│                   txt文件存储                             │
│                        ↓                                  │
│  ┌──────────────────────────────────────────────────┐   │
│  │         实时应用阶段 (Runtime)                   │   │
│  ├──────────────────────────────────────────────────┤   │
│  │ 1. 数据导入 (Data Import)                        │   │
│  │ 2. 光源旋转查询 (Light Rotation Query)           │   │
│  │ 3. 实时Relighting (Real-time Relighting)        │   │
│  │ 4. 着色器应用 (Shader Application)              │   │
│  └──────────────────────────────────────────────────┘   │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

---

## 2. 离线预计算阶段

### 2.1 光照采样模块

**功能**: 从环境中采样光照信息

**输入**:
- 环境贴图 (cubemap)
- 采样点数量 (numSamples)

**输出**:
- 采样方向数组
- 采样光照值数组

**实现**:
```cpp
class LightSampler {
public:
    struct Sample {
        glm::vec3 direction;
        glm::vec3 radiance;
    };
    
    std::vector<Sample> samples;
    
    void SampleFromCubemap(const TextureCubeMap& cubemap, int numSamples);
    void SampleUniform(int numSamples);
    void SampleFibonacci(int numSamples);
};
```

### 2.2 球谐投影模块

**功能**: 将光照投影到球谐函数

**输入**:
- 采样光照数据
- 球谐阶数 (order)

**输出**:
- 球谐系数 (9个或16个)

**实现**:
```cpp
class SHProjector {
public:
    struct SHCoefficients {
        glm::vec4 coeffs[9];  // 2阶球谐
    };
    
    SHCoefficients ProjectLight(const std::vector<LightSampler::Sample>& samples);
    glm::vec3 EvaluateBasis(int index, const glm::vec3& direction);
};
```

### 2.3 旋转预计算模块

**功能**: 预计算不同旋转角度下的球谐系数

**输入**:
- 原始球谐系数
- 旋转角度数组 (0°, 15°, 30°, ...)

**输出**:
- 旋转后的球谐系数数组

**实现**:
```cpp
class RotationPrecomputer {
public:
    struct RotatedCoefficients {
        float angle;
        SHCoefficients coeffs;
    };
    
    std::vector<RotatedCoefficients> PrecomputeRotations(
        const SHCoefficients& original,
        int numRotations
    );
    
    glm::mat3 GetRotationMatrix(float angleY);
    SHCoefficients RotateSH(const SHCoefficients& coeffs, const glm::mat3& rotation);
};
```

### 2.4 数据导出模块

**功能**: 将预计算数据导出为txt文件

**输入**:
- 旋转后的球谐系数数组

**输出**:
- txt文件

**实现**:
```cpp
class DataExporter {
public:
    void ExportToTxt(
        const std::string& filename,
        const std::vector<RotationPrecomputer::RotatedCoefficients>& data
    );
    
    void ExportSingleRotation(
        std::ofstream& file,
        float angle,
        const SHCoefficients& coeffs
    );
};
```

---

## 3. 实时应用阶段

### 3.1 数据导入模块

**功能**: 从txt文件读取预计算数据

**输入**:
- txt文件路径

**输出**:
- 旋转后的球谐系数数组

**实现**:
```cpp
class DataImporter {
public:
    struct RotatedCoefficients {
        float angle;
        SHCoefficients coeffs;
    };
    
    std::vector<RotatedCoefficients> ImportFromTxt(const std::string& filename);
    
    SHCoefficients ParseSHCoefficients(const std::string& line);
};
```

### 3.2 光源旋转查询模块

**功能**: 根据当前光源旋转角度查询对应的球谐系数

**输入**:
- 当前旋转角度
- 预计算数据

**输出**:
- 插值后的球谐系数

**实现**:
```cpp
class RotationQuery {
public:
    SHCoefficients QueryCoefficients(
        float currentAngle,
        const std::vector<DataImporter::RotatedCoefficients>& data
    );
    
    SHCoefficients InterpolateCoefficients(
        const SHCoefficients& coeff1,
        const SHCoefficients& coeff2,
        float t
    );
};
```

### 3.3 Relighting模块

**功能**: 应用预计算的球谐系数进行实时relighting

**输入**:
- 当前球谐系数
- 表面法线
- 材质参数

**输出**:
- 着色结果

**实现**:
```cpp
class Relighter {
public:
    glm::vec3 ComputeRelighting(
        const SHCoefficients& coeffs,
        const glm::vec3& normal,
        const glm::vec3& albedo
    );
    
    glm::vec3 EvaluateSH(const SHCoefficients& coeffs, const glm::vec3& normal);
};
```

---

## 4. 数据格式设计

### 4.1 txt文件格式

```
# PRT Precomputed Radiance Transfer Data
# Generated: 2025-11-27
# Scene: Cornell Box
# Light Rotations: 24 (每15度一个)
# SH Order: 2 (9个系数)

# Light Rotation: 0 degrees
l00: 0.5 0.5 0.5
l1m1: 0.1 0.1 0.1
l10: 0.2 0.2 0.2
l1p1: 0.1 0.1 0.1
l2m2: 0.05 0.05 0.05
l2m1: 0.05 0.05 0.05
l20: 0.1 0.1 0.1
l2p1: 0.05 0.05 0.05
l2p2: 0.05 0.05 0.05

# Light Rotation: 15 degrees
l00: 0.48 0.52 0.5
...
```

### 4.2 二进制格式 (可选)

```
Header:
  - Magic: "PRTD" (4 bytes)
  - Version: 1 (4 bytes)
  - NumRotations: N (4 bytes)
  - SHOrder: 2 (4 bytes)

Data:
  For each rotation:
    - Angle: float (4 bytes)
    - Coefficients: 9 * vec3 (108 bytes)
```

---

## 5. 集成到主程序

### 5.1 主程序修改

```cpp
class VulkanExample {
private:
    // PRT相关
    std::unique_ptr<LightSampler> lightSampler;
    std::unique_ptr<SHProjector> shProjector;
    std::unique_ptr<RotationPrecomputer> rotationPrecomputer;
    std::unique_ptr<DataExporter> dataExporter;
    std::unique_ptr<DataImporter> dataImporter;
    std::unique_ptr<RotationQuery> rotationQuery;
    std::unique_ptr<Relighter> relighter;
    
    std::vector<DataImporter::RotatedCoefficients> prtData;
    
public:
    void PrecomputePRT();
    void LoadPRTData(const std::string& filename);
    void UpdatePRTLighting(float lightRotationAngle);
};
```

### 5.2 工作流程

```
1. 初始化:
   - 创建所有模块实例
   - 加载环境贴图

2. 预计算 (可选):
   - 调用PrecomputePRT()
   - 导出数据到txt文件

3. 运行时:
   - 加载PRT数据
   - 每帧更新光源旋转角度
   - 查询对应的球谐系数
   - 应用到着色器
```

---

## 6. 性能考虑

### 6.1 内存占用

```
单个旋转的数据:
  - 9个vec3系数 = 108字节

24个旋转角度:
  - 24 * 108 = 2592字节 ≈ 2.5 KB

非常小！
```

### 6.2 计算复杂度

```
预计算阶段:
  - 光照采样: O(numSamples)
  - 球谐投影: O(numSamples * 9)
  - 旋转预计算: O(numRotations * 9)
  - 总体: O(numSamples + numRotations)

实时阶段:
  - 数据查询: O(1) 或 O(log numRotations)
  - 插值: O(9)
  - Relighting: O(9)
  - 总体: O(1)
```

---

## 7. 扩展性

### 7.1 支持多个光源

```cpp
struct MultiLightPRT {
    std::vector<SHCoefficients> lightSources;
    
    glm::vec3 ComputeRelighting(
        const glm::vec3& normal,
        const glm::vec3& albedo
    );
};
```

### 7.2 支持更高阶球谐

```cpp
// 3阶 (16个系数)
struct SHCoefficients3 {
    glm::vec4 coeffs[16];
};

// 4阶 (25个系数)
struct SHCoefficients4 {
    glm::vec4 coeffs[25];
};
```

---

## 8. 下一步

### 实现顺序
1. ✅ 理论研究
2. ⏳ 系统架构设计
3. 实现球谐函数库
4. 实现光照采样
5. 实现球谐投影
6. 实现旋转预计算
7. 实现数据导出
8. 实现数据导入
9. 集成到主程序
10. 测试和优化

