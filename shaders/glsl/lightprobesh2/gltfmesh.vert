#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform Global
{
    mat4 viewproj[6];  // 6 个面的视图投影矩阵
    vec4 cameraPos[6]; // 6 个面的相机位置
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

layout (location = 0) out vec3 outWorldPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outUV;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	vec4 worldPos = ubo.model * pc.modelOffset * vec4(inPos, 1.0);

	outWorldPos = worldPos.xyz;
	outNormal = mat3(ubo.model * pc.modelOffset) * inNormal;
	outUV = inUV;
	outUV.t = 1.0 - inUV.t;

	gl_Position = global.viewproj[gl_ViewIndex] * worldPos;
}
