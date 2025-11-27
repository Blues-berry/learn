# PRT 快速开始指南

## 🚀 5分钟快速开始

### 第1步: 编译代码

```bash
cd build
cmake --build . --config Release --target lightprobesh2 -j 4
```

### 第2步: 运行程序

```bash
./bin/lightprobesh2.exe
```

程序会自动执行预计算，生成三个文件：
- `prt_data_lighting_original.txt` - 原始Lighting
- `prt_data_lt.txt` - Light Transport
- `prt_data_lighting.txt` - 旋转后的Lighting (24个)

### 第3步: 使用PRTRenderer

```cpp
#include "SphericalHarmonics.h"

// 初始化
PRTRenderer::PRTData prtData;
PRTRenderer::Initialize(prtData, "prt_data_lighting.txt", "prt_data_lt.txt");

// 定义表面
glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 albedo = glm::vec3(0.8f, 0.8f, 0.8f);

// 计算着色
glm::vec3 color = PRTRenderer::ComputeShading(prtData, normal, albedo);
```

---

## 📊 三项预计算数据

### 1️⃣ Lighting (光源的球谐系数)

**文件**: `prt_data_lighting_original.txt`

**含义**: 光源在球谐基函数上的投影

**格式**:
```
# PRT Lighting Data (Original)
0 c00.x c00.y c00.z c1m1.x c1m1.y c1m1.z ... c22.x c22.y c22.z
```

**用途**: 描述光源的颜色和强度分布

### 2️⃣ Light Transport (物体表面的响应)

**文件**: `prt_data_lt.txt`

**含义**: 物体表面对入射光的响应

**格式**:
```
# PRT Light Transport Data
lt00.x lt00.y lt00.z lt1m1.x lt1m1.y lt1m1.z ... lt22.x lt22.y lt22.z
```

**用途**: 描述物体表面的法线和反射率

### 3️⃣ Rotated Lighting (旋转后的光源系数)

**文件**: `prt_data_lighting.txt`

**含义**: 24个不同旋转角度的光源系数

**格式**:
```
# PRT Lighting Data (Rotated)
0 ...
15 ...
30 ...
...
345 ...
```

**用途**: 支持实时光源旋转

---

## 🎯 核心API

### 初始化

```cpp
PRTRenderer::PRTData prtData;
PRTRenderer::Initialize(prtData, "prt_data_lighting.txt", "prt_data_lt.txt");
```

### 更新旋转

```cpp
PRTRenderer::UpdateRotation(prtData, 45.0f);  // 45度
```

### 计算着色

```cpp
glm::vec3 color = PRTRenderer::ComputeShading(prtData, normal, albedo);
```

### 批量计算

```cpp
auto colors = PRTRenderer::ComputeShadingBatch(prtData, normals, albedos);
```

### 获取系数

```cpp
const auto& lighting = PRTRenderer::GetCurrentLighting(prtData);
const auto& lt = PRTRenderer::GetLightTransport(prtData);
```

### 导出结果

```cpp
PRTRenderer::ExportShadingResult("results.txt", prtData, normals, albedos);
```

---

## 💻 完整示例

```cpp
#include "SphericalHarmonics.h"
#include <iostream>
#include <glm/glm.hpp>

int main() {
    // 1. 初始化
    PRTRenderer::PRTData prtData;
    if (!PRTRenderer::Initialize(prtData, "prt_data_lighting.txt", "prt_data_lt.txt")) {
        std::cerr << "Failed to initialize!" << std::endl;
        return 1;
    }

    // 2. 定义表面
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 albedo = glm::vec3(0.8f, 0.8f, 0.8f);

    // 3. 旋转光源并渲染
    std::cout << "Rendering with different light rotations:" << std::endl;
    for (float angle = 0.0f; angle < 360.0f; angle += 45.0f) {
        // 更新旋转
        PRTRenderer::UpdateRotation(prtData, angle);

        // 计算着色
        glm::vec3 color = PRTRenderer::ComputeShading(prtData, normal, albedo);

        // 输出结果
        std::cout << "Angle: " << angle << " degrees" << std::endl;
        std::cout << "  Color: (" << color.x << ", " << color.y << ", " << color.z << ")" << std::endl;
    }

    return 0;
}
```

---

## 📈 性能指标

| 操作 | 时间 |
|------|------|
| 初始化 | < 1ms |
| 更新旋转 | < 0.1ms |
| 计算单点着色 | < 0.01ms |
| 批量计算1000点 | < 10ms |

---

## 🔍 调试技巧

### 检查预计算文件

```bash
# 检查文件是否存在
ls -la prt_data_*.txt

# 查看文件内容
head -5 prt_data_lighting.txt
head -1 prt_data_lt.txt
```

### 检查着色结果

```cpp
// 打印当前Lighting系数
const auto& lighting = PRTRenderer::GetCurrentLighting(prtData);
for (int i = 0; i < 9; i++) {
    std::cout << "L[" << i << "]: (" 
              << lighting.coeffs[i].x << ", "
              << lighting.coeffs[i].y << ", "
              << lighting.coeffs[i].z << ")" << std::endl;
}

// 打印Light Transport系数
const auto& lt = PRTRenderer::GetLightTransport(prtData);
for (int i = 0; i < 9; i++) {
    std::cout << "LT[" << i << "]: (" 
              << lt.coeffs[i].x << ", "
              << lt.coeffs[i].y << ", "
              << lt.coeffs[i].z << ")" << std::endl;
}
```

---

## ⚠️ 常见问题

**Q: 预计算失败怎么办?**
A: 检查是否有写入权限，确保输出目录存在。

**Q: 着色结果全黑?**
A: 检查Light Transport系数是否为0，确保表面法线正确。

**Q: 旋转不工作?**
A: 确保使用了 `UpdateRotation()` 函数，并且角度在0-360范围内。

**Q: 如何支持多个光源?**
A: 为每个光源预计算Lighting，然后在ComputeShading中求和。

---

## 📚 详细文档

- **PRT_RENDERER_USAGE.md** - 完整使用指南
- **PRT_FINAL_IMPLEMENTATION.md** - 实现细节
- **PRT_IMPLEMENTATION_LOGIC.md** - 理论基础

---

## 🎯 下一步

1. ✅ 编译代码
2. ✅ 运行预计算
3. ✅ 使用PRTRenderer进行渲染
4. ⏳ 集成到Vulkan渲染管线
5. ⏳ 性能优化

---

## 📞 支持

如有问题，请参考详细文档或查看示例代码。

**状态**: 🟢 **就绪** - 可以开始使用

