#version 450

layout (location = 0) in vec3 inWorldPos;

layout (set = 0, binding = 0) uniform Global {
    mat4 projection;
    mat4 view;
    vec4 cameraPos;
    float exposure;
    float gamma;
    int useLightSource;
    float lightIntensity;
    vec3 lightPosition;
    vec3 lightColor;
} global;

layout (set = 1, binding = 1) uniform Material {
    float roughness;
    float metallic;
    float specular;
    int useLighting;
    vec4 albedo;
    int useSH;
    int useReflection;
} material;

layout (location = 0) out vec4 outColor;

void main() {
    // Use material color directly for the light source
    vec3 finalColor = material.albedo.rgb;
    
    // Apply intensity and add some emissive effect
    finalColor = finalColor * (1.0 + material.specular * 0.5);
    
    // Add fresnel effect for better visibility
    vec3 viewDir = normalize(global.cameraPos.xyz - inWorldPos);
    vec3 normal = normalize(inWorldPos);
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 2.0);
    finalColor += finalColor * fresnel * 0.5;
    
    // Output final color with full alpha
    outColor = vec4(finalColor, 1.0);
}