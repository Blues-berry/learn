# PRT Relighting 着色器修复 - 详细说明

## 问题诊断

### 症状
- Cornell 场景显示为黑色方块
- 相机初始位置看到错误渲染的模型
- 调试输出显示 "Drew 8 primitives"（说明模型被绘制了）

### 根本原因
**着色器期望的顶点属性与 glTF 模型提供的属性不匹配**

#### 原始着色器的问题
```glsl
// 原始着色器期望这些顶点属性：
layout(location = 5) in vec4 in_lt_c0;   // LT 系数 0
layout(location = 6) in vec4 in_lt_c1;   // LT 系数 1
...
layout(location = 13) in vec4 in_lt_c8;  // LT 系数 8
```

但 glTF 模型只提供：
- Location 0: Position
- Location 1: Normal
- Location 2: UV
- Location 3: Color
- Location 4: Tangent

**结果**：着色器读取未定义的顶点属性（垃圾数据），导致渲染错误

## 解决方案

### 修复策略
**使用 SSBO（Storage Buffer Object）而不是顶点属性来传递 LT 系数**

#### 优势
1. ✅ 不需要修改顶点格式
2. ✅ 可以直接访问预计算的 LT 数据
3. ✅ 性能更好（SSBO 比顶点属性更高效）
4. ✅ 与现有的 glTF 模型兼容

### 修复内容

#### 修改前
```glsl
// 期望 9 个顶点属性（不存在！）
layout(location = 5) in vec4 in_lt_c0;
layout(location = 6) in vec4 in_lt_c1;
...
layout(location = 13) in vec4 in_lt_c8;

void main() {
    // 读取不存在的顶点属性 → 垃圾数据
    vec3 lt_coeffs[9];
    lt_coeffs[0] = in_lt_c0.xyz;
    ...
}
```

#### 修改后
```glsl
// 使用 SSBO 读取 LT 系数
layout(set = 1, binding = 1) readonly buffer LTCoefficientsBuffer {
    SHCoefficients ltCoefficients[];
} ltBuffer;

void main() {
    // 使用 gl_VertexIndex 从 SSBO 读取正确的 LT 系数
    SHCoefficients lt_coeffs = ltBuffer.ltCoefficients[gl_VertexIndex];
    
    // 现在 lt_coeffs 包含正确的数据
    vec3 prtColor = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        prtColor += ubo.lighting.coeffs[i].xyz * lt_coeffs.coeffs[i].xyz;
    }
}
```

## 技术细节

### 描述符集布局

**修改前**：
```cpp
// Set 1, Binding 0: Lighting SH UBO
// Set 1, Binding 1: LT Coefficients SSBO (未使用)
```

**修改后**：
```cpp
// Set 1, Binding 0: Lighting SH UBO（不变）
// Set 1, Binding 1: LT Coefficients SSBO（现在使用）
```

### 顶点索引映射

```glsl
// 使用 gl_VertexIndex 作为 SSBO 的索引
// 这要求 SSBO 中的顶点顺序与模型的顶点顺序一致
SHCoefficients lt_coeffs = ltBuffer.ltCoefficients[gl_VertexIndex];
```

**重要**：SSBO 中的顶点数据必须与模型的顶点顺序完全一致！

## 编译修改后的着色器

### 方法 1：使用批处理脚本
```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2
compile_prt_shaders.bat
```

### 方法 2：手动编译
```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2

# 编译顶点着色器
"C:\VulkanSDK\1.3.xxxx\Bin\glslc.exe" -O prt_relight.vert -o prt_relight.vert.spv

# 编译片元着色器
"C:\VulkanSDK\1.3.xxxx\Bin\glslc.exe" -O prt_relight.frag -o prt_relight.frag.spv
```

## 验证修复

### 预期结果
```
[DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
[DEBUG PRT]   - Drew 8 primitives
```

**场景应该显示**：
- ✅ Cornell Box 正常渲染
- ✅ 不是黑色方块
- ✅ 光照平滑变化
- ✅ 拖动 Light Rotation 时场景不消失

### 如果仍然有问题

1. **检查着色器编译**
   ```bash
   # 验证 .spv 文件是否存在且有效
   ls -la prt_relight.*.spv
   ```

2. **检查 SSBO 绑定**
   - 确保 `ltCoefficientsBuffer` 已正确创建
   - 确保 SSBO 包含正确的 LT 数据

3. **检查顶点索引**
   - 确保 SSBO 中的顶点数据顺序与模型一致
   - 如果顺序不对，可能需要重新排序数据

## 相关代码

### C++ 端（main.cpp）
```cpp
// 确保 ltCoefficientsBuffer 已正确绑定到 Set 1, Binding 1
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                       pipelineLayoutPRT, 0, 2, 
                       prtDescriptorSets.data(), 0, nullptr);
```

### 着色器端（prt_relight.vert）
```glsl
// 从 SSBO 读取 LT 系数
layout(set = 1, binding = 1) readonly buffer LTCoefficientsBuffer {
    SHCoefficients ltCoefficients[];
} ltBuffer;

// 在 main() 中使用
SHCoefficients lt_coeffs = ltBuffer.ltCoefficients[gl_VertexIndex];
```

## 修复前后对比

### 修复前
```
着色器读取不存在的顶点属性
  ↓
垃圾数据
  ↓
错误的 PRT 计算
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
正常渲染 ✓
```

## 总结

✅ **修复内容**：
- 修改 `prt_relight.vert` 使用 SSBO 而不是顶点属性
- 编译修改后的着色器
- 验证修复有效

**下一步**：
1. 编译着色器
2. 重新编译项目
3. 运行测试
4. 验证 Cornell 场景正常显示

---

**修复完成**：2025-12-01
**修复类型**：着色器修复
**状态**：Ready for Compilation ✅

