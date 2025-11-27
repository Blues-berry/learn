#version 450

// PRT Relighting Vertex Shader

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inColor;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outColor;

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

layout (set = 1, binding = 0) uniform Model {
    mat4 matrix;
    mat4 normalMatrix;
} model;

void main() {
    // 变换到世界空间
    outWorldPos = (model.matrix * vec4(inPos, 1.0)).xyz;
    
    // 变换法线到世界空间
    outNormal = normalize((model.normalMatrix * vec4(inNormal, 0.0)).xyz);
    
    // 传递纹理坐标和颜色
    outTexCoord = inTexCoord;
    outColor = inColor;
    
    // 投影变换
    gl_Position = global.viewproj * vec4(outWorldPos, 1.0);
}

