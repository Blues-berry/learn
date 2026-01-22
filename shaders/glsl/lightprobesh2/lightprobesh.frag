#version 450

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;

layout (set = 0, binding = 0) uniform Global
{
	mat4 projection;
	mat4 view;

	vec4 lights[4];
	vec4 lightColor[4];
	vec4 cameraPos;
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
} material;

layout (location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
#define ALBEDO material.elbedo.rgb

vec3 Uncharted2Tonemap(vec3 x) {
	float A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
	return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 9.0; // todo: param/const
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
	vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		color += (kD * ALBEDO / PI + spec) * dotNL;
	}

	return color;
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

vec3 simplePBR(vec3 N, vec3 V, vec3 albedo, float metallic) {
	vec3 F0 = mix(vec3(0.04), albedo, metallic);
	vec3 irradiance = evaluateSH(N);
	vec3 diffuse = irradiance * albedo * (1.0 - metallic) / PI;
	return diffuse;
}

void main()
{
	vec3 N = normalize(inNormal);
	vec3 V = normalize(global.cameraPos.xyz - inWorldPos);
	vec3 R = reflect(-V, N); 

	float metallic = material.metallic;
	float roughness = material.roughness;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, ALBEDO, metallic);
	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);
	vec2 brdf = texture(samplerBRDFLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;

	vec3 Lo = vec3(0.0);
//	for(int i = 0; i < global.lights.length(); i++) {
//		vec3 L = normalize(global.lights[i].xyz - inWorldPos);
//		Lo += specularContribution(L, V, N, F0, metallic, roughness);
//	}
	
	vec3 diffuse = vec3(0.0);

	if (material.useSH > 0) {
		diffuse = simplePBR(N, V, ALBEDO, metallic);
	} else {
		
		vec3 irradiance = texture(samplerIrradiance, N).rgb;

		// Diffuse based on irradiance
		vec3 kD = 1.0 - F;
		kD *= 1.0 - metallic;
		
		diffuse = kD * irradiance * ALBEDO;
	}
	
	vec3 specular = vec3(0.0);
	
	if (material.useReflection > 0) {
		// Specular reflectance
		vec3 reflection = prefilteredReflection(R, roughness).rgb;
		specular = reflection * (F * brdf.x + brdf.y);
	}
	vec3 ambient = (diffuse + specular);
	vec3 color = ambient + Lo;

	// Tone mapping
	color = Uncharted2Tonemap(color * global.exposure);
	color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
	// Gamma correction
	color = pow(color, vec3(1.0f / global.gamma));

	outColor = vec4(color, 1.0);
}