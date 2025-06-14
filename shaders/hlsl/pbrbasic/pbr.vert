

struct VSInput
{
    [[vk::location(0)]] float3 Pos : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
};
// 定义顶点输入结构体 VSInput：
// - Pos：顶点位置，float3，绑定到位置 0，语义 POSITION0。
// - Normal：顶点法线，float3，绑定到位置 1，语义 NORMAL0。

struct UBO
{
    float4x4 projection;
    float4x4 model;
    float4x4 view;
    float3 camPos;
};
// 定义 Uniform Buffer 结构体 UBO：
// - projection：投影矩阵，4x4。
// - model：模型矩阵，4x4。
// - view：视图矩阵，4x4。
// - camPos：相机位置，float3。

cbuffer ubo : register(b0) { UBO ubo; }
// 声明常量缓冲区 ubo：
// - 类型：UBO 结构体。
// - 寄存器：b0（对应 C++ 中的 uniformBuffers.object）。

struct VSOutput
{
    float4 Pos : SV_POSITION;
    [[vk::location(0)]] float3 WorldPos : POSITION0;
    [[vk::location(1)]] float3 Normal : NORMAL0;
};
// 定义顶点输出结构体 VSOutput：
// - Pos：裁剪空间位置，float4，语义 SV_POSITION（系统值，传递给光栅化）。
// - WorldPos：世界空间位置，float3，绑定到位置 0，语义 POSITION0。
// - Normal：世界空间法线，float3，绑定到位置 1，语义 NORMAL0。

struct PushConsts {
    float3 objPos;
};
// 定义推送常量结构体 PushConsts：
// - objPos：对象位置偏移，float3。

[[vk::push_constant]] PushConsts pushConsts;
// 声明推送常量 pushConsts：
// - 类型：PushConsts 结构体。
// - 使用 vk::push_constant 属性，绑定到推送常量。

VSOutput main(VSInput input)
{
    // 主函数，顶点着色器入口，输入 VSInput，返回 VSOutput。
    VSOutput output = (VSOutput)0;
    // 初始化输出结构体，所有成员为 0。

    float3 locPos = mul(ubo.model, float4(input.Pos, 1.0)).xyz;
    // 计算局部位置：
    // - 将输入位置 (x, y, z) 扩展为 float4 (x, y, z, 1.0)。
    // - 应用模型矩阵变换（ubo.model）。
    // - 提取 xyz 分量（丢弃 w）。

    output.WorldPos = locPos + pushConsts.objPos;
    // 计算世界空间位置：
    // - 局部位置 + 对象位置偏移（pushConsts.objPos）。

    output.Normal = normalize(mul((float3x3)ubo.model, input.Normal));
    // 计算世界空间法线：
    // - 将模型矩阵转换为 3x3 矩阵（忽略平移）。
    // - 变换输入法线。
    // - 假设模型矩阵无非均匀缩放，原代码未归一化法线。

    output.Pos = mul(ubo.projection, mul(ubo.view, float4(output.WorldPos, 1.0)));
    // 计算裁剪空间位置：
    // - 将世界位置扩展为 float4 (x, y, z, 1.0)。
    // - 依次应用视图矩阵和投影矩阵变换。

    return output;
    // 返回输出结构体，传递到片段着色器。
}