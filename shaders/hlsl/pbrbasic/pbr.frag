// 版权所有 2020 Google LLC
#define NUM_LIGHTS 64            // 光源数量
#define CLUSTER_SIZE_X 8        // X 轴集群数
#define CLUSTER_SIZE_Y 8        // Y 轴集群数
#define CLUSTER_SIZE_Z 1        // Z 轴集群数
#define TOTAL_CLUSTERS CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z
#define LIGHT_INDEX_LIST_SIZE (NUM_LIGHTS * TOTAL_CLUSTERS) // 全局光源索引列表大小

struct VSOutput {
    [[vk::location(0)]] float3 WorldPos : POSITION0; // 世界空间位置
    [[vk::location(1)]] float3 Normal : NORMAL0;     // 法线
};

struct UBO {
    float4x4 projection; // 投影矩阵
    float4x4 model;      // 模型矩阵
    float4x4 view;       // 视图矩阵
    float3 camPos;       // 相机位置
    uint maxlightindexnum; // 最大光源索引数
};

cbuffer ubo : register(b0) { UBO ubo; } // 绑定到寄存器 b0

struct Light {
    float4 position;       // 光源位置
    float4 colorAndRadius; // 颜色和半径
    float4 direction;      // 方向
    float4 cutOff;         // 截止参数
};

cbuffer uboParams : register(b1) { // 光源数据缓冲区
    Light lights[NUM_LIGHTS];
};

struct Indices {
    uint clusterIndexList;
    float3 padding; // 填充以确保 16 字节对齐
};

cbuffer clusterIndexList : register(b2) {
    Indices indices[LIGHT_INDEX_LIST_SIZE];
};

struct Cluster {
    uint counts;      // 光源数量
    uint offsets;     // 光源偏移
    float2 padding;   // 填充以确保 16 字节对齐
};

cbuffer clusterCountsandOffsets : register(b3) { // 占用原来的 b3 绑定点
    Cluster clusterCountsandOffsets[TOTAL_CLUSTERS];
};

struct PushConsts {
    float4x4 model;      // 模型矩阵（新增）
    float4 color;        // 光源颜色（新增）
    float isLightSphere; // 是否为光源球体（新增）
    float roughness;     // 粗糙度
    float metallic;      // 金属度
    float r;             // 红色分量
    float g;             // 绿色分量
    float b;             // 蓝色分量
};
[[vk::push_constant]] PushConsts material; // 推送常量

static const float PI = 3.14159265359;

float3 materialcolor() {
    return float3(material.r, material.g, material.b);
}

// 法线分布函数 (GGX)
float D_GGX(float dotNH, float roughness) {
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    return (alpha2) / (PI * denom * denom);
}

// 几何遮挡函数 (Schlick-Smith GGX)
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    return GL * GV;
}

// 菲涅尔函数 (Schlick)
float3 F_Schlick(float cosTheta, float metallic) {
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), materialcolor(), metallic);
    float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    return F;
}

// 镜面 BRDF 计算
float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness) {
    float3 H = normalize(V + L);
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    float dotLH = clamp(dot(L, H), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.0, 1.0);

    float3 lightColor = float3(1.0, 1.0, 1.0);
    float3 color = float3(0.0, 0.0, 0.0);

    if (dotNL > 0.0) {
        float rroughness = max(0.05, roughness);
        float D = D_GGX(dotNH, roughness);
        float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
        float3 F = F_Schlick(dotNV, metallic);
        float3 spec = D * F * G / (4.0 * dotNL * dotNV);
        color += spec * dotNL * lightColor;
    }
    return color;
}

// 光照辐射计算
float radiance(float radius, float3 lightVec, float3 N, float3 L) {
    float distance = length(lightVec);
    if (distance > radius) return 0.0;
    float attenuation = pow(clamp(1.0 - distance / radius, 0.0, 1.0), 2.0);
    float dotNL = max(dot(N, L), 0.0);
    return attenuation * dotNL;
}

float4 main(VSOutput input) : SV_TARGET {
    // 如果是光源球体，绘制颜色并添加简单光照
    if (material.isLightSphere > 0.5) {
        float3 N = normalize(input.Normal);
        float3 L = normalize(ubo.camPos - input.WorldPos); // 光从相机方向入射
        float diffuse = max(dot(N, L), 0.0);
        float3 color = material.color.xyz * (0.3 + 0.7 * diffuse); // 基础亮度 + 漫反射
        return float4(color, 1.0);
    }

    // 原有光照计算逻辑
    float3 N = normalize(input.Normal);
    float3 V = normalize(ubo.camPos - input.WorldPos);
    float roughness = material.roughness;

    // 计算屏幕空间位置
    float4 worldPos = float4(input.WorldPos, 1.0);
    float4 viewPos = mul(ubo.view, worldPos);
    float4 clipPos = mul(ubo.projection, viewPos);
    clipPos /= clipPos.w;
    float2 screenPos = clipPos.xy * 0.5 + 0.5;

    // 计算对数深度
    float viewZ = -viewPos.z;
    float zNear = 0.1;
    float zFar = 256.0;
    uint clusterZ = uint(log(viewZ / zNear) / log(zFar / zNear) * CLUSTER_SIZE_Z);
    clusterZ = clamp(clusterZ, 0u, CLUSTER_SIZE_Z - 1);

    // 计算集群索引
    uint clusterX = uint(screenPos.x * CLUSTER_SIZE_X);
    uint clusterY = uint(screenPos.y * CLUSTER_SIZE_Y);
    clusterX = clamp(clusterX, 0u, CLUSTER_SIZE_X - 1);
    clusterY = clamp(clusterY, 0u, CLUSTER_SIZE_Y - 1);
    clusterZ = clamp(clusterZ, 0u, CLUSTER_SIZE_Z - 1);
    uint clusterIdx = clusterZ * CLUSTER_SIZE_X * CLUSTER_SIZE_Y + clusterY * CLUSTER_SIZE_X + clusterX;

    // 获取光源列表（使用合并后的缓冲区）
    uint lightCount = clusterCountsandOffsets[clusterIdx].counts;
    uint lightOffset = clusterCountsandOffsets[clusterIdx].offsets;

    float3 Lo = float3(0.0, 0.0, 0.0);
    if (lightCount > 0) {
        for (int i = lightOffset; i < lightOffset + lightCount; i++) {
            float3 lightVec = lights[indices[i].clusterIndexList].position.xyz - input.WorldPos;
            float3 L = normalize(lightVec);
            float radianceFactor = radiance(lights[indices[i].clusterIndexList].colorAndRadius.w, lightVec, N, L);
            float3 lightColor = lights[indices[i].clusterIndexList].colorAndRadius.xyz;
            Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor;
        }
    }

    /*


            for (uint i = 0; i < 64; i++) {

    float3 lightVec = lights[i].position.xyz - input.WorldPos;
    float3 L = normalize(lightVec);
    float radianceFactor = radiance(lights[i].colorAndRadius.w, lightVec, N, L);
    float3 lightColor = lights[i].colorAndRadius.xyz;
    Lo += BRDF(L, V, N, material.metallic, material.roughness) * lightColor * radianceFactor;
}
    
    */

    // 组合环境光和镜面光
    float3 color = materialcolor() * 0.02;
    color += Lo;

    // Gamma 校正
    color = pow(color, float3(0.4545, 0.4545, 0.4545));
    return float4(color, 1.0);
}