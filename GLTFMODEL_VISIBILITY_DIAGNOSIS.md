# 🔍 gltfModel不可见问题诊断

## 🎯 问题

gltfModel没有出现，连黑色都没有

---

## 🔍 诊断步骤

### 第1步: 编译并运行

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
./build/Release/lightprobesh2.exe
```

### 第2步: 查看控制台输出

查找以下输出：

```
LoadgltfModel: Loading FlightHelmet from models/FlightHelmet/glTF/FlightHelmet.gltf
LoadgltfModel: Loaded FlightHelmet, total models: 1
LoadgltfModel: Loading CesiumMan from models/CesiumMan/glTF/CesiumMan.gltf
LoadgltfModel: Loaded CesiumMan, total models: 2
PrepareScene: gltfModels.size()=2, gltfmodelIndex=0
```

---

## ✅ 修复方案

### 修改1: 添加调试输出到LoadgltfModel

**文件**: `main.cpp` - `LoadgltfModel()` 函数

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

✅ **已完成**

### 修改2: 添加调试输出到PrepareScene

**文件**: `main.cpp` - `PrepareScene()` 函数

```cpp
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

✅ **已完成**

### 修改3: 添加空指针检查到drawFrame

**文件**: `main.cpp` - `drawFrame()` 函数

```cpp
if (gltfModel) { 
    gltfModel->Draw(cmd, mainPass->descriptorSet, ETechnique::MAIN); 
}
```

✅ **已完成**

---

## 📊 修改统计

| 文件 | 函数 | 改动 | 状态 |
|------|------|------|------|
| main.cpp | LoadgltfModel() | 添加调试输出 | ✅ |
| main.cpp | PrepareScene() | 添加调试输出和范围检查 | ✅ |
| main.cpp | drawFrame() | 添加空指针检查 | ✅ |

---

## 🧪 编译和测试

### 编译步骤

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 运行程序

```bash
./build/Release/lightprobesh2.exe
```

### 查看输出

程序启动时应该看到：

```
LoadgltfModel: Loading FlightHelmet from models/FlightHelmet/glTF/FlightHelmet.gltf
LoadgltfModel: Loaded FlightHelmet, total models: 1
LoadgltfModel: Loading CesiumMan from models/CesiumMan/glTF/CesiumMan.gltf
LoadgltfModel: Loaded CesiumMan, total models: 2
PrepareScene: gltfModels.size()=2, gltfmodelIndex=0
```

---

## 🔧 故障排除

### 如果看到: "WARNING - gltfModels is empty"

**问题**: 模型加载失败

**检查**:
1. 模型文件是否存在
2. 文件路径是否正确
3. 模型加载是否有错误

### 如果看到: "WARNING - gltfmodelIndex out of range"

**问题**: `gltfmodelIndex` 超出范围

**解决**: 在 `PrepareScene()` 中添加范围检查

```cpp
if (gltfmodelIndex >= gltfModels.size()) {
    gltfmodelIndex = 0;
}
```

### 如果gltfModel仍然不可见

**检查**:
1. 模型是否被正确加载
2. PSO是否被正确准备
3. 描述符集是否被正确绑定
4. 着色器是否有问题

---

## 📝 预期输出

### 正常情况

```
LoadgltfModel: Loading FlightHelmet from models/FlightHelmet/glTF/FlightHelmet.gltf
LoadgltfModel: Loaded FlightHelmet, total models: 1
LoadgltfModel: Loading CesiumMan from models/CesiumMan/glTF/CesiumMan.gltf
LoadgltfModel: Loaded CesiumMan, total models: 2
PrepareScene: gltfModels.size()=2, gltfmodelIndex=0
```

### 异常情况

```
PrepareScene: WARNING - gltfModels is empty or gltfmodelIndex out of range!
```

---

## 🎉 总结

**问题**: gltfModel不可见

**诊断方法**:
1. 查看控制台输出
2. 检查gltfModels是否为空
3. 检查gltfmodelIndex是否超出范围
4. 检查gltfModel指针是否为空

**修复方案**:
1. ✅ 添加调试输出
2. ✅ 添加范围检查
3. ✅ 添加空指针检查

**下一步**: 编译并查看控制台输出


