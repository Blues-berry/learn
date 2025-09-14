#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform Global
{
	mat4 projection;
	mat4 view;

	vec4 lights[4];
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
	gl_Position = global.projection * global.view * local.model * vec4(inPos.xyz, 1.0);
}
