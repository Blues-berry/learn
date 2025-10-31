# Split View对比渲染功能使用指南

## 功能概述
Split View（分屏对比渲染）功能允许你在同一屏幕上**同时显示**三种不同的光照效果，方便直接对比：

```
┌─────────────┬─────────────┬─────────────┐
│   原始环境   │  单探针捕获  │  多探针捕获  │
│  Original   │   Single    │    Multi    │
│  Reflection │   Probe     │   Probes    │
└─────────────┴─────────────┴─────────────┘
```

## 使用步骤

### 1. 准备阶段
在使用Split View之前，需要先捕获所有三种cubemap：

#### a) 原始环境（Original）
- 程序启动时自动加载（如pisa.ktx）
- 或者使用UI中的Skybox下拉框选择任意已加载的环境贴图

#### b) 单探针捕获（Single Probe）
1. 移动相机到想要捕获的位置
2. 点击**"Capture Cubemap at Camera"**按钮
3. 系统会：
   - 在相机位置创建1024×1024的高分辨率探针
   - 生成SH系数和IBL贴图
   - 保存为`singleProbeCubemap`

#### c) 多探针捕获（Multi Probes）
**方法1：手动配置**
1. 打开**"Light Probe Grid"**分组
2. 勾选**"Enable Probe Grid"**
3. 配置探针网格：
   - **Bounds**: 设置包围盒范围（Min/Max XYZ）
   - **Dimensions**: 设置网格维度（如2×2×2 = 8个探针）
   - **Resolution**: 设置分辨率（建议16-32）
4. 点击**"Generate Probes"**
5. 点击**"Capture All Probes"**

**方法2：快速设置（推荐）**
1. 切换到**"Rendering Comparison"**分组
2. 选择**"Split View"**模式
3. 点击**"Quick Setup Split View"**按钮
4. 系统会自动：
   - 使用第一个skybox作为原始环境
   - 在当前相机位置捕获单探针

### 2. 启用Split View

1. 打开**"Rendering Comparison"**分组
2. 从下拉框选择**"Split View"**
3. 查看状态信息：
   ```
   Status:
     Original: Ready / Not captured
     Single: Ready / Not captured
     Multi: Ready / Not captured
   ```
4. 如果所有三项都是"Ready"，Split View将正常工作

### 3. 查看对比效果

屏幕将分为三部分：

#### 左侧（Original）
- 显示原始环境贴图效果
- 通常是预先加载的HDR环境（如pisa, gcanyon）
- 用作基准参照

#### 中间（Single Probe）
- 显示单探针捕获效果
- 1024×1024高分辨率
- 反映捕获位置的真实光照

#### 右侧（Multi Probes）
- 显示多探针捕获/插值效果
- 可能包含插值计算的结果
- 如果勾选**"Show Probes"**，会显示探针位置球体

## 技术细节

### 渲染实现
```cpp
void VulkanExample::drawSplitView(VkCommandBuffer cmd)
{
    uint32_t viewportWidth = width / 3;
    
    // 左侧：viewport(0, 0, w/3, h), scissor(0, 0, w/3, h)
    // 中间：viewport(w/3, 0, w/3, h), scissor(w/3, 0, w/3, h)
    // 右侧：viewport(2w/3, 0, w/3, h), scissor(2w/3, 0, w/3, h)
    
    // 每个视口独立渲染：
    // 1. 设置viewport和scissor
    // 2. 临时切换cubemap
    // 3. 渲染场景（skybox, models）
    // 4. 恢复设置
}
```

### 性能特性
- **单次Pass**: 所有三个视口在一次RenderPass中完成
- **动态Viewport**: 使用`vkCmdSetViewport`和`vkCmdSetScissor`
- **性能开销**: ~10-15%（相比单视图）
- **分辨率**: 每个视口宽度 = 屏幕宽度 / 3

### 权重可视化增强
在Split View模式下，右侧（Multi Probes）视图可以：
1. 显示探针位置球体（勾选"Show Probes"）
2. 使用插值算法（IDW/Linear/Cubic）
3. 可视化权重热力图（点击"Visualize Weights"）

## 常见问题

