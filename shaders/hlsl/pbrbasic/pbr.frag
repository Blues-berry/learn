#define NUM_LIGHTS 64
#define CLUSTER_SIZE_X 8
#define CLUSTER_SIZE_Y 8
#define CLUSTER_SIZE_Z 8
#define TOTAL_CLUSTERS CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z
#define LIGHT_INDEX_LIST_SIZE (NUM_LIGHTS * TOTAL_CLUSTERS)
// 定义常量，与 C++ 代码一致：
// - NUM_LIGHTS：光源数量，64。
// - CLUSTER_SIZE_X/Y/Z：集群网格尺寸，8x8x1。
// - TOTAL_CLUSTERS：总集群数，64。
// - LIGHT_INDEX_LIST_SIZE：光源索引列表大小，4096。

struct VSOutput {
    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
};
// 定义片段着色器输入结构体 VSOutput（与顶点着色器输出一致）：
// - WorldPos：世界空间位置，float3，位置 0。
// - Normal：世界空间法线，float3，位置 1。

struct UBO {
    float4x4 projection;
    float4x4 model;
    float4x4 view;
    float3 camPos;
    uint maxlightindexnum;
};
// 定义 Uniform Buffer 结构体 UBO：
// - projection：投影矩阵。
// - model：模型矩阵。
// - view：视图矩阵。
// - camPos：相机位置。
// - maxlightindexnum：最大光源索引数（未在代码中使用）。

cbuffer ubo : register(b0) { UBO ubo; }
// 声明常量缓冲区 ubo，寄存器 b0。

struct Light {
    float4 position;
    float4 colorAndRadius;
    float4 direction;
    float4 cutOff;
};
// 定义光源结构体 Light，与 C++ 一致：
// - position：位置。
// - colorAndRadius：颜色和半径。
// - direction：方向。
// - cutOff：截止角度。

cbuffer uboParams : register(b1) {
    Light lights[NUM_LIGHTS];
};
// 声明光源数据缓冲区 uboParams：
// - 包含 NUM_LIGHTS（64）个 Light 结构体。
// - 寄存器 b1（对应 uniformBuffers.params）。

struct Indices {
    uint clusterIndexList;
    float3 padding;
};
// 定义光源索引结构体 Indices：
// - clusterIndexList：光源索引。
// - padding：填充 12 字节，确保 16 字节对齐。

cbuffer clusterIndexList : register(b2) {
    Indices indices[LIGHT_INDEX_LIST_SIZE];
};
// 声明光源索引列表缓冲区：
// - 包含 LIGHT_INDEX_LIST_SIZE（4096）个 Indices。
// - 寄存器 b2（对应 uniformBuffers.clusterIndexList）。

struct Cluster {
    uint counts;
    uint offsets;
    float2 padding;
};
// 定义集群结构体 Cluster：
// - counts：光源数量。
// - offsets：索引列表偏移。
// - padding：填充 8 字节，确保 16 字节对齐。

cbuffer clusterCountsandOffsets : register(b3) {
    Cluster clusterCountsandOffsets[TOTAL_CLUSTERS];
};
// 声明集群计数和偏移缓冲区：
// - 包含 TOTAL_CLUSTERS（64）个 Cluster。
// - 寄存器 b3（对应 uniformBuffers.clusterData）。

struct PushConsts {
    [[vk::offset(12)]] float roughness;
    [[vk::offset(16)]] float metallic;
    [[vk::offset(20)]] float r;
    [[vk::offset(24)]] float g;
    [[vk::offset(28)]] float b;
};
// 定义推送常量结构体 PushConsts：
// - roughness：粗糙度，偏移 12。
// - metallic：金属度，偏移 16。
// - r, g, b：颜色，偏移 20, 24, 28。
// - 偏移从 12 开始，跳过顶点着色器的 float3（12 字节）。

[[vk::push_constant]] PushConsts material;
// 声明推送常量 material。

static const float PI = 3.14159265359;
// 定义 π 常量。

float3 materialcolor() {
    // 函数返回材质颜色。
    return float3(material.r, material.g, material.b);
    // 从推送常量获取 RGB 分量。
}

float D_GGX(float dotNH, float roughness) {
    // 函数计算 GGX 法线分布函数（NDF）。
    float alpha = roughness * roughness;
    // 计算 α = 粗糙度²。
    float alpha2 = alpha * alpha;
    // 计算 α²。
    float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
    // 计算分母：(N·H)² * (α² - 1) + 1。
    return (alpha2) / (PI * denom * denom);
    // 返回：α² / (π * 分母²)。
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness) {
    // 函数计算 Schlick-Smith GGX 几何遮挡函数。
    float r = (roughness + 1.0);
    // 计算 r = 粗糙度 + 1。
    float k = (r * r) / 8.0;
    // 计算 k = r² / 8。
    float GL = dotNL / (dotNL * (1.0 - k) + k);
    // 计算光线遮挡项：N·L / (N·L * (1 - k) + k)。
    float GV = dotNV / (dotNV * (1.0 - k) + k);
    // 计算视线遮挡项：N·V / (N·V * (1 - k) + k)。
    return GL * GV;
        // 继续计算几何遮挡函数，返回光线和视线遮挡的乘积。
    return GL * GV;
    // 返回几何遮挡项：GL（光线遮挡） * GV（视线遮挡）。
}

