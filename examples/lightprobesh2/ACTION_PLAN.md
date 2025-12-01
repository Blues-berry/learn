# PRT Relighting 修复 - 行动计划

## 🎯 目标

修复启用 PRT Relighting 后 Cornell 场景显示为黑色方块的问题

## ⚠️ 问题诊断

**症状**：黑色方块而不是 Cornell Box

**原因**：着色器期望的顶点属性不存在

**解决**：使用 SSBO 代替顶点属性

## 📋 立即行动（现在就做）

### 1️⃣ 编译着色器（5 分钟）

```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2
compile_prt_shaders.bat
```

**验证**：应该看到：
```
Compiling prt_relight.vert...
OK
Compiling prt_relight.frag...
OK
All shaders compiled successfully!
```

### 2️⃣ 重新编译项目（10 分钟）

```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\build
cmake --build . --config Release
```

**验证**：编译成功，无错误

### 3️⃣ 运行程序（1 分钟）

```bash
bin\lightprobesh2.exe
```

### 4️⃣ 测试修复（5 分钟）

1. 加载 Cornell 模型
   - UI → Model → cornell

2. 启用 PRT Relighting
   - UI → PRT Relighting ✓

3. 验证场景显示
   - ✅ 应该看到 Cornell Box（不是黑色方块）
   - ✅ 应该看到光照效果

4. 拖动 Light Rotation
   - ✅ 场景应该平滑渲染
   - ✅ 光照应该随旋转而改变

## 📊 预期结果

### 修复前
```
启用 PRT Relighting → 黑色方块 ✗
```

### 修复后
```
启用 PRT Relighting → Cornell Box 正常显示 ✓
拖动 Light Rotation → 光照平滑变化 ✓
```

## 🔍 验证清单

- [ ] 着色器已编译
  - [ ] `prt_relight.vert.spv` 存在
  - [ ] `prt_relight.frag.spv` 存在

- [ ] 项目已编译
  - [ ] 无编译错误
  - [ ] 可执行文件已生成

- [ ] 程序运行正常
  - [ ] 程序启动无崩溃
  - [ ] UI 正常显示

- [ ] 修复验证
  - [ ] Cornell 模型可见
  - [ ] 不是黑色方块
  - [ ] 光照效果正确
  - [ ] 拖动滑块时场景不消失

## 🚨 如果出现问题

### 问题 1：着色器编译失败
```bash
# 检查 Vulkan SDK
glslc.exe --version

# 手动编译查看错误
glslc.exe prt_relight.vert
```

### 问题 2：场景仍然是黑色方块
```bash
# 清除缓存并重新编译
cd build
cmake --build . --config Release --clean-first
```

### 问题 3：程序崩溃
- 检查控制台输出中的错误信息
- 启用 Vulkan 验证层
- 查看 `SHADER_FIX_EXPLANATION.md`

## 📚 参考文档

| 文档 | 说明 |
|------|------|
| `FINAL_SHADER_FIX_SUMMARY.md` | 修复总结 |
| `SHADER_FIX_EXPLANATION.md` | 详细说明 |
| `COMPLETE_FIX_STEPS.md` | 完整步骤 |

## ⏱️ 时间估计

| 步骤 | 时间 |
|------|------|
| 编译着色器 | 5 分钟 |
| 重新编译项目 | 10 分钟 |
| 运行程序 | 1 分钟 |
| 测试修复 | 5 分钟 |
| **总计** | **~20 分钟** |

## 🎉 成功标志

✅ Cornell Box 正常显示
✅ 光照效果正确
✅ 拖动 Light Rotation 时场景不消失
✅ 控制台输出正确的调试信息

## 📞 需要帮助？

1. 查看 `SHADER_FIX_EXPLANATION.md` 了解技术细节
2. 查看 `COMPLETE_FIX_STEPS.md` 了解详细步骤
3. 检查控制台输出中的错误信息
4. 启用 Vulkan 验证层获取更多信息

## 🔄 下一步

修复完成后：
1. ✅ 验证 PRT Relighting 功能正常
2. ✅ 测试其他模型
3. ✅ 性能基准测试
4. ✅ 优化光照质量

---

**准备好了吗？开始吧！** 🚀

1. 编译着色器
2. 重新编译项目
3. 运行测试
4. 验证修复

**预计时间**：20 分钟

**预期结果**：PRT Relighting 功能完全正常工作 ✅

