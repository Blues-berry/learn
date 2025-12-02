#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;

layout(push_constant) uniform PushConstantBlock {
    mat4 model;
    vec4 baseColor;
} pushConstants;

void main()
{
    outPosition = vec4(inWorldPos, 1.0);
    outNormal = vec4(normalize(inNormal), 1.0);
    outAlbedo = pushConstants.baseColor;
}

