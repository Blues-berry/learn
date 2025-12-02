#include "SphericalHarmonics.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

// 常数 (PI is defined in header as inline constexpr)
const float PHI = 1.61803398875f;

// 球谐基函数计算
std::array<float, 9> SphericalHarmonics::EvaluateBasis(const glm::vec3& direction) {
    std::array<float, 9> basis;
    for (int i = 0; i < 9; i++) {
        basis[i] = EvaluateBasis(i, direction);
    }
    return basis;
}

float SphericalHarmonics::EvaluateBasis(int index, const glm::vec3& direction) {
    float x = direction.x;
    float y = direction.y;
    float z = direction.z;
    
    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;
    
    switch (index) {
        case 0: return 0.282095f;
        case 1: return 0.488603f * y;
        case 2: return 0.488603f * z;
        case 3: return 0.488603f * x;
        case 4: return 1.092548f * x * y;
        case 5: return 1.092548f * y * z;
        case 6: return 0.315392f * (3.0f * z2 - 1.0f);
        case 7: return 1.092548f * x * z;
        case 8: return 0.546274f * (x2 - y2);
        default: return 0.0f;
    }
}

SHCoefficients SphericalHarmonics::ProjectLight(const std::vector<glm::vec3>& directions,
                                                const std::vector<glm::vec3>& radiances) {
    SHCoefficients result;
    int numSamples = directions.size();
    
    if (numSamples == 0) return result;
    
    for (int i = 0; i < 9; i++) {
        glm::vec3 coeff(0.0f);
        for (int j = 0; j < numSamples; j++) {
            float basis = EvaluateBasis(i, directions[j]);
            coeff += radiances[j] * basis;
        }
        result.coeffs[i] = coeff * (4.0f * PI / numSamples);
    }
    
    return result;
}

glm::vec3 SphericalHarmonics::ReconstructLight(const SHCoefficients& coeffs, const glm::vec3& direction) {
    glm::vec3 result(0.0f);
    auto basis = EvaluateBasis(direction);
    
    for (int i = 0; i < 9; i++) {
        result += coeffs.coeffs[i] * basis[i];
    }
    
    return result;
}

std::vector<glm::vec3> SphericalHarmonics::GenerateFibonacciSamples(int numSamples) {
    std::vector<glm::vec3> samples;
    
    for (int i = 0; i < numSamples; i++) {
        float theta = acos(1.0f - 2.0f * i / numSamples);
        float phi = 2.0f * PI * i / PHI;
        
        float x = sin(theta) * cos(phi);
        float y = cos(theta);
        float z = sin(theta) * sin(phi);
        
        samples.push_back(glm::normalize(glm::vec3(x, y, z)));
    }
    
    return samples;
}

std::vector<glm::vec3> SphericalHarmonics::GenerateUniformSamples(int numSamples) {
    std::vector<glm::vec3> samples;
    int sqrtSamples = (int)sqrt(numSamples);
    
    for (int i = 0; i < sqrtSamples; i++) {
        for (int j = 0; j < sqrtSamples; j++) {
            float theta = acos(1.0f - 2.0f * (i + 0.5f) / sqrtSamples);
            float phi = 2.0f * PI * (j + 0.5f) / sqrtSamples;
            
            float x = sin(theta) * cos(phi);
            float y = cos(theta);
            float z = sin(theta) * sin(phi);
            
            samples.push_back(glm::normalize(glm::vec3(x, y, z)));
        }
    }
    
    return samples;
}

glm::mat3 SphericalHarmonics::GetRotationMatrixY(float angleRadians) {
    float c = cos(angleRadians);
    float s = sin(angleRadians);
    
    return glm::mat3(
        c, 0, s,
        0, 1, 0,
        -s, 0, c
    );
}

SHCoefficients SphericalHarmonics::RotateSHY(const SHCoefficients& coeffs, float angleRadians) {
    // 2阶球谐函数的旋转矩阵 (绕Y轴)
    // 参考: Sloan et al. "Clustered Principal Component Analysis for Real-time Rendering"

    float c = cos(angleRadians);
    float s = sin(angleRadians);

    SHCoefficients result;

    // l=0: Y00 (不受旋转影响)
    result.coeffs[0] = coeffs.coeffs[0];

    // l=1: Y1m, Y10, Y1p
    // 旋转矩阵应用于 (Y1m, Y10, Y1p) = (Y1-1, Y10, Y11)
    result.coeffs[1] = coeffs.coeffs[1];  // Y1-1 (y分量)
    result.coeffs[2] = coeffs.coeffs[2];  // Y10  (z分量)
    result.coeffs[3] = coeffs.coeffs[3];  // Y11  (x分量)

    // l=2: 5个系数需要旋转
    // 这是一个简化的旋转，对于绕Y轴的旋转
    float c2 = c * c;
    float s2 = s * s;
    float cs = c * s;

    // Y2-2 (xy项)
    result.coeffs[4] = coeffs.coeffs[4] * c2 - coeffs.coeffs[8] * cs;

    // Y2-1 (yz项)
    result.coeffs[5] = coeffs.coeffs[5];

    // Y20 (z2项)
    result.coeffs[6] = coeffs.coeffs[6];

    // Y21 (xz项)
    result.coeffs[7] = coeffs.coeffs[7] * c2 + coeffs.coeffs[4] * cs;

    // Y22 (x2-y2项)
    result.coeffs[8] = coeffs.coeffs[8] * c2 + coeffs.coeffs[4] * s2;

    return result;
}

