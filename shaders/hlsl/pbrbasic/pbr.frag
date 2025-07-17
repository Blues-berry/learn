
#define NUM_LIGHTS 64
#define CLUSTER_SIZE_X 8
#define CLUSTER_SIZE_Y 8
#define CLUSTER_SIZE_Z 8
#define TOTAL_CLUSTERS (CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z)
#define LIGHT_INDEX_LIST_SIZE (NUM_LIGHTS * TOTAL_CLUSTERS)

struct VSOutput {
    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
};

struct UBO {
    float4x4 projection;
    float4x4 model;
    float4x4 view;
    float3 camPos;
    uint maxlightindexnum;
};

cbuffer ubo : register(b0) { UBO ubo; }

struct Light {
    float4 position;
    float4 colorAndRadius;
    float4 direction;
    float4 cutOff;
};

cbuffer uboParams : register(b1) {
    Light lights[NUM_LIGHTS];
};

struct ClusterIndexList {
    uint clusterIndexList;
    float padding1;
    float padding2;
    float padding3;
};

cbuffer clusterIndexList : register(b2) {
    ClusterIndexList clusterIndexList[LIGHT_INDEX_LIST_SIZE];
};

struct Cluster {
    uint counts;
    uint offsets;
    float2 padding;
};

cbuffer clusterCountsandOffsets : register(b3) {
    Cluster clusterCountsandOffsets[TOTAL_CLUSTERS];
};

struct PushConsts {
    [[vk::offset(12)]] float roughness;
    [[vk::offset(16)]] float metallic;
    [[vk::offset(20)]] float r;
    [[vk::offset(24)]] float g;
    [[vk::offset(28)]] float b;
};

[[vk::push_constant]] PushConsts material;

static const float PI = 3.14159265359;

float3 materialcolor() {
    return float3(material.r, material.g, material.b);
}

float D_GGX(float dotNH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom);
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

float3 F_Schlick(float cosTheta, float metallic) {
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), materialcolor(), metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return F;
}

float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness) {
    float3 H = normalize(V + L);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    float dotLH = clamp(dot(L, H), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);

    float3 color = float3(0.0, 0.0, 0.0);
    if (dotNL > 0.0) {
        float rroughness = max(0.05, roughness);
        float D = D_GGX(dotNH, roughness);
        float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
        float3 F = F_Schlick(dotNV, metallic);
        float3 spec = D * F * G / (4.0 * dotNL * dotNV);
        float3 diffuse = (1.0 - F) * (1.0 - metallic) * materialcolor() / PI;
        color = spec + diffuse * dotNL;
    }
    return color;
}

float radiance(float radius, float3 lightVec, float3 N, float3 L) {
    float distance = length(lightVec);
    if (distance > radius) return 0.0;
    float attenuation = pow(clamp(1.0 - distance / radius, 0.0, 1.0), 2.0);
    float dotNL = max(dot(N, L), 0.0);
    return attenuation * dotNL;
}

float4 main(VSOutput input) : SV_TARGET {
    float3 N = normalize(input.Normal);
    float3 V = normalize(ubo.camPos - input.WorldPos);
    float roughness = material.roughness;

    float4 worldPos = float4(input.WorldPos, 1.0);
    float4 viewPos = mul(ubo.view, worldPos);
    float4 clipPos = mul(ubo.projection, viewPos);

    clipPos /= clipPos.w;
    float2 screenPos = clipPos.xy * 0.5 + 0.5;

    float viewZ = -viewPos.z;
    float zNear = 0.1;
    float zFar = 256.0;
    uint clusterZ = uint((log(max(0.0001, viewZ / zNear) / log(zFar / zNear)) * CLUSTER_SIZE_Z));
    clusterZ = clamp(clusterZ, 0u, CLUSTER_SIZE_Z - 1);

    uint clusterX = uint(screenPos.x * CLUSTER_SIZE_X);
    uint clusterY = uint(screenPos.y * CLUSTER_SIZE_Y);
    clusterX = clamp(clusterX, 0u, CLUSTER_SIZE_X - 1);
    clusterY = clamp(clusterY, 0u, CLUSTER_SIZE_Y - 1);

    uint clusterIdx = clusterZ * CLUSTER_SIZE_X * CLUSTER_SIZE_Y + clusterY * CLUSTER_SIZE_X + clusterX;
    uint lightCount = clusterCountsandOffsets[clusterIdx].counts;
    uint lightOffset = clusterCountsandOffsets[clusterIdx].offsets;

    float3 Lo = float3(0.0, 0.0, 0.0);
    if (lightCount > 0) {
        for (uint i = lightOffset; i < min(lightOffset + lightCount, LIGHT_INDEX_LIST_SIZE); i++) {
            float3 lightVec = lights[clusterIndexList[i].clusterIndexList].position.xyz - input.WorldPos;
            float3 L = normalize(lightVec);
            float radianceFactor = radiance(lights[clusterIndexList[i].clusterIndexList].colorAndRadius.w, lightVec, N, L);
            float3 lightColor = lights[clusterIndexList[i].clusterIndexList].colorAndRadius.xyz;
            Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor;
        }
    }

    float3 color = materialcolor() * 0.02;
    color += Lo;
    color = pow(color, float3(0.4545, 0.4545, 0.4545));
    return float4(color, 1.0);
}
