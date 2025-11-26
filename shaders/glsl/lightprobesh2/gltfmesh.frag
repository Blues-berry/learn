#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform Global
{
    mat4 projection;   // ✅ 修复：改为分开的 projection 和 view，与 skybox 一致
    mat4 view;
    vec4 lights[4];
    vec4 cameraPos;
    float exposure;
    float gamma;
} global;

layout (set = 0, binding = 1) uniform SHCoefficients {
	vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
} sh;

layout (set = 0, binding = 2) uniform sampler2D samplerBRDFLUT;
layout (set = 0, binding = 3) uniform samplerCube samplerIrradiance;
layout (set = 0, binding = 4) uniform samplerCube prefilteredMap;

layout (set = 1, binding = 1) uniform Material
{
	float roughness;
	float metallic;
	float specular;
	float padding;
	vec4 albedo;
	int useSH;
	int useReflection;
	int useTexture;
	int padding2;
} material;

layout (set = 2, binding = 0) uniform sampler2D baseColorMap;

layout(push_constant) uniform PushConstant {
	mat4 modelOffset;
	vec4 baseColor;
} pc;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795

vec3 Uncharted2Tonemap(vec3 x) {
	float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 evaluateSH(vec3 N) {
	float x = N.x, y = N.y, z = N.z;
	float x2 = x * x, y2 = y * y, z2 = z * z;
	vec3 shBasis[9] = vec3[](
		vec3(0.282095),
		vec3(0.488603 * y),
		vec3(0.488603 * z),
		vec3(0.488603 * x),
		vec3(1.092548 * x * y),
		vec3(1.092548 * y * z),
		vec3(0.315392 * (3.0 * z2 - 1.0)),
		vec3(1.092548 * x * z),
		vec3(0.546274 * (x2 - y2))
	);
	return sh.l00.xyz * shBasis[0] +
		sh.l1m1.xyz * shBasis[1] +
		sh.l10.xyz * shBasis[2] +
		sh.l1p1.xyz * shBasis[3] +
		sh.l2m2.xyz * shBasis[4] +
		sh.l2m1.xyz * shBasis[5] +
		sh.l20.xyz * shBasis[6] +
		sh.l2p1.xyz * shBasis[7] +
		sh.l2p2.xyz * shBasis[8];
}

vec4 sampleBaseColor(vec2 uv) {
	if (material.useTexture != 0) {
		return texture(baseColorMap, uv);
	}
	return material.albedo;
}

void main()
{
	vec3 N = normalize(inNormal);
	vec3 V = normalize(global.cameraPos.xyz - inWorldPos);

	vec4 sampled = sampleBaseColor(inUV);
	vec3 albedo = sampled.rgb;
	if (material.useTexture == 0) {
		albedo = pc.baseColor.rgb;
	}

	vec3 color = albedo * 0.2f; // baseline ambient
	if (material.useSH != 0) {
		color += clamp(evaluateSH(N), vec3(0.0f), vec3(4.0f)) * albedo;
	}

	vec3 lightDir = normalize(vec3(1.0f, 1.0f, 1.0f));
	float NdotL = max(dot(N, lightDir), 0.0f);
	color += albedo * NdotL;

	vec3 H = normalize(V + lightDir);
	float NdotH = max(dot(N, H), 0.0f);
	float gloss = mix(8.0f, 32.0f, clamp(material.roughness, 0.0f, 1.0f));
	color += vec3(material.specular) * pow(NdotH, gloss);

	color = clamp(color, vec3(0.0f), vec3(20.0f));
	vec3 mapped = color * global.exposure;
	mapped = mapped / (mapped + vec3(1.0f));
	mapped = pow(mapped, vec3(1.0f / max(global.gamma, 0.0001f)));

	outColor = vec4(mapped, sampled.a);
}