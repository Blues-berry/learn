#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform Global
{
	mat4 projection;
	mat4 view;

	vec4 lights[4];
	vec4 cameraPos;
	float exposure;
	float gamma;
} global;

layout (set = 1, binding = 1) uniform Material
{
	float roughness;
	float metallic;
	float specular;
	float padding;
	vec4 elbedo;//基础颜色（Base Color）或反照率（Albedo）。
} material;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO material.elbedo

void main()
{
	vec3 N = normalize(inNormal);
	vec3 V = normalize(global.cameraPos.xyz - inWorldPos);
	vec3 R = reflect(-V, N); 

	float metallic = material.metallic;
	float roughness = material.roughness;

	outColor = ALBEDO;
}