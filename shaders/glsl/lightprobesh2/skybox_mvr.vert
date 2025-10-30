#version 450
#extension GL_EXT_multiview : enable

layout (location = 0) in vec3 inPos;
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

layout (set = 1, binding = 0) uniform Local
{
	mat4 model;
} local;

layout (location = 0) out vec3 outUVW;

out gl_PerVertex 
{
	vec4 gl_Position;
};

void main() 
{
	outUVW = inPos;
	
	// ✅ 关键修复：使用w=0.0使天空盒不受平移影响
	// 在齐次坐标中：
	// - w=1.0: 位置向量，受平移影响
	// - w=0.0: 方向向量，只受旋转影响
	// 这是标准的天空盒渲染技巧
	vec4 pos = global.viewProject[gl_ViewIndex] * vec4(inPos.xyz, 0.0);
	gl_Position = vec4(pos.xy, pos.w, pos.w);  // 确保天空盒在最远处
}
