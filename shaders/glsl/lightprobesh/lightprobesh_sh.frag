#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (binding = 0) uniform UBO {
    mat4 projection;
    mat4 model;
    mat4 view;
    vec3 camPos;
} ubo;

layout (binding = 1) uniform UBOParams {
    vec4 lights[4];
    float exposure;
    float gamma;
} uboParams;

layout (binding = 5) uniform SHCoefficients {
    vec3 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
} sh;

layout(push_constant) uniform PushConsts {
    layout(offset = 12) float roughness;
    layout(offset = 16) float metallic;
    layout(offset = 20) float specular;
    layout(offset = 24) float r;
    layout(offset = 28) float g;
    layout(offset = 32) float b;
} material;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO vec3(material.r, material.g, material.b)

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// SH 基函数，与 Vulkan 代码中的 getSHBasis 一致
vec3 evaluateSH(vec3 N) {
    float x = N.x, y = N.y, z = N.z;
    float x2 = x * x, y2 = y * y, z2 = z * z;
    vec3 shBasis[9] = vec3[](
        vec3(0.282095),                     // l=0, m=0
        vec3(0.488603 * y),                 // l=1, m=-1
        vec3(0.488603 * z),                 // l=1, m=0
        vec3(0.488603 * x),                 // l=1, m=1
        vec3(1.092548 * x * y),             // l=2, m=-2
        vec3(1.092548 * y * z),             // l=2, m=-1
        vec3(0.315392 * (3.0 * z2 - 1.0)), // l=2, m=0
        vec3(1.092548 * x * z),             // l=2, m=1
        vec3(0.546274 * (x2 - y2))          // l=2, m=2
    );
    return sh.l00 * shBasis[0] +
           sh.l1m1 * shBasis[1] +
           sh.l10 * shBasis[2] +
           sh.l1p1 * shBasis[3] +
           sh.l2m2 * shBasis[4] +
           sh.l2m1 * shBasis[5] +
           sh.l20 * shBasis[6] +
           sh.l2p1 * shBasis[7] +
           sh.l2p2 * shBasis[8];
}

// 简单的 PBR 模型，结合 SH 漫反射光照
vec3 simplePBR(vec3 N, vec3 V, vec3 albedo, float metallic, float roughness) {
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 irradiance = evaluateSH(N); // 使用 SH 重建漫反射光照
    vec3 diffuse = irradiance * albedo * (1.0 - metallic) / PI;

    // 忽略镜面反射（SH 主要用于漫反射）
    vec3 specular = vec3(0.0); // 可选：添加简单的镜面项

    return diffuse + specular;
}

void main() {
    vec3 N = normalize(inNormal);
    vec3 V = normalize(ubo.camPos - inWorldPos);

    // 计算 SH 光照的漫反射
    vec3 color = simplePBR(N, V, ALBEDO, material.metallic, material.roughness);

    // 添加点光源贡献，与 PBR 着色器一致
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < uboParams.lights.length(); ++i) {
        vec3 L = normalize(uboParams.lights[i].xyz - inWorldPos);
        vec3 H = normalize(V + L);
        float dotNL = clamp(dot(N, L), 0.0, 1.0);
        if (dotNL > 0.0) {
            // 简化的点光源贡献
            vec3 lightColor = vec3(1.0); // 可从 uboParams.lights[i].w 获取强度
            Lo += ALBEDO * lightColor * dotNL / PI;
        }
    }

    color += Lo;

    // 色调映射和伽马校正，与 PBR 着色器一致
    color = Uncharted2Tonemap(color * uboParams.exposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / uboParams.gamma));

    outColor = vec4(color, 1.0);
}