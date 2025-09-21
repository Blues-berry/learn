#version 450

layout (location = 0) in vec3 pos;

layout (set = 0, binding = 0) uniform Global
{
	mat4 projection;
	mat4 view;

	vec4 lights[4];
	vec4 cameraPos;
	float exposure;
	float gamma;
} global;

void main()
{
	vec4 worldPos = vec4(pos, 1.0);
	gl_Position =  global.projection * global.view * worldPos;
	gl_PointSize = 1.0;
}