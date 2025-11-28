# GPU端预计算实现指南

## 📌 核心需求

### 问题分析
1. **预计算在CPU端** - 需要转移到GPU端
2. **Light Transport逐顶点计算** - 需要GPU并行处理
3. **UI导出功能** - 需要添加导出按钮和进度反馈
4. **文件导出测试** - 需要验证文件正确性

### 目标
- ✅ GPU端完成所有预计算
- ✅ 逐顶点Light Transport计算
- ✅ UI导出功能
- ✅ 文件导出验证

---

## 🔧 实现步骤

### 第一步：GPU端光照投影计算

**目标：** 将CPU端的光照投影转移到GPU

**关键代码：**

```cpp
// 在 PRTComputeShader::ComputeLightingProjection() 中实现
bool PRTComputeShader::ComputeLightingProjection(
    const std::vector<glm::vec3>& directions,
    const std::vector<glm::vec3>& radiances,
    GPUSHCoefficients& outputCoeffs)
{
    // 1. 创建采样数据
    std::vector<GPUSample> samples(directions.size());
    for (size_t i = 0; i < directions.size(); i++) {
        samples[i].direction = glm::vec4(directions[i], 0.0f);
        samples[i].radiance = glm::vec4(radiances[i], 0.0f);
    }
    
    // 2. 上传到GPU
    if (!UploadToGPU(samplesBuffer, samples)) {
        LOG_ERROR("Failed to upload samples");
        return false;
    }
    
    // 3. 执行计算着色器
    if (!ExecuteComputeShader(1, 1, 1)) {
        LOG_ERROR("Failed to execute compute shader");
        return false;
    }
    
    // 4. 下载结果
    if (!DownloadFromGPU(outputCoefficientsBuffer, outputCoeffs)) {
        LOG_ERROR("Failed to download results");
        return false;
    }
    
    return true;
}
```

**着色器代码 (sh_compute.comp)：**

```glsl
#version 450

struct GPUSample {
    vec4 direction;
    vec4 radiance;
};

struct GPUSHCoefficients {
    vec4 coeffs[9];
};

layout(set = 0, binding = 0) buffer SamplesBuffer {
    GPUSample samples[];
};

layout(set = 0, binding = 1) buffer OutputCoefficients {
    GPUSHCoefficients outputCoeffs;
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

// 球谐基函数
float shBasis(int index, vec3 dir) {
    switch(index) {
        case 0: return 0.282095f;
        case 1: return -0.488603f * dir.y;
        case 2: return 0.488603f * dir.z;
        case 3: return -0.488603f * dir.x;
        case 4: return 1.092548f * dir.x * dir.y;
        case 5: return -1.092548f * dir.y * dir.z;
        case 6: return 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
        case 7: return -1.092548f * dir.x * dir.z;
        case 8: return 0.546274f * (dir.x * dir.x - dir.y * dir.y);
    }
    return 0.0f;
}

void main() {
    // 初始化输出
    for (int i = 0; i < 9; i++) {
        outputCoeffs.coeffs[i] = vec4(0.0f);
    }
    
    // 对所有采样方向求和
    float weight = 4.0f * 3.14159265f / float(samples.length());
    for (int s = 0; s < samples.length(); s++) {
        vec3 dir = normalize(samples[s].direction.xyz);
        vec3 radiance = samples[s].radiance.xyz;
        
        // 计算球谐系数
        for (int i = 0; i < 9; i++) {
            float basis = shBasis(i, dir);
            outputCoeffs.coeffs[i].rgb += radiance * basis * weight;
        }
    }
}
```

---

### 第二步：GPU端Light Transport计算（逐顶点）

**目标：** 实现逐顶点的Light Transport计算

**关键代码：**

