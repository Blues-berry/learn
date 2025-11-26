#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inWorldPos;
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
	vec4 albedo;
	int useSH;
    int useReflection;
	int useTexture;
	int padding2;
} material;

layout (set = 2, binding = 0) uniform sampler2D baseColorMap;

// Push constants: receive tint color
layout(push_constant) uniform PushConstant {
	mat4 modelOffset;
} pc;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
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

vec4 sampleBaseColor(vec2 uv)
{
	if (material.useTexture != 0) {
		return texture(baseColorMap, uv);
	}
	return material.albedo;
}

void main()
{
	vec3 N = normalize(inNormal);
	// ✅ 修复：使用 gl_ViewIndex 选择正确的相机位置（multiview 渲染）
	vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);
	vec3 R = reflect(-V, N);

	vec4 baseColor = sampleBaseColor(inUV);
	vec3 albedo = baseColor.rgb;

	vec3 diffuse = albedo * 0.2;
	if (material.useSH != 0) {
		diffuse += max(evaluateSH(N), vec3(0.0)) * albedo;
	}

	vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
	float NdotL = max(dot(N, lightDir), 0.0);
	diffuse += albedo * NdotL * 0.5;

	vec3 H = normalize(V + lightDir);
	float NdotH = max(dot(N, H), 0.0);
	vec3 specular = vec3(material.specular) * pow(NdotH, mix(8.0, 64.0, clamp(material.roughness, 0.0, 1.0)));

	vec3 color = diffuse + specular;
	color = max(color, vec3(0.0));
	color = pow(color * global.exposure, vec3(1.0 / max(global.gamma, 0.0001)));

	outColor = vec4(color, baseColor.a);
}

