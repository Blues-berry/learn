#version 450

// PRT relighting fragment shader
// OPTIMIZED: Added tone mapping to match PBR rendering

layout (location = 0) in vec3 inColor;
layout (location = 1) in float inExposure;
layout (location = 2) in float inGamma;

layout (location = 0) out vec4 outFragColor;

// Uncharted2 tone mapping (matches PBR shader)
vec3 Uncharted2Tonemap(vec3 x) {
    float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

void main()
{
    vec3 color = inColor;

    // Apply tone mapping (same as PBR shader)
    color = Uncharted2Tonemap(color * inExposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));

    // Gamma correction (same as PBR shader)
    color = pow(color, vec3(1.0f / inGamma));

    outFragColor = vec4(color, 1.0);
}