SHCoefficients SphericalHarmonics::Lerp(const SHCoefficients& a, const SHCoefficients& b, float t) {
    SHCoefficients result;
    for (int i = 0; i < 9; i++) {
        result.coeffs[i] = glm::mix(a.coeffs[i], b.coeffs[i], t);
    }
    return result;
}

// LightSampler 实现
std::vector<LightSampler::Sample> LightSampler::SampleFromCubemap(const std::vector<glm::vec3>& cubemapData,
                                                                   int cubemapSize,
                                                                   int numSamples) {
    std::vector<Sample> samples;
    auto directions = SphericalHarmonics::GenerateFibonacciSamples(numSamples);
    
    for (const auto& dir : directions) {
        // 简化: 从立方体贴图中采样
        // 实际应用中需要正确的立方体贴图坐标转换
        Sample sample;
        sample.direction = dir;
        sample.radiance = glm::vec3(1.0f); // 占位符
        samples.push_back(sample);
    }
    
    return samples;
}

std::vector<LightSampler::Sample> LightSampler::SampleUniformColor(const glm::vec3& color, int numSamples) {
    std::vector<Sample> samples;
    auto directions = SphericalHarmonics::GenerateFibonacciSamples(numSamples);
    
    for (const auto& dir : directions) {
        Sample sample;
        sample.direction = dir;
        sample.radiance = color;
        samples.push_back(sample);
    }
    
    return samples;
}

// PRTPrecomputer 实现
std::vector<PRTPrecomputer::RotatedCoefficients> PRTPrecomputer::PrecomputeRotations(
    const SHCoefficients& original,
    int numRotations,
    float maxAngle) {
    
    std::vector<RotatedCoefficients> result;
    
    for (int i = 0; i < numRotations; i++) {
        float angle = (i / (float)numRotations) * maxAngle;
        float angleRad = angle * PI / 180.0f;
        
        RotatedCoefficients rc;
        rc.angle = angle;
        rc.coeffs = SphericalHarmonics::RotateSHY(original, angleRad);
        result.push_back(rc);
    }
    
    return result;
}

SHCoefficients PRTPrecomputer::PrecomputeLighting(const std::vector<glm::vec3>& directions,
                                                  const std::vector<glm::vec3>& radiances) {
    return SphericalHarmonics::ProjectLight(directions, radiances);
}

SHCoefficients PRTPrecomputer::PrecomputeLightTransport(
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& albedo,
    const std::vector<glm::vec3>& sampleDirections) {

    // Light Transport = albedo * max(0, dot(normal, direction)) * visibility
    // 这里我们简化为: albedo * max(0, dot(normal, direction))
    // 实际应用中需要加入visibility项 (阴影/遮挡)

    SHCoefficients result;
    int numSamples = sampleDirections.size();

    if (numSamples == 0) return result;

    for (int i = 0; i < 9; i++) {
        glm::vec3 coeff(0.0f);
        for (int j = 0; j < numSamples; j++) {
            const glm::vec3& dir = sampleDirections[j];

            // 计算cosine项 (Lambert's law)
            float cosTheta = glm::max(0.0f, glm::dot(normal, dir));

            // 计算球谐基函数
            float basis = SphericalHarmonics::EvaluateBasis(i, dir);

            // Light Transport = albedo * cosine * basis
            coeff += albedo * cosTheta * basis;
        }
        result.coeffs[i] = coeff * (4.0f * PI / numSamples);
    }

    return result;
}

// DataExporter 实现
std::string DataExporter::SHCoefficientsToString(const SHCoefficients& coeffs) {
    std::stringstream ss;
    for (int i = 0; i < 9; i++) {
        ss << coeffs.coeffs[i].x << " " << coeffs.coeffs[i].y << " " << coeffs.coeffs[i].z;
        if (i < 8) ss << " ";
    }
    return ss.str();
}

