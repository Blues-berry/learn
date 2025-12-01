# PRT 黑色立方体 - 完整诊断指南

## 问题症状

启用 PRT Relighting 后，模型中的某个立方体显示为黑色，而 PBR 模式下正常。

## 已实施的诊断修改

### 1. 着色器诊断 (prt_relight.vert)

添加了颜色编码的错误指示：

```glsl
// 洋红色 (1,0,1) = 顶点索引越界
if (vid >= ltBuffer.ltCoefficients.length()) {
    outColor = vec3(1.0, 0.0, 1.0);
    return;
}

// 青色 (0,1,1) = LT 系数全零
if (allZero) {
    outColor = vec3(0.0, 1.0, 1.0);
    return;
}
```

### 2. C++ 诊断代码

#### A. 检查零系数顶点 (preparePRTRelighting)

```cpp
int zeroCount = 0;
for (size_t i = 0; i < precomputedLTCoefficients.size(); ++i) {
    bool isZero = true;
    for (int j = 0; j < 9; ++j) {
        float len = glm::length(precomputedLTCoefficients[i].coeffs[j]);
        if (len > 0.001f) {
            isZero = false;
            break;
        }
    }
    if (isZero) {
        zeroCount++;
        if (zeroCount <= 5) {
            std::cout << "[DEBUG PRT] WARNING: Vertex " << i 
                      << " has all-zero LT coefficients!" << std::endl;
        }
    }
}
```

#### B. 检查黑色材质 (draw loop)

```cpp
// 检查是否有黑色材质
for (auto* prim : node->mesh->primitives) {
    if (prim->material.baseColorFactor.x < 0.01f &&
        prim->material.baseColorFactor.y < 0.01f &&
        prim->material.baseColorFactor.z < 0.01f) {
        std::cout << "[DEBUG PRT] WARNING: Found black material!" << std::endl;
    }
}
```

#### C. 检查无效法向量 (ExportPRTDataGPU)

```cpp
int invalidNormalCount = 0;
for (int i = 0; i < vcount; ++i) {
    glm::vec3 normal = glm::normalize(vtx[i].normal);
    float normLen = glm::length(normal);
    
    if (normLen < 0.1f || glm::isnan(normal.x)) {
        if (invalidNormalCount < 5) {
            std::cout << "[DEBUG PRT] WARNING: Vertex " << i 
                      << " has invalid normal!" << std::endl;
        }
        invalidNormalCount++;
        normal = glm::vec3(0.0f, 1.0f, 0.0f);  // 使用默认法向量
    }
}
```

## 使用诊断工具

### 步骤 1: 编译

```bash
cd build
msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2
```

### 步骤 2: 运行程序

```bash
bin\Release\lightprobesh2.exe
```

### 步骤 3: 启用 PRT

1. 打开 UI
2. 点击 "PRT Relighting" 展开
3. 勾选 "Enable PRT Relighting"

### 步骤 4: 观察输出

#### 控制台输出

查看以下信息：

- `[DEBUG PRT] WARNING: Vertex X has all-zero LT coefficients!`
  → 某个顶点的 LT 数据为零

- `[DEBUG PRT] WARNING: Found black material!`
  → 某个 primitive 的材质颜色是黑色

- `[DEBUG PRT] WARNING: Vertex X has invalid normal!`
  → 某个顶点的法向量无效

#### 视觉诊断

- **洋红色立方体** = 索引越界（严重错误）
- **青色立方体** = LT 系数全零（数据缺失）
- **黑色立方体** = 材质颜色黑色或 PRT 计算失败
- **正常颜色** = PRT 正常工作

## 根据诊断结果的修复

### 情况 1: 看到青色立方体

**原因**：LT 系数全零

**修复**：
1. 点击 "Export PRT (GPU)" 重新导出 PRT 数据
2. 重新启用 PRT Relighting
3. 如果问题持续，检查模型的法向量

### 情况 2: 看到黑色立方体但无警告

**原因**：材质颜色黑色或 SH 点积计算问题

**修复**：
1. 检查模型的材质设置
2. 验证光照 SH 系数是否正确
3. 检查 PRT 数据文件是否存在

### 情况 3: 看到洋红色立方体

**原因**：严重的索引越界错误

**修复**：
1. 检查模型加载是否正确
2. 验证 LT 缓冲区大小是否与顶点数匹配
3. 查看 `preparePRTRelighting()` 中的 LT 缓冲区创建

### 情况 4: 看到无效法向量警告

**原因**：模型的某些顶点法向量为零或无效

**修复**：
1. 检查模型文件是否正确
2. 尝试在建模软件中重新计算法向量
3. 重新导出模型

## 文件修改列表

- `shaders/glsl/lightprobesh2/prt_relight.vert` - 着色器诊断
- `shaders/glsl/lightprobesh2/prt_relight.vert.spv` - 编译后的着色器
- `examples/lightprobesh2/main.cpp` - C++ 诊断代码

## 下一步

1. 运行修改后的程序
2. 根据诊断输出确定问题原因
3. 采取相应的修复方案
4. 重新测试

## 常见问题

**Q: 所有立方体都是青色？**
A: PRT 数据导出失败或数据文件丢失。检查 `prt_output/` 目录。

**Q: 只有一个立方体是青色？**
A: 该立方体的顶点的 LT 系数为零。可能是法向量无效或计算失败。

**Q: 看不到任何诊断信息？**
A: 检查是否启用了 PRT Relighting。查看控制台输出是否有其他错误。

