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

layout (location = 0) out vec4 outColor;

void main() {
    // 直接输出光源颜色，不考虑光照计算
    outColor = vec4(global.lightColor * global.lightIntensity, 1.0);
}
