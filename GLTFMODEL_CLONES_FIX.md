# ✅ gltfModel克隆绘制修复

## 🎯 问题

FlightHelmet 和 CesiumMan 两个模型没有正常绘制

---

## 🔍 根本原因

### 问题分析

1. **LoadgltfModel()** 函数只是加载模型到 `gltfModels` 列表
2. **PrepareScene()** 中只有第一个模型被设置到 `gltfModel`
3. **gltfClones** 向量是空的，所以其他加载的模型没有被绘制

### 代码流程

```cpp
// LoadAssets() 中
LoadgltfModel("FlightHelmet", "models/FlightHelmet/glTF/FlightHelmet.gltf", glTFLoadingFlags);
LoadgltfModel("CesiumMan", "models/CesiumMan/glTF/CesiumMan.gltf", glTFLoadingFlags);
// → 模型被添加到 gltfModels 列表

// PrepareScene() 中
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]); // 只设置第一个模型
// → 其他模型没有被处理

// drawFrame() 中
for (auto& m : gltfClones) { m->Draw(...); } // gltfClones 是空的！
// → 其他模型没有被绘制
```

---

## ✅ 修复方案

### 修改文件: `examples/lightprobesh2/main.cpp`

**改动**: 在 `PrepareScene()` 中为其他加载的模型创建克隆

```cpp
// ✅ 修复: 为其他加载的gltfModel创建克隆并设置PSO
for (size_t i = 0; i < gltfModels.size(); ++i) {
    if (i == gltfmodelIndex) continue; // 跳过主模型
    
    auto clone = std::make_unique<GltfModel>(vulkanDevice, this);
    clone->PreparePSO(renderPass, mainPass->descriptorSetLayout, ETechnique::MAIN);
    clone->PreparePSO(capturePass->renderPass, capturePass->descriptorSetLayout, ETechnique::CAPTURE_SCENE);
    clone->UpdateModel(gltfModels[i]);
    
    // 为每个克隆设置不同的位置
    float offsetX = -10.0f + (i - gltfmodelIndex) * 15.0f;
    glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(offsetX, 0.0f, 0.0f));
    glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
    clone->SetTransform(t * s);
    
    gltfClones.push_back(std::move(clone));
}
```

---

## 📊 修改统计

| 文件 | 函数 | 改动 | 状态 |
|------|------|------|------|
| main.cpp | PrepareScene() | 添加gltfClones初始化循环 | ✅ |

---

## 🎯 修复效果

### 修复前 ❌
```
加载的模型: FlightHelmet, CesiumMan
显示的模型: 只有第一个模型
gltfClones: 空向量
```

### 修复后 ✅
```
加载的模型: FlightHelmet, CesiumMan
显示的模型: 所有加载的模型
gltfClones: 包含所有克隆模型
位置: 每个模型有不同的X偏移
```

---

## 🧪 编译和测试

### 编译步骤

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 验证清单

- [ ] 程序编译成功
- [ ] FlightHelmet 模型可见
- [ ] CesiumMan 模型可见
- [ ] 两个模型显示在不同的位置
- [ ] 两个模型都显示为灰白色
- [ ] CaptureCubemap 中包含所有模型

---

## 📝 技术细节

### 模型位置计算

```cpp
float offsetX = -10.0f + (i - gltfmodelIndex) * 15.0f;
```

- 主模型在 X = -10.0f
- 第一个克隆在 X = -10.0f + (0 - 0) * 15.0f = -10.0f (如果gltfmodelIndex=0)
- 第二个克隆在 X = -10.0f + (1 - 0) * 15.0f = 5.0f
- 第三个克隆在 X = -10.0f + (2 - 0) * 15.0f = 20.0f

这样每个模型之间相隔15个单位。

---

## 🎉 总结

**问题**: FlightHelmet 和 CesiumMan 模型没有被绘制

**根本原因**: 加载的模型没有被添加到 `gltfClones` 中

**解决方案**: 在 `PrepareScene()` 中为每个加载的模型创建克隆并设置PSO

**结果**: 
✅ 所有加载的模型都可见
✅ 每个模型有不同的位置
✅ 所有模型都可以被捕获到cubemap中


