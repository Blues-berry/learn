# PRT 黑色立方体问题 - 诊断与修复

## 🎯 问题

使用 PRT (Precomputed Radiance Transfer) 渲染时，模型中的某个立方体显示为**完全黑色**，而 PBR 模式下正常显示。

## ⚡ 快速开始 (5 分钟)

### 1️⃣ 编译
```bash
cd build
msbuild vulkanExamples.sln /p:Configuration=Release /t:lightprobesh2
```

### 2️⃣ 运行
```bash
bin\Release\lightprobesh2.exe
```

### 3️⃣ 诊断
- UI → PRT Relighting → 勾选 "Enable PRT Relighting"
- 查看控制台输出和立方体颜色

### 4️⃣ 根据结果修复
| 现象 | 原因 | 修复 |
|------|------|------|
| 青色立方体 | LT 数据缺失 | 重新导出 PRT |
| 黑色立方体 | 材质/法向量问题 | 检查模型 |
| 洋红色立方体 | 索引错误 | 检查加载 |

## 📚 文档导航

### 🚀 快速参考
**[QUICK_PRT_FIX_REFERENCE.md](QUICK_PRT_FIX_REFERENCE.md)** ⭐ 推荐首先阅读
- 快速诊断步骤
- 常见问题和解决方案
- 最快的入门指南

### 📖 详细指南
**[PRT_DIAGNOSTIC_GUIDE.md](PRT_DIAGNOSTIC_GUIDE.md)**
- 完整的诊断步骤
- 诊断颜色编码说明
- 根据诊断结果的修复方案

### 🔬 技术分析
**[PRT_ROOT_CAUSE_ANALYSIS.md](PRT_ROOT_CAUSE_ANALYSIS.md)**
- 根本原因分析
- 技术细节
- 可能的问题原因

### 🛠️ 修改详情
**[CHANGES_MADE.md](CHANGES_MADE.md)**
- 所有代码修改
- 修改位置和内容
- 编译步骤

### 📋 工作总结
**[WORK_COMPLETED.md](WORK_COMPLETED.md)**
- 完成的工作清单
- 质量保证
- 下一步建议

### 🗂️ 文档索引
**[PRT_FIX_INDEX.md](PRT_FIX_INDEX.md)**
- 所有文档的导航中心
- 快速查询表
- 常见修复方案

## 🎨 诊断颜色速查

| 颜色 | 含义 | 修复 |
|------|------|------|
| 🟣 洋红色 | 索引越界 | 检查模型加载 |
| 🔵 青色 | LT 系数全零 | 重新导出 PRT |
| ⚫ 黑色 | 渲染失败 | 检查材质/法向量 |
| ✅ 正常 | 正常渲染 | 无需修复 |

## 📢 控制台警告

查看以下警告信息：

```
[DEBUG PRT] WARNING: Vertex X has all-zero LT coefficients!
[DEBUG PRT] WARNING: Found black material!
[DEBUG PRT] WARNING: Vertex X has invalid normal!
```

## 🔧 修复方案

### 方案 A: 重新导出 PRT 数据
```
1. UI → PRT GPU Export → Export PRT (GPU)
2. 等待导出完成
3. 重新启用 PRT Relighting
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

## 📝 修改的文件

| 文件 | 修改 | 目的 |
|------|------|------|
| `prt_relight.vert` | 添加诊断代码 | 检测越界和零系数 |
| `main.cpp` | 添加三个诊断检查 | 检测数据问题 |

## ✅ 工作完成情况

- ✓ 问题分析完成
- ✓ 诊断工具实施
- ✓ 代码编译成功
- ✓ 文档完整详细
- ✓ 可立即使用

## 🎓 学习资源

### PRT 相关
- **PRT 理论**: Precomputed Radiance Transfer 是一种离线光照计算技术
- **SH 系数**: 使用球谐函数表示光照和表面响应
- **LT 系数**: Light Transport 系数，表示表面对光照的响应

### 相关文件
- `prt_lighting.comp` - PRT 光照计算着色器
- `prt_lt.comp` - LT 系数计算着色器
- `PRTComputeShader.h/cpp` - PRT 计算管理类

## 🚀 下一步

1. **立即诊断** - 按照快速开始步骤运行程序
2. **查看结果** - 根据控制台输出和视觉效果判断问题
3. **选择修复** - 根据诊断结果选择相应的修复方案
4. **验证** - 重新启用 PRT 验证修复是否成功

## ❓ 常见问题

**Q: 所有立方体都是青色？**
A: PRT 数据文件丢失或导出失败。检查 `prt_output/` 目录。

**Q: 只有一个立方体是黑色？**
A: 该立方体的数据有问题。尝试重新导出 PRT。

**Q: 没有看到任何诊断信息？**
A: 确保已启用 PRT Relighting。查看完整的控制台输出。

## 📞 获取帮助

- 🚀 **快速答案**: 查看 `QUICK_PRT_FIX_REFERENCE.md`
- 📖 **详细步骤**: 查看 `PRT_DIAGNOSTIC_GUIDE.md`
- [object Object] 查看 `PRT_ROOT_CAUSE_ANALYSIS.md`
- 🗂️ **导航中心**: 查看 `PRT_FIX_INDEX.md`

## 📄 许可证

本诊断工具和文档是 Vulkan 示例项目的一部分。

---

**状态**: ✅ 诊断工具已实施，可立即使用
**最后更新**: 2024年12月
**下一步**: 运行诊断并根据结果采取修复措施

👉 **开始诊断**: 查看 [QUICK_PRT_FIX_REFERENCE.md](QUICK_PRT_FIX_REFERENCE.md)

