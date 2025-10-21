#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;

layout (set = 0, binding = 0) uniform GlobalUbo
{
    mat4 project;
    mat4 view;
    vec4 light[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} uboGlobal;

layout(push_constant) uniform PushConsts {
    mat4 model;
} primitive;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;

void main() 
{
    outNormal = inNormal;
    outColor = inColor;
    outUV = inUV;
    gl_Position = uboGlobal.project * uboGlobal.view * primitive.model * vec4(inPos.xyz, 1.0);
    
    vec4 pos = uboGlobal.view * vec4(inPos, 1.0);
    outNormal = mat3(uboGlobal.view) * inNormal;
    vec3 lPos = mat3(uboGlobal.view) * uboGlobal.light[0].xyz;  // 使用 light[0]
    outLightVec = uboGlobal.light[0].xyz - pos.xyz;  // 使用 light[0]
    outViewVec = uboGlobal.cameraPos.xyz - pos.xyz;  // 使用 cameraPos
}