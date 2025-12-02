#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>

// 常数定义 (inline to avoid multiple definition)
inline constexpr float PI = 3.14159265359f;

// 球谐函数系数结构 (2阶, 9个系数)
struct SHCoefficients {
    std::array<glm::vec3, 9> coeffs;
    
    SHCoefficients() {
        for (int i = 0; i < 9; i++) {
            coeffs[i] = glm::vec3(0.0f);
        }
    }
    
    SHCoefficients operator+(const SHCoefficients& other) const {
        SHCoefficients result;
        for (int i = 0; i < 9; i++) {
            result.coeffs[i] = coeffs[i] + other.coeffs[i];
        }
        return result;
    }
    
    SHCoefficients operator*(float scale) const {
        SHCoefficients result;
        for (int i = 0; i < 9; i++) {
            result.coeffs[i] = coeffs[i] * scale;
        }
        return result;
    }
    
    SHCoefficients operator/(float scale) const {
        return *this * (1.0f / scale);
    }
};

// 球谐函数计算类
class SphericalHarmonics {
public:
    // 计算球谐基函数值
    static std::array<float, 9> EvaluateBasis(const glm::vec3& direction);
    
    // 计算单个球谐基函数
    static float EvaluateBasis(int index, const glm::vec3& direction);
    
    // 投影光照到球谐函数
    static SHCoefficients ProjectLight(const std::vector<glm::vec3>& directions,
                                       const std::vector<glm::vec3>& radiances);

    // 投影任意函数到球谐基 (用于Light Transport)
    template<typename Func>
    static SHCoefficients Project(Func func, const std::vector<glm::vec3>& directions) {
        SHCoefficients result;
        int numSamples = directions.size();

        if (numSamples == 0) return result;

        for (int i = 0; i < 9; i++) {
            glm::vec3 coeff(0.0f);
            for (int j = 0; j < numSamples; j++) {
                float basis = EvaluateBasis(i, directions[j]);
                float value = func(directions[j]);
                coeff += glm::vec3(value) * basis;
            }
            result.coeffs[i] = coeff * (4.0f * PI / numSamples);
        }

        return result;
    }

    // 从球谐系数重建光照
    static glm::vec3 ReconstructLight(const SHCoefficients& coeffs, const glm::vec3& direction);
    
    // 生成均匀采样方向 (Fibonacci球)
    static std::vector<glm::vec3> GenerateFibonacciSamples(int numSamples);
    
    // 生成均匀采样方向 (简单网格)
    static std::vector<glm::vec3> GenerateUniformSamples(int numSamples);
    
    // 旋转球谐系数 (绕Y轴)
    static SHCoefficients RotateSHY(const SHCoefficients& coeffs, float angleRadians);
    
    // 旋转矩阵 (绕Y轴)
    static glm::mat3 GetRotationMatrixY(float angleRadians);
    
    // 线性插值两个球谐系数
    static SHCoefficients Lerp(const SHCoefficients& a, const SHCoefficients& b, float t);
};

// 光照采样器
class LightSampler {
public:
    struct Sample {
        glm::vec3 direction;
        glm::vec3 radiance;
    };
    
    // 从立方体贴图采样
    static std::vector<Sample> SampleFromCubemap(const std::vector<glm::vec3>& cubemapData,
                                                  int cubemapSize,
                                                  int numSamples);
    
    // 从均匀颜色采样
    static std::vector<Sample> SampleUniformColor(const glm::vec3& color, int numSamples);
};

// PRT预计算器
class PRTPrecomputer {
public:
    struct RotatedCoefficients {
        float angle;
        SHCoefficients coeffs;
    };

    struct LightTransportData {
        std::vector<SHCoefficients> coefficients;  // 每个顶点/像素的LT系数
    };

    // 预计算不同旋转角度的球谐系数
    static std::vector<RotatedCoefficients> PrecomputeRotations(
        const SHCoefficients& original,
        int numRotations,
        float maxAngle = 360.0f
    );

