#version 450

// PRT Relighting Fragment Shader
// 使用预计算的球谐系数进行实时relighting

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inColor;

layout(location = 0) out vec4 outColor;

// 全局UBO (与主着色器一致)
layout (set = 0, binding = 0) uniform Global {
    mat4 viewproj;
    vec4 cameraPos;
    vec4 lights[4];
    float exposure;
    float gamma;
    int useLightSource;
    float lightIntensity;
    vec3 lightPosition;
    vec3 lightColor;
} global;

// 材质UBO
layout (set = 1, binding = 0) uniform Material {
    float roughness;
    float metallic;
    float specular;
    int useLighting;
    vec4 albedo;
    int useSH;
    int useReflection;
} material;

// PRT球谐系数 (9个vec4)
layout (set = 2, binding = 0) uniform SHCoefficients {
    vec4 l00;
    vec4 l1m1;
    vec4 l10;
    vec4 l1p1;
    vec4 l2m2;
    vec4 l2m1;
    vec4 l20;
    vec4 l2p1;
    vec4 l2p2;
} shCoeffs;

// 计算球谐基函数
vec3 EvaluateSHBasis(int index, vec3 normal) {
    float x = normal.x;
    float y = normal.y;
    float z = normal.z;
    
    float x2 = x * x;
    float y2 = y * y;
    float z2 = z * z;
    
    switch(index) {
        case 0: return vec3(0.282095);
        case 1: return vec3(0.488603 * y);
        case 2: return vec3(0.488603 * z);
        case 3: return vec3(0.488603 * x);
        case 4: return vec3(1.092548 * x * y);
        case 5: return vec3(1.092548 * y * z);
        case 6: return vec3(0.315392 * (3.0 * z2 - 1.0));
        case 7: return vec3(1.092548 * x * z);
        case 8: return vec3(0.546274 * (x2 - y2));
        default: return vec3(0.0);
    }
}

// 从球谐系数重建光照
vec3 ReconstructLighting(vec3 normal) {
    vec3 lighting = vec3(0.0);
    
    // 0阶
    lighting += shCoeffs.l00.rgb * EvaluateSHBasis(0, normal);
    
    // 1阶
    lighting += shCoeffs.l1m1.rgb * EvaluateSHBasis(1, normal);
    lighting += shCoeffs.l10.rgb * EvaluateSHBasis(2, normal);
    lighting += shCoeffs.l1p1.rgb * EvaluateSHBasis(3, normal);
    
    // 2阶
    lighting += shCoeffs.l2m2.rgb * EvaluateSHBasis(4, normal);
    lighting += shCoeffs.l2m1.rgb * EvaluateSHBasis(5, normal);
    lighting += shCoeffs.l20.rgb * EvaluateSHBasis(6, normal);
    lighting += shCoeffs.l2p1.rgb * EvaluateSHBasis(7, normal);
    lighting += shCoeffs.l2p2.rgb * EvaluateSHBasis(8, normal);
    
    return max(lighting, vec3(0.0));
}

// Tone mapping
vec3 ToneMap(vec3 color) {
    // Reinhard tone mapping
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / global.gamma));
    return color;
}

void main() {
    // 规范化法线
    vec3 N = normalize(inNormal);
    
    // 获取材质颜色
    vec3 albedo = material.albedo.rgb;
    if (inColor != vec3(0.0)) {
        albedo = inColor;
    }
    
    // 从球谐系数重建光照
    vec3 lighting = ReconstructLighting(N);
    
    // 应用材质颜色
    vec3 finalColor = albedo * lighting;
    
    // 如果启用了光源，添加直接光照
    if (global.useLightSource == 1) {
        vec3 L = normalize(global.lightPosition - inWorldPos);
        float NdotL = max(dot(N, L), 0.0);
        vec3 directLight = global.lightColor * global.lightIntensity * NdotL;
        finalColor += albedo * directLight * 0.5;
    }
    
    // 应用exposure
    finalColor *= global.exposure;
    
    // Tone mapping
    finalColor = ToneMap(finalColor);
    
    outColor = vec4(finalColor, 1.0);
}

