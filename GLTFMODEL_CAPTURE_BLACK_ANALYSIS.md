# 🔍 捕获的 gltfModel 是黑色的原因分析

## 🐛 问题

捕获到的 gltfModel 没有光照信息，显示为黑色。

## 🔍 根本原因

### 问题 1: Fragment Shader 中使用了错误的相机位置

**文件**: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`

**第 147 行**:
```glsl
vec3 V = normalize(global.cameraPos[0].xyz - inWorldPos);
```

**问题**:
- 硬编码使用 `cameraPos[0]`（第一个面的相机位置）
- 在 multiview 渲染中，应该使用 `gl_ViewIndex` 来选择正确的相机位置
- 导致其他 5 个面使用错误的相机位置 → 光照计算错误 → 黑色

### 问题 2: gltfModel 在 CAPTURE_SCENE 中没有正确的光照

**原因**:
- gltfmesh_mvr.frag 中的光照计算依赖于正确的相机位置
- 由于相机位置错误，光照计算失败
- 最终输出黑色

## ✅ 修复方案

### 修复 1: 在 Fragment Shader 中使用 gl_ViewIndex

**文件**: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`

```glsl
void main()
{
    vec3 N = normalize(inNormal);
    // ✅ 使用 gl_ViewIndex 选择正确的相机位置
    vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);
    vec3 R = reflect(-V, N);

    // ... 其他代码 ...
}
```

### 修复 2: 确保 gltfModel 在 CAPTURE_SCENE 中有正确的材质

**问题**: gltfModel 可能没有正确初始化材质数据

**解决**: 在 CaptureCubemap() 中确保材质数据已初始化

```cpp
void VulkanExample::CaptureCubemap(const glm::vec3& position)
{
    // ... 创建 probe ...

    if (!gltfModel) {
        gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
        gltfModel->UpdateModel(previewModel->getModel());
        
        // ✅ 准备 PSO ...
        gltfModel->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
        gltfModel->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
        
        // ✅ 确保材质数据已初始化
        gltfModel->SetUseSHAndReflection(false, false);  // 初始化材质
    }

    probe->SetGltfModel(gltfModel.get());
    probe->CaptureCubeMap(queue);
}
```

## 📊 修复效果

| 方面 | 修复前 | 修复后 |
|------|--------|--------|
| **相机位置** | ❌ 硬编码为 [0] | ✅ 使用 gl_ViewIndex |
| **光照计算** | ❌ 错误 | ✅ 正确 |
| **捕获结果** | ❌ 黑色 | ✅ 有光照 |

## 🔑 关键要点

1. **Multiview 渲染中必须使用 gl_ViewIndex**
   - 每个面有不同的视图矩阵和相机位置
   - 不能硬编码使用某一个面的数据

2. **Fragment Shader 中需要访问 gl_ViewIndex**
   - 需要在 fragment shader 中声明 `#extension GL_EXT_multiview : enable`
   - 然后使用 `gl_ViewIndex` 来索引数组

3. **确保材质数据已初始化**
   - 材质数据影响光照计算结果

## 🚀 修复步骤

1. 修改 `gltfmesh_mvr.frag` 中的相机位置计算
2. 重新编译着色器
3. 重新编译 C++ 代码
4. 测试 CaptureCubemap 功能

## 📝 修改文件列表

1. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` - 使用 gl_ViewIndex
2. ✅ `shaders/glsl/lightprobesh2/gltfmesh_mvr.vert` - 已正确使用 gl_ViewIndex

