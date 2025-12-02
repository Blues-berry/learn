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

启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块，cornell场景消失


11.29 19.51
待验证:
启用 PRT Relighting 后，拖动 "Light" -> "Rotation" 滑块，场景中的光照是否会实时、平滑地变化？
光照效果是否符合物理预期？（例如，当聚光灯转向模型的红色墙壁时，整个场景是否会泛出柔和的红光？）
下一步建议的工作
在验证完当前的功能后，我们可以进行以下优化和扩展：

完善顶点数据提取:
问题: 当前我们从 GPU 拷贝顶点数据用于 LT 计算，这要求原始顶点缓冲区有 VK_BUFFER_USAGE_TRANSFER_SRC_BIT 标志。如果模型加载器没有设置这个标志，会导致验证层错误或拷贝失败。
方案: 更稳健的做法是，在 GltfModel 加载模型时，就在 CPU 端保留一份顶点数据的拷贝（位置和法线），这样就不再需要从 GPU 回读。
优化 PRT 数据管理:
问题: 目前，每次启动程序都需要手动点击 "Export PRT (GPU)" 来生成数据，然后才能加载并用于渲染。
方案: 在 preparePRTRelighting 函数中增加逻辑：如果 prt_data_lt_batch.txt 文件不存在，则自动调用一次 ExportPRTDataGPU() 来生成它。这样用户第一次运行时会自动预计算，之后就可以直接加载，体验更流畅。
增强渲染效果:
问题: 当前的 PRT Relighting 只计算了漫反射（diffuse）部分，没有考虑镜面反射（specular）。
方案:
简单方案: 可以在 prt_relight.frag 中混合一些基于 IBL 的镜面反射，但这与 PRT 的动态光照不完全匹配。
高级方案 (SH Environment Map): 将旋转后的光照 SH 系数传递给片元着色器，用于实时重建一个低频的环境贴图，然后从中采样用于镜面反射。这需要更复杂的着色器和数学知识。

spot 4, 32

减少调试信息输出频率，没有变化的内容不做输出


我明白了，我需要预计算出光源旋转对于着色的影响，并在PRT渲染中实时旋转光源，实时获取渲染效果


我重新描述一下，你分析分析。首先应该预计算三项内容，LIGHT ,LIGHT TRANSPORT 和光源旋转情况下的light 系数。在导出的时候，参考的是光源的初始位置和潜在的旋转方式预计算light 旋转系数。但是，在导出后，旋转光源应该能够得到和PBR类似的渲染结果。也就是，cornel模型随着光源旋转，出现环境光效果。但是目前的情况很奇怪，我使用PRT之后，在不开启enable light的情况下，没有显示初始的cornel模型


12.2 


一致（cosine 加权），亮度会更合理。
确认运行期的逻辑：不要把“enable light（实时光照开关）”当成“是否使用预计算数据”的全局开关。
如果 UI 的 “enable light” 是指实时光源/PBR 通道开关，而 PRT 是独立的通道，那么当关闭实时光时，应仍然使用预计算的 LIGHT × LT 来进行环境照明（即 PRT 通道仍然有效）。
如当前关闭了光而 PRTRenderer 又没有参与，就可能导致全黑。
建议：当关闭实时光时，走 PRT 渲染分支；当打开实时光时，可以叠加或切换。
导出/加载数据的注意点

你现在导出了每顶点的 LT（ExportLightTransportBatch），而 PRTRenderer::Initialize 里只加载了单个 LT（ImportLightTransport 只读第一行）。这意味着运行时用了“一个全局 LT”，有偏差。
快速打通方案（先验证流程正确性）：
导出阶段：保留 batch 导出不变。
运行时：仍用一个 LT（全局），先验证旋转是否生效、亮度是否合理。
完整方案（后续）：
加载 per-vertex LT（按顶点或按块读）；
在顶点/像素着色时按索引取对应的 LT 系数进行 dot(LIGHT_SH, LT_SH)；
这需要调整 PRTRenderer 的数据结构与绑定。
你现在可以做什么

用 Developer Command Prompt 编译（或运行 examples/lightprobesh2/compile.bat）。
在 main.cpp 中把 LT 的可见性函数替换为带余弦项的版本：
auto ltFunc = [&](const glm::vec3& dir) { return glm::max(0.0f, glm::dot(normals[i], dir)); };
ltCpuBatch.push_back(SphericalHarmonics::Project(ltFunc, directions));
运行后导出数据，确认：
prt_output/prt_data_lighting.txt 有 24 行；
prt_output/prt_data_lt.txt 行数等于顶点数（或至少不为 0）；
确认在 UI 中“关闭实时光”时仍然走 PRT 分支（使用预计算的 LIGHT×LT）。
如果仍然偏暗，打印/检查：
LIGHT 第 0 项（l=0）是否合理（通常最大）；
LT 第 0 项是否非零；
旋转查询 Relighter::QueryCoefficients 是否返回随角度变化的 LIGHT。
如果你希望，我可以：

直接把 main.cpp 中的 LT 可见性函数替换为 Lambert 余弦项；
给出 per-vertex LT 加载和使用的最小改动示例（读取批量 LT，在渲染时索引访问）；
帮你在 PRTRenderer 加一个“实时光关、PRT开”的逻辑开关，避免全黑。

PBR的着色效果是黄色，PRT似乎看不到有什么着色效果。去掉PRT计算时有关spot的设置，改成和PBR一样的光源设置，结果和enable light后的PBR一样，并删除有关spot的UI看看效果如何。

考虑将PRT和