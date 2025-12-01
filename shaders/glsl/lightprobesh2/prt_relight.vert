#version 450

// PRT relighting vertex shader
// FIXED: Use SSBO for LT coefficients and match Global UBO layout with main pass (projection + view)

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outColor;
layout (location = 1) out float outExposure;
layout (location = 2) out float outGamma;

struct SHCoefficients {
    vec4 coeffs[9]; // .xyz stores coefficient
};

// Global UBO must exactly match MainPass::GlobalUbo layout (Pass.h)
layout (set = 0, binding = 0) uniform Global
{
    mat4 projection;  // 投影矩阵
    mat4 view;        // 视图矩阵
    vec4 light[4];    // 光照信息（未用）
    vec4 cameraPos;   // 相机位置（未用）
    float exposure;   // 曝光（未用）
    float gamma;      // 伽马（未用）
    int useLightSource;   // 未用
    float lightIntensity; // 未用
    vec3 lightPosition;   // 未用
    vec3 lightColor;      // 未用
} global;

// Rotated lighting SH (Irradiance)
layout(set = 1, binding = 0) uniform LightingUBO {
    SHCoefficients lighting;
} ubo;

// Per-vertex LT coefficients SSBO
layout(set = 1, binding = 1) readonly buffer LTCoefficientsBuffer {
    SHCoefficients ltCoefficients[];
} ltBuffer;

// Push constants for per-object data
layout(push_constant) uniform PushConstantBlock {
    mat4 model;
    vec4 baseColor;
} pushConstants;

void main()
{
    // Vertex transform using projection and view from Global UBO
    gl_Position = global.projection * global.view * pushConstants.model * vec4(inPosition, 1.0);

    // Get the LT coefficients for this vertex from SSBO
    // The index buffer contains absolute indices (adjusted by vertexStart during model loading)
    // gl_VertexIndex gives us the index value from the index buffer (which is already global)
    uint vid = uint(gl_VertexIndex);

    // Bounds check: if vid is out of range, output magenta to make the problem visible
    SHCoefficients lt_coeffs;
    if (vid >= ltBuffer.ltCoefficients.length()) {
        // Out of bounds - output magenta to make the problem visible
        outColor = vec3(1.0, 0.0, 1.0);
        return;
    } else {
        lt_coeffs = ltBuffer.ltCoefficients[vid];
    }

    // Check if LT coefficients are all zero (indicates missing or invalid data)
    bool allZero = true;
    for (int i = 0; i < 9; i++) {
        if (length(lt_coeffs.coeffs[i].xyz) > 0.001) {
            allZero = false;
            break;
        }
    }

    if (allZero) {
        // All LT coefficients are zero - output cyan to indicate this
        outColor = vec3(0.0, 1.0, 1.0);
        return;
    }

    // PRT Relighting: dot product of Lighting SH and per-vertex LT SH
    vec3 prtColor = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        prtColor += ubo.lighting.coeffs[i].xyz * lt_coeffs.coeffs[i].xyz;
    }

    // Modulate the PRT lighting result by the material's base color (albedo)
    vec3 finalColor = pushConstants.baseColor.rgb * prtColor;

    // Clamp to avoid negative values and pass to fragment shader
    outColor = max(vec3(0.0), finalColor);

    // Pass exposure and gamma to the fragment shader
    outExposure = global.exposure;
    outGamma = global.gamma;
}

