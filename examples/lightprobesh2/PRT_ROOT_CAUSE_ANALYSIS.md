# PRT 黑色立方体 - 根本原因分析

## 问题概述

PRT 渲染时一个立方体显示为黑色，而 PBR 模式正常。这表明 PRT 系统中存在选择性的数据缺失或计算错误。

## 技术分析

### 1. 顶点索引映射

**VulkanglTFModel 的索引处理：**

```cpp
// 在 VulkanglTFModel.cpp 中
uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
// 后续所有索引都被调整：
indexBuffer.push_back(buf[index] + vertexStart);
```

这意味着：
- 每个 primitive 的索引都被转换为全局索引
- 索引缓冲区中的值是相对于整个顶点缓冲区的

**PRT 着色器的使用：**

```glsl
uint vid = uint(gl_VertexIndex);
SHCoefficients lt_coeffs = ltBuffer.ltCoefficients[vid];
```

这应该是正确的，因为 `gl_VertexIndex` 从索引缓冲区读取值。

### 2. 可能的真正原因

#### 原因 A: 模型有多个 Node/Mesh/Primitive

根据日志：
```
[DEBUG PRT] Model: nodes=1, vertices=64, indices=96
[DEBUG PRT] Draw summary: primitives=8, nodes=1, meshes=1
```

- 1 个 node
- 1 个 mesh
- 8 个 primitive
- 64 个顶点

这意味着 64 个顶点被分成了 8 个 primitive。如果 PRT 数据导出时使用了不同的顶点顺序或遗漏了某些顶点，就会导致索引不匹配。

#### 原因 B: PRT 数据导出的顶点顺序

在 `ExportPRTDataGPU()` 中：

```cpp
// 读取模型的所有顶点
const vkglTF::Vertex* vtx = reinterpret_cast<const vkglTF::Vertex*>(staging.mapped);
for (int i = 0; i < vcount; ++i) {
    positions.emplace_back(vtx[i].pos);
    normals.emplace_back(glm::normalize(vtx[i].normal));
    albedos.emplace_back(glm::vec3(1.0f));
}
```

这按照顶点缓冲区的顺序读取所有顶点，这应该是正确的。

#### 原因 C: 某些顶点的法向量为零或无效

如果某个顶点的法向量为零或非常小，那么：
1. `glm::normalize()` 可能返回 NaN 或零向量
2. LT 系数计算会失败
3. 最终 LT 系数为零

这会导致那个顶点（以及使用它的所有 primitive）显示为黑色。

#### 原因 D: 材质颜色为黑色

在 PRT 着色器中：

```glsl
vec3 finalColor = pushConstants.baseColor.rgb * prtColor;
```

如果某个 primitive 的 `baseColor` 是黑色 `(0, 0, 0)`，那么最终颜色就是黑色。

### 3. 诊断策略

#### 步骤 1: 检查材质颜色

在 C++ 代码中添加：

```cpp
std::cout << "[DEBUG PRT] Primitive " << primitiveCount 
          << " baseColor: (" << primitive->material.baseColorFactor.x 
          << ", " << primitive->material.baseColorFactor.y 
          << ", " << primitive->material.baseColorFactor.z << ")" << std::endl;
```

#### 步骤 2: 检查法向量

在导出 PRT 数据时添加：

```cpp
for (int i = 0; i < vcount; ++i) {
    float normLen = glm::length(normals[i]);
    if (normLen < 0.1f) {
        std::cout << "[WARNING] Vertex " << i << " has invalid normal: " 
                  << normLen << std::endl;
    }
}
```

#### 步骤 3: 检查 LT 系数

在加载 PRT 数据时，检查每个 primitive 对应的顶点的 LT 系数：

```cpp
// 对于每个 primitive，检查其顶点的 LT 系数
for (auto* primitive : node->mesh->primitives) {
    bool hasValidLT = false;
    for (uint32_t i = 0; i < primitive->indexCount; ++i) {
        uint32_t idx = indexBuffer[primitive->firstIndex + i];
        if (glm::length(precomputedLTCoefficients[idx].coeffs[0]) > 0.001f) {
            hasValidLT = true;
            break;
        }
    }
    if (!hasValidLT) {
        std::cout << "[WARNING] Primitive has no valid LT coefficients!" << std::endl;
    }
}
```

## 建议的修复

1. **立即诊断**：运行修改后的代码，查看诊断输出
2. **检查材质**：确保黑色立方体的材质颜色不是黑色
3. **检查法向量**：确保模型的所有顶点都有有效的法向量
4. **重新导出 PRT**：如果发现问题，重新导出 PRT 数据

## 预期的修复结果

修复后，所有立方体应该显示相同的 PRT 光照效果（可能颜色不同，但不会是黑色）。