float3 F_Schlick(float cosTheta, float metallic) {
    // 函数计算 Schlick 菲涅尔项，模拟表面反射。
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), materialcolor(), metallic);
    // 计算基础反射率 F0：
    // - 非金属：默认 0.04（常见近似值）。
    // - 金属：使用材质颜色（material.r, g, b）。
    // - 使用 metallic 参数在两者之间插值。
    float3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    // 计算菲涅尔项：
    // - F = F0 + (1 - F0) * (1 - cosθ)^5。
    // - cosTheta：通常为 N·V 或 L·H。
    // - 使用 5 次幂模拟菲涅尔效应。
    return F;
    // 返回菲涅尔反射率。
}

float3 BRDF(float3 L, float3 V, float3 N, float metallic, float roughness) {
    // 函数计算 PBR 的镜面 BRDF（双向反射分布函数）。
    float3 H = normalize(V + L);
    // 计算半角向量 H = 归一化(V + L)。
    float dotNV = clamp(dot(N, V), 0.0, 1.0);
    // 计算 N·V（法线与视线的点积），限制在 [0, 1]。
    float dotNL = clamp(dot(N, L), 0.0, 1.0);
    // 计算 N·L（法线与光线的点积），限制在 [0, 1]。
    float dotLH = clamp(dot(L, H), 0.0, 1.0);
    // 计算 L·H（光线与半角向量的点积），限制在 [0, 1]。
    float dotNH = clamp(dot(N, H), 0.0, 1.0);
    // 计算 N·H（法线与半角向量的点积），限制在 [0, 1]。

    float3 color = float3(0.0, 0.0, 0.0);
    // 初始化输出颜色为黑色。

    if (dotNL > 0.0) {
        // 如果光线照到表面（N·L > 0），计算镜面反射。
        float rroughness = max(0.05, roughness);
        // 确保粗糙度至少为 0.05，避免除零或不真实效果。
        float D = D_GGX(dotNH, roughness);
        // 计算法线分布函数（GGX）。
        float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
        // 计算几何遮挡函数（Schlick-Smith GGX）。
        float3 F = F_Schlick(dotNV, metallic);
        // 计算菲涅尔项（Schlick）。
        float3 spec = D * F * G / (4.0 * dotNL * dotNV);
        // 计算镜面 BRDF：
        // - 公式：D * F * G / (4 * N·L * N·V)。
        // - 表示镜面反射贡献。

        float3 diffuse = (1.0 - F) * (1.0 - metallic) * materialcolor() / PI;
                // 计算镜面 漫反射：
        color = spec + diffuse * dotNL;
        // 将镜面反射累加到颜色（注释表明这是修复后的逻辑，仅累加镜面项）。
    }
    return color;
    // 返回 BRDF 计算结果（镜面反射贡献）。
}


float radiance(float radius, float3 lightVec, float3 N, float3 L) {
    // 函数计算光源的辐射强度（考虑距离衰减和角度）。
    float distance = length(lightVec);
    // 计算片段到光源的距离。
    if (distance > radius) return 0.0;
    // 如果距离超过光源半径，返回 0（无贡献）。
    float attenuation = pow(clamp(1.0 - distance / radius, 0.0, 1.0), 2.0);
    // 计算距离衰减：
    // - 公式：(1 - 距离/半径)^2。
    // - clamp 确保值在 [0, 1]。
    float dotNL = max(dot(N, L), 0.0);
    // 计算 N·L，限制非负，表示光线对表面的贡献。
    return attenuation * dotNL;
    // 返回辐射强度：衰减 * N·L。
}

