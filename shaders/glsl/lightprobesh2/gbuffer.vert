#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;

layout(set = 0, binding = 0) uniform Global
{
    mat4 projection;
    mat4 view;
} global;

layout(push_constant) uniform PushConstantBlock {
    mat4 model;
    vec4 baseColor;
} pushConstants;

void main()
{
    gl_Position = global.projection * global.view * pushConstants.model * vec4(inPosition, 1.0);
    outWorldPos = (pushConstants.model * vec4(inPosition, 1.0)).xyz;
    outNormal = normalize((pushConstants.model * vec4(inNormal, 0.0)).xyz);
    outUV = inUV;
}

