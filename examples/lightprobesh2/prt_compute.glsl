#version 450
#extension GL_ARB_separate_shader_objects : enable

// ============================================================================
// PRT Compute Shader - GPU端球谐函数计算和预计算
// ============================================================================

// 工作组大小
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

// ============================================================================
// 数据结构定义（与C++端对应）
// ============================================================================

// 球谐系数（9个系数，每个RGB三通道）
struct SHCoefficients {
    vec4 coeffs[9];  // 使用vec4便于对齐，w分量未使用
};

// 采样方向和辐射度
struct Sample {
    vec4 direction;   // xyz为方向，w未使用
    vec4 radiance;    // xyz为辐射度，w未使用
};

// Light Transport输入数据
struct LTInput {
    vec4 position;    // xyz为位置，w未使用
    vec4 normal;      // xyz为法向量，w未使用
    vec4 albedo;      // xyz为反射率，w未使用
};

// ============================================================================
// Buffer绑定
// ============================================================================

// 采样方向和辐射度 (只读)
layout(std430, binding = 0) readonly buffer SamplesBuffer {
    Sample samples[];
};

// 输入球谐系数 (只读)
layout(std430, binding = 1) readonly buffer InputCoefficientsBuffer {
    SHCoefficients inputCoeffs[];
};

// 输出球谐系数 (读写)
layout(std430, binding = 2) buffer OutputCoefficientsBuffer {
    SHCoefficients outputCoeffs[];
};

// Light Transport输入数据 (只读)
layout(std430, binding = 3) readonly buffer LTInputBuffer {
    LTInput ltInputs[];
};

// 旋转参数 (只读)
layout(std140, binding = 4) uniform RotationParams {
    float angleRadians;
    float padding[3];
} rotationParams;

// 计算参数 (只读)
layout(std140, binding = 5) uniform ComputeParams {
    uint numSamples;
    uint numVertices;
    uint computeMode;  // 0: Lighting, 1: LT, 2: Rotation
    uint padding;
} computeParams;

// ============================================================================
// 球谐基函数计算 (2阶, 9个系数)
// ============================================================================

// 计算单个球谐基函数值
float EvaluateBasis(int index, vec3 direction)
{
    float x = direction.x;
    float y = direction.y;
    float z = direction.z;
    
    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;
    
    switch(index) {
        case 0: return 0.282095;
        case 1: return 0.488603 * y;
        case 2: return 0.488603 * z;
        case 3: return 0.488603 * x;
        case 4: return 1.092548 * x * y;
        case 5: return 1.092548 * y * z;
        case 6: return 0.315392 * (3.0 * z2 - 1.0);
        case 7: return 1.092548 * x * z;
        case 8: return 0.546274 * (x2 - y2);
        default: return 0.0;
    }
}

// 计算所有9个基函数值
void EvaluateAllBasis(vec3 direction, out float basis[9])
{
    for(int i = 0; i < 9; i++) {
        basis[i] = EvaluateBasis(i, direction);
    }
}

// ============================================================================
// 光照投影计算 (Lighting Projection)
// ============================================================================

void ComputeLightingProjection()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 1) return;  // 只有一个输出系数
    
    const float PI = 3.14159265359;
    float normalization = (4.0 * PI) / float(computeParams.numSamples);
    
    // 初始化输出系数
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i] = vec4(0.0);
    }
    
    // 对所有采样方向进行投影
    for(uint s = 0; s < computeParams.numSamples; s++) {
        vec3 direction = samples[s].direction.xyz;
        vec3 radiance = samples[s].radiance.xyz;
        
        // 计算该方向的基函数值
        float basis[9];
        EvaluateAllBasis(direction, basis);
        
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
// Light Transport计算 (Per-Vertex)
// ============================================================================

void ComputeLightTransport()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= computeParams.numVertices) return;
    
    const float PI = 3.14159265359;
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
        
        // 计算该方向的基函数值
        float basis[9];
        EvaluateAllBasis(direction, basis);
        
        // 累加到系数中: LT = albedo * cosTheta * basis
        for(int i = 0; i < 9; i++) {
            outputCoeffs[idx].coeffs[i].xyz += albedo * cosTheta * basis[i];
        }
    }
    
    // 应用归一化因子
    for(int i = 0; i < 9; i++) {
        outputCoeffs[idx].coeffs[i].xyz *= normalization;
    }
}

// ============================================================================
// 球谐旋转计算 (绕Y轴)
// ============================================================================

void ComputeRotatedSH()
{
    uint idx = gl_GlobalInvocationID.x;
    if(idx >= 1) return;  // 只有一个输出系数
    
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
    output.coeffs[1] = input.coeffs[1];  // Y1-1 (y分量)
    output.coeffs[2] = input.coeffs[2];  // Y10  (z分量)
    output.coeffs[3] = input.coeffs[3];  // Y11  (x分量)
    
    // l=2: 5个系数需要旋转
    // Y2-2 (xy项)
    output.coeffs[4] = input.coeffs[4] * c2 - input.coeffs[8] * cs;
    
    // Y2-1 (yz项)
    output.coeffs[5] = input.coeffs[5];
    
    // Y20 (z2项)
    output.coeffs[6] = input.coeffs[6];
    
    // Y21 (xz项)
    output.coeffs[7] = input.coeffs[7] * c2 + input.coeffs[4] * cs;
    
    // Y22 (x2-y2项)
    output.coeffs[8] = input.coeffs[8] * c2 + input.coeffs[4] * s2;
    
    outputCoeffs[idx] = output;
}

// ============================================================================
// 主计算入口
// ============================================================================

void main()
{
    // 根据计算模式调用不同的计算函数
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