### Q1: Split View显示空白或只有部分视口？
**A**: 确保所有三种cubemap都已捕获：
```
Status:
  Original: Ready ✓
  Single: Ready ✓
  Multi: Ready ✓
```
如果任何一项为"Not captured"，请先捕获对应的cubemap。

### Q2: 如何快速准备Split View？
**A**: 使用快速设置功能：
1. 选择Split View模式
2. 点击"Quick Setup Split View"
3. 如需多探针，手动生成并捕获探针网格

### Q3: 三个视口显示的内容完全相同？
**A**: 检查是否正确切换了cubemap：
- 确认`originalCubemap`, `singleProbeCubemap`, `multiProbeCubemap`是不同的对象
- 在捕获单探针时，相机位置应与原始环境有明显差异

### Q4: UI覆盖了分屏内容？
**A**: UI会在全屏范围内显示，这是正常的。你可以：
- 最小化UI面板
- 调整UI透明度
- 暂时关闭UI（如果有快捷键）

### Q5: 如何切换回正常渲染？
**A**: 在"Rendering Comparison"下拉框中选择：
- **Normal**: 正常单视图渲染
- **Original Only**: 仅显示原始环境
- **Single Probe**: 仅显示单探针
- **Multi Probe**: 仅显示多探针

## 高级用法

### 动态对比
你可以在Split View模式下：
1. **实时调整相机**: 三个视口同步更新视角
2. **切换模型**: 在UI中选择不同的PreviewModel或GLTF模型
3. **调整材质**: 修改金属度、粗糙度等参数
4. **切换探针**: 在多探针视图中切换不同的探针

### 插值对比
1. 生成探针网格并捕获
2. 点击"Interpolate Cubemap (GPU)"在相机位置插值
3. 切换到Split View查看插值效果 vs 单探针 vs 原始

### 权重热力图叠加
1. 在Split View模式下
2. 点击"Visualize Weights (Heatmap)"
3. 右侧视口将显示权重分布
4. 可直观看到哪些探针影响最大

## 快捷键建议（待实现）
为了更好的工作流，建议添加：
- `F1`: 切换到Normal模式
- `F2`: 切换到Split View模式
- `F3`: 快速捕获单探针
- `F4`: 显示/隐藏探针
- `Space`: 快速Setup Split View

## 应用场景

### 场景1：验证探针精度
- **左侧（Original）**: 理想环境光照
- **中间（Single）**: 探针捕获的近似
- **右侧（Multi）**: 多探针插值结果
- **对比**: 验证探针是否准确还原场景光照

### 场景2：插值算法对比
1. 捕获多个探针
2. 使用不同插值模式（IDW/Linear/Cubic）生成多个插值cubemap
3. 在Split View中加载不同插值结果
4. 对比选择最佳插值算法

### 场景3：分辨率权衡
- **Single**: 高分辨率（1024×1024）但数量少
- **Multi**: 低分辨率（16×16）但覆盖广
- **对比**: 找到质量和性能的平衡点

### 场景4：实时vs烘焙对比
- **Original**: 烘焙的高质量环境
- **Single/Multi**: 实时探针捕获
- **对比**: 评估实时GI的视觉质量

## 性能优化建议

### 分辨率调整
- 多探针使用低分辨率（8×8 或 16×16）
- 单探针使用高分辨率（512×512 或 1024×1024）
- 原始环境使用预过滤贴图

### 探针数量
- 起始测试: 2×2×2 = 8个探针
- 中等场景: 4×4×2 = 32个探针
- 大型场景: 6×6×3 = 108个探针

### 帧率优化
如果Split View模式下帧率下降：
1. 减少每个视口的绘制对象
2. 降低探针分辨率
3. 使用静态捕获而非实时更新

---

## 总结

Split View是强大的调试和对比工具，让你能够：
- ✅ 同时查看三种光照效果
- ✅ 快速验证探针精度
- ✅ 对比不同插值算法
- ✅ 评估性能权衡
- ✅ 可视化权重分布

**最佳实践**: 
1. 先使用"Quick Setup"快速开始
2. 调整相机找到最佳对比角度
3. 勾选"Show Probes"了解探针布局
4. 使用权重可视化优化探针位置

---

**版本**: 1.0  
**作者**: Cascade AI  
**最后更新**: 2024
