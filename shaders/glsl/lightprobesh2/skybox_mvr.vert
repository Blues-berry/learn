#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

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
} local;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	outUVW = inPos;
	gl_Position = global.viewProject[gl_ViewIndex] * vec4(inPos.xyz, 1.0);
	gl_Position.z = gl_Position.w;
}
