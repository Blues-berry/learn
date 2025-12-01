# PRT Relighting 测试指南

## 问题修复总结

**问题**：启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块，Cornell 场景消失

**根本原因**：描述符集绑定冲突和管线布局不匹配

**修复方案**：手动管理 PRT 管线的渲染，避免使用 `gltfModel->Draw()` 函数

## 编译步骤

### 1. 编译着色器（如果未编译）

```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2

# 编译 PRT relighting 着色器
"C:\VulkanSDK\1.3.xxxx\Bin\glslc.exe" -O prt_relight.vert -o prt_relight.vert.spv
"C:\VulkanSDK\1.3.xxxx\Bin\glslc.exe" -O prt_relight.frag -o prt_relight.frag.spv
```

### 2. 编译项目

```bash
cd C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\build
cmake --build . --config Release
```

## 运行和测试

### 步骤 1：启动程序

```bash
bin\lightprobesh2.exe
```

### 步骤 2：加载 Cornell 模型

1. 在 UI 中找到 "Model" 下拉菜单
2. 选择 "cornell" 或 "CornellBox-Original"
3. 等待模型加载完成

### 步骤 3：启用 PRT Relighting

1. 在 UI 中找到 "PRT Relighting" 复选框
2. 勾选启用 PRT Relighting
3. 观察控制台输出，应该看到：
   ```
   [DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
   [DEBUG PRT]   - pipelinePRT: 0x...
   [DEBUG PRT]   - pipelineLayoutPRT: 0x...
   [DEBUG PRT]   - descriptorSetPRT: 0x...
   [DEBUG PRT]   - mainPass->descriptorSet: 0x...
   [DEBUG PRT]   - Drew X primitives
   ```

### 步骤 4：测试光源旋转

1. 在 UI 中找到 "Light" 部分
2. 找到 "Light Rotation" 滑块
3. **缓慢拖动**滑块从 0 到 6.28（完整旋转）
4. **预期结果**：
   - Cornell 场景应该始终可见
   - 光照应该平滑地随旋转而改变
   - 场景不应该消失或闪烁

### 步骤 5：验证光照效果

观察以下现象：
- 当光源旋转到红色墙壁时，场景应该泛出红光
- 当光源旋转到绿色墙壁时，场景应该泛出绿光
- 当光源旋转到白色天花板时，场景应该变亮
- 当光源旋转到黑色地板时，场景应该变暗

## 调试信息解读

### 控制台输出

```
[DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
```
- 表示 PRT 管线被正确激活

```
[DEBUG PRT] Drew X primitives
```
- X 应该是一个正数（Cornell 模型的原始数量）
- 如果 X = 0，说明没有绘制任何东西，需要检查模型加载

### 如果场景消失

检查以下几点：

1. **着色器编译错误**
   - 检查 `prt_relight.vert.spv` 和 `prt_relight.frag.spv` 是否存在
   - 重新编译着色器

2. **描述符集绑定错误**
   - 查看控制台是否有 Vulkan 验证错误
   - 检查 `descriptorSetPRT` 是否有效

3. **管线布局不匹配**
   - 确保 `pipelineLayoutPRT` 与着色器期望的布局一致
   - 检查 `preparePRTRelightingPipeline()` 函数

4. **顶点缓冲区绑定错误**
   - 确保 `gltfModel->getModel()->bindBuffers(cmd)` 被正确调用
   - 检查顶点格式是否与着色器兼容

## 性能考虑

- PRT Relighting 使用球谐函数重建光照，计算量较小
- 每帧的性能开销应该与标准 PBR 渲染相似
- 如果帧率下降，检查是否有其他渲染通道被激活

## 下一步工作

1. **优化光照质量**
   - 增加 SH 采样数量以提高精度
   - 调整旋转步长（目前是 24 个旋转）

2. **添加镜面反射**
   - 在 PRT 着色器中添加基于 IBL 的镜面反射
   - 使用旋转后的环境贴图

3. **性能优化**
   - 使用 GPU 计算着色器预计算 LT 系数
   - 实现 LOD 系统以支持更大的模型

## 参考文件

- `main.cpp`：主渲染循环（第 714-780 行）
- `prt_relight.vert`：顶点着色器
- `prt_relight.frag`：片元着色器
- `PRT_ROTATION_BUG_FIX.md`：详细的技术文档

