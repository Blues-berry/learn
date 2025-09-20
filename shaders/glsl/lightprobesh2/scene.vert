#version 450
#extension GL_EXT_multiview : enable
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view[6];//改成6了
    mat4 projection;
} ubo;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;

void main() {
    gl_Position = ubo.projection * ubo.view[gl_ViewIndex] * vec4(inPosition, 1.0);
    outUV = inUV;
    outNormal = inNormal;
}
/*
1. gl_ViewIndex 是什么？

定义：gl_ViewIndex 是一个 GLSL 内置整数变量（int 类型），由 Vulkan 的 VK_KHR_multiview 扩展引入。它表示当前渲染的视图（View）的索引，用于多视图渲染场景。
来源：gl_ViewIndex 由 Vulkan 驱动在启用多视图扩展时自动提供，着色器无需显式声明即可使用，但需要通过 #extension GL_EXT_multiview : enable 启用对多视图的支持。
作用：它允许着色器在单次渲染通行证中区分不同的视图（例如，立方体贴图的 6 个面或立体渲染的左右眼视图）。通过 gl_ViewIndex，着色器可以根据当前视图索引选择不同的变换矩阵或其他参数，从而为每个视图生成不同的输出。
gl_ViewIndex 是 Vulkan 多视图渲染（VK_KHR_multiview）的 GLSL 接口变量，只有在着色器中显式启用 #extension GL_EXT_multiview : enable 后才能使用。
没有启用该扩展，编译器无法识别 gl_ViewIndex，因此报错并终止编译。

*/