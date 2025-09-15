#version 450

#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBO
{
	mat4 mvp[6];
	float deltaPhi;
	float deltaTheta;
	float roughness;
	uint numSamples;
} ubo;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() 
{
	outUVW = inPos;
	gl_Position = ubo.mvp[gl_ViewIndex] * vec4(inPos.xyz, 1.0);
}
