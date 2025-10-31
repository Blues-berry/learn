# GltfModel 纹理加载和代码重构指南

## 概览
本文档说明如何为`GltfModel`添加纹理支持，并将`main.cpp`中的代码重构到独立文件中。

---

## 1. GltfModel纹理支持改进

### 1.1 已完成的修改

#### gltfload.h
```cpp
// ✅ 添加了纹理相关结构（参考gltfloading.cpp）
struct Image {
    vks::Texture2D texture;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

struct Texture {
    int32_t imageIndex = -1;
};

struct Material {
    glm::vec4 baseColorFactor = glm::vec4(1.0f);
    int32_t baseColorTextureIndex = -1;
    float roughness = 1.0f;
    float metallic = 0.5f;
};

// MaterialBuffer中添加useTexture标志
struct MaterialBuffer {
    // ... 原有字段 ...
    int32_t useTexture = 0;  // 新增
    int32_t padding2 = 0;
};

// 新增方法
void LoadModelWithTextures(const std::string& filename, uint32_t fileLoadingFlags);
const std::vector<Image>& GetImages() const;
const std::vector<Material>& GetMaterials() const;

private:
    // 纹理加载方法（参考gltfloading.cpp）
    void loadImages(tinygltf::Model& input);
    void loadTextures(tinygltf::Model& input);
    void loadMaterials(tinygltf::Model& input);
    
    // 新增成员变量
    VkQueue copyQueue;
    std::vector<Image> images;
    std::vector<Texture> textures;
    std::vector<Material> materials;
    tinygltf::Model gltfInput;
```

####gltfload.cpp
```cpp
// ✅ 修改构造函数，添加copyQueue参数
GltfModel::GltfModel(vks::VulkanDevice* dev, IExampleInterfasce* example, VkQueue queue) 
    : device(dev), iLoader(example), copyQueue(queue)
{
    PreparePerBatchResource();
    UpdateSet();
}

// ✅ 析构函数清理纹理资源
GltfModel::~GltfModel()
{
    Destroy();
}

void GltfModel::Destroy()
{
    // ... 原有清理 ...
    
    // ✅ 清理纹理资源
    for (auto& image : images) {
        if (image.texture.view != VK_NULL_HANDLE) {
            vkDestroyImageView(device->logicalDevice, image.texture.view, nullptr);
        }
        if (image.texture.image != VK_NULL_HANDLE) {
            vkDestroyImage(device->logicalDevice, image.texture.image, nullptr);
        }
        if (image.texture.sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device->logicalDevice, image.texture.sampler, nullptr);
        }
        if (image.texture.deviceMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->logicalDevice, image.texture.deviceMemory, nullptr);
        }
    }
    images.clear();
    textures.clear();
    materials.clear();
}
```

### 1.2 需要添加的实现

在`gltfload.cpp`末尾添加这些方法（完全参考`gltfloading.cpp`）：

