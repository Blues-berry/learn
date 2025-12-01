# PRT 黑色立方体 - 快速参考

## 问题
PRT 渲染时某个立方体显示为黑色。

## 快速诊断

### 1. 编译
```bash
cd build
msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2
```

### 2. 运行
```bash
bin\Release\lightprobesh2.exe
```

### 3. 启用 PRT
- UI → PRT Relighting → 勾选 "Enable PRT Relighting"

### 4. 查看控制台输出
```
[DEBUG PRT] WARNING: Vertex X has all-zero LT coefficients!
[DEBUG PRT] WARNING: Found black material!
[DEBUG PRT] WARNING: Vertex X has invalid normal!
```

## 诊断颜色

| 颜色 | 含义 | 解决方案 |
|------|------|--------|
| 洋红色 | 索引越界 | 检查模型加载 |
| 青色 | LT 系数全零 | 重新导出 PRT |
| 黑色 | 材质黑色/计算失败 | 检查材质或法向量 |
| 正常 | PRT 正常 | 无需修复 |

## 快速修复

### 情况 A: 看到青色立方体
```
1. UI → PRT GPU Export → Export PRT (GPU)
2. 等待导出完成
3. 重新启用 PRT Relighting
```

### 情况 B: 看到黑色立方体 + 无警告
```
1. 检查模型材质是否为黑色
2. 检查 prt_output/ 目录中的数据文件
3. 尝试重新导出 PRT 数据
```

### 情况 C: 看到无效法向量警告
```
1. 模型法向量有问题
2. 在建模软件中重新计算法向量
3. 重新导出模型
```

## 文件位置

| 文件 | 路径 |
|------|------|
| 着色器 | `shaders/glsl/lightprobesh2/prt_relight.vert` |
| 程序 | `examples/lightprobesh2/main.cpp` |
| 可执行文件 | `build/bin/Release/lightprobesh2.exe` |
| PRT 数据 | `prt_output/prt_data_lt_batch.txt` |

## 关键代码位置

### 着色器诊断 (prt_relight.vert)
- 行 54-60: 越界检查
- 行 62-70: 零系数检查

### C++ 诊断 (main.cpp)
- 行 ~750: 黑色材质检查
- 行 ~1730: 零系数检查
- 行 ~1630: 无效法向量检查

## 常见问题

**Q: 所有立方体都是青色？**
A: PRT 数据文件丢失或导出失败。检查 `prt_output/` 目录。

**Q: 只有一个立方体是黑色？**
A: 该立方体的数据有问题。尝试重新导出 PRT。

**Q: 没有看到任何诊断信息？**
A: 确保已启用 PRT Relighting。查看完整的控制台输出。

## 下一步

1. 运行诊断
2. 根据输出确定问题
3. 采取相应修复
4. 重新测试

详见 `PRT_DIAGNOSTIC_GUIDE.md` 获取完整指南。

