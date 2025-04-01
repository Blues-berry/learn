// Copyright 2020 Google LLC

struct VSOutput
{
[[vk::location(0)]] float3 WorldPos : POSITION0;  // 世界空间位置
[[vk::location(1)]] float3 Normal : NORMAL0;      // 法线
};
struct UBO
{
	float4x4 projection;
	float4x4 model;
	float4x4 view;
	float3 camPos;
};

cbuffer ubo : register(b0) { UBO ubo; }  // 绑定到寄存器 b0

struct Light {
	float4 position;
	float4 colorAndRadius;
	float4 direction;
	float4 cutOff;
};

struct UBOShared {
	Light lights[4];
};

cbuffer uboParams : register(b1) { UBOShared uboParams; };

struct PushConsts {
[[vk::offset(12)]] float roughness;  // 粗糙度
[[vk::offset(16)]] float metallic;   // 金属度
[[vk::offset(20)]] float r;          // 红色分量
[[vk::offset(24)]] float g;          // 绿色分量
[[vk::offset(28)]] float b;          // 蓝色分量
};
[[vk::push_constant]] PushConsts material;  // 定义推送常量

static const float PI = 3.14159265359;

//#define ROUGHNESS_PATTERN 1

float3 materialcolor()
{
	return float3(material.r, material.g, material.b);
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
float3 F_Schlick(float cosTheta, float metallic)
{
	float3 F0 = lerp(float3(0.04, 0.04, 0.04), materialcolor(), metallic); // * material.specular
	float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
	return F;
}

// Specular BRDF composition --------------------------------------------






float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness)
{
	// Precalculate vectors and dot products
	float3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	// Light color fixed
	float3 lightColor = float3(1.0, 1.0, 1.0);

	float3 color = float3(0.0, 0.0, 0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		float3 F = F_Schlick(dotNV, metallic);

		float3 spec = D * F * G / (4.0 * dotNL * dotNV);

		color += spec * dotNL * lightColor;
	}

	return color;
}
/*
// calculates the color when using a spot light.
float3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // combine results
    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}
*/
float radiance(float radius, float3 lightVec, float3 N,float3 L )
{
	float distance=length(lightVec);
// 半径范围裁剪
	if(distance>radius) return 0.0;
//计算衰减
		float attenuation = pow(clamp(1.0-distance / radius,0.0,1.0), 2.0);
		float dotNL=max(dot(N,L),0.0);
	return attenuation*dotNL;


}
float spotlight(int i,float3 L)//i=lightindex

{
        float3 lightDir = normalize(-uboParams.lights[i].direction.xyz);  // 光源方向
        float theta = dot(L, lightDir);  // 光向量与光源方向的夹角余弦
        // 定义内锥角和外锥角（以弧度为单位）
        float cutOff = uboParams.lights[i].cutOff.x;      // 内锥角，例如 12.5°
        float outerCutOff =uboParams.lights[i].cutOff.y; // 外锥角，例如 17.5°
        float epsilon = cutOff - outerCutOff;   // 内、外锥角差值
        float spotlight_intensity = pow(clamp((theta - outerCutOff) / epsilon, uboParams.lights[i].cutOff.z, 1.0),int(uboParams.lights[i].cutOff.w));  // 计算强度
return spotlight_intensity;
}


// ----------------------------------------------------------------------------
float4 main(VSOutput input) : SV_TARGET
{
	float3 N = normalize(input.Normal);
	float3 V = normalize(ubo.camPos - input.WorldPos);

	float roughness = material.roughness;

	// Add striped pattern to roughness based on vertex position
#ifdef ROUGHNESS_PATTERN
	roughness = max(roughness, step(frac(input.WorldPos.y * 2.02), 0.5));
#endif

	// Specular contribution
	float3 Lo = float3(0.0, 0.0, 0.0);

for (int i = 0; i < 3; i++) {
    float3 lightVec = uboParams.lights[i].position.xyz - input.WorldPos;
    float3 L = normalize(lightVec);
    float radianceFactor = radiance(
        uboParams.lights[i].colorAndRadius.w, lightVec, N, L
    );
    
/*	//spot light
// Spot light for the 4th light (index 3)
    if (i == 3) {
        float3 lightDir = normalize(-uboParams.lights[i].direction.xyz);  // 光源方向
        float theta = dot(L, lightDir);  // 光向量与光源方向的夹角余弦
        
        // 定义内锥角和外锥角（以弧度为单位）
        float cutOff = uboParams.lights[i].cutOff.x;      // 内锥角，例如 12.5°
        float outerCutOff =uboParams.lights[i].cutOff.y; // 外锥角，例如 17.5°
        float epsilon = cutOff - outerCutOff;   // 内、外锥角差值
        float intensity = pow(clamp((theta - outerCutOff) / epsilon, uboParams.lights[i].cutOff.z, 1.0),int(uboParams.lights[i].cutOff.w));  // 计算强度
	    // 传递光源颜色到BRDF是
		float3 lightColor = uboParams.lights[i].colorAndRadius.xyz;
    Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor*intensity;
	//Lo += lightColor *intensity;
	}

    // 传递光源颜色到BRDF是
	else{
	float3 lightColor = uboParams.lights[i].colorAndRadius.xyz;
    Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor;
	}
*/

	float3 lightColor = uboParams.lights[i].colorAndRadius.xyz;
    Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor;

}
// spot light
    float3 lightVec = uboParams.lights[3].position.xyz - input.WorldPos;
    float3 L = normalize(lightVec);
    float radianceFactor = radiance(
        uboParams.lights[3].colorAndRadius.w, lightVec, N, L
    );
	float3 lightColor = uboParams.lights[3].colorAndRadius.xyz;
    Lo +=  lightColor * radianceFactor*spotlight(3,L);
	// Combine with ambient
	float3 color = materialcolor() * 0.02;
	color += Lo;

	// Gamma correct
	color = pow(color, float3(0.4545, 0.4545, 0.4545));

	return float4(color, 1.0);
}