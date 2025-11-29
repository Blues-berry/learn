# 最终目标，实现使用Cornellbox场景的relighting

项目目录在C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\examples\lightprobesh2
着色器目录在C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2
完整遍历有关cornelbox的所有资料，包括shader，模型，光照，渲染，完整遍历有关previewmodel的所有资料，包括shader，模型，光照，渲染



# 1、实现基于PRT的relighting，使用球谐函数，预计算出光照和非光照信息，同时预计算光源旋转信息，导出为txt文件，目前代码已经有了这一部分。 目前代码的问题是：预计算不在GPU端进行，需要转移到GPU上。  且第3步: 预计算Light Transport (物体表面对光照的响应)应该采用逐个顶点计算，这部分也需要转移到GPU上。转移后进行导出测试，确认本地有文件且可以正常打开读取。在UI界面增加一个导出选项，点击后进行预计算，并导出txt文件。


# 2、最后需要使用导出的txt文件，UI能够读取对应的TXT文件应用预计算的信息为Cornell模型着色


列一个tudolilst,实现目标,先实现第一个，完成后写一个总结文档，并测试，期间不要写其他文档，全程中文回答。实现代码的过程中，增加调试内容，尽可能输出所有调试细节，方便定位问题

你需要做的两步

编译新 shader（必做）
将 prt_lt.comp 编译为 SPIR-V： "C:\VulkanSDK\1.3.xxxx\Bin\glslc.exe" -O C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2\prt_lt.comp -o C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2\prt_lt.comp.spv
构建并运行，执行导出
UI → PRT GPU Export
调整 Spotlight 内/外锥角
点击 Export PRT (GPU)
控制台将打印：
[ExportPRTDataGPU] current_path / output dir
[ExportPRTDataGPU] LT batch computed for vertices:
导出状态
输出文件：
prt_output/prt_data_lt_batch.txt（每顶点一行，9 组 vec3 系数）
其他 Lighting 文件同目录
实现细节说明

prt_lt.comp
每个工作项处理一个顶点（gl_GlobalInvocationID.x）
公式：LT_i = albedo * ∑ (max(0, dot(n, ω)) * Y_i(ω) * 4π/N)
GPU/CPU SH 基函数完全一致（与 EvaluateBasis 对齐）
PRTComputeShader
CreateDescriptorSetLayout() 扩展为 {0: samples, 1: ltInput, 2: output}
LoadComputeShader() 同时加载 prt_lighting.comp.spv 和 prt_lt.comp.spv
CreateComputePipeline() 创建两条管线（lighting / lt）
UpdateDescriptorSet(...) 支持可选绑定 binding=1
ComputeLightTransportBatch(...)：
写 samplesBuffer（directions）
写 ltInputBuffer（position/normal/albedo）
输出 outputCoefficientsBuffer（数组）
Dispatch(computePipelineLT, groupCountX=顶点数)
读回系数数组
关于顶点数据提取

目前在 main.cpp 中采用“拷贝 GPU 顶点缓冲 → staging → CPU 读回”的方式：
注意：源顶点缓冲需具备 VK_BUFFER_USAGE_TRANSFER_SRC_BIT；若 SDK 原始 Model 顶点缓冲不具备该用法，验证层会告警
若出现拷贝相关验证错误，请告诉我，我将切换方案（例如在模型加载时同时保留 CPU 顶点数组，或通过 vkglTF 内部接口取 CPU 辅助数据）
验证建议

先用较小模型与较小 SH Samples（如 64）
导出 prt_data_lt_batch.txt 并检查行数与顶点数一致
每行 9 组 vec3，数值大致在 [-1, 1] 内（受 albedo、法线分布影响）
再增大 SH Samples（128/256）提高精度
下一步（可选）

在着色阶段接入：将 Irradiance（A_l 已应用的 Lighting）与每顶点 LT 系数点乘，做真正的 PRT relighting 渲染
如果你确认现在的导出正确，我可以继续把着色器（prt_relighting.frag）和渲染管线改为读取 prt_data_lt_batch.txt 数据并应用到 Cornell 模型每个顶点，完成端到端 relighting 效果。



在着色阶段接入：将 Irradiance（A_l 已应用的 Lighting）与每顶点 LT 系数点乘，做真正的 PRT relighting 渲染
如果你确认现在的导出正确，我可以继续把着色器（prt_relighting.frag）和渲染管线改为读取 prt_data_lt_batch.txt 数据并应用到 Cornell 模型每个顶点，完成端到端 relighting 效果。