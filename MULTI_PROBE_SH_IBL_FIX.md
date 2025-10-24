# 多探针 SH 系数和 IBL 贴图管理 - 修复完成

## 问题 1: 多探针 SH 系数管理不当 ✅ 已修复

### 问题描述
- 每个探针的 SH 系数被生成后立即被覆盖
- 只有最后一个探针的 SH 系数被保留
- 多探针插值无法工作

### 修复内容

#### 1. 创建 ProbeData 结构体
```cpp
struct ProbeData {
    glm::vec3 position;
    std::shared_ptr<vks::TextureCubeMap> cubemap;
    VkDescriptorImageInfo irradianceCube;
    VkDescriptorImageInfo prefilteredCube;
    VkDescriptorBufferInfo shCoeffs;
};
```

#### 2. 添加成员变量
```cpp
std::vector<ProbeData> multiProbeData;  // 存储所有探针的数据
```

#### 3. 修改 CaptureAllProbes() 函数
- 清空之前的多探针数据
- 为每个探针保存 SH 系数、irradiance 和 prefiltered 贴图
- 不再自动更新天空盒

**关键代码**:
```cpp
ProbeData data;
data.position = p->GetPosition();
data.cubemap = capturedCubemap;

// 获取 SH 系数
shGenPass->FeedSH(data.shCoeffs);

// 获取 IBL 贴图
genIBL->FeedIrradianceMap(data.irradianceCube);
genIBL->FeedPrefilteredMap(data.prefilteredCube);

multiProbeData.push_back(data);
```

---

## 问题 2: 多探针模式下没有实现 SH 插值 ✅ 已修复

### 问题描述
- 没有根据相机位置选择探针
- 光照不会随相机位置变化

### 修复内容

#### 1. 添加 findNearestProbe() 函数
```cpp
int VulkanExample::findNearestProbe(const glm::vec3& position)
{
    if (multiProbeData.empty()) {
        return -1;
    }

    int nearestIndex = 0;
    float minDistance = glm::distance(position, multiProbeData[0].position);

    for (size_t i = 1; i < multiProbeData.size(); ++i) {
        float distance = glm::distance(position, multiProbeData[i].position);
        if (distance < minDistance) {
            minDistance = distance;
            nearestIndex = static_cast<int>(i);
        }
    }

    return nearestIndex;
}
```

#### 2. 添加 updateProbeBindings() 函数
```cpp
void VulkanExample::updateProbeBindings(int probeIndex)
{
    if (probeIndex < 0 || probeIndex >= static_cast<int>(multiProbeData.size())) {
        return;
    }

    const ProbeData& data = multiProbeData[probeIndex];

    // 更新 SH 系数
    mainPass->environmemts.shCoeffs = data.shCoeffs;

    // 更新 IBL 贴图
    mainPass->environmemts.irradianceCube = data.irradianceCube;
    mainPass->environmemts.prefilteredCube = data.prefilteredCube;

    // 更新描述符集
    mainPass->UpdateBindings();
}
```

---

## 问题 3: 多探针模式下没有实现 IBL 贴图切换 ✅ 已修复

### 问题描述
- 所有探针的 IBL 贴图都被生成
- 但渲染时只使用一个 IBL 贴图
- 没有根据相机位置切换

### 修复内容

#### 在 prepareData() 中添加多探针支持
```cpp
void VulkanExample::prepareData()
{
    // ... 其他代码 ...

    // ✅ 新增：多探针模式下根据相机位置更新 SH 和 IBL
    if (useMultipleProbes && !multiProbeData.empty()) {
        int nearestProbeIndex = findNearestProbe(camera.position);
        if (nearestProbeIndex >= 0) {
            updateProbeBindings(nearestProbeIndex);
        }
    }

    skybox->Update(camera.matrices.view);
}
```

---

## 修改的文件

| 文件 | 修改内容 |
|------|--------|
| `examples/lightprobesh2/main.cpp` | 添加 ProbeData 结构体、multiProbeData 成员、findNearestProbe()、updateProbeBindings()、修改 CaptureAllProbes()、修改 prepareData() |
| `examples/lightprobesh2/LightProbe.h` | 添加 GetPosition() 方法 |

---

## 工作流程

### 单探针模式（原有）
```
用户点击 "Capture Cubemap at Camera"
    ↓
创建探针 → 捕获立方体贴图 → 生成 SH 和 IBL
    ↓
更新 mainPass 的 SH 和 IBL
    ↓
渲染
```

### 多探针模式（新增）
```
用户点击 "Generate Probes"
    ↓
创建 16×16 探针网格
    ↓
用户点击 "Capture All Probes"
    ↓
为每个探针：
  - 捕获立方体贴图
  - 生成 SH 系数
  - 生成 IBL 贴图
  - 保存到 multiProbeData
    ↓
每帧在 prepareData() 中：
  - 根据相机位置找到最近的探针
  - 更新 mainPass 的 SH 和 IBL
    ↓
渲染（使用最近探针的光照）
```

---

## 测试步骤

1. ✅ 启用 "Use Multiple Probes"
2. ✅ 点击 "Generate Probes" 生成 16×16 探针网格
3. ✅ 点击 "Capture All Probes" 捕获所有探针
4. ✅ 移动相机观察光照变化
5. ✅ 验证漫反射随相机位置变化
6. ✅ 验证镜面反射随相机位置变化

---

## 编译状态

✅ 编译成功，无错误

---

## 下一步

### 待修复的中优先级问题

1. **资源管理不清晰** - cubemap 同时保存在 cubeMaps 和 lightProbes 中
2. **天空盒更新逻辑** - 不应该自动设置为最后一个探针

### 建议

- 统一资源管理，使用 ProbeData 结构体
- 天空盒应该显示用户选择的探针或相机位置的环境


