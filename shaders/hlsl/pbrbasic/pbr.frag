
#define NUM_LIGHTS 16
#define CLUSTER_SIZE_X 4
#define CLUSTER_SIZE_Y 4
#define CLUSTER_SIZE_Z 4
#define TOTAL_CLUSTERS (CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z)
#define LIGHT_INDEX_LIST_SIZE (NUM_LIGHTS * TOTAL_CLUSTERS)

struct VSOutput {
    float3 WorldPos : POSITION0;
    float3 Normal : NORMAL0;
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
    float roughness;
    float metallic;
    float r;
    float g;
    float b;
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
    // 确保输入向量是单位向量
    L = normalize(L);
    V = normalize(V);
    N = normalize(N);
    
    // 计算半向量并确保其有效
    float3 H = V + L;
    float lenH = length(H);
    
    // 防止除以零
    if (lenH < 0.001) {
        return float3(0.0, 0.0, 0.0);
    }
    
    H /= lenH;
    
    // 计算各种点积并进行安全的范围限制
    float dotNV = clamp(dot(N, V), 0.001, 1.0);
    float dotNL = clamp(dot(N, L), 0.001, 1.0);
    float dotLH = clamp(dot(L, H), 0.0, 1.0);
    float dotNH = clamp(dot(N, H), 0.001, 1.0);

    float3 color = float3(0.0, 0.0, 0.0);
    
    // 只在有效的光照方向上计算BRDF
    if (dotNL > 0.001) {
        // 确保粗糙度在有效范围内
        float rroughness = clamp(roughness, 0.05, 1.0);
        
        // 计算各种BRDF组件
        float D = D_GGX(dotNH, rroughness);
        float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
        float3 F = F_Schlick(dotNV, clamp(metallic, 0.0, 1.0));
        
        // 防止除以零或非常小的值
        float denominator = max(4.0 * dotNL * dotNV, 0.001);
        float3 spec = D * F * G / denominator;
        
        // 计算漫反射部分
        float3 diffuse = (1.0 - F) * (1.0 - metallic) * materialcolor() / PI;
        
        // 组合镜面反射和漫反射
        color = spec + diffuse * dotNL;
        
        // 确保结果在合理范围内
        color = clamp(color, float3(0.0, 0.0, 0.0), float3(10.0, 10.0, 10.0));
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
    
    // 多重边界检查，确保集群索引在有效范围内
    if (clusterIdx < TOTAL_CLUSTERS && 
        clusterX < CLUSTER_SIZE_X && 
        clusterY < CLUSTER_SIZE_Y && 
        clusterZ < CLUSTER_SIZE_Z) {
        
        // 严格验证光照计数和偏移量
        if (lightCount > 0 && lightCount <= NUM_LIGHTS && 
            lightOffset < LIGHT_INDEX_LIST_SIZE) {
            
            // 增强的边界检查
            uint maxLightIndexNum = min(ubo.maxlightindexnum, LIGHT_INDEX_LIST_SIZE);
            
            // 防止整数溢出
            uint safeOffset = lightOffset;
            if (safeOffset >= maxLightIndexNum) {
                safeOffset = 0;
                lightCount = 0; // 无效的偏移量，不处理任何光源
            }
            
            // 计算安全的光照数量
            uint remainingLights = 0;
            if (maxLightIndexNum > safeOffset) {
                remainingLights = maxLightIndexNum - safeOffset;
            }
            uint safeCount = min(lightCount, remainingLights);
            
            // 计算安全的最大索引
            uint maxIndex = safeOffset;
            if (maxIndex <= LIGHT_INDEX_LIST_SIZE - safeCount) {
                maxIndex += safeCount;
            } else {
                maxIndex = LIGHT_INDEX_LIST_SIZE;
            }
            
            uint processedLights = 0;
            
            // 使用安全的边界进行迭代
            for (uint i = safeOffset; i < maxIndex && processedLights < safeCount; i++) {
                // 获取光源索引并确保在有效范围内
                uint lightIndex = 0;
                if (i < LIGHT_INDEX_LIST_SIZE) {
                    lightIndex = clusterIndexList[i].clusterIndexList;
                }
                
                if (lightIndex < NUM_LIGHTS) {
                    // 安全地读取光源数据
                    Light currentLight = lights[lightIndex];
                    
                    // 计算光照向量
                    float3 lightVec = currentLight.position.xyz - input.WorldPos;
                    float lightDistance = length(lightVec);
                    float radius = currentLight.colorAndRadius.w;
                    
                    // 只处理在半径范围内的光源，并添加额外的有效性检查
                    if (lightDistance <= radius && lightDistance > 0.001) {
                        float3 L = normalize(lightVec);
                        
                        // 确保法线和光照方向有效
                        if (length(L) > 0.99 && length(N) > 0.99) {
                            float radianceFactor = radiance(radius, lightVec, N, L);
                            float3 lightColor = currentLight.colorAndRadius.xyz;
                            
                            // 确保光照颜色有效
                            if (!any(isnan(lightColor)) && !any(isinf(lightColor))) {
                                Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor;
                            }
                        }
                    }
                    
                    processedLights++;
                }
            }
        }
    }
    
    // 确保最终颜色有效
    Lo = clamp(Lo, float3(0.0, 0.0, 0.0), float3(16.0, 16.0, 16.0));

    float3 color = materialcolor() * 0.02;
    color += Lo;
    color = pow(color, float3(0.4545, 0.4545, 0.4545));
    return float4(color, 1.0);
}
