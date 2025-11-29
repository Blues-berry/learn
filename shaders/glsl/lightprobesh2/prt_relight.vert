#version 450

// PRT relighting vertex shader

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outColor;

struct SHCoefficients {
    vec4 coeffs[9]; // .xyz stores coefficient
};

// Per-vertex LT coefficients (read-only)
layout(set = 1, binding = 0) buffer LTCoefficients {
    SHCoefficients lt[];
} ltBuffer;

// Rotated lighting SH (Irradiance)
layout(set = 1, binding = 1) uniform LightingUBO {
    SHCoefficients lighting;
} ubo;

layout(set = 0, binding = 0) uniform UBO {
	mat4 projection;
	mat4 model;
	mat4 view;
} uboMatrices;

void main() 
{
    // Standard vertex transformation
    gl_Position = uboMatrices.projection * uboMatrices.view * uboMatrices.model * vec4(inPosition, 1.0);

    // PRT Relighting: dot product of Lighting SH and per-vertex LT SH
    vec3 finalColor = vec3(0.0);
    for (int i = 0; i < 9; i++) {
        finalColor += ubo.lighting.coeffs[i].xyz * ltBuffer.lt[gl_VertexIndex].coeffs[i].xyz;
    }

    // Clamp to avoid negative values from SH approximation
    outColor = max(vec3(0.0), finalColor);
}