float4 main(VSOutput input) : SV_TARGET {
    // 主函数，片段着色器入口，输入 VSOutput，返回颜色。
    float3 N = normalize(input.Normal);
    // 归一化输入法线，确保单位长度。
    float3 V = normalize(ubo.camPos - input.WorldPos);
    // 计算视线向量 V = 归一化(相机位置 - 片段世界位置)。
    float roughness = material.roughness;
    // 获取材质粗糙度。

    float4 worldPos = float4(input.WorldPos, 1.0);
    // 将世界位置扩展为 float4，w=1.0。
    float4 viewPos = mul(ubo.view, worldPos);
    // 变换到视图空间：view * worldPos。
    float4 clipPos = mul(ubo.projection, viewPos);
    // 变换到裁剪空间：projection * viewPos。
/*
    // 检查透视除法是否有效
    if (clipPos.w <= 0.0 || any(abs(clipPos.xy) > clipPos.w)) {
        float3 color = materialcolor() * 0.02;
        color = pow(color, float3(0.4545, 0.4545, 0.4545));
        return float4(color, 1.0);
    }
*/
    clipPos /= clipPos.w;
    // 转换为 NDC 坐标，除以 w 分量。
    float2 screenPos = clipPos.xy * 0.5 + 0.5;
    // 映射到屏幕空间 [0, 1]：NDC.xy * 0.5 + 0.5。

    float viewZ = -viewPos.z;
    // 获取视图空间 Z 值（负值，因为 Vulkan Z 指向屏幕外）。
    float zNear = 0.1;
    float zFar = 256.0;
    // 定义近裁剪面和远裁剪面，与 C++ 相机设置一致。
    uint clusterZ = uint(float((log(viewZ / zNear) / log(zFar / zNear) * CLUSTER_SIZE_Z)));
    // 计算 Z 方向集群索引：
    // - 使用对数深度公式：log(z / zNear) / log(zFar / zNear)。
    // - 乘以 CLUSTER_SIZE_Z（1），映射到集群范围。
    clusterZ = clamp(clusterZ, 0u, CLUSTER_SIZE_Z - 1);
    // 限制 Z 索引在 [0, CLUSTER_SIZE_Z - 1]（0）。

    uint clusterX = uint(screenPos.x * CLUSTER_SIZE_X);
    uint clusterY = uint(screenPos.y * CLUSTER_SIZE_Y);
    // 计算 X/Y 方向集群索引：
    // - screenPos.x/y（[0, 1]）乘以 CLUSTER_SIZE_X/Y（8）。
    clusterX = clamp(clusterX, 0u, CLUSTER_SIZE_X - 1);
    clusterY = clamp(clusterY, 0u, CLUSTER_SIZE_Y - 1);
    // 限制 X/Y 索引在 [0, 7]。
    clusterZ = clamp(clusterZ, 0u, CLUSTER_SIZE_Z - 1);
    // 再次限制 Z 索引（冗余，因为 CLUSTER_SIZE_Z = 1）。
    uint clusterIdx = clusterZ * CLUSTER_SIZE_X * CLUSTER_SIZE_Y + clusterY * CLUSTER_SIZE_X + clusterX;
    // 计算集群索引：
    // - 公式：z * 8 * 8 + y * 8 + x。
    // - 由于 CLUSTER_SIZE_Z = 1，实际为 y * 8 + x。

    uint lightCount = clusterCountsandOffsets[clusterIdx].counts;
    // 获取当前集群的光源数量。
    uint lightOffset = clusterCountsandOffsets[clusterIdx].offsets;
    // 获取当前集群的索引列表偏移。

    float3 Lo = float3(0.0, 0.0, 0.0);
    // 初始化直接光照贡献（积分项）。
    if (lightCount > 0) {
        // 如果集群有光源，计算光照。
        for (int i = lightOffset; i < lightOffset + lightCount; i++) {
            // 遍历集群的光源索引。
            float3 lightVec = lights[indices[i].clusterIndexList].position.xyz - input.WorldPos;
            // 计算光源向量：光源位置 - 片段位置。
            float3 L = normalize(lightVec);
            // 归一化光源方向。
            float radianceFactor = radiance(lights[indices[i].clusterIndexList].colorAndRadius.w, lightVec, N, L);
            // 计算辐射强度：
            // - 半径：light.colorAndRadius.w。
            // - 光源向量、N、L 传入 radiance 函数。
            float3 lightColor = lights[indices[i].clusterIndexList].colorAndRadius.xyz;
            // 获取光源颜色（RGB）。
            Lo += BRDF(L, V, N, material.metallic, roughness) * lightColor * radianceFactor;
            // 累加光照贡献：
            // - BRDF：镜面反射。
            // - lightColor：光源颜色。
            // - radianceFactor：辐射强度（衰减和 N·L）。
        }
    }

    float3 color = materialcolor() * 0.02;
    // 计算基础颜色：
    // - 材质颜色 * 0.02（模拟环境光或漫反射，系数较小）。
    color += Lo;
    // 累加直接光照贡献。

    color = pow(color, float3(0.4545, 0.4545, 0.4545));
    // 应用 Gamma 校正：
    // - 幂 0.4545 ≈ 1/2.2，将线性颜色转换为 sRGB。
    return float4(color, 1.0);
    // 返回最终颜色：(R, G, B, 1.0)。
}