```cpp
// ✅ 加载图像（参考gltfloading.cpp第172-215行）
void GltfModel::loadImages(tinygltf::Model& input)
{
    images.resize(input.images.size());
    for (size_t i = 0; i < input.images.size(); i++) {
        tinygltf::Image& glTFImage = input.images[i];
        unsigned char* buffer = nullptr;
        VkDeviceSize bufferSize = 0;
        bool deleteBuffer = false;
        
        // RGB转RGBA
        if (glTFImage.component == 3) {
            bufferSize = glTFImage.width * glTFImage.height * 4;
            buffer = new unsigned char[bufferSize];
            unsigned char* rgba = buffer;
            unsigned char* rgb = &glTFImage.image[0];
            for (size_t j = 0; j < glTFImage.width * glTFImage.height; ++j) {
                memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                rgba += 4;
                rgb += 3;
            }
            deleteBuffer = true;
        }
        else {
            buffer = &glTFImage.image[0];
            bufferSize = glTFImage.image.size();
        }
        
        // 加载纹理
        images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, 
                                     glTFImage.width, glTFImage.height, device, copyQueue);
        
        if (deleteBuffer) {
            delete[] buffer;
        }
    }
}

// ✅ 加载纹理引用（参考gltfloading.cpp第217-226行）
void GltfModel::loadTextures(tinygltf::Model& input)
{
    textures.resize(input.textures.size());
    for (size_t i = 0; i < input.textures.size(); i++) {
        textures[i].imageIndex = input.textures[i].source;
    }
}

// ✅ 加载材质（参考gltfloading.cpp第228-248行）
void GltfModel::loadMaterials(tinygltf::Model& input)
{
    materials.resize(input.materials.size());
    for (size_t i = 0; i < input.materials.size(); i++) {
        tinygltf::Material glTFMaterial = input.materials[i];
        
        // 基础颜色因子
        if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
            materials[i].baseColorFactor = glm::make_vec4(
                glTFMaterial.values["baseColorFactor"].ColorFactor().data());
        }
        
        // 基础颜色纹理索引
        if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
            materials[i].baseColorTextureIndex = 
                glTFMaterial.values["baseColorTexture"].TextureIndex();
        }
        
        // PBR参数
        if (glTFMaterial.values.find("roughnessFactor") != glTFMaterial.values.end()) {
            materials[i].roughness = 
                static_cast<float>(glTFMaterial.values["roughnessFactor"].Factor());
        }
        
        if (glTFMaterial.values.find("metallicFactor") != glTFMaterial.values.end()) {
            materials[i].metallic = 
                static_cast<float>(glTFMaterial.values["metallicFactor"].Factor());
        }
    }
}

// ✅ 加载带纹理的模型
void GltfModel::LoadModelWithTextures(const std::string& filename, uint32_t fileLoadingFlags)
{
    // 使用tinygltf加载glTF文件
    tinygltf::TinyGLTF loader;
    std::string error, warning;
    
    bool fileLoaded = loader.LoadASCIIFromFile(&gltfInput, &error, &warning, filename);
    
    if (!fileLoaded) {
        std::cerr << "[GltfModel] Failed to load glTF file: " << filename << std::endl;
        if (!error.empty()) {
            std::cerr << "Error: " << error << std::endl;
        }
        if (!warning.empty()) {
            std::cerr << "Warning: " << warning << std::endl;
        }
        return;
    }
    
    // 加载纹理数据
    loadImages(gltfInput);
    loadTextures(gltfInput);
    loadMaterials(gltfInput);
    
    // 使用vkglTF加载几何数据
    auto vkModel = std::make_shared<vkglTF::Model>();
    vkModel->loadFromFile(filename, device, copyQueue, fileLoadingFlags);
    UpdateModel(vkModel);
    
    // 更新材质参数
    if (!materials.empty()) {
        materialData.roughness = materials[0].roughness;
        materialData.metallic = materials[0].metallic;
        materialData.elbedo = materials[0].baseColorFactor;
        materialData.useTexture = (materials[0].baseColorTextureIndex >= 0) ? 1 : 0;
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
    }
    
    std::cout << "[GltfModel] Loaded model with " << images.size() << " textures" << std::endl;
}
```

---

## 2. main.cpp中的使用方式

### 2.1 修改GltfModel创建

```cpp
// ❌ 旧代码
gltfModel = std::make_unique<GltfModel>(vulkanDevice, this);

// ✅ 新代码 - 传入copyQueue
gltfModel = std::make_unique<GltfModel>(vulkanDevice, this, queue);
```

### 2.2 加载带纹理的模型

```cpp
// 方法1：使用LoadModelWithTextures直接加载
gltfModel->LoadModelWithTextures(
    getAssetPath() + "models/FlightHelmet/glTF/FlightHelmet.gltf",
    vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY
);

// 方法2：先UpdateModel再手动加载纹理（原有方式）
gltfModel->UpdateModel(gltfModels[gltfmodelIndex]);
```

---

## 3. main.cpp代码重构方案

### 3.1 创建AssetManager类

**文件**: `AssetManager.h` / `AssetManager.cpp`

**职责**:
- 管理所有资产加载（cubemaps, models, textures）
- 提供统一的资产访问接口

