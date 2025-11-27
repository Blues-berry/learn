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
    // 输出调试信息
    // 注意：在片段着色器中不能直接使用printf，我们需要通过输出颜色来调试
    
    // 使用材质的颜色作为基础颜色
    vec3 baseColor = material.albedo.rgb;
    
    // 使用全局光源颜色和强度
    vec3 finalColor = baseColor * global.lightIntensity * 10.0; // 增加亮度
    
    // 添加一些自发光效果，使光源更明显
    float fresnel = pow(1.0 - dot(normalize(inWorldPos - global.cameraPos.xyz), normalize(inWorldPos)), 2.0);
    finalColor += finalColor * fresnel * 0.5;
    
    // 强制输出纯色用于调试
    // 取消下面这行注释可以测试着色器是否正常工作
    // finalColor = vec3(1.0, 0.0, 0.0); // 强制红色
    
    outColor = vec4(finalColor, 1.0);
    
    // 调试输出：如果颜色太暗，强制显示为亮绿色
    if (length(finalColor) < 0.1) {
        outColor = vec4(0.0, 1.0, 0.0, 1.0); // 亮绿色表示有问题
    }
}
