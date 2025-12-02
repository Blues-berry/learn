# PRT 快速开始指南

## 编译

```powershell
# 推荐：使用 PowerShell 脚本
powershell -ExecutionPolicy Bypass -File "examples\lightprobesh2\compile.ps1"

# 或使用批处理脚本
examples\lightprobesh2\compile.bat

# 或手动编译（需要 VS 开发者命令提示符）
cmake --build build --config Debug --target lightprobesh2 -j 4
```

## 运行

```bash
bin\lightprobesh2.exe
```

## PRT 工作流程

### 预计算阶段（一次性）

1. **启动程序**
   - 加载 Cornell Box 场景

2. **导出 PRT 数据**
   - 点击 UI 中的 "Export PRT Data" 按钮
   - 程序会：
     - 生成采样方向（Fibonacci 球采样）
     - 计算光源的球谐系数（LIGHT）
     - 计算场景的光传输系数（LT）
     - 预计算 24 个旋转角度的 LIGHT 系数
     - 导出到 `prt_output/` 目录

3. **验证导出数据**
   ```
   prt_output/
   ├── prt_data_lighting.txt  (24 行，每行一个旋转角度)
   └── prt_data_lt.txt        (多行，每个顶点一行)
   ```

### 运行时阶段

1. **启用 PRT 渲染**
   - 勾选 UI 中的 "Enable PRT" 复选框
   - 程序会加载预计算的数据

2. **旋转光源**
   - 使用 UI 控件旋转光源
   - 程序会：
     - 根据旋转角度查询对应的 LIGHT 系数
     - 计算 dot(LIGHT_SH, LT_SH) 得到着色
     - 实时更新 Cornell Box 的颜色

3. **观察效果**
   - Cornell Box 应该显示环境光效果
   - 颜色应该随光源旋转而变化
   - 效果应该接近 PBR 渲染

## 关键概念

### LIGHT（光源球谐系数）
- 表示光源的方向和强度分布
- 预计算 24 个旋转角度的系数
- 运行时根据旋转角度插值查询

### LT（光传输系数）
- 表示场景表面对入射光的响应
- 包含 Lambert 余弦项（max(0, dot(N, L))）
- 每个顶点一个系数

### 着色公式
```
lighting = ReconstructLight(LIGHT_SH, normal)
shading = albedo * lighting
```

## 调试技巧

### 问题1：导出后没有看到文件
- 检查 `prt_output/` 目录是否存在
- 检查文件大小是否非零
- 查看程序输出日志

### 问题2：启用 PRT 后全黑
- 检查数据文件是否正确导出
- 检查 LIGHT 系数是否非零
- 检查 LT 系数是否非零
- 尝试增加光源强度

### 问题3：旋转无效果
- 检查旋转系数是否正确计算
- 检查旋转角度是否正确更新
- 查看 Relighter::QueryCoefficients 是否返回不同的系数

### 问题4：效果与 PBR 不同
- 这是正常的，PRT 是预计算的近似
- 可以调整采样数量提高精度
- 可以调整光源强度和颜色

## 文件位置

| 文件 | 位置 |
|------|------|
| 可执行文件 | `bin/lightprobesh2.exe` |
| 源代码 | `examples/lightprobesh2/` |
| 导出数据 | `prt_output/` |
| 编译脚本 | `examples/lightprobesh2/compile.ps1` |

## 常用命令

```bash
# 编译
powershell -ExecutionPolicy Bypass -File "examples\lightprobesh2\compile.ps1"

# 运行
bin\lightprobesh2.exe

# 查看导出数据
type prt_output\prt_data_lighting.txt
type prt_output\prt_data_lt.txt

# 清理编译产物
cmake --build build --config Debug --target clean
```

## 参考文档

- `BUILD_SUCCESS_SUMMARY.md` - 编译修复总结
- `PRT_DEBUG_CHECKLIST.md` - 调试检查清单
- `COMPREHENSIVE_PRT_ANALYSIS.md` - 详细架构分析
- `PRT_ARCHITECTURE_ANALYSIS.md` - 架构分析

## 下一步

1. ✅ 编译成功
2. ⏭️ 运行程序并导出 PRT 数据
3. ⏭️ 验证导出的数据文件
4. ⏭️ 启用 PRT 渲染并观察效果
5. ⏭️ 旋转光源并验证动态效果
6. ⏭️ 根据需要调试和优化

祝你成功！[object Object]