```cpp
class AssetManager {
public:
    AssetManager(vks::VulkanDevice* device, VkQueue queue, 
                 std::function<std::string(const std::string&)> getAssetPath);
    
    // 加载立方体贴图
    void LoadCubeMap(const std::string& name, const std::string& path, VkFormat format);
    
    // 加载预览模型
    void LoadPreviewModel(const std::string& name, const std::string& path, uint32_t flags);
    
    // 加载glTF模型
    void LoadGltfModel(const std::string& name, const std::string& path, uint32_t flags);
    
    // 访问器
    const std::vector<std::shared_ptr<vks::TextureCubeMap>>& GetCubeMaps() const;
    const std::vector<std::string>& GetCubemapNames() const;
    const std::vector<std::shared_ptr<vkglTF::Model>>& GetPreviewModels() const;
    const std::vector<std::string>& GetPreviewModelNames() const;
    
private:
    vks::VulkanDevice* device;
    VkQueue queue;
    std::function<std::string(const std::string&)> getAssetPath;
    
    std::vector<std::shared_ptr<vks::TextureCubeMap>> cubeMaps;
    std::vector<std::string> cubemapNames;
    std::vector<std::shared_ptr<vkglTF::Model>> previewModels;
    std::vector<std::string> previewModelNames;
    std::vector<std::shared_ptr<vkglTF::Model>> gltfModels;
    std::vector<std::string> gltfModelNames;
};
```

**用法**:
```cpp
// 在VulkanExample中
assetManager = std::make_unique<AssetManager>(
    vulkanDevice, queue, 
    [this](const std::string& path) { return getAssetPath() + path; }
);

// 加载资产
assetManager->LoadCubeMap("pisa", "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT);
assetManager->LoadPreviewModel("sphere", "models/sphere.gltf", glTFLoadingFlags);
```

### 3.2 创建SceneManager类

**文件**: `SceneManager.h` / `SceneManager.cpp`

**职责**:
- 管理场景对象（skybox, previewModel, gltfModel）
- 处理场景更新和渲染

```cpp
class SceneManager {
public:
    SceneManager(vks::VulkanDevice* device, IExampleInterfasce* example, VkQueue queue);
    
    void PrepareScene(VkRenderPass renderPass, 
                     VkDescriptorSetLayout mainPassLayout,
                     VkRenderPass captureRenderPass,
                     VkDescriptorSetLayout capturePassLayout);
    
    void UpdateSkybox(std::shared_ptr<vks::TextureCubeMap> cubemap);
    void UpdatePreviewModel(std::shared_ptr<vkglTF::Model> model);
    void UpdateGltfModel(std::shared_ptr<vkglTF::Model> model);
    
    void DrawScene(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique technique);
    
    Skybox* GetSkybox() { return skybox.get(); }
    PreviewModel* GetPreviewModel() { return previewModel.get(); }
    GltfModel* GetGltfModel() { return gltfModel.get(); }
    
private:
    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;
    VkQueue queue;
    
    std::unique_ptr<Skybox> skybox;
    std::unique_ptr<PreviewModel> previewModel;
    std::unique_ptr<GltfModel> gltfModel;
};
```

### 3.3 创建ProbeManager类

**文件**: `ProbeManager.h` / `ProbeManager.cpp`

**职责**:
- 管理所有光照探针
- 处理探针生成、捕获和插值

```cpp
class ProbeManager {
public:
    ProbeManager(vks::VulkanDevice* device, IExampleInterfasce* example, VkQueue queue);
    
    void SetProbeGridConfig(const ProbeGridConfig& config);
    void GenerateProbes(Skybox* skybox, PreviewModel* previewModel, GltfModel* gltfModel);
    void CaptureAllProbes();
    void CaptureSingleProbe(const glm::vec3& position);
    
    void DrawProbes(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique technique);
    
    const std::vector<std::unique_ptr<LightProbe>>& GetProbes() const { return probes; }
    CubemapInterpolation* GetInterpolation() { return cubemapInterpolation.get(); }
    
private:
    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;
    VkQueue queue;
    
    ProbeGridConfig config;
    std::vector<std::unique_ptr<LightProbe>> probes;
    std::unique_ptr<CubemapInterpolation> cubemapInterpolation;
};
```

### 3.4 重构后的main.cpp结构