```cpp
// 在 PRTComputeShader::ComputeLightTransportBatch() 中实现
bool PRTComputeShader::ComputeLightTransportBatch(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec3>& albedos,
    const std::vector<glm::vec3>& directions,
    std::vector<GPUSHCoefficients>& outputCoeffsBatch)
{
    // 1. 准备顶点数据
    std::vector<GPULTInput> ltInputs(positions.size());
    for (size_t i = 0; i < positions.size(); i++) {
        ltInputs[i].position = glm::vec4(positions[i], 0.0f);
        ltInputs[i].normal = glm::vec4(normals[i], 0.0f);
        ltInputs[i].albedo = glm::vec4(albedos[i], 0.0f);
    }
    
    // 2. 准备采样数据
    std::vector<GPUSample> samples(directions.size());
    for (size_t i = 0; i < directions.size(); i++) {
        samples[i].direction = glm::vec4(directions[i], 0.0f);
        samples[i].radiance = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
    }
    
    // 3. 上传数据到GPU
    if (!UploadToGPU(ltInputBuffer, ltInputs)) {
        LOG_ERROR("Failed to upload LT input");
        return false;
    }
    
    if (!UploadToGPU(samplesBuffer, samples)) {
        LOG_ERROR("Failed to upload samples");
        return false;
    }
    
    // 4. 执行计算着色器（每个顶点一个工作组）
    uint32_t numVertices = positions.size();
    if (!ExecuteComputeShader(numVertices, 1, 1)) {
        LOG_ERROR("Failed to execute compute shader");
        return false;
    }
    
    // 5. 下载结果
    outputCoeffsBatch.resize(numVertices);
    if (!DownloadFromGPU(outputCoefficientsBuffer, outputCoeffsBatch)) {
        LOG_ERROR("Failed to download results");
        return false;
    }
    
    return true;
}
```

**Light Transport着色器代码：**

```glsl
#version 450

struct GPULTInput {
    vec4 position;
    vec4 normal;
    vec4 albedo;
};

struct GPUSample {
    vec4 direction;
    vec4 radiance;
};

struct GPUSHCoefficients {
    vec4 coeffs[9];
};

layout(set = 0, binding = 0) buffer LTInputBuffer {
    GPULTInput ltInputs[];
};

layout(set = 0, binding = 1) buffer SamplesBuffer {
    GPUSample samples[];
};

layout(set = 0, binding = 2) buffer OutputCoefficients {
    GPUSHCoefficients outputCoeffs[];
};

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

float shBasis(int index, vec3 dir) {
    // 同上
}

void main() {
    uint vertexId = gl_GlobalInvocationID.x;
    
    // 获取顶点数据
    GPULTInput input = ltInputs[vertexId];
    vec3 normal = normalize(input.normal.xyz);
    vec3 albedo = input.albedo.xyz;
    
    // 初始化输出
    for (int i = 0; i < 9; i++) {
        outputCoeffs[vertexId].coeffs[i] = vec4(0.0f);
    }
    
    // 对所有采样方向求和
    float weight = 4.0f * 3.14159265f / float(samples.length());
    for (int s = 0; s < samples.length(); s++) {
        vec3 dir = normalize(samples[s].direction.xyz);
        
        // 计算Lambert余弦项
        float cosine = max(0.0f, dot(normal, dir));
        
        // 计算球谐系数
        for (int i = 0; i < 9; i++) {
            float basis = shBasis(i, dir);
            outputCoeffs[vertexId].coeffs[i].rgb += albedo * basis * cosine * weight;
        }
    }
}
```

---

### 第三步：数据导出功能

**关键代码：**

