# PRT 黑色立方体问题 - 修复总结

## 问题

使用 PRT (Precomputed Radiance Transfer) 渲染时，模型中的某个立方体显示为完全黑色，而 PBR 模式下正常显示。

## 根本原因

问题可能由以下几个原因之一造成：

1. **LT 系数全零** - 某个顶点的 Light Transport 系数为零
2. **材质颜色黑色** - 某个 primitive 的基础颜色是黑色
3. **法向量无效** - 某个顶点的法向量为零或无效
4. **索引映射错误** - 顶点索引与 LT 数据不匹配

## 实施的解决方案

### 1. 着色器诊断增强

**文件**: `shaders/glsl/lightprobesh2/prt_relight.vert`

添加了错误检测和颜色编码：
- 越界检查 → 输出洋红色
- 零系数检查 → 输出青色
- 有效数据 → 正常 PRT 渲染

### 2. C++ 诊断代码

**文件**: `examples/lightprobesh2/main.cpp`

添加了三个诊断检查：

#### A. 零系数检查 (行 ~1730)
检查加载的 PRT 数据中是否有全零的顶点系数。

#### B. 黑色材质检查 (行 ~750)
检查是否有材质颜色为黑色的 primitive。

#### C. 无效法向量检查 (行 ~1630)
检查模型顶点的法向量是否有效，并修复无效法向量。

## 编译和运行

### 编译着色器

```bash
cd shaders/glsl/lightprobesh2
glslc -O prt_relight.vert -o prt_relight.vert.spv
glslc -O prt_relight.frag -o prt_relight.frag.spv
```

### 编译程序

```bash
cd build
msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2
```

### 运行程序

```bash
bin\Release\lightprobesh2.exe
```

## 诊断步骤

1. **启用 PRT Relighting**
   - 在 UI 中勾选 "Enable PRT Relighting"

2. **观察控制台输出**
   - 查找 `[DEBUG PRT] WARNING` 消息
   - 这会告诉你具体是什么问题

3. **观察视觉输出**
   - 洋红色 = 索引越界
   - 青色 = LT 系数全零
   - 黑色 = 材质黑色或其他问题

## 预期结果

修复后，应该看到以下之一：

1. **所有立方体正常渲染** - PRT 系统工作正常
2. **某个立方体是青色** - 需要重新导出 PRT 数据
3. **某个立方体是黑色** - 需要检查材质或法向量
4. **某个立方体是洋红色** - 严重的索引错误

## 后续修复

根据诊断结果：

### 如果看到青色立方体
```
1. 点击 "Export PRT (GPU)" 重新导出
2. 重新启用 PRT Relighting
```

### 如果看到黑色立方体
```
1. 检查模型材质设置
2. 验证光照 SH 系数
3. 检查 PRT 数据文件
```

### 如果看到无效法向量警告
```
1. 检查模型文件
2. 在建模软件中重新计算法向量
3. 重新导出模型
```

## 文件修改

| 文件 | 修改内容 |
|------|--------|
| prt_relight.vert | 添加诊断代码（越界、零系数检查） |
| prt_relight.vert.spv | 重新编译的着色器 |
| main.cpp | 添加零系数、黑色材质、无效法向量检查 |

## 验证

编译后运行程序，启用 PRT Relighting，观察：

1. 是否有诊断警告信息
2. 立方体是否显示诊断颜色（洋红/青色）
3. 是否有性能问题

## 注意事项

- 诊断代码会增加少量开销，但仅在启用 PRT 时生效
- 某些诊断检查只在第一帧执行（使用 `static` 标志）
- 无效法向量会被替换为默认向上向量 (0,1,0)

## 相关文档

- `PRT_DIAGNOSTIC_GUIDE.md` - 详细的诊断指南
- `PRT_ROOT_CAUSE_ANALYSIS.md` - 根本原因分析
- `PRT_BLACK_CUBE_FIX.md` - 修复详情

