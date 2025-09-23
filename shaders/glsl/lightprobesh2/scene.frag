#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;

layout(set = 0, binding = 1) uniform samplerCube textureSampler;

layout(location = 0) out vec4 outColor;

void main() {
vec3 envColor = texture(textureSampler, normalize(inNormal)).rgb;
    outColor = vec4(envColor, 1.0);
}