```cpp
// 在 DataExporter 中实现
bool DataExporter::ExportPRTData(
    const std::string& baseFilename,
    const GPUSHCoefficients& lighting,
    const std::vector<GPUSHCoefficients>& lightTransport,
    const std::vector<RotatedCoefficients>& rotations)
{
    // 导出光照系数
    std::string lightingFile = baseFilename + "_lighting.txt";
    if (!ExportLighting(lightingFile, lighting)) {
        LOG_ERROR("Failed to export lighting");
        return false;
    }
    
    // 导出Light Transport系数
    std::string ltFile = baseFilename + "_lt.txt";
    if (!ExportLightTransport(ltFile, lightTransport)) {
        LOG_ERROR("Failed to export light transport");
        return false;
    }
    
    // 导出旋转系数
    std::string rotationsFile = baseFilename + "_rotations.txt";
    if (!ExportRotations(rotationsFile, rotations)) {
        LOG_ERROR("Failed to export rotations");
        return false;
    }
    
    LOG_INFO("Successfully exported PRT data to " + baseFilename);
    return true;
}

bool DataExporter::ExportLighting(
    const std::string& filename,
    const GPUSHCoefficients& coeffs)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open file: " + filename);
        return false;
    }
    
    file << "LIGHTING_COEFFICIENTS\n";
    file << "9\n";
    
    for (int i = 0; i < 9; i++) {
        file << coeffs.coeffs[i].x << " "
             << coeffs.coeffs[i].y << " "
             << coeffs.coeffs[i].z << "\n";
    }
    
    file.close();
    return true;
}

bool DataExporter::ExportLightTransport(
    const std::string& filename,
    const std::vector<GPUSHCoefficients>& coeffsBatch)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open file: " + filename);
        return false;
    }
    
    file << "LIGHT_TRANSPORT_COEFFICIENTS\n";
    file << coeffsBatch.size() << "\n";
    file << "9\n";
    
    for (const auto& coeffs : coeffsBatch) {
        for (int i = 0; i < 9; i++) {
            file << coeffs.coeffs[i].x << " "
                 << coeffs.coeffs[i].y << " "
                 << coeffs.coeffs[i].z << " ";
        }
        file << "\n";
    }
    
    file.close();
    return true;
}

bool DataExporter::ExportRotations(
    const std::string& filename,
    const std::vector<RotatedCoefficients>& rotations)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open file: " + filename);
        return false;
    }
    
    file << "ROTATIONS\n";
    file << rotations.size() << "\n";
    
    for (const auto& rot : rotations) {
        file << rot.angle << " ";
        for (int i = 0; i < 9; i++) {
            file << rot.coeffs.coeffs[i].x << " "
                 << rot.coeffs.coeffs[i].y << " "
                 << rot.coeffs.coeffs[i].z << " ";
        }
        file << "\n";
    }
    
    file.close();
    return true;
}
```

---

### 第四步：UI导出功能

**在 main.cpp 中添加：**