```cpp
class VulkanExample : public VulkanExampleBase, public IExampleInterfasce
{
public:
    VulkanExample() : VulkanExampleBase() { /* 简化的构造函数 */ }
    ~VulkanExample() override { /* 简化的析构函数 */ }
    
    void prepare() override {
        VulkanExampleBase::prepare();
        
        // 创建管理器
        assetManager = std::make_unique<AssetManager>(vulkanDevice, queue, ...);
        sceneManager = std::make_unique<SceneManager>(vulkanDevice, this, queue);
        probeManager = std::make_unique<ProbeManager>(vulkanDevice, this, queue);
        
        // 加载资产
        assetManager->LoadAssets();
        
        // 准备渲染管线
        PreparePasses();
        
        // 准备场景
        sceneManager->PrepareScene(renderPass, mainPass->descriptorSetLayout, ...);
        
        // 准备探针
        probeManager->GenerateProbes(
            sceneManager->GetSkybox(),
            sceneManager->GetPreviewModel(),
            sceneManager->GetGltfModel()
        );
        
        prepared = true;
    }
    
    void drawFrame(VkCommandBuffer cmd) {
        if (compareMode == RenderCompareMode::SPLIT_VIEW) {
            drawSplitView(cmd);
            return;
        }
        
        mainPass->Draw(cmd, frameBuffers[currentBuffer], width, height, [this](VkCommandBuffer cmd) {
            sceneManager->DrawScene(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            
            if (showProbes) {
                probeManager->DrawProbes(cmd, mainPass->descriptorSet, ETechnique::MAIN);
            }
            
            drawUI(cmd);
        });
    }
    
private:
    // 管理器
    std::unique_ptr<AssetManager> assetManager;
    std::unique_ptr<SceneManager> sceneManager;
    std::unique_ptr<ProbeManager> probeManager;
    
    // 渲染管线
    std::unique_ptr<MainPass> mainPass;
    std::unique_ptr<GenBRDFLutPass> brdfPass;
    std::unique_ptr<GenSHComputePass> shGenPass;
    std::unique_ptr<GenIBLPass> genIBL;
    
    // UI状态
    bool showProbes = false;
    RenderCompareMode compareMode = RenderCompareMode::NORMAL;
};
```

---

## 4. 实施步骤

### 阶段1：纹理支持（立即）
1. ✅ 修改`gltfload.h`添加纹理结构和方法声明
2. ✅ 修改`gltfload.cpp`添加纹理清理代码
3. ⏳ 在`gltfload.cpp`添加`loadImages()`, `loadTextures()`, `loadMaterials()`实现
4. ⏳ 在`gltfload.cpp`添加`LoadModelWithTextures()`实现
5. ⏳ 修改`main.cpp`中的`GltfModel`创建，传入`queue`参数

### 阶段2：AssetManager（1-2小时）
1. 创建`AssetManager.h`和`AssetManager.cpp`
2. 将`LoadCubeMap()`, `LoadPreviewModel()`, `LoadgltfModel()`移到AssetManager
3. 在`main.cpp`中使用AssetManager

### 阶段3：SceneManager（1-2小时）
1. 创建`SceneManager.h`和`SceneManager.cpp`
2. 将`PrepareScene()`, `UpdateSkyBox()`移到SceneManager
3. 在`main.cpp`中使用SceneManager

### 阶段4：ProbeManager（2-3小时）
1. 创建`ProbeManager.h`和`ProbeManager.cpp`
2. 将`PrepareProbes()`, `CaptureAllProbes()`, `CaptureCubemap()`移到ProbeManager
3. 在`main.cpp`中使用ProbeManager

### 阶段5：最终清理（1小时）
1. 删除`main.cpp`中的重复代码
2. 更新文档
3. 测试所有功能

---

## 5. 预期收益

### 代码质量
- **main.cpp**: 从 ~900行 减少到 ~300行
- **模块化**: 每个类职责单一，易于维护
- **可测试性**: 每个管理器可以独立测试

### 开发效率
- **新功能**: 添加新资产类型只需修改AssetManager
- **调试**: 问题隔离在特定管理器中
- **复用**: 管理器可以在其他项目中复用

### 性能
- 无性能损失（只是代码重组）
- 更好的内存管理（RAII模式）
- 更清晰的资源生命周期

---

## 6. 纹理在着色器中的使用

### 修改片段着色器
```glsl
// gltfmesh.frag
layout (binding = 2, set = 1) uniform sampler2D baseColorTexture;

layout (binding = 1, set = 1) uniform MaterialUBO {
    float roughness;
    float metallic;
    float specular;
    float padding;
    vec4 elbedo;
    int useSH;
    int useReflection;
    int useTexture;  // 新增
    int padding2;
} material;

void main() {
    vec4 baseColor = material.elbedo;
    
    // 如果启用纹理，从纹理采样
    if (material.useTexture > 0) {
        baseColor *= texture(baseColorTexture, inUV);
    }
    
    // ... PBR计算 ...
}
```

---

**版本**: 1.0  
**创建**: 2024  
**最后更新**: 2024
