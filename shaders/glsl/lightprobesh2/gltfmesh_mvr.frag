#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;

layout (set = 0, binding = 0) uniform Global
{
	mat4 viewProject[6];
	vec4 cameraPos[6];
	vec4 mainLight;
	float exposure;
	float gamma;
} global;

layout (set = 0, binding = 1) uniform SHCoefficients {
	vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
} sh;

layout (binding = 2) uniform sampler2D samplerBRDFLUT;
layout (binding = 3) uniform samplerCube samplerIrradiance;
layout (binding = 4) uniform samplerCube prefilteredMap;

layout (set = 1, binding = 1) uniform Material
{
	float roughness;
	float metallic;
	float specular;
	float padding;
	vec4 elbedo;
	int useSH;
    int useReflection;
	int useTexture;
	int padding2;
} material;

layout (set = 2, binding = 0) uniform sampler2D samplerColorMap;

// Push constants: receive tint color
layout(push_constant) uniform PushConstant {
	mat4 modelOffset;
	vec4 tint;
} pc;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO material.elbedo.rgb

void main()
{
	vec4 sampledColor = texture(samplerColorMap, inUV);
	if (material.useTexture == 0) {
		sampledColor = vec4(material.elbedo.rgb, material.elbedo.a);
	}

	vec4 baseColor = sampledColor * vec4(inColor, 1.0);

	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);

	float ndotl = max(dot(N, L), 0.15);
	vec3 diffuse = ndotl * inColor;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);

	vec3 finalColor = diffuse * baseColor.rgb + specular;
	outColor = vec4(finalColor, baseColor.a) * pc.tint;
}

