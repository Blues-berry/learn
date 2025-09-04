# 如何添加新的天空盒纹理

## 1. 准备天空盒纹理文件

- 将新的天空盒纹理文件(.ktx格式)放入以下目录：
  ```
  assets/textures/hdr/
  ```
- 文件命名建议：`<名称>_cube.ktx` (例如: `forest_cube.ktx`)
- 纹理格式要求：
  - 必须是立方体贴图(cubemap)
  - 推荐使用VK_FORMAT_R16G16B16A16_SFLOAT格式
  - 分辨率建议为1024x1024或2048x2048

## 2. 代码修改步骤

### 2.1 添加纹理成员变量

在`Textures`结构体中添加新的纹理对象：
```cpp
struct Textures {
    vks::TextureCubeMap environmentCube;  // 现有纹理1
    vks::TextureCubeMap environmentCube2; // 现有纹理2
    vks::TextureCubeMap environmentCube3; // 新增纹理
    // ...其他纹理
};
```

### 2.2 更新天空盒名称列表

在构造函数中更新`skyboxNames`数组：
```cpp
skyboxNames = {"NO Skybox", "Pisa", "Grand Canyon", "Forest"};
```

### 2.3 加载新纹理

在`loadTextures()`函数中添加加载代码：
```cpp
// 加载新天空盒纹理
textures.environmentCube3.loadFromFile(
    getAssetPath() + "textures/hdr/forest_cube.ktx",
    VK_FORMAT_R16G16B16A16_SFLOAT,
    vulkanDevice,
    queue);
```

### 2.4 更新描述符集

在`setupDescriptors()`中为新纹理创建描述符集(如果需要)：
```cpp
// 如果需要单独的描述符集
VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.skybox3));
```

### 2.5 修改切换逻辑

更新`loadSkyboxTexture()`函数：
```cpp
virtual void loadSkyboxTexture() {
    VkDescriptorImageInfo* currentSkyboxDescriptor = nullptr;
    switch(skyboxIndex) {
        case 1: currentSkyboxDescriptor = &textures.environmentCube.descriptor; break;
        case 2: currentSkyboxDescriptor = &textures.environmentCube2.descriptor; break;
        case 3: currentSkyboxDescriptor = &textures.environmentCube3.descriptor; break;
    }
    // ...其余代码不变
}
```

## 3. 测试验证

1. 启动程序
2. 在UI界面检查新天空盒是否出现在选项中
3. 选择新天空盒，检查：
   - 背景是否正确显示
   - 物体反射是否正确更新
   - 性能是否正常

## 4. 常见问题

### 纹理不显示
- 检查文件路径是否正确
- 验证纹理文件是否完整
- 检查控制台是否有加载错误

### 反射效果不正确
- 确保纹理是HDR格式
- 检查立方体贴图是否完整(6个面)

### 性能下降
- 降低纹理分辨率
- 检查纹理过滤设置

## 5. 注意事项

- 添加新纹理后需要重建命令缓冲区
- 大尺寸纹理会显著增加内存使用
- 建议保持所有天空盒纹理格式一致