# PRT 黑色立方体问题诊断与修复

## 问题描述

使用 PRT (Precomputed Radiance Transfer) 渲染时，模型中的某个立方体显示为完全黑色，而 PBR 模式下正常显示。

### 症状
- 启用 PRT Relighting 后，一个立方体变成黑色
- 其他立方体显示正常的 PRT 光照效果
- PBR 模式下所有立方体都正常

## 根本原因分析

### 可能的原因

1. **顶点索引映射错误**
   - glTF 模型加载时，索引缓冲区中的值被调整为全局索引（加了 `vertexStart`）
   - PRT 着色器使用 `gl_VertexIndex` 访问 LT 系数 SSBO
   - 如果某个 primitive 的所有顶点的 LT 系数都是零，该 primitive 会显示为黑色

2. **LT 系数数据缺失或无效**
   - PRT 数据导出时可能遗漏了某些顶点
   - 某些顶点的 LT 系数计算失败，导致为零

3. **着色器逻辑问题**
   - 着色器中的 SH 系数点积计算可能有问题
   - 材质颜色调制可能导致黑色输出

## 实施的诊断修复

### 1. 着色器调试增强 (prt_relight.vert)

添加了颜色编码的诊断输出：

```glsl
// 越界检查 - 输出洋红色 (magenta)
if (vid >= ltBuffer.ltCoefficients.length()) {
    outColor = vec3(1.0, 0.0, 1.0);  // 洋红色 = 索引越界
    return;
}

// 零系数检查 - 输出青色 (cyan)
bool allZero = true;
for (int i = 0; i < 9; i++) {
    if (length(lt_coeffs.coeffs[i].xyz) > 0.001) {
        allZero = false;
        break;
    }
}
if (allZero) {
    outColor = vec3(0.0, 1.0, 1.0);  // 青色 = LT 系数全零
    return;
}
```

### 2. C++ 代码诊断增强 (main.cpp)

在 PRT 数据加载时检查零系数：

```cpp
// 检查零系数顶点
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

## 诊断步骤

1. **编译并运行程序**
   ```bash
   cd build
   msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2
   bin\Release\lightprobesh2.exe
   ```

2. **启用 PRT Relighting**
   - 在 UI 中勾选 "Enable PRT Relighting"

3. **观察颜色输出**
   - **洋红色** = 顶点索引越界（严重错误）
   - **青色** = LT 系数全零（数据缺失）
   - **正常颜色** = PRT 渲染正常

4. **查看控制台输出**
   - 查找 "WARNING: Vertex X has all-zero LT coefficients!" 消息
   - 这会告诉你哪些顶点的 LT 数据无效

## 可能的修复方案

### 方案 A: 重新导出 PRT 数据
如果发现零系数顶点：
1. 点击 UI 中的 "Export PRT (GPU)" 按钮
2. 重新生成 PRT 数据
3. 重新启用 PRT Relighting

### 方案 B: 检查模型加载
如果问题持续：
1. 验证模型顶点数量是否与 LT 数据匹配
2. 检查 PRT 数据导出时是否正确读取了所有顶点
3. 查看 `prt_output/prt_data_lt_batch.txt` 文件的行数

### 方案 C: 修复 LT 系数计算
如果 LT 数据导出时就是零：
1. 检查 `ComputeLightTransportBatch` 的实现
2. 验证采样方向和法向量是否正确
3. 检查 SH 投影计算是否有问题

## 文件修改

- `shaders/glsl/lightprobesh2/prt_relight.vert` - 添加诊断代码
- `examples/lightprobesh2/main.cpp` - 添加零系数检查

## 编译着色器

```bash
cd shaders/glsl/lightprobesh2
glslc -O prt_relight.vert -o prt_relight.vert.spv
glslc -O prt_relight.frag -o prt_relight.frag.spv
```

## 预期结果

运行修复后的程序，如果看到：
- **青色立方体** → LT 数据缺失，需要重新导出
- **洋红色立方体** → 严重的索引错误，需要调查模型加载
- **正常渲染** → PRT 系统工作正常

## 下一步

根据诊断结果，采取相应的修复方案。

