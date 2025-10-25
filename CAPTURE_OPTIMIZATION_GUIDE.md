# 多探针捕获优化指南 - 如何更好地捕获场景信息

## 🎯 核心问题分析

### 1. 当前捕获的是什么？

**立方体贴图内容**:
- ✅ 天空盒 (环境光照)
- ✅ glTF 模型 (场景几何)
- ✅ 预览模型 (可选)
- ❌ 动态光源信息
- ❌ 材质细节
- ❌ 法线贴图
- ❌ 深度信息

**捕获范围**:
- 距离探针 0.1 ~ 256 单位
- 90° FOV (每个立方体面)
- 完整 360° 球面覆盖

---

## 📊 UI 参数详细说明

### 探针网格配置

| 参数 | 类型 | 范围 | 说明 |
|------|------|------|------|
| **Probe Min X/Y/Z** | Float | -∞ ~ +∞ | 网格最小边界坐标 |
| **Probe Max X/Y/Z** | Float | -∞ ~ +∞ | 网格最大边界坐标 |
| **Probe Dim X/Y/Z** | Int | 1 ~ 20 | 各轴方向的探针数量 |
| **Probe Resolution** | Int | 4 ~ 256 | 立方体贴图分辨率 |

### 参数计算示例

**示例配置**:
```
Min: (-10, 0, -10)
Max: (10, 4, 10)
Dim: (3, 2, 3)
Resolution: 64
```

**计算结果**:
```
网格大小: 20 × 4 × 20
单元大小: (20/3, 4/2, 20/3) = (6.67, 2, 6.67)

探针位置:
- (0,0,0): (-10+3.33, 0+1, -10+3.33) = (-6.67, 1, -6.67)
- (1,0,0): (-10+10, 0+1, -10+3.33) = (0, 1, -6.67)
- (2,0,0): (-10+16.67, 0+1, -10+3.33) = (6.67, 1, -6.67)
... 总计 18 个探针

总内存: 18 × 6 × 64×64 × 8 bytes = ~442 MB
捕获时间: ~900ms (假设每个探针 50ms)
```

---

## 🔍 捕获流程详解

### 步骤 1: 探针生成 (PrepareProbes)

```cpp
// 清空旧探针
lightProbes.clear();

// 计算单元格大小
cellSize = (maxBounds - minBounds) / dimensions;

// 在每个单元中心放置探针
for (int x = 0; x < dimX; ++x) {
    for (int y = 0; y < dimY; ++y) {
        for (int z = 0; z < dimZ; ++z) {
            // 单元中心位置
            pos = minBounds + (x, y, z + 0.5) * cellSize;
            
            // 创建探针
            probe = new LightProbe(device, res, res);
            probe->SetPosition(pos);
            probe->setSkybox(skybox);
            probe->setPreviewModel(previewModel);
            probe->SetGltfModel(gltfModel);
            
            lightProbes.push_back(probe);
        }
    }
}
```

### 步骤 2: 立方体贴图捕获 (CaptureCubeMap)

```cpp
// 为每个探针的 6 个面捕获
for (uint32_t face = 0; face < 6; ++face) {
    // 1. 设置视图矩阵 (6 个方向)
    viewMatrix = lookAt(position, position + direction[face], up[face]);
    
    // 2. 设置投影矩阵 (90° FOV)
    projMatrix = perspective(90°, 1.0, 0.1, 256.0);
    
    // 3. 渲染场景
    renderPass->Draw(cmdBuffer, [this](VkCommandBuffer cmd) {
        skybox->Draw(cmd, descriptorSet, CAPTURE_SCENE);
        gltfModel->Draw(cmd, descriptorSet, CAPTURE_SCENE);
    });
    
    // 4. 同步 + 布局转换
    vkQueueWaitIdle(queue);
    transitionImageLayout(cubemap, SHADER_READ_ONLY_OPTIMAL);
}
```

### 步骤 3: 插值权重计算 (ComputeWeights)

```cpp
// 反距离加权 (IDW)
for (int i = 0; i < probes.size(); i++) {
    distance = length(queryPos - probes[i].position);
    
    if (distance > maxDistance) {
        weight[i] = 0.0;
    } else {
        // 权重 = 1 / (distance + epsilon)
        weight[i] = 1.0 / (distance + 0.01);
    }
}

// 归一化
totalWeight = sum(weight);
for (int i = 0; i < probes.size(); i++) {
    weight[i] /= totalWeight;
}
```

---

## 💡 改进建议

### 1. 增强捕获内容

**添加深度信息**:
```cpp
// 在 CaptureScenePass 中输出深度
layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDepth;

void main() {
    outColor = texture(skybox, direction);
    outDepth = gl_FragCoord.z;  // 线性化深度
}
```

**捕获法线贴图**:
```cpp
// 在第二个通道中捕获法线
layout(location = 0) out vec3 outNormal;

void main() {
    outNormal = normalize(normal);
}
```

### 2. 改进插值算法

**当前**: 返回权重最高的探针
**改进**: 在 GPU 上进行像素级混合

```glsl
// 计算着色器中的插值
vec3 interpolated = vec3(0.0);
for (int i = 0; i < numProbes; i++) {
    float weight = weights[i];
    vec3 sample = textureCube(cubemaps[i], direction).rgb;
    interpolated += sample * weight;
}
```

### 3. 支持多种插值方法

| 方法 | 优点 | 缺点 | 适用场景 |
|------|------|------|---------|
| **最近邻** | 快速 | 不连续 | 实时预览 |
| **线性** | 平衡 | 可能模糊 | 一般场景 |
| **三线性** | 平滑 | 较慢 | 高质量渲染 |
| **球面** | 自然 | 复杂 | 特殊效果 |

### 4. 添加调试可视化

**权重热力图**:
```cpp
// 在场景中显示权重分布
for (int i = 0; i < probes.size(); i++) {
    float weight = weights[i];
    vec3 color = heatmap(weight);  // 0=蓝, 1=红
    DrawSphere(probes[i].position, color, alpha=0.5);
}
```

**探针覆盖范围**:
```cpp
// 显示每个探针的影响范围
for (auto& probe : probes) {
    DrawWireSphere(probe.position, maxDistance, color=green);
}
```

### 5. 性能优化

**动态分辨率**:
```cpp
// 根据探针数量自动调整
if (probeCount > 100) {
    resolution = 16;   // 低质量
} else if (probeCount > 50) {
    resolution = 32;   // 中质量
} else {
    resolution = 64;   // 高质量
}
```

**缓存插值结果**:
```cpp
struct CacheEntry {
    glm::vec3 position;
    std::shared_ptr<TextureCubeMap> result;
    float timestamp;
};

// 相同位置的查询直接返回缓存
if (cache.contains(position)) {
    return cache[position].result;
}
```

---

## 📈 最佳实践

### 场景 1: 快速预览
```
Dim: 2×2×2 = 8 个探针
Resolution: 16×16
预期时间: ~400ms
内存: ~6 MB
```

### 场景 2: 平衡质量
```
Dim: 3×2×3 = 18 个探针
Resolution: 32×32
预期时间: ~900ms
内存: ~27 MB
```

### 场景 3: 高质量渲染
```
Dim: 4×3×4 = 48 个探针
Resolution: 64×64
预期时间: ~2400ms
内存: ~110 MB
```

---

## 🎯 下一步行动

1. ✅ 理解当前捕获流程
2. ⏳ 实现像素级插值
3. ⏳ 添加权重可视化
4. ⏳ 支持多种插值方法
5. ⏳ 实现缓存机制


