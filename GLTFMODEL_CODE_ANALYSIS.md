# GltfModel 绘制代码详细注释

## Draw() 函数完整分析

```cpp
void GltfModel::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech)
{
    // ============ 第一步：安全检查 ============
    if (!model) {
        return;  // 模型未加载，直接返回
    }

    uint32_t techIdx = (uint32_t)tech;

    // ✅ 安全检查：确保 PSO 已经准备好
    // PSO = Pipeline State Object（管线状态对象）
    // 如果管线或管线布局未初始化，说明 PreparePSO() 未被调用
    if (techniques[techIdx].pso == VK_NULL_HANDLE || 
        techniques[techIdx].pipelineLayout == VK_NULL_HANDLE) {
        std::cerr << "GltfModel::Draw - PSO not prepared for technique " << techIdx << "\n";
        return;
    }

    // ============ 第二步：绑定描述符集 ============
    // 创建描述符集数组，包含两个描述符集：
    // - Set 0: globalSet（来自 MainPass，包含全局数据）
    // - Set 1: descriptorSet（模型自己的数据）
    std::vector<VkDescriptorSet> sets = {
        globalSet,      // Set 0: 全局数据（投影、视图、光照等）
        descriptorSet   // Set 1: 模型数据（变换、材质）
    };

    // 绑定描述符集到管线
    // 参数说明：
    // - cmd: 命令缓冲区
    // - VK_PIPELINE_BIND_POINT_GRAPHICS: 图形管线绑定点
    // - pipelineLayout: 管线布局（定义了描述符集的布局）
    // - 0: 第一个描述符集的绑定点
    // - sets.size(): 描述符集数量
    // - sets.data(): 描述符集数组指针
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                           techniques[techIdx].pipelineLayout, 
                           0, static_cast<uint32_t>(sets.size()), 
                           sets.data(), 0, NULL);

    // ============ 第三步：绑定管线 ============
    // 绑定图形管线到命令缓冲区
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                     techniques[techIdx].pso);

    // ============ 第四步：定义 Push Constant 结构 ============
    struct PushConstantBlock {
        glm::mat4 modelOffset;  // 模型变换矩阵（平移 + 缩放）
        glm::vec4 tint;         // 颜色着色（RGB + Alpha）
    } pc;

    // ============ 第五步：定义实例位置和颜色 ============
    // 定义 4 个不同位置的偏移量
    const glm::vec3 offsets[4] = {
        glm::vec3(-20.0f, 0.0f, 0.0f),   // 左
        glm::vec3(20.0f,  0.0f, 0.0f),   // 右
        glm::vec3(0.0f,   0.0f, -20.0f), // 后
        glm::vec3(0.0f,   0.0f, 20.0f)   // 前
    };
    
    const float scale = 50.0f;  // 统一缩放因子
    
    // 定义 3 种颜色（循环使用）
    const glm::vec3 colors[3] = {
        glm::vec3(1.0f, 0.3f, 0.3f),  // 红色
        glm::vec3(0.3f, 1.0f, 0.3f),  // 绿色
        glm::vec3(0.3f, 0.5f, 1.0f)   // 蓝色
    };

    // ============ 第六步：多实例绘制循环 ============
    // ✅ 修复：绘制 4 个不同位置的模型（MAIN 和 CAPTURE_SCENE 都一样）
    for (int i = 0; i < 4; ++i) {
        // 计算模型变换矩阵：先平移，再缩放
        // 顺序很重要：translate * scale 表示先缩放后平移
        pc.modelOffset = glm::translate(glm::mat4(1.0f), offsets[i]) * 
                        glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        
        // 设置颜色（循环使用 3 种颜色）
        pc.tint = glm::vec4(colors[i % 3], 1.0f);
        
        // 发送 Push Constant 到着色器
        // 参数说明：
        // - cmd: 命令缓冲区
        // - pipelineLayout: 管线布局
        // - VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT: 
        //   Push Constant 对顶点和片段着色器都可见
        // - 0: Push Constant 的偏移量
        // - sizeof(PushConstantBlock): Push Constant 的大小（80 字节）
        // - &pc: Push Constant 数据指针
        vkCmdPushConstants(cmd, techniques[techIdx].pipelineLayout, 
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                          0, sizeof(PushConstantBlock), &pc);

        // ============ 第七步：根据技术类型调用不同的绘制方法 ============
        // ✅ 修复：CAPTURE_SCENE 需要传递 pipelineLayout 和 bindImageSet 参数
        if (tech == ETechnique::CAPTURE_SCENE) {
            // 场景捕获模式：用于光照探针捕获
            // 参数说明：
            // - cmd: 命令缓冲区
            // - vkglTF::RenderFlags::BindImages: 绑定图像资源的标志
            // - pipelineLayout: 管线布局（用于绑定图像）
            // - 1: 描述符集索引（Set 1）
            model->draw(cmd, vkglTF::RenderFlags::BindImages, 
                       techniques[techIdx].pipelineLayout, 1);
        } else {
            // 标准渲染模式：使用默认绑定
            model->draw(cmd);
        }
    }
}
```

---

## 关键概念解释

### 1. 描述符集的两层架构

**为什么需要两个描述符集？**

- **Set 0 (globalSet)**：包含所有模型共享的全局数据
  - 投影矩阵、视图矩阵
  - 光照信息
  - 环境光照（SH、IBL）
  - 优点：多个模型可以共享同一个 Set 0，减少内存占用

- **Set 1 (descriptorSet)**：包含每个模型特有的数据
  - 模型变换矩阵
  - 材质参数
  - 优点：每个模型可以有不同的材质和变换

### 2. Push Constant vs UBO

| 特性 | Push Constant | UBO |
|------|---------------|-----|
| 大小 | 最多 256 字节 | 无限制 |
| 更新频率 | 每次绘制调用 | 较少更新 |
| 性能 | 快速（直接写入） | 较慢（需要缓冲区） |
| 用途 | 每实例数据 | 共享数据 |

**本代码中的使用：**
- Push Constant：modelOffset（每实例变换）+ tint（每实例颜色）
- UBO：全局数据、材质参数

### 3. 矩阵变换顺序

```cpp
pc.modelOffset = glm::translate(...) * glm::scale(...);
```

**执行顺序（从右到左）：**
1. 先执行 scale：缩放顶点
2. 再执行 translate：平移缩放后的顶点

**在着色器中的应用：**
```glsl
gl_Position = projection * view * modelOffset * model * vec4(inPos, 1.0);
```

---

## 性能分析

### 优点
✅ 使用 Push Constant 减少描述符集更新
✅ 共享全局描述符集，减少内存占用
✅ 单次 Draw Call 绘制 4 个实例

### 改进空间
⚠️ 可以使用 Indirect Draw 进一步减少 CPU 开销
⚠️ 可以使用 Instancing 替代循环


