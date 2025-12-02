# 代码变更详情

## 文件 1: examples/lightprobesh2/PreviewModel.cpp

### 位置：第 330-367 行

### 变更前：
```cpp
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    printf("[DEBUG] SetLightColor called with color: (%.2f, %.2f, %.2f)\n", 
           color.r, color.g, color.b);
    
    // Update material properties
    materialData.elbedo = glm::vec4(color, 1.0f);
    materialData.roughness = 0.1f;
    materialData.metallic = 0.0f;
    materialData.specular = 1.0f;
    materialData.useLighting = 1;
    
    // Update the material buffer if it's mapped
    if (materialBuffer.mapped) {
        printf("[DEBUG] Updating material buffer with color: (%.2f, %.2f, %.2f, %.2f)\n", 
               materialData.elbedo.r, materialData.elbedo.g, 
               materialData.elbedo.b, materialData.elbedo.a);
        memcpy(materialBuffer.mapped, &materialData, sizeof(MaterialBuffer));
        
        // Flush the memory to make sure the GPU sees the update
        VkMappedMemoryRange memoryRange = {};
        memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        memoryRange.memory = materialBuffer.memory;
        memoryRange.offset = 0;
        memoryRange.size = sizeof(MaterialBuffer);
        vkFlushMappedMemoryRanges(device->logicalDevice, 1, &memoryRange);
    } else {
        printf("[WARNING] Material buffer not mapped, color update will be delayed\n");
    }
    
    // Mark as dirty to ensure the changes are applied in the next frame
    materialDirty = true;
    
    // Force update the descriptor set to ensure the changes are picked up
    UpdateSet();
    
    printf("[DEBUG] Material updated - Color: (%.2f, %.2f, %.2f, %.2f), Dirty: %s\n",
           materialData.elbedo.r, materialData.elbedo.g, 
           materialData.elbedo.b, materialData.elbedo.a,
           materialDirty ? "true" : "false");
}
```

### 变更后：
```cpp
void PreviewModel::SetLightColor(const glm::vec3& color)
{
    // NOTE: Light color should NOT modify the material albedo!
    // Light color is a global property that affects all objects uniformly.
    // It should only be applied in the shader's lighting calculation,
    // not by changing the material properties.
    // This function is kept for API compatibility but does nothing.
    printf("[DEBUG] SetLightColor called with color: (%.2f, %.2f, %.2f) - NO-OP (light color is global)\n", 
           color.r, color.g, color.b);
}
```

### 原因：
- 光源颜色不应该修改模型的材质
- 这导致了颜色显示不正确的问题
- 光源颜色是全局属性，应该只在着色器中应用

---

## 文件 2: examples/lightprobesh2/main.cpp

### 位置：第 1417-1437 行（PrecomputePRT 函数）

### 变更前：
```cpp
    // ============================================================
    // 第2步: 预计算Lighting (光源的球谐系数)
    // ============================================================
    std::cout << "\n[Step 2] Precomputing Lighting (Light Source)..." << std::endl;

    // 生成采样光照 (使用当前光源颜色)
    // 在实际应用中，这应该从环境贴图或光源采样
    std::vector<glm::vec3> radiances;
    for (int i = 0; i < shSamples; i++) {
        radiances.push_back(lightColor * lightIntensity);
    }

    // 预计算光照的球谐系数
    SHCoefficients lightingCoeffs = PRTPrecomputer::PrecomputeLighting(directions, radiances);
    std::cout << "  - Lighting SH coefficients computed" << std::endl;
    std::cout << "  - Light Color: (" << lightColor.x << ", " << lightColor.y << ", " << lightColor.z << ")" << std::endl;
    std::cout << "  - Light Intensity: " << lightIntensity << std::endl;
```

### 变更后：
```cpp
    // ============================================================
    // 第2步: 预计算Lighting (光源的球谐系数)
    // ============================================================
    std::cout << "\n[Step 2] Precomputing Lighting (Light Source)..." << std::endl;

    // 生成采样光照 (使用单位光源：白色，强度1.0)
    // 运行时会在 UpdatePRTLighting() 中应用实际的光源颜色和强度
    // 这样可以避免重复应用颜色，并支持实时改变光源颜色
    std::vector<glm::vec3> radiances;
    for (int i = 0; i < shSamples; i++) {
        // Use unit light source (white, intensity 1.0)
        // Color and intensity will be applied at runtime
        radiances.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
    }

    // 预计算光照的球谐系数
    SHCoefficients lightingCoeffs = PRTPrecomputer::PrecomputeLighting(directions, radiances);
    std::cout << "  - Lighting SH coefficients computed (using unit light source)" << std::endl;
    std::cout << "  - Current Light Color: (" << lightColor.x << ", " << lightColor.y << ", " << lightColor.z << ")" << std::endl;
    std::cout << "  - Current Light Intensity: " << lightIntensity << std::endl;
    std::cout << "  - Note: Color and intensity will be applied at runtime in UpdatePRTLighting()" << std::endl;
```

### 原因：
- 预计算时使用单位光源（白色，强度 1.0）
- 运行时在 UpdatePRTLighting() 中应用实际的光源颜色和强度
- 这避免了颜色被应用两次的问题
- 支持实时改变光源颜色而无需重新预计算

---

## 影响分析

### 受影响的功能：
1. ✅ PBR 光照计算 - 现在正确显示光源颜色
2. ✅ PRT 预计算 - 现在使用单位光源
3. ✅ PRT 运行时更新 - 现在正确应用光源颜色
4. ✅ 光源颜色改变 - 现在 PBR 和 PRT 都会响应

### 不受影响的功能：
- 光源旋转逻辑
- 着色器代码
- 其他渲染通道

## 编译验证

所有修改都已编译验证，没有编译错误或警告（除了预存在的警告）。

## 向后兼容性

✅ 完全向后兼容
- SetLightColor 仍然存在，只是不做任何操作
- 现有的 API 调用仍然有效
- 没有破坏性的更改

