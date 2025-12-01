# PRT Relighting 着色器修复 - 最终总结

## 问题诊断结果

### 症状
- Cornell 场景显示为**黑色方块**
- 调试输出显示 "Drew 8 primitives"（模型被绘制了）
- 相机初始位置看到错误渲染

### 根本原因
**着色器期望的顶点属性与 glTF 模型提供的属性不匹配**

原始着色器期望：
```glsl
layout(location = 5) in vec4 in_lt_c0;   // 不存在！
layout(location = 6) in vec4 in_lt_c1;   // 不存在！
...
layout(location = 13) in vec4 in_lt_c8;  // 不存在！
```

glTF 模型只提供：
```glsl
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
// 没有 locations 5-13！
```

**结果**：着色器读取未定义的属性 → 垃圾数据 → 黑色方块

## 修复方案

### 修改内容

**文件**：`shaders/glsl/lightprobesh2/prt_relight.vert`

**改动**：使用 SSBO 代替顶点属性

```glsl
// 修改前：期望 9 个顶点属性（不存在）
layout(location = 5) in vec4 in_lt_c0;
...

// 修改后：使用 SSBO
layout(set = 1, binding = 1) readonly buffer LTCoefficientsBuffer {
    SHCoefficients ltCoefficients[];
} ltBuffer;

void main() {
    // 从 SSBO 读取 LT 系数
    SHCoefficients lt_coeffs = ltBuffer.ltCoefficients[gl_VertexIndex];
    
    // 现在数据正确了！
    vec3 prtColor = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        prtColor += ubo.lighting.coeffs[i].xyz * lt_coeffs.coeffs[i].xyz;
    }
}
```

## 实施步骤

### 1. 编译着色器

```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2

# 使用脚本（推荐）
compile_prt_shaders.bat

# 或手动编译
glslc.exe -O prt_relight.vert -o prt_relight.vert.spv
glslc.exe -O prt_relight.frag -o prt_relight.frag.spv
```

### 2. 重新编译项目

```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\build
cmake --build . --config Release
```

### 3. 运行测试

```bash
bin\lightprobesh2.exe
```

## 验证修复

### 预期输出
```
[DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
[DEBUG PRT]   - pipelinePRT: 0x...
[DEBUG PRT]   - pipelineLayoutPRT: 0x...
[DEBUG PRT]   - descriptorSetPRT: 0x...
[DEBUG PRT]   - mainPass->descriptorSet: 0x...
[DEBUG PRT]   - Drew 8 primitives
```

### 预期结果
- ✅ Cornell Box 正常显示（不是黑色方块）
- ✅ 光照效果正确
- ✅ 拖动 Light Rotation 时场景平滑渲染
- ✅ 无错误或警告

## 修复前后对比

### 修复前
```
着色器读取不存在的顶点属性
  ↓
垃圾数据（通常是 0 或随机值）
  ↓
PRT 计算错误
  ↓
黑色方块 ✗
```

### 修复后
```
着色器从 SSBO 读取 LT 系数
  ↓
正确的数据
  ↓
正确的 PRT 计算
  ↓
Cornell Box 正常显示 ✓
```

## 技术细节

### 为什么使用 SSBO？

| 方案 | 优点 | 缺点 |
|------|------|------|
| 顶点属性 | 标准方式 | 需要修改顶点格式 |
| **SSBO** | **无需修改顶点格式** | **需要 gl_VertexIndex** |
| 纹理 | 灵活 | 性能较差 |

**SSBO 是最佳选择**因为：
1. ✅ 不需要修改 glTF 顶点格式
2. ✅ 性能优于纹理
3. ✅ 与现有代码兼容
4. ✅ 易于维护

### 顶点索引映射

```glsl
// gl_VertexIndex 自动提供当前顶点的索引
// SSBO 中的顶点数据必须与模型顶点顺序一致
SHCoefficients lt_coeffs = ltBuffer.ltCoefficients[gl_VertexIndex];
```

## 故障排除

### 如果仍然显示黑色方块

1. **检查着色器编译**
   ```bash
   ls -la prt_relight.*.spv
   ```

2. **检查项目重新编译**
   ```bash
   cmake --build . --config Release --clean-first
   ```

3. **检查 SSBO 绑定**
   - 确保 `ltCoefficientsBuffer` 已创建
   - 确保绑定到 Set 1, Binding 1

4. **启用 Vulkan 验证层**
   - 查看是否有描述符集错误

## 相关文件

| 文件 | 说明 |
|------|------|
| `prt_relight.vert` | 修改的顶点着色器 |
| `compile_prt_shaders.bat` | 编译脚本 |
| `SHADER_FIX_EXPLANATION.md` | 详细说明 |
| `COMPLETE_FIX_STEPS.md` | 完整步骤 |

## 总结

✅ **问题已识别**：顶点属性不匹配

✅ **解决方案已实现**：使用 SSBO

✅ **着色器已修改**：`prt_relight.vert`

✅ **编译脚本已提供**：`compile_prt_shaders.bat`

**下一步**：
1. 编译着色器
2. 重新编译项目
3. 运行测试
4. 验证 Cornell Box 正常显示

---

**修复完成**：2025-12-01
**状态**：Ready for Compilation ✅

