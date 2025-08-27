// 顶点着色器 - PBR 基础实现
// 用于物理基础渲染的顶点着色器

struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION0;    // 顶点位置，绑定到位置 0
    [[vk::location(1)]] float3 Normal : NORMAL0;   // 顶点法线，绑定到位置 1
};

// Uniform Buffer 结构体
struct UBO
{
    float4x4 projection;  // 投影矩阵 4x4
    float4x4 model;       // 模型矩阵 4x4
    float4x4 view;        // 视图矩阵 4x4
    float3 camPos;        // 相机位置 float3
    uint maxlightindexnum; // 最大光源索引数量
};

// 常量缓冲区 ubo，寄存器 b0
cbuffer ubo : register(b0) { UBO ubo; }

// 顶点着色器输出结构体
struct VSOutput
{
    float4 Pos : SV_POSITION;                      // 裁剪空间位置，系统值，传递给片段着色器
    [[vk::location(0)]] float3 WorldPos : POSITION0; // 世界空间位置，绑定到位置 0
    [[vk::location(1)]] float3 Normal : NORMAL0;    // 世界空间法线，绑定到位置 1
};

// 推送常量结构体
struct PushConsts {
    float3 objPos;        // 对象位置偏移
};

// 推送常量 pushConsts
[[vk::push_constant]] PushConsts pushConsts;

VSOutput main(VSInput input)
{
    // 初始化输出结构体，所有成员为 0
    VSOutput output = (VSOutput)0;
    
    // 计算局部位置：
    // - 将顶点位置 (x, y, z) 扩展为 float4 (x, y, z, 1.0)
    // - 应用模型矩阵变换
    // - 提取 xyz 分量，忽略 w 分量
    float3 locPos = mul(ubo.model, float4(input.Pos, 1.0)).xyz;
    
    // 计算世界空间位置：
    // - 局部位置 + 对象位置偏移
    output.WorldPos = locPos + pushConsts.objPos;
    
    // 计算世界空间法线：
    // - 将模型矩阵转换为 3x3 矩阵（忽略平移）
    // - 变换输入法线
    // - 确保法线被归一化
    output.Normal = normalize(mul((float3x3)ubo.model, input.Normal));
    
    // 计算裁剪空间位置：
    // - 将世界位置扩展为 float4 (x, y, z, 1.0)
    // - 依次应用视图矩阵和投影矩阵变换
    output.Pos = mul(ubo.projection, mul(ubo.view, float4(output.WorldPos, 1.0)));
    
    // 返回输出结构体，传递到片段着色器
    return output;
}