    // 预计算光照 (从环境光源采样)
    static SHCoefficients PrecomputeLighting(const std::vector<glm::vec3>& directions,
                                             const std::vector<glm::vec3>& radiances);

    // 预计算Light Transport (从场景采样)
    // 这需要对场景中的每个点计算其对入射光的响应
    static SHCoefficients PrecomputeLightTransport(
        const glm::vec3& position,
        const glm::vec3& normal,
        const glm::vec3& albedo,
        const std::vector<glm::vec3>& sampleDirections
    );
};

// 数据导出器
class DataExporter {
public:
    // 导出Lighting系数到txt文件
    static bool ExportLighting(const std::string& filename,
                              const std::vector<PRTPrecomputer::RotatedCoefficients>& rotatedLighting);

    // 导出Light Transport系数到txt文件（单个或批量）
    static bool ExportLightTransport(const std::string& filename,
                                    const SHCoefficients& ltCoeffs);
    static bool ExportLightTransportBatch(const std::string& filename,
                                         const std::vector<SHCoefficients>& ltCoeffsBatch);

    // 导出完整的PRT数据 (Lighting + LT + Rotations)
    static bool ExportPRTData(const std::string& baseFilename,
                             const std::vector<PRTPrecomputer::RotatedCoefficients>& rotatedLighting,
                             const SHCoefficients& ltCoeffs);

    // 从txt文件导入Lighting
    static std::vector<PRTPrecomputer::RotatedCoefficients> ImportLighting(const std::string& filename);

    // 从txt文件导入Light Transport
    static SHCoefficients ImportLightTransport(const std::string& filename);

private:
    static std::string SHCoefficientsToString(const SHCoefficients& coeffs);
    static SHCoefficients StringToSHCoefficients(const std::string& str);
};

// 实时Relighter
class Relighter {
public:
    // 计算relighting结果
    static glm::vec3 ComputeRelighting(const SHCoefficients& coeffs,
                                       const glm::vec3& normal,
                                       const glm::vec3& albedo);

    // 查询旋转角度对应的系数 (带插值)
    static SHCoefficients QueryCoefficients(
        float currentAngle,
        const std::vector<PRTPrecomputer::RotatedCoefficients>& data
    );
};

// PRT渲染接口
class PRTRenderer {
public:
    struct PRTData {
        SHCoefficients lighting;                                    // 当前Lighting系数
        SHCoefficients lightTransport;                              // Light Transport系数
        std::vector<PRTPrecomputer::RotatedCoefficients> rotations; // 所有旋转的Lighting
        float currentRotationAngle;                                 // 当前旋转角度
    };

    // 初始化PRT数据
    static bool Initialize(PRTData& prtData,
                          const std::string& lightingFile,
                          const std::string& ltFile);

    // 更新旋转角度并查询对应的Lighting系数
    static void UpdateRotation(PRTData& prtData, float rotationAngleDegrees);

    // 计算指定点的着色颜色
    static glm::vec3 ComputeShading(const PRTData& prtData,
                                   const glm::vec3& normal,
                                   const glm::vec3& albedo);

    // 批量计算多个点的着色
    static std::vector<glm::vec3> ComputeShadingBatch(
        const PRTData& prtData,
        const std::vector<glm::vec3>& normals,
        const std::vector<glm::vec3>& albedos
    );

    // 获取当前Lighting系数
    static const SHCoefficients& GetCurrentLighting(const PRTData& prtData);

    // 获取Light Transport系数
    static const SHCoefficients& GetLightTransport(const PRTData& prtData);

    // 导出当前状态的着色结果到文件
    static bool ExportShadingResult(const std::string& filename,
                                   const PRTData& prtData,
                                   const std::vector<glm::vec3>& normals,
                                   const std::vector<glm::vec3>& albedos);
};