SHCoefficients DataExporter::StringToSHCoefficients(const std::string& str) {
    SHCoefficients coeffs;
    std::stringstream ss(str);
    
    for (int i = 0; i < 9; i++) {
        ss >> coeffs.coeffs[i].x >> coeffs.coeffs[i].y >> coeffs.coeffs[i].z;
    }
    
    return coeffs;
}

bool DataExporter::ExportLighting(const std::string& filename,
                                 const std::vector<PRTPrecomputer::RotatedCoefficients>& rotatedLighting) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "# PRT Lighting Data (Rotated)\n";
    file << "# Generated: 2025-11-27\n";
    file << "# Rotations: " << rotatedLighting.size() << "\n";
    file << "# SH Order: 2 (9 coefficients)\n";
    file << "# Format: angle coeff[0].xyz coeff[1].xyz ... coeff[8].xyz\n\n";

    for (const auto& rc : rotatedLighting) {
        file << rc.angle << " " << SHCoefficientsToString(rc.coeffs) << "\n";
    }

    file.close();
    return true;
}

bool DataExporter::ExportLightTransport(const std::string& filename,
                                       const SHCoefficients& ltCoeffs) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "# PRT Light Transport Data\n";
    file << "# Generated: 2025-11-27\n";
    file << "# SH Order: 2 (9 coefficients)\n";
    file << "# Format: coeff[0].xyz coeff[1].xyz ... coeff[8].xyz\n\n";
    file << SHCoefficientsToString(ltCoeffs) << "\n";

    file.close();
    return true;
}

// 批量导出每个顶点的LT系数
bool DataExporter::ExportLightTransportBatch(const std::string& filename,
                                            const std::vector<SHCoefficients>& ltBatch) {
    std::ofstream file(filename);
    if (!file.is_open()) return false;

    file << "# PRT Light Transport Data (Per-Vertex)\n";
    file << "# Generated: 2025-11-27\n";
    file << "# Vertices: " << ltBatch.size() << "\n";
    file << "# SH Order: 2 (9 coefficients)\n";
    file << "# Format per line: coeff[0].xyz coeff[1].xyz ... coeff[8].xyz\n\n";

    for (const auto& lt : ltBatch) {
        file << SHCoefficientsToString(lt) << "\n";
    }

    file.close();
    return true;
}

bool DataExporter::ExportPRTData(const std::string& baseFilename,
                                const std::vector<PRTPrecomputer::RotatedCoefficients>& rotatedLighting,
                                const SHCoefficients& ltCoeffs) {
    // 导出Lighting
    std::string lightingFile = baseFilename + "_lighting.txt";
    if (!ExportLighting(lightingFile, rotatedLighting)) {
        return false;
    }

    // 导出Light Transport
    std::string ltFile = baseFilename + "_lt.txt";
    if (!ExportLightTransport(ltFile, ltCoeffs)) {
        return false;
    }

    return true;
}

std::vector<PRTPrecomputer::RotatedCoefficients> DataExporter::ImportLighting(const std::string& filename) {
    std::vector<PRTPrecomputer::RotatedCoefficients> result;
    std::ifstream file(filename);

    if (!file.is_open()) return result;

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        float angle;
        ss >> angle;

        // 读取剩余的系数字符串
        std::string coeffStr;
        std::getline(ss, coeffStr);

        PRTPrecomputer::RotatedCoefficients rc;
        rc.angle = angle;
        rc.coeffs = StringToSHCoefficients(coeffStr);
        result.push_back(rc);
    }

    file.close();
    return result;
}

SHCoefficients DataExporter::ImportLightTransport(const std::string& filename) {
    SHCoefficients result;
    std::ifstream file(filename);

    if (!file.is_open()) return result;

    std::string line;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        result = StringToSHCoefficients(line);
        break;  // 只读第一行数据
    }

    file.close();
    return result;
}

// Relighter 实现
glm::vec3 Relighter::ComputeRelighting(const SHCoefficients& coeffs,
                                       const glm::vec3& normal,
                                       const glm::vec3& albedo) {
    glm::vec3 lighting = SphericalHarmonics::ReconstructLight(coeffs, normal);
    return albedo * lighting;
}

