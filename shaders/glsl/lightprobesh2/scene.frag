#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 1) uniform sampler2D textureSampler;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(textureSampler, inUV);
    outColor = texColor; // 简单纹理颜色输出
}