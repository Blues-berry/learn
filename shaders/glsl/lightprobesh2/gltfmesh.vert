#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform Global
{
    mat4 projection;    // ✅ 修复：改为分开的 projection 和 view，与 skybox 一致
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;

layout (set = 1, binding = 0) uniform Local
{
	mat4 model;
} ubo;

// Push constants: per-draw offset and tint
layout(push_constant) uniform PushConstant {
	mat4 modelOffset;
} pc;

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	mat4 modelMatrix = ubo.model * pc.modelOffset;
	vec4 worldPos = modelMatrix * vec4(inPos, 1.0);

	outWorldPos = worldPos.xyz;
	outNormal = mat3(modelMatrix) * inNormal;
	outUV = inUV;
	outUV.t = 1.0 - inUV.t;

	gl_Position = global.projection * global.view * worldPos;
}
