# 🔍 gltfModel初始化调试指南

## 🎯 问题

gltfModel没有出现，连黑色都没有

---

## 🔍 可能的原因

### 原因1: gltfModels列表为空
- `LoadgltfModel()` 可能没有被调用
- 或者模型加载失败

### 原因2: gltfmodelIndex超出范围
- `gltfmodelIndex` 初始值可能不正确
- 或者 `gltfModels.size()` 为0

### 原因3: gltfModel指针为空
- `PrepareScene()` 中没有创建 `gltfModel`
- 或者 `gltfModel` 被销毁了

---

## ✅ 修复方案

### 修改1: 添加调试输出

**文件**: `main.cpp` - `PrepareScene()` 函数

```cpp
// ✅ 调试: 检查gltfModels是否为空
std::cerr << "PrepareScene: gltfModels.size()=" << gltfModels.size() 
          << ", gltfmodelIndex=" << gltfmodelIndex << "\n";

if (!gltfModels.empty() && gltfmodelIndex < gltfModels.size()) {
    // 创建gltfModel
    gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);
    // ...
} else {
    std::cerr << "PrepareScene: WARNING - gltfModels is empty or gltfmodelIndex out of range!\n";
}
```

### 修改2: 添加空指针检查

**文件**: `main.cpp` - `drawFrame()` 函数

```cpp
if (gltfModel) { 
    gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); 
}
```

---

## 🧪 调试步骤

### 第1步: 编译并运行

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
./build/Release/lightprobesh2.exe
```

### 第2步: 查看控制台输出

查找以下输出：
```
PrepareScene: gltfModels.size()=2, gltfmodelIndex=0
```

如果看到：
```
PrepareScene: WARNING - gltfModels is empty or gltfmodelIndex out of range!
```

说明 `gltfModels` 为空或索引超出范围。

---

## 🔧 可能的解决方案

### 如果gltfModels为空

**问题**: 模型加载失败

**检查**:
1. 模型文件是否存在
   - `models/FlightHelmet/glTF/FlightHelmet.gltf`
   - `models/CesiumMan/glTF/CesiumMan.gltf`

2. 文件路径是否正确
   - 检查 `getAssetPath()` 返回的路径

3. 模型加载是否有错误
   - 在 `LoadgltfModel()` 中添加错误检查

### 如果gltfmodelIndex超出范围

**问题**: `gltfmodelIndex` 初始值不正确

**解决**: 在 `PrepareScene()` 中添加范围检查

```cpp
if (gltfmodelIndex >= gltfModels.size()) {
    gltfmodelIndex = 0; // 重置为0
}
```

---

## 📝 完整的调试流程

### 步骤1: 检查模型加载

在 `LoadgltfModel()` 中添加调试输出：

```cpp
void VulkanExample::LoadgltfModel(const std::string& name, const std::string& cubemapPath, uint32_t glTFLoadingFlags)
{
    std::cerr << "LoadgltfModel: Loading " << name << " from " << cubemapPath << "\n";
    auto model = std::make_shared<vkglTF::Model>();
    model->loadFromFile(getAssetPath() + cubemapPath, vulkanDevice, queue, glTFLoadingFlags);
    
    gltfModels.emplace_back(model);
    gltfModelNames.emplace_back(name);
    std::cerr << "LoadgltfModel: Loaded " << name << ", total models: " << gltfModels.size() << "\n";
}
```

### 步骤2: 检查初始化

在 `PrepareScene()` 中查看调试输出

### 步骤3: 检查绘制

在 `drawFrame()` 中添加调试输出：

```cpp
if (gltfModel) { 
    std::cerr << "drawFrame: Drawing gltfModel\n";
    gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); 
} else {
    std::cerr << "drawFrame: gltfModel is null!\n";
}
```

---

## 🎉 总结

**问题**: gltfModel没有出现

**可能原因**:
1. gltfModels列表为空
2. gltfmodelIndex超出范围
3. gltfModel指针为空

**解决方案**:
1. 添加调试输出
2. 添加空指针检查
3. 添加范围检查

**下一步**: 编译并查看控制台输出，根据输出信息进行调试