SHCoefficients Relighter::QueryCoefficients(
    float currentAngle,
    const std::vector<PRTPrecomputer::RotatedCoefficients>& data) {
    
    if (data.empty()) return SHCoefficients();
    
    // 规范化角度到 [0, 360)
    while (currentAngle < 0.0f) currentAngle += 360.0f;
    while (currentAngle >= 360.0f) currentAngle -= 360.0f;
    
    // 找到最近的两个数据点
    int idx1 = 0, idx2 = 0;
    float minDist = 360.0f;
    
    for (int i = 0; i < data.size(); i++) {
        float dist = fabs(data[i].angle - currentAngle);
        if (dist < minDist) {
            minDist = dist;
            idx1 = i;
        }
    }
    
    // 找到第二个最近的点
    minDist = 360.0f;
    for (int i = 0; i < data.size(); i++) {
        if (i == idx1) continue;
        float dist = fabs(data[i].angle - currentAngle);
        if (dist < minDist) {
            minDist = dist;
            idx2 = i;
        }
    }
    
    // 线性插值
    float angle1 = data[idx1].angle;
    float angle2 = data[idx2].angle;
    float t = 0.5f;
    
    if (fabs(angle2 - angle1) > 0.001f) {
        t = (currentAngle - angle1) / (angle2 - angle1);
        t = glm::clamp(t, 0.0f, 1.0f);
    }
    
    return SphericalHarmonics::Lerp(data[idx1].coeffs, data[idx2].coeffs, t);
}

// ============================================================================
// PRTRenderer 实现
// ============================================================================

bool PRTRenderer::Initialize(PRTData& prtData,
                            const std::string& lightingFile,
                            const std::string& ltFile) {
    // 导入Lighting数据
    prtData.rotations = DataExporter::ImportLighting(lightingFile);
    if (prtData.rotations.empty()) {
        std::cerr << "[PRTRenderer] Failed to import lighting data from: " << lightingFile << std::endl;
        return false;
    }

    // 导入Light Transport数据
    prtData.lightTransport = DataExporter::ImportLightTransport(ltFile);

    // 初始化当前Lighting为第一个旋转角度
    prtData.currentRotationAngle = 0.0f;
    prtData.lighting = prtData.rotations[0].coeffs;

    std::cout << "[PRTRenderer] Initialized successfully" << std::endl;
    std::cout << "  - Loaded " << prtData.rotations.size() << " rotations" << std::endl;
    std::cout << "  - Light Transport coefficients loaded" << std::endl;

    return true;
}

void PRTRenderer::UpdateRotation(PRTData& prtData, float rotationAngleDegrees) {
    prtData.currentRotationAngle = rotationAngleDegrees;
    prtData.lighting = Relighter::QueryCoefficients(rotationAngleDegrees, prtData.rotations);
}

glm::vec3 PRTRenderer::ComputeShading(const PRTData& prtData,
                                     const glm::vec3& normal,
                                     const glm::vec3& albedo) {
    // Note: ComputeRelighting already applies albedo, so we don't multiply again
    return Relighter::ComputeRelighting(prtData.lighting, normal, albedo);
}

std::vector<glm::vec3> PRTRenderer::ComputeShadingBatch(
    const PRTData& prtData,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec3>& albedos) {

    std::vector<glm::vec3> results;
    results.reserve(normals.size());

    for (size_t i = 0; i < normals.size(); i++) {
        glm::vec3 albedo = (i < albedos.size()) ? albedos[i] : glm::vec3(0.8f);
        results.push_back(ComputeShading(prtData, normals[i], albedo));
    }

    return results;
}

const SHCoefficients& PRTRenderer::GetCurrentLighting(const PRTData& prtData) {
    return prtData.lighting;
}

const SHCoefficients& PRTRenderer::GetLightTransport(const PRTData& prtData) {
    return prtData.lightTransport;
}

bool PRTRenderer::ExportShadingResult(const std::string& filename,
                                     const PRTData& prtData,
                                     const std::vector<glm::vec3>& normals,
                                     const std::vector<glm::vec3>& albedos) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[PRTRenderer] Failed to open file: " << filename << std::endl;
        return false;
    }

    file << "# PRT Shading Results\n";
    file << "# Rotation Angle: " << prtData.currentRotationAngle << " degrees\n";
    file << "# Format: normal.xyz albedo.xyz shading.xyz\n\n";

    auto shadingResults = ComputeShadingBatch(prtData, normals, albedos);

    for (size_t i = 0; i < normals.size(); i++) {
        const auto& normal = normals[i];
        const auto& albedo = (i < albedos.size()) ? albedos[i] : glm::vec3(0.8f);
        const auto& shading = shadingResults[i];

        file << normal.x << " " << normal.y << " " << normal.z << " "
             << albedo.x << " " << albedo.y << " " << albedo.z << " "
             << shading.x << " " << shading.y << " " << shading.z << "\n";
    }

    file.close();
    std::cout << "[PRTRenderer] Exported shading results to: " << filename << std::endl;
    return true;
}

