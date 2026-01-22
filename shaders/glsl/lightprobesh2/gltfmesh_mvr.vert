#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 inColor;

layout (set = 0, binding = 0) uniform Global
{
	mat4 viewProject[6];
	vec4 cameraPos[6];
	vec4 mainLight;
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
	// ✅ 使用multiview：根据gl_ViewIndex选择对应的视图投影矩阵
	mat4 model = ubo.model * pc.modelOffset;
	vec4 worldPos = model * vec4(inPos, 1.0);

	vec3 worldNormal = normalize(mat3(model) * inNormal);
	outNormal = worldNormal;
	outColor = inColor.rgb;
	outUV = vec2(inUV.x, 1.0 - inUV.y);
	outWorldPos = worldPos.xyz;

	outViewVec = global.cameraPos[gl_ViewIndex].xyz - worldPos.xyz;
	outLightVec = global.mainLight.xyz - worldPos.xyz;

	// 使用gl_ViewIndex来选择正确的视图投影矩阵
	gl_Position = global.viewProject[gl_ViewIndex] * worldPos;
}

