#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec3 inWorldPos;

layout (set = 0, binding = 0) uniform Global
{
	mat4 viewProject[6];
	vec4 cameraPos[6];
	vec4 mainLight;
	vec4 mainLightColor;
	float exposure;
	float gamma;
} global;

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

vec3 Uncharted2Tonemap(vec3 x) {
	float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	vec3 H = normalize(V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	vec3 color = vec3(0.0);
	if (dotNL > 0.0) {
		float D = D_GGX(dotNH, roughness);
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		vec3 F = F_Schlick(dotNV, F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		color += (kD * ALBEDO / PI + spec) * dotNL;
	}
	return color;
}


void main()
{
	vec4 sampledColor = texture(samplerColorMap, inUV);
	if (material.useTexture == 0) {
		sampledColor = vec4(material.elbedo.rgb, material.elbedo.a);
	}
	vec4 baseColor = sampledColor * vec4(inColor, 1.0);

	vec3 N = normalize(inNormal);
	vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);

	float metallic = material.metallic;
	float roughness = material.roughness;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, baseColor.rgb, metallic);

	vec3 lightVec = global.mainLight.xyz - inWorldPos;
	float dist2 = max(dot(lightVec, lightVec), 1.0);
	vec3 L = lightVec * inversesqrt(dist2);
	float attenuation = global.mainLight.w / dist2;

	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	vec3 direct = (attenuation * global.mainLightColor.rgb) * dotNL * baseColor.rgb;
	vec3 spec = (attenuation * global.mainLightColor.rgb) * specularContribution(L, V, N, F0, metallic, roughness);
	vec3 ambient = baseColor.rgb * 0.03;
	vec3 color = ambient + direct + spec;

	color = Uncharted2Tonemap(color * global.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	color = pow(color, vec3(1.0f / global.gamma));

	outColor = vec4(color, baseColor.a) * pc.tint;
}