```cpp
// 添加导出状态
bool isExporting = false;
std::string exportProgress = "";

// 在 VulkanExample::draw() 中添加UI按钮
void ShowPRTExportUI(vks::UIOverlay* overlay) {
    if (overlay->header("PRT Export")) {
        if (overlay->button("Export PRT Data")) {
            // 开始预计算和导出
            ExportPRTData();
        }
        
        if (isExporting) {
            overlay->text("Exporting... " + exportProgress);
        }
    }
}

// 实现导出函数
void ExportPRTData() {
    if (isExporting) return;
    
    isExporting = true;
    exportProgress = "Computing lighting...";
    
    // 1. 获取Cornell Box模型
    auto model = previewModel->getModel();
    if (!model) {
        LOG_ERROR("No model loaded");
        isExporting = false;
        return;
    }
    
    // 2. 生成采样方向
    std::vector<glm::vec3> directions = 
        SphericalHarmonics::GenerateFibonacciSamples(32);
    std::vector<glm::vec3> radiances(directions.size(), glm::vec3(1.0f));
    
    // 3. GPU端光照投影
    exportProgress = "Computing lighting projection...";
    GPUSHCoefficients lightingCoeffs;
    if (!prtCompute->ComputeLightingProjection(directions, radiances, lightingCoeffs)) {
        LOG_ERROR("Failed to compute lighting");
        isExporting = false;
        return;
    }
    
    // 4. 提取顶点数据
    exportProgress = "Extracting vertex data...";
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec3> albedos;
    
    for (auto& mesh : model->meshes) {
        for (auto& vertex : mesh.vertices) {
            positions.push_back(vertex.pos);
            normals.push_back(vertex.normal);
            albedos.push_back(glm::vec3(0.8f)); // 默认反射率
        }
    }
    
    // 5. GPU端Light Transport计算
    exportProgress = "Computing light transport...";
    std::vector<GPUSHCoefficients> ltCoeffsBatch;
    if (!prtCompute->ComputeLightTransportBatch(
        positions, normals, albedos, directions, ltCoeffsBatch)) {
        LOG_ERROR("Failed to compute light transport");
        isExporting = false;
        return;
    }
    
    // 6. GPU端球谐旋转计算
    exportProgress = "Computing rotations...";
    std::vector<RotatedCoefficients> rotations;
    if (!prtCompute->ComputeMultipleRotations(
        lightingCoeffs, 24, 360.0f, rotations)) {
        LOG_ERROR("Failed to compute rotations");
        isExporting = false;
        return;
    }
    
    // 7. 导出到文件
    exportProgress = "Exporting to files...";
    std::string outputPath = "prt_data";
    if (!DataExporter::ExportPRTData(
        outputPath, lightingCoeffs, ltCoeffsBatch, rotations)) {
        LOG_ERROR("Failed to export data");
        isExporting = false;
        return;
    }
    
    // 8. 验证文件
    exportProgress = "Verifying files...";
    if (!VerifyExportedFiles(outputPath)) {
        LOG_ERROR("File verification failed");
        isExporting = false;
        return;
    }
    
    exportProgress = "Export completed!";
    isExporting = false;
    LOG_INFO("PRT data exported successfully");
}

// 验证导出的文件
bool VerifyExportedFiles(const std::string& basePath) {
    // 检查文件是否存在
    std::ifstream lightingFile(basePath + "_lighting.txt");
    std::ifstream ltFile(basePath + "_lt.txt");
    std::ifstream rotationsFile(basePath + "_rotations.txt");
    
    if (!lightingFile.is_open() || !ltFile.is_open() || !rotationsFile.is_open()) {
        LOG_ERROR("One or more files not found");
        return false;
    }
    
    // 验证文件格式
    std::string header;
    int count;
    
    // 验证lighting文件
    lightingFile >> header >> count;
    if (header != "LIGHTING_COEFFICIENTS" || count != 9) {
        LOG_ERROR("Invalid lighting file format");
        return false;
    }
    
    // 验证lt文件
    ltFile >> header >> count;
    if (header != "LIGHT_TRANSPORT_COEFFICIENTS") {
        LOG_ERROR("Invalid LT file format");
        return false;
    }
    
    // 验证rotations文件
    rotationsFile >> header >> count;
    if (header != "ROTATIONS" || count != 24) {
        LOG_ERROR("Invalid rotations file format");
        return false;
    }
    
    lightingFile.close();
    ltFile.close();
    rotationsFile.close();
    
    LOG_INFO("File verification passed");
    return true;
}
```

---

## 📊 实现检查清单

### GPU端实现
- [ ] 实现 `ComputeLightingProjection()` 方法
- [ ] 实现 `ComputeLightTransportBatch()` 方法
- [ ] 实现 `ComputeMultipleRotations()` 方法
- [ ] 编写光照投影着色器
- [ ] 编写Light Transport着色器
- [ ] 编写球谐旋转着色器
- [ ] 实现数据上传/下载机制
- [ ] 测试GPU计算结果

### 数据导出
- [ ] 实现 `ExportLighting()` 函数
- [ ] 实现 `ExportLightTransport()` 函数
- [ ] 实现 `ExportRotations()` 函数
- [ ] 实现 `ExportPRTData()` 函数
- [ ] 实现文件验证函数
- [ ] 测试文件导出

### UI功能
- [ ] 添加导出按钮
- [ ] 添加进度显示
- [ ] 实现导出流程
- [ ] 测试UI交互
- [ ] 添加错误提示

### 测试验证
- [ ] 验证GPU计算正确性
- [ ] 验证文件格式正确
- [ ] 验证文件可读性
- [ ] 性能测试
- [ ] 端到端测试

---

## 🎯 预期成果

✅ **GPU端预计算** - 所有计算在GPU上完成
✅ **逐顶点Light Transport** - 支持大量顶点的并行计算
✅ **UI导出功能** - 一键导出预计算数据
✅ **文件验证** - 确保导出文件正确性

---

**最后更新：** 2025-11-28

