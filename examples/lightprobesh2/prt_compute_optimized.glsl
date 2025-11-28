#version 450
#extension GL_ARB_separate_shader_objects : enable

// ============================================================================
// PRT Compute Shader (优化版本) - GPU端球谐函数计算和预计算
// ============================================================================

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// ============================================================================
// 常数定义（预计算，避免重复计算）
// ============================================================================

const float PI = 3.14159265359;
const float C0 = 0.282095;      // Y00系数
const float C1 = 0.488603;      // Y1m系数
const float C2 = 1.092548;      // Y2m系数
const float C3 = 0.315392;      // Y20系数
const float C4 = 0.546274;      // Y22系数

// ============================================================================
// 数据结构定义
// ============================================================================

struct SHCoefficients {
    vec4 coeffs[9];
};

struct Sample {
    vec4 direction;
    vec4 radiance;
};

struct LTInput {
    vec4 position;
    vec4 normal;
    vec4 albedo;
};

// ============================================================================
// Buffer绑定
// ============================================================================

layout(std430, binding = 0) readonly buffer SamplesBuffer {
    Sample samples[];
};

layout(std430, binding = 1) readonly buffer InputCoefficientsBuffer {
    SHCoefficients inputCoeffs[];
};

layout(std430, binding = 2) buffer OutputCoefficientsBuffer {
    SHCoefficients outputCoeffs[];
};

layout(std430, binding = 3) readonly buffer LTInputBuffer {
    LTInput ltInputs[];
};

layout(std140, binding = 4) uniform RotationParams {
    float angleRadians;
    float padding[3];
} rotationParams;

layout(std140, binding = 5) uniform ComputeParams {
    uint numSamples;
    uint numVertices;
    uint computeMode;
    uint padding;
} computeParams;

// ============================================================================
// 优化的球谐基函数计算（向量化）
// ============================================================================

// 计算所有9个基函数值（优化版本）
void EvaluateAllBasisOptimized(vec3 dir, out float basis[9])
{
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;
    
    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;
    
    // 预计算常用项
    float xy = x * y;
    float yz = y * z;
    float xz = x * z;
    float x2_minus_y2 = x2 - y2;
    float three_z2_minus_1 = 3.0 * z2 - 1.0;
    
    // 第一组基函数
    basis[0] = C0;              // Y00
    basis[1] = C1 * y;          // Y1-1
    basis[2] = C1 * z;          // Y10
    basis[3] = C1 * x;          // Y11
    
    // 第二组基函数
    basis[4] = C2 * xy;         // Y2-2
    basis[5] = C2 * yz;         // Y2-1
    basis[6] = C3 * three_z2_minus_1;  // Y20
    basis[7] = C2 * xz;         // Y21
    basis[8] = C4 * x2_minus_y2; // Y22
}

// 单个基函数计算（用于调试）
float EvaluateBasis(int index, vec3 dir)
{
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;
    
    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;
    
    switch(index) {
        case 0: return C0;
        case 1: return C1 * y;
        case 2: return C1 * z;
        case 3: return C1 * x;
        case 4: return C2 * x * y;
        case 5: return C2 * y * z;
        case 6: return C3 * (3.0 * z2 - 1.0);
        case 7: return C2 * x * z;
        case 8: return C4 * (x2 - y2);
        default: return 0.0;
    }
}

// ============================================================================
// 光照投影计算（优化版本）
// ============================================================================

void ComputeLightingProjection()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 1) return;
    
    float normalization = (4.0 * PI) / float(computeParams.numSamples);
    
    // 初始化输出系数
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i] = vec4(0.0);
    }
    
    // 对所有采样方向进行投影
    for(uint s = 0; s < computeParams.numSamples; s++) {
        vec3 direction = samples[s].direction.xyz;
        vec3 radiance = samples[s].radiance.xyz;
        
        // 计算该方向的所有基函数值
        float basis[9];
        EvaluateAllBasisOptimized(direction, basis);
        
        // 累加到系数中
        for(int i = 0; i < 9; i++) {
            outputCoeffs[idx].coeffs[i].xyz += radiance * basis[i];
        }
    }
    
    // 应用归一化因子
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i].xyz *= normalization;
    }
}

// ============================================================================
// Light Transport计算（优化版本）
// ============================================================================

void ComputeLightTransport()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= computeParams.numVertices) return;
    
    float normalization = (4.0 * PI) / float(computeParams.numSamples);
    
    LTInput ltInput = ltInputs[idx];
    vec3 normal = normalize(ltInput.normal.xyz);
    vec3 albedo = ltInput.albedo.xyz;
    
    // 初始化输出系数
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i] = vec4(0.0);
    }
    
    // 对所有采样方向进行Light Transport计算
    for(uint s = 0; s < computeParams.numSamples; s++) {
        vec3 direction = normalize(samples[s].direction.xyz);
        
        // Lambert's law: max(0, dot(normal, direction))
        float cosTheta = max(0.0, dot(normal, direction));
        
        // 计算该方向的所有基函数值
        float basis[9];
        EvaluateAllBasisOptimized(direction, basis);
        
        // 累加到系数中: LT = albedo * cosTheta * basis
        vec3 contribution = albedo * cosTheta;
        for(int i = 0; i < 9; i++) {
            outputCoeffs[idx].coeffs[i].xyz += contribution * basis[i];
        }
    }
    
    // 应用归一化因子
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i].xyz *= normalization;
    }
}

// ============================================================================
// 球谐旋转计算（绕Y轴）
// ============================================================================

void ComputeRotatedSH()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 1) return;
    
    float c = cos(rotationParams.angleRadians);
    float s = sin(rotationParams.angleRadians);
    float c2 = c * c;
    float s2 = s * s;
    float cs = c * s;
    
    SHCoefficients input = inputCoeffs[idx];
    SHCoefficients output;
    
    // l=0: Y00 (不受旋转影响)
    output.coeffs[0] = input.coeffs[0];
    
    // l=1: Y1m, Y10, Y1p (不受绕Y轴旋转影响)
    output.coeffs[1] = input.coeffs[1];
    output.coeffs[2] = input.coeffs[2];
    output.coeffs[3] = input.coeffs[3];
    
    // l=2: 5个系数需要旋转
    output.coeffs[4] = input.coeffs[4] * c2 - input.coeffs[8] * cs;
    output.coeffs[5] = input.coeffs[5];
    output.coeffs[6] = input.coeffs[6];
    output.coeffs[7] = input.coeffs[7] * c2 + input.coeffs[4] * cs;
    output.coeffs[8] = input.coeffs[8] * c2 + input.coeffs[4] * s2;
    
    outputCoeffs[idx] = output;
}

// ============================================================================
// 主计算入口
// ============================================================================

void main()
{
    switch(computeParams.computeMode) {
        case 0:  // Lighting Projection
            ComputeLightingProjection();
            break;
        case 1:  // Light Transport
            ComputeLightTransport();
            break;
        case 2:  // SH Rotation
            ComputeRotatedSH();
            break;
    }
}

