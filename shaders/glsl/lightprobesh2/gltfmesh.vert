#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inColor;

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
	vec4 tint;
} pc;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;
layout (location = 5) out vec3 outWorldPos;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	mat4 model = ubo.model * pc.modelOffset;
	vec4 worldPos = model * vec4(inPos, 1.0);

	vec3 worldNormal = normalize(mat3(model) * inNormal);
	outNormal = worldNormal;
	outColor = inColor.rgb;
	outUV = vec2(inUV.x, 1.0 - inUV.y);
	outWorldPos = worldPos.xyz;

	outViewVec = global.cameraPos.xyz - worldPos.xyz;
	outLightVec = global.lights[0].xyz - worldPos.xyz;

	gl_Position = global.projection * global.view * worldPos;
}
