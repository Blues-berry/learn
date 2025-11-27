#include "SphericalHarmonics.h"
#include <iostream>
#include <glm/glm.hpp>

// 简单的PRT系统测试
void TestSphericalHarmonics() {
    std::cout << "=== Testing Spherical Harmonics ===" << std::endl;
    
    // 测试1: 基函数计算
    std::cout << "\nTest 1: Basis Function Evaluation" << std::endl;
    glm::vec3 testDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    auto basis = SphericalHarmonics::EvaluateBasis(testDir);
    std::cout << "Direction: (" << testDir.x << ", " << testDir.y << ", " << testDir.z << ")" << std::endl;
    std::cout << "Basis values: ";
    for (int i = 0; i < 9; i++) {
        std::cout << basis[i] << " ";
    }
    std::cout << std::endl;
    
    // 测试2: 采样生成
    std::cout << "\nTest 2: Sample Generation" << std::endl;
    auto samples = SphericalHarmonics::GenerateFibonacciSamples(16);
    std::cout << "Generated " << samples.size() << " Fibonacci samples" << std::endl;
    std::cout << "First 3 samples:" << std::endl;
    for (int i = 0; i < 3 && i < samples.size(); i++) {
        std::cout << "  Sample " << i << ": (" << samples[i].x << ", " << samples[i].y << ", " << samples[i].z << ")" << std::endl;
    }
    
    // 测试3: 光照投影
    std::cout << "\nTest 3: Light Projection" << std::endl;
    std::vector<glm::vec3> directions = SphericalHarmonics::GenerateFibonacciSamples(32);
    std::vector<glm::vec3> radiances;
    for (int i = 0; i < directions.size(); i++) {
        radiances.push_back(glm::vec3(1.0f, 1.0f, 1.0f));
    }
    
    SHCoefficients coeffs = SphericalHarmonics::ProjectLight(directions, radiances);
    std::cout << "Projected SH coefficients:" << std::endl;
    for (int i = 0; i < 9; i++) {
        std::cout << "  coeff[" << i << "]: (" << coeffs.coeffs[i].x << ", " 
                  << coeffs.coeffs[i].y << ", " << coeffs.coeffs[i].z << ")" << std::endl;
    }
    
    // 测试4: 光照重建
    std::cout << "\nTest 4: Light Reconstruction" << std::endl;
    glm::vec3 reconstructed = SphericalHarmonics::ReconstructLight(coeffs, testDir);
    std::cout << "Reconstructed light: (" << reconstructed.x << ", " << reconstructed.y << ", " << reconstructed.z << ")" << std::endl;
    
    // 测试5: 旋转
    std::cout << "\nTest 5: SH Rotation" << std::endl;
    float angle = 45.0f * 3.14159f / 180.0f;
    SHCoefficients rotatedCoeffs = SphericalHarmonics::RotateSHY(coeffs, angle);
    std::cout << "Rotated SH coefficients (45 degrees):" << std::endl;
    for (int i = 0; i < 9; i++) {
        std::cout << "  coeff[" << i << "]: (" << rotatedCoeffs.coeffs[i].x << ", " 
                  << rotatedCoeffs.coeffs[i].y << ", " << rotatedCoeffs.coeffs[i].z << ")" << std::endl;
    }
    
    // 测试6: 预计算旋转
    std::cout << "\nTest 6: Precompute Rotations" << std::endl;
    auto rotations = PRTPrecomputer::PrecomputeRotations(coeffs, 8, 360.0f);
    std::cout << "Precomputed " << rotations.size() << " rotations:" << std::endl;
    for (int i = 0; i < rotations.size(); i++) {
        std::cout << "  Rotation " << i << " (angle: " << rotations[i].angle << " degrees)" << std::endl;
    }
    
    // 测试7: 数据导出和导入
    std::cout << "\nTest 7: Data Export/Import" << std::endl;
    std::string testFile = "test_prt_data";
    if (DataExporter::ExportLighting(testFile + "_lighting.txt", rotations)) {
        std::cout << "Successfully exported Lighting to " << testFile << "_lighting.txt" << std::endl;

        auto importedData = DataExporter::ImportLighting(testFile + "_lighting.txt");
        std::cout << "Successfully imported " << importedData.size() << " rotations" << std::endl;

        if (importedData.size() > 0) {
            std::cout << "First imported rotation angle: " << importedData[0].angle << std::endl;
        }
    } else {
        std::cout << "Failed to export data!" << std::endl;
    }
    
    // 测试8: 旋转查询和插值
    std::cout << "\nTest 8: Rotation Query and Interpolation" << std::endl;
    float queryAngle = 22.5f;
    SHCoefficients queriedCoeffs = Relighter::QueryCoefficients(queryAngle, rotations);
    std::cout << "Queried coefficients for angle " << queryAngle << " degrees:" << std::endl;
    std::cout << "  coeff[0]: (" << queriedCoeffs.coeffs[0].x << ", " 
              << queriedCoeffs.coeffs[0].y << ", " << queriedCoeffs.coeffs[0].z << ")" << std::endl;
    
    // 测试9: Relighting计算
    std::cout << "\nTest 9: Relighting Computation" << std::endl;
    glm::vec3 normal = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    glm::vec3 relightingResult = Relighter::ComputeRelighting(coeffs, normal, albedo);
    std::cout << "Relighting result: (" << relightingResult.x << ", " << relightingResult.y << ", " << relightingResult.z << ")" << std::endl;
    
    // 测试10: 插值
    std::cout << "\nTest 10: SH Coefficients Interpolation" << std::endl;
    SHCoefficients coeffs1 = coeffs;
    SHCoefficients coeffs2 = rotatedCoeffs;
    SHCoefficients lerpedCoeffs = SphericalHarmonics::Lerp(coeffs1, coeffs2, 0.5f);
    std::cout << "Interpolated coefficients (t=0.5):" << std::endl;
    std::cout << "  coeff[0]: (" << lerpedCoeffs.coeffs[0].x << ", " 
              << lerpedCoeffs.coeffs[0].y << ", " << lerpedCoeffs.coeffs[0].z << ")" << std::endl;
    
    std::cout << "\n=== All Tests Completed ===" << std::endl;
}

// 主函数 (可选，用于独立测试)
#ifdef PRT_TEST_STANDALONE
int main() {
    TestSphericalHarmonics();
    return 0;
}
#endif

