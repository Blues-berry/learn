# PRT 黑色立方体修复 - 文档索引

## 问题概述

使用 PRT (Precomputed Radiance Transfer) 渲染时，模型中的某个立方体显示为完全黑色，而 PBR 模式下正常显示。

## 文档导航

### 快速开始
- **[QUICK_PRT_FIX_REFERENCE.md](QUICK_PRT_FIX_REFERENCE.md)** ⭐ 
  - 快速诊断步骤
  - 常见问题和解决方案
  - 最快的入门指南

### 详细指南
- **[PRT_DIAGNOSTIC_GUIDE.md](PRT_DIAGNOSTIC_GUIDE.md)**
  - 完整的诊断步骤
  - 诊断颜色编码说明
  - 根据诊断结果的修复方案

### 技术分析
- **[PRT_ROOT_CAUSE_ANALYSIS.md](PRT_ROOT_CAUSE_ANALYSIS.md)**
  - 根本原因分析
  - 技术细节
  - 可能的问题原因

### 修改详情
- **[CHANGES_MADE.md](CHANGES_MADE.md)**
  - 所有代码修改
  - 修改位置和内容
  - 编译步骤

### 总结
- **[PRT_FIX_SUMMARY.md](PRT_FIX_SUMMARY.md)**
  - 修复总结
  - 实施的解决方案
  - 预期结果

## 快速诊断流程

```
1. 编译程序
   cd build
   msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2

2. 运行程序
   bin\Release\lightprobesh2.exe

3. 启用 PRT
   UI → PRT Relighting → 勾选 "Enable PRT Relighting"

4. 观察输出
   - 查看控制台警告信息
   - 观察立方体颜色
   - 记录诊断结果

5. 根据结果修复
   - 青色立方体 → 重新导出 PRT
   - 黑色立方体 → 检查材质/法向量
   - 洋红色立方体 → 检查索引映射
```

## 诊断颜色速查表

| 颜色 | 含义 | 原因 | 修复 |
|------|------|------|------|
| 洋红色 | 索引越界 | 顶点索引超出范围 | 检查模型加载 |
| 青色 | LT 系数全零 | PRT 数据缺失/无效 | 重新导出 PRT |
| 黑色 | 渲染失败 | 材质黑色或计算错误 | 检查材质/法向量 |
| 正常 | 正常渲染 | PRT 工作正常 | 无需修复 |

## 控制台警告速查表

| 警告信息 | 含义 | 修复 |
|---------|------|------|
| `Vertex X has all-zero LT coefficients!` | 某顶点 LT 数据为零 | 重新导出 PRT |
| `Found black material!` | 某 primitive 材质黑色 | 检查模型材质 |
| `Vertex X has invalid normal!` | 某顶点法向量无效 | 检查模型法向量 |

## 文件修改清单

| 文件 | 修改 | 目的 |
|------|------|------|
| `prt_relight.vert` | 添加诊断代码 | 检测越界和零系数 |
| `main.cpp` | 添加三个诊断检查 | 检测数据问题 |

## 编译命令

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

## 常见修复方案

### 方案 A: 重新导出 PRT 数据
```
1. 启动程序
2. UI → PRT GPU Export → Export PRT (GPU)
3. 等待导出完成
4. 重新启用 PRT Relighting
```

### 方案 B: 检查模型
```
1. 在建模软件中打开模型
2. 检查法向量是否正确
3. 检查材质颜色是否为黑色
4. 重新导出模型
```

### 方案 C: 清理缓存
```
1. 删除 prt_output/ 目录
2. 重新启动程序
3. 重新导出 PRT 数据
```

## 下一步

1. **立即诊断**: 按照快速诊断流程运行程序
2. **查看结果**: 根据控制台输出和视觉效果判断问题
3. **选择修复**: 根据诊断结果选择相应的修复方案
4. **验证**: 重新启用 PRT 验证修复是否成功

## 获取帮助

- 查看 `QUICK_PRT_FIX_REFERENCE.md` 获取快速答案
- 查看 `PRT_DIAGNOSTIC_GUIDE.md` 获取详细步骤
- 查看 `PRT_ROOT_CAUSE_ANALYSIS.md` 了解技术细节

## 相关资源

- **PRT 理论**: Precomputed Radiance Transfer 是一种离线光照计算技术
- **SH 系数**: 使用球谐函数表示光照和表面响应
- **LT 系数**: Light Transport 系数，表示表面对光照的响应

---

**最后更新**: 2024年12月
**状态**: 诊断工具已实施，等待测试反馈

