#version 450

// PRT relighting vertex shader

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outColor;

// Incoming vertex attributes for PRT
// We pass the 9 SH coefficients as 9 separate vec4 attributes
layout(location = 5) in vec4 in_lt_c0;
layout(location = 6) in vec4 in_lt_c1;
layout(location = 7) in vec4 in_lt_c2;
layout(location = 8) in vec4 in_lt_c3;
layout(location = 9) in vec4 in_lt_c4;
layout(location = 10) in vec4 in_lt_c5;
layout(location = 11) in vec4 in_lt_c6;
layout(location = 12) in vec4 in_lt_c7;
layout(location = 13) in vec4 in_lt_c8;

struct SHCoefficients {
    vec4 coeffs[9]; // .xyz stores coefficient
};

// Rotated lighting SH (Irradiance)
layout(set = 1, binding = 0) uniform LightingUBO {
    SHCoefficients lighting;
} ubo;

layout(set = 0, binding = 0) uniform UBO {
	mat4 projection;
	mat4 view;
} uboMatrices;

// Push constants for per-object data
layout(push_constant) uniform PushConstantBlock {
    mat4 model;
    vec4 baseColor;
} pushConstants;

void main()
{
    // Standard vertex transformation using the model matrix from the push constant
    gl_Position = uboMatrices.projection * uboMatrices.view * pushConstants.model * vec4(inPosition, 1.0);

    // Reconstruct the LT coefficients from vertex attributes
    vec3 lt_coeffs[9];
    lt_coeffs[0] = in_lt_c0.xyz;
    lt_coeffs[1] = in_lt_c1.xyz;
    lt_coeffs[2] = in_lt_c2.xyz;
    lt_coeffs[3] = in_lt_c3.xyz;
    lt_coeffs[4] = in_lt_c4.xyz;
    lt_coeffs[5] = in_lt_c5.xyz;
    lt_coeffs[6] = in_lt_c6.xyz;
    lt_coeffs[7] = in_lt_c7.xyz;
    lt_coeffs[8] = in_lt_c8.xyz;

    // PRT Relighting: dot product of Lighting SH and per-vertex LT SH
    vec3 prtColor = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        prtColor += ubo.lighting.coeffs[i].xyz * lt_coeffs[i];
    }

    // Modulate the PRT lighting result by the material's base color (albedo)
    vec3 finalColor = pushConstants.baseColor.rgb * prtColor;

    // Clamp to avoid negative values and pass to fragment shader
    outColor = max(vec3(0.0), finalColor);
}

