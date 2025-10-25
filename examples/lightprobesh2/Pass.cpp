#include "Pass.h"
#include "glm/gtc/matrix_transform.hpp"
#include <array>

// 包含头文件：Pass.h 提供渲染通道相关类定义，glm 提供矩阵变换功能，array 用于固定大小数组。

/**
 * @brief ComputePass类的构造函数
 * @param device_ 指向VulkanDevice对象的指针，用于管理Vulkan设备相关资源
 * @param example 指向IExampleInterface接口的指针，提供示例功能的访问接口
 */
ComputePass::ComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example) : device(device_), iLoader(example) // 使用成员初始化列表初始化device和iLoader成员变量
{
    // 构造函数：初始化 ComputePass 类，存储 Vulkan 设备指针和示例接口指针。
}

ComputePass::~ComputePass()
{
    // 析构函数：清理 Vulkan 资源，确保在对象销毁时释放所有相关资源。
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr); // 销毁计算管线。
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr); // 销毁描述符集布局。
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr); // 销毁管线布局。
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr); // 销毁描述符池。
    }
}

/**
 * @brief ComputePass类的Draw方法，用于执行计算着色器的绘制命令
 * 
 * @param cmd VkCommand类型的命令缓冲区，用于记录命令
 */
void ComputePass::Draw(VkCommandBuffer cmd)
{
    // Draw 方法：调用 Dispatch 方法执行计算着色器命令。
    Dispatch(cmd); // 将命令缓冲区传递给 Dispatch 方法。
}

FullScreenPass::FullScreenPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkFormat format_) : device(device_), iLoader(example), format(format_)
{
    // 构造函数：初始化全屏渲染通道，设置 Vulkan 设备、示例接口和图像格式。
    clearValue.color.float32[0] = 0.f; // 设置清除颜色值的红色分量为 0。
    clearValue.color.float32[1] = 0.f; // 设置绿色分量为 0。
    clearValue.color.float32[2] = 0.f; // 设置蓝色分量为 0。
    clearValue.color.float32[3] = 0.f; // 设置透明度分量为 0。

    beginInfo = vks::initializers::renderPassBeginInfo(); // 初始化渲染通道开始信息。
    beginInfo.clearValueCount = 1; // 设置清除值数量为 1。
    beginInfo.pClearValues = &clearValue; // 指定清除值数组。

    GenerateSampler(); // 调用方法生成采样器。
}

/**
 * 全屏渲染通道的析构函数
 * 用于释放和清理与全屏渲染相关的所有 Vulkan 资源
 */
FullScreenPass::~FullScreenPass()
{
    // 析构函数：清理全屏渲染通道的 Vulkan 资源。
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device->logicalDevice, sampler, nullptr); // 销毁采样器。
    }

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr); // 销毁图形管线。
    }

    if (image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, image, nullptr); // 销毁图像对象。
    }

    if (view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, view, nullptr); // 销毁图像视图。
    }

    if (deviceMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, deviceMemory, nullptr); // 释放设备内存。
    }

    if (renderpass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device->logicalDevice, renderpass, nullptr); // 销毁渲染通道。
    }

    if (fbo != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device->logicalDevice, fbo, nullptr); // 销毁帧缓冲区。
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr); // 销毁描述符集布局。
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr); // 销毁管线布局。
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr); // 销毁描述符池。
    }
}

// 生成全屏渲染通道所需的采样器对象
void FullScreenPass::GenerateSampler()
{
    // 创建采样器，用于纹理采样。
    VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo(); // 初始化采样器创建信息。
    samplerCI.magFilter = VK_FILTER_LINEAR; // 设置放大过滤为线性。
    samplerCI.minFilter = VK_FILTER_LINEAR; // 设置缩小过滤为线性。
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // 设置 Mipmap 模式为线性。
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // U 方向寻址模式为边缘裁剪。
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // V 方向寻址模式为边缘裁剪。
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; // W 方向寻址模式为边缘裁剪。
    samplerCI.minLod = 0.0f; // 设置最小 LOD 为 0。
    samplerCI.maxLod = 1.0f; // 设置最大 LOD 为 1。
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // 设置边界颜色为不透明白色。
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCI, nullptr, &sampler)); // 创建采样器并检查结果。
}

/**
 * @brief 准备全屏渲染通道所需的所有资源
 */
void FullScreenPass::Prepare()
{
    // 准备全屏渲染所需的资源。
    PrepareRenderPass(); // 准备渲染通道。
    PreparePipeline(); // 准备渲染管线。
    PrepareFrameBuffer(); // 准备帧缓冲区。
    PrepareData(); // 准备渲染数据。
}

void FullScreenPass::FeedDescriptor(VkDescriptorImageInfo& descriptor)
{
    // 设置描述符图像信息，用于着色器访问纹理。
    descriptor.sampler = sampler; // 指定采样器。
    descriptor.imageView = view; // 指定图像视图。
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 设置图像布局为着色器只读。
}

void FullScreenPass::Draw(VkCommandBuffer cmd)
{
    // 执行全屏渲染的绘制命令。
    if (pipeline == VK_NULL_HANDLE)
    {
        return; // 如果管线未初始化，直接返回。
    }

    beginInfo.renderPass = renderpass; // 设置渲染通道。
    beginInfo.renderArea.extent.width = width; // 设置渲染区域宽度。
    beginInfo.renderArea.extent.height = height; // 设置渲染区域高度。
    beginInfo.framebuffer = fbo; // 设置帧缓冲区。

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE); // 开始渲染通道。
    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f); // 设置视口。
    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0); // 设置剪刀矩形。
    vkCmdSetViewport(cmd, 0, 1, &viewport); // 应用视口设置。
    vkCmdSetScissor(cmd, 0, 1, &scissor); // 应用剪刀矩形设置。
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); // 绑定图形管线。
    vkCmdDraw(cmd, 3, 1, 0, 0); // 绘制一个三角形（全屏三角形）。
    vkCmdEndRenderPass(cmd); // 结束渲染通道。
}

/**
 * @brief 准备渲染通道(Render Pass)的函数
 */
void FullScreenPass::PrepareRenderPass()
{
    // 创建渲染通道。
    VkAttachmentDescription attDesc = {}; // 初始化附件描述。
    attDesc.format = format; // 设置附件格式。
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 加载时清除附件。
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 存储附件内容。
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 模板加载不关心。
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 模板存储不关心。
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 初始布局未定义。
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 最终布局为着色器只读。
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // 颜色附件引用。

    VkSubpassDescription subpassDescription = {}; // 初始化子通道描述。
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // 设置为图形管线。
    subpassDescription.colorAttachmentCount = 1; // 一个颜色附件。
    subpassDescription.pColorAttachments = &colorReference; // 指定颜色附件引用。

    std::array<VkSubpassDependency, 2> dependencies; // 定义两个子通道依赖。

    // 第一个依赖：从外部子通道到当前子通道。
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL; // 源为外部子通道。
    dependencies[0].dstSubpass = 0; // 目标为第一个子通道。
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // 源阶段为管道底部。
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // 目标阶段为颜色附件输出。
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // 源访问为内存读取。
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // 目标访问为颜色附件读写。
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT; // 按区域依赖。

    // 第二个依赖：从当前子通道到外部子通道。
    dependencies[1].srcSubpass = 0; // 源为第一个子通道。
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL; // 目标为外部子通道。
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // 源阶段为颜色附件输出。
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // 目标阶段为管道底部。
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // 源访问为颜色附件读写。
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // 目标访问为内存读取。
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT; // 按区域依赖。

    VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo(); // 初始化渲染通道创建信息。
    renderPassCI.attachmentCount = 1; // 一个附件。
    renderPassCI.pAttachments = &attDesc; // 指定附件描述。
    renderPassCI.subpassCount = 1; // 一个子通道。
    renderPassCI.pSubpasses = &subpassDescription; // 指定子通道描述。
    renderPassCI.dependencyCount = 2; // 两个依赖。
    renderPassCI.pDependencies = dependencies.data(); // 指定依赖数组。

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderpass)); // 创建渲染通道并检查结果。
}

GenBRDFLutPass::GenBRDFLutPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : FullScreenPass(device_, example, VK_FORMAT_R16G16_SFLOAT)
{
    // 构造函数：初始化 BRDF 查找表生成通道，继承自 FullScreenPass，指定格式为 R16G16_SFLOAT。
    width = 512; // 设置输出宽度为 512。
    height = 512; // 设置输出高度为 512。
}

GenBRDFLutPass::~GenBRDFLutPass()
{
    // 析构函数：清理资源（继承自 FullScreenPass 的清理）。
}

void GenBRDFLutPass::PreparePipeline()
{
    // 创建 BRDF 查找表的渲染管线。
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(nullptr, 0); // 初始化管线布局，无描述符集。
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout)); // 创建管线布局。

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE); // 设置三角形列表拓扑。
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE); // 设置填充模式，无背面剔除，逆时针为正面。
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE); // 设置颜色混合，启用所有通道。
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState); // 设置颜色混合状态。
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL); // 禁用深度和模板测试。
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1); // 设置一个视口和剪刀。
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT); // 禁用多重采样。
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR }; // 启用动态视口和剪刀。
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables); // 设置动态状态。
    VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo(); // 无顶点输入。
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages; // 定义两个着色器阶段（顶点和片段）。

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderpass); // 初始化图形管线创建信息。
    pipelineCI.pInputAssemblyState = &inputAssemblyState; // 设置输入装配状态。
    pipelineCI.pRasterizationState = &rasterizationState; // 设置光栅化状态。
    pipelineCI.pColorBlendState = &colorBlendState; // 设置颜色混合状态。
    pipelineCI.pMultisampleState = &multisampleState; // 设置多重采样状态。
    pipelineCI.pViewportState = &viewportState; // 设置视口状态。
    pipelineCI.pDepthStencilState = &depthStencilState; // 设置深度模板状态。
    pipelineCI.pDynamicState = &dynamicState; // 设置动态状态。
    pipelineCI.stageCount = 2; // 两个着色器阶段。
    pipelineCI.pStages = shaderStages.data(); // 指定着色器阶段数组。
    pipelineCI.pVertexInputState = &emptyInputState; // 设置顶点输入状态。

    shaderStages[0] = iLoader->LoadShader("lightprobesh2/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT); // 加载顶点着色器。
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT); // 加载片段着色器。

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline)); // 创建图形管线并检查结果。
}

void GenBRDFLutPass::PrepareFrameBuffer()
{
    // 创建帧缓冲区，用于 BRDF 查找表渲染。
    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo(); // 初始化图像创建信息。
    imageCI.imageType = VK_IMAGE_TYPE_2D; // 设置图像类型为 2D。
    imageCI.format = format; // 设置图像格式。
    imageCI.extent.width = width; // 设置图像宽度。
    imageCI.extent.height = height; // 设置图像高度。
    imageCI.extent.depth = 1; // 设置图像深度为 1。
    imageCI.mipLevels = 1; // 设置单级 Mipmap。
    imageCI.arrayLayers = 1; // 设置单层。
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL; // 设置图像平铺为最优。
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // 设置图像用途为颜色附件和采样。
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &image)); // 创建图像。

    VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo(); // 初始化内存分配信息。
    VkMemoryRequirements memReqs = {}; // 初始化内存需求。
    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs); // 获取图像内存需求。
    memAlloc.allocationSize = memReqs.size; // 设置分配大小。
    memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // 获取设备本地内存类型。
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &deviceMemory)); // 分配内存。
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0)); // 绑定图像内存。

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo(); // 初始化图像视图创建信息。
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D; // 设置视图类型为 2D。
    viewCI.format = format; // 设置视图格式。
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 设置颜色方面。
    viewCI.subresourceRange.levelCount = 1; // 设置单级 Mipmap。
    viewCI.subresourceRange.layerCount = 1; // 设置单层。
    viewCI.image = image; // 指定图像。
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &view)); // 创建图像视图。

    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo(); // 初始化帧缓冲区创建信息。
    framebufferCI.renderPass = renderpass; // 设置渲染通道。
    framebufferCI.attachmentCount = 1; // 一个附件。
    framebufferCI.pAttachments = &view; // 指定图像视图。
    framebufferCI.width = width; // 设置帧缓冲区宽度。
    framebufferCI.height = height; // 设置帧缓冲区高度。
    framebufferCI.layers = 1; // 设置单层。

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &fbo)); // 创建帧缓冲区。
}

GenSHComputePass::GenSHComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example)
{
    // 构造函数：初始化球谐（SH）计算通道，继承自 ComputePass。
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // 一个图像采样器描述符。
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }, // 一个存储缓冲区描述符。
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1); // 初始化描述符池。
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool)); // 创建描述符池。

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0), // 绑定 0：图像采样器。
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1), // 绑定 1：存储缓冲区。
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings); // 初始化描述符集布局。
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout)); // 创建描述符集布局。

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1); // 初始化描述符集分配。
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet)); // 分配描述符集。

    VkPipelineLayoutCreateInfo pplInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1); // 初始化管线布局。
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pplInfo, nullptr, &pipelineLayout)); // 创建管线布局。

    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout); // 初始化计算管线。
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/sh_compute.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT); // 加载计算着色器。
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline)); // 创建计算管线。

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, // 创建缓冲区，支持统一缓冲区和存储缓冲区。
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, // 使用设备本地内存。
        &shCoeffBuffer, // 存储球谐系数。
        sizeof(SHCoefficients)); // 缓冲区大小为 SHCoefficients 结构。
}

GenSHComputePass::~GenSHComputePass()
{
    // 析构函数：清理球谐计算通道资源。
    shCoeffBuffer.destroy(); // 销毁球谐系数缓冲区。
}

void GenSHComputePass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    // 设置立方体贴图并更新描述符集。
    cubemap = cube; // 存储立方体贴图。

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &cubemap->descriptor), // 更新绑定 0 的图像采样器。
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &shCoeffBuffer.descriptor), // 更新绑定 1 的存储缓冲区。
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL); // 更新描述符集。
}

void GenSHComputePass::FeedSH(VkDescriptorBufferInfo& descriptor)
{
    // 提供球谐系数缓冲区的描述符信息。
    descriptor = shCoeffBuffer.descriptor; // 设置描述符为球谐缓冲区。
}

void GenSHComputePass::Generate(VkQueue queue)
{
    // 生成球谐系数。
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true); // 创建主命令缓冲区。

    Draw(cmdBuf); // 调用 Draw 方法执行计算。

    device->flushCommandBuffer(cmdBuf, queue); // 提交并刷新命令缓冲区。
}

void GenSHComputePass::Dispatch(VkCommandBuffer cmd)
{
    // 执行计算着色器分派。
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline); // 绑定计算管线。
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr); // 绑定描述符集。
    vkCmdDispatch(cmd, 1, 1, 1); // 分派计算任务（单工作组）。
}

MainPass::MainPass(vks::VulkanDevice* device_)
    : device(device_)
    , beginInfo(vks::initializers::renderPassBeginInfo())
{
    // 构造函数：初始化主渲染通道。
    clearValue.resize(2); // 设置两个清除值（颜色和深度）。

    clearValue[0].color.float32[0] = 0.2f; // 设置颜色清除值（灰色）。
    clearValue[0].color.float32[1] = 0.2f;
    clearValue[0].color.float32[2] = 0.2f;
    clearValue[0].color.float32[3] = 0.f;

    clearValue[1].depthStencil.depth = 1.0; // 设置深度清除值为 1.0。
    clearValue[1].depthStencil.stencil = 0; // 设置模板清除值为 0。

    beginInfo.clearValueCount = 2; // 设置清除值数量。
    beginInfo.pClearValues = clearValue.data(); // 指定清除值数组。

    PreparePerPassResource(); // 准备每通道资源。
}

MainPass::~MainPass()
{
    // 析构函数：清理主渲染通道资源。
    globalBuffer.unmap(); // 解除全局缓冲区映射。
    globalBuffer.destroy(); // 销毁全局缓冲区。

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr); // 销毁描述符集布局。
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr); // 销毁描述符池。
        descriptorPool = VK_NULL_HANDLE;
    }
}

void MainPass::SetUp(VkRenderPass pass)
{
    // 设置渲染通道。
    beginInfo.renderPass = pass; // 指定渲染通道。
}

void MainPass::UpdateGlobal(const GlobalUbo& ubo)
{
    // 更新全局统一缓冲区对象（UBO）数据。
    memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo)); // 复制 UBO 数据到缓冲区。
}

void MainPass::Draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width, uint32_t height, std::function<void(VkCommandBuffer)> &&encoder)
{
    // 执行主渲染通道的绘制。
    beginInfo.renderArea.extent.width = width; // 设置渲染区域宽度。
    beginInfo.renderArea.extent.height = height; // 设置渲染区域高度。
    beginInfo.framebuffer = framebuffer; // 设置帧缓冲区。

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE); // 开始渲染通道。

    VkViewport viewport = vks::initializers::viewport((float)beginInfo.renderArea.extent.width, (float)beginInfo.renderArea.extent.height, 0.0f, 1.0f); // 设置视口。
    vkCmdSetViewport(cmd, 0, 1, &viewport); // 应用视口设置。

    VkRect2D scissor = vks::initializers::rect2D(viewport.width, viewport.height, 0, 0); // 设置剪刀矩形。
    vkCmdSetScissor(cmd, 0, 1, &scissor); // 应用剪刀矩形设置。

    encoder(cmd); // 调用用户提供的编码函数执行绘制。

    vkCmdEndRenderPass(cmd); // 结束渲染通道。
}

void MainPass::UpdateBindings()
{
    
    // 更新描述符集绑定。
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalBuffer.descriptor), // 绑定 0：全局 UBO。
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &environmemts.shCoeffs), // 绑定 1：球谐系数。
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &environmemts.brdfView), // 绑定 2：BRDF 查找表。
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &environmemts.irradianceCube), // 绑定 3：辐照度立方体贴图。
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &environmemts.prefilteredCube), // 绑定 4：预过滤立方体贴图。
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL); // 更新描述符集。
}

void MainPass::PreparePerPassResource()
{
    // 准备主渲染通道的资源。
    // ✅ 修复：增加描述符池大小，确保有足够的描述符
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 }, // 增加到4：globalBuffer + shCoeffs + 预留
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 }, // 增加到8：brdf + irradiance + prefiltered + 预留
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1); // 初始化描述符池。
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool)); // 创建描述符池。

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0), // 绑定 0：统一缓冲区（顶点和片段着色器）。
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1), // 绑定 1：球谐系数（片段着色器）。
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2), // 绑定 2：BRDF 查找表。
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3), // 绑定 3：辐照度立方体贴图。
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4), // 绑定 4：预过滤立方体贴图。
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings); // 初始化描述符集布局。
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout)); // 创建描述符集布局。

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1); // 初始化描述符集分配。
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet)); // 分配描述符集。

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, // 创建统一缓冲区。
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // 主机可见且一致性内存。
        &globalBuffer, // 存储全局 UBO。
        sizeof(GlobalUbo)); // 缓冲区大小为 GlobalUbo 结构。
    globalBuffer.map(); // 映射缓冲区以供主机访问。
}

GenIBLCubeMipPass::GenIBLCubeMipPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap_, VkFormat format, uint32_t mip_, uint32_t width_, uint32_t height_)
    : FullScreenPass(device_, example, format)
    , cubemap(cubemap_)
    , mipmap(mip_)
{
    // 构造函数：初始化 IBL 立方体贴图 Mipmap 生成通道，继承自 FullScreenPass。
    width = width_; // 设置宽度。
    height = height_; // 设置高度。

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, // 创建统一缓冲区。
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // 主机可见且一致性内存。
        &ubo, // 存储 IBL 生成 UBO。
        sizeof(IBLGenUBO)); // 缓冲区大小为 IBLGenUBO 结构。
    ubo.map(); // 映射缓冲区。

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo(); // 初始化图像视图创建信息。
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY; // 设置视图类型为 2D 数组（用于立方体贴图）。
    viewCI.format = format; // 设置格式。
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 设置颜色方面。
    viewCI.subresourceRange.baseMipLevel = mipmap; // 设置 Mipmap 级别。
    viewCI.subresourceRange.levelCount = 1; // 单级 Mipmap。
    viewCI.subresourceRange.baseArrayLayer = 0; // 从第 0 层开始。
    viewCI.subresourceRange.layerCount = 6; // 6 层（立方体贴图的 6 个面）。
    viewCI.image = cubemap; // 指定立方体贴图图像。
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &view)); // 创建图像视图。
}

GenIBLCubeMipPass::~GenIBLCubeMipPass()
{
    // 析构函数：清理 IBL 立方体贴图生成通道资源。
    ubo.destroy(); // 销毁统一缓冲区。
}

void GenIBLCubeMipPass::Draw(VkCommandBuffer cmd, vkglTF::Model& model)
{
    // 执行 IBL 立方体贴图的绘制。
    if (pipeline == VK_NULL_HANDLE)
    {
        return; // 如果管线未初始化，直接返回。
    }

    beginInfo.renderPass = renderpass; // 设置渲染通道。
    beginInfo.renderArea.extent.width = width; // 设置渲染区域宽度。
    beginInfo.renderArea.extent.height = height; // 设置渲染区域高度。
    beginInfo.framebuffer = fbo; // 设置帧缓冲区。

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE); // 开始渲染通道。
    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f); // 设置视口。
    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0); // 设置剪刀矩形。
    vkCmdSetViewport(cmd, 0, 1, &viewport); // 应用视口设置。
    vkCmdSetScissor(cmd, 0, 1, &scissor); // 应用剪刀矩形设置。
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline); // 绑定图形管线。
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL); // 绑定描述符集。

    model.draw(cmd); // 绘制模型。

    vkCmdEndRenderPass(cmd); // 结束渲染通道。
}

void GenIBLCubeMipPass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    // 设置立方体贴图并更新描述符集。
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo.descriptor), // 绑定 0：统一缓冲区。
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cube->descriptor), // 绑定 1：立方体贴图。
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL); // 更新描述符集。
}

void GenIBLCubeMipPass::PrepareRenderPass()
{
    // 创建 IBL 渲染通道。
    VkAttachmentDescription attDesc = {}; // 初始化附件描述。
    attDesc.format = format; // 设置附件格式。
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 加载时清除。
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 存储附件内容。
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 模板加载不关心。
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 模板存储不关心。
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 初始布局未定义。
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 最终布局为着色器只读。
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // 颜色附件引用。

    VkSubpassDescription subpassDescription = {}; // 初始化子通道描述。
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // 设置为图形管线。
    subpassDescription.colorAttachmentCount = 1; // 一个颜色附件。
    subpassDescription.pColorAttachments = &colorReference; // 指定颜色附件引用。

    std::array<VkSubpassDependency, 2> dependencies; // 定义两个子通道依赖。
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL; // 源为外部子通道。
    dependencies[0].dstSubpass = 0; // 目标为第一个子通道。
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // 源阶段为管道底部。
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // 目标阶段为颜色附件输出。
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT; // 源访问为内存读取。
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // 目标访问为颜色附件读写。
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT; // 按区域依赖。

    dependencies[1].srcSubpass = 0; // 源为第一个子通道。
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL; // 目标为外部子通道。
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // 源阶段为颜色附件输出。
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // 目标阶段为管道底部。
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // 源访问为颜色附件读写。
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT; // 目标访问为内存读取。
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT; // 按区域依赖。

    VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo(); // 初始化渲染通道创建信息。
    renderPassCI.attachmentCount = 1; // 一个附件。
    renderPassCI.pAttachments = &attDesc; // 指定附件描述。
    renderPassCI.subpassCount = 1; // 一个子通道。
    renderPassCI.pSubpasses = &subpassDescription; // 指定子通道描述。
    renderPassCI.dependencyCount = 2; // 两个依赖。
    renderPassCI.pDependencies = dependencies.data(); // 指定依赖数组。

    const uint32_t viewMask = 0b00111111; // 设置视图掩码（6 个面）。
    const uint32_t correlationMask = 0b00111111; // 设置相关掩码。

    VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{}; // 初始化多视图渲染信息。
    renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO; // 设置结构体类型。
    renderPassMultiviewCI.subpassCount = 1; // 一个子通道。
    renderPassMultiviewCI.pViewMasks = &viewMask; // 指定视图掩码。
    renderPassMultiviewCI.correlationMaskCount = 1; // 一个相关掩码。
    renderPassMultiviewCI.pCorrelationMasks = &correlationMask; // 指定相关掩码。

    renderPassCI.pNext = &renderPassMultiviewCI; // 链接多视图信息。

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderpass)); // 创建渲染通道。
}

void GenIBLCubeMipPass::PreparePipeline()
{
    // 创建 IBL 渲染管线。
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // 一个统一缓冲区描述符。
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 }, // 四个图像采样器描述符。
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1); // 初始化描述符池。
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool)); // 创建描述符池。

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0), // 绑定 0：统一缓冲区。
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1), // 绑定 1：立方体贴图。
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings); // 初始化描述符集布局。
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout)); // 创建描述符集布局。

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1); // 初始化描述符集分配。
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet)); // 分配描述符集。

    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1); // 初始化管线布局。
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout)); // 创建管线布局。

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE); // 设置三角形列表拓扑。
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE); // 设置填充模式，无背面剔除。
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE); // 设置颜色混合。
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState); // 设置颜色混合状态。
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL); // 禁用深度和模板测试。
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1); // 设置一个视口和剪刀。
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT); // 禁用多重采样。
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR }; // 启用动态视口和剪刀。
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables); // 设置动态状态。
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages; // 定义两个着色器阶段。

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderpass); // 初始化图形管线创建信息。
    pipelineCI.pInputAssemblyState = &inputAssemblyState; // 设置输入装配状态。
    pipelineCI.pRasterizationState = &rasterizationState; // 设置光栅化状态。
    pipelineCI.pColorBlendState = &colorBlendState; // 设置颜色混合状态。
    pipelineCI.pMultisampleState = &multisampleState; // 设置多重采样状态。
    pipelineCI.pViewportState = &viewportState; // 设置视口状态。
    pipelineCI.pDepthStencilState = &depthStencilState; // 设置深度模板状态。
    pipelineCI.pDynamicState = &dynamicState; // 设置动态状态。
    pipelineCI.stageCount = 2; // 两个着色器阶段。
    pipelineCI.pStages = shaderStages.data(); // 指定着色器阶段数组。
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV }); // 设置顶点输入状态（位置、法线、UV）。

    FeeShader(shaderStages); // 调用子类实现的着色器加载方法。

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline)); // 创建图形管线。
}

void GenIBLCubeMipPass::PrepareFrameBuffer()
{
    // 创建 IBL 帧缓冲区。
    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo(); // 初始化帧缓冲区创建信息。
    framebufferCI.renderPass = renderpass; // 设置渲染通道。
    framebufferCI.attachmentCount = 1; // 一个附件。
    framebufferCI.pAttachments = &view; // 指定图像视图。
    framebufferCI.width = width; // 设置宽度。
    framebufferCI.height = height; // 设置高度。
    framebufferCI.layers = 1; // 设置单层。

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &fbo)); // 创建帧缓冲区。
}

void GenIBLCubeMipPass::PrepareData()
{
    // 准备 IBL 数据（模型视图投影矩阵）。
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f); // 创建透视投影矩阵（90 度 FOV，近裁剪面 0.1，远裁剪面 512）。

    IBLGenUBO uboData = {}; // 初始化 IBL 统一缓冲区对象。

    // ✅ 修复：立方体贴图的六个面的 MVP 矩阵
    // Vulkan 立方体贴图面顺序: +X, -X, +Y, -Y, +Z, -Z
    // 对应的视图矩阵应该从该面的方向看向原点

    // 使用 lookAt 方式生成视图矩阵，更直观且正确
    glm::mat4 views[6] = {
        glm::lookAt(glm::vec3( 1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // +X 面：从右边看
        glm::lookAt(glm::vec3(-1, 0, 0), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // -X 面：从左边看
        glm::lookAt(glm::vec3( 0, 1, 0), glm::vec3(0, 0, 0), glm::vec3(0,  0,  1)), // +Y 面：从上面看
        glm::lookAt(glm::vec3( 0,-1, 0), glm::vec3(0, 0, 0), glm::vec3(0,  0, -1)), // -Y 面：从下面看
        glm::lookAt(glm::vec3( 0, 0, 1), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0)), // +Z 面：从前面看
        glm::lookAt(glm::vec3( 0, 0,-1), glm::vec3(0, 0, 0), glm::vec3(0, -1,  0))  // -Z 面：从后面看
    };

    for (int i = 0; i < 6; ++i) {
        uboData.mvp[i] = project * views[i];
    }

    FeedUBO(uboData); // 调用子类实现的 UBO 数据填充方法。

    memcpy(ubo.mapped, &uboData, sizeof(IBLGenUBO)); // 复制 UBO 数据到缓冲区。
}

void GenIrradianceCubeMip::FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shaderStages)
{
    // 加载辐照度立方体贴图的着色器。
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT); // 加载顶点着色器。
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT); // 加载片段着色器。
}

void GenIrradianceCubeMip::FeedUBO(IBLGenUBO & uboData)
{
    // 设置辐照度立方体贴图的 UBO 数据。
    uboData.deltaPhi = (2.0f * float(M_PI)) / 180.0f; // 设置 phi 角度增量（用于球面采样）。
    uboData.deltaTheta = (0.5f * float(M_PI)) / 64.0f; // 设置 theta 角度增量。
}

void GenPrefilterEnvMapMip::FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shaderStages)
{
    // 加载预过滤环境贴图的着色器。
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT); // 加载顶点着色器。
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT); // 加载片段着色器。
}

void GenPrefilterEnvMapMip::FeedUBO(IBLGenUBO& uboData)
{
    // 设置预过滤环境贴图的 UBO 数据。
    uboData.roughness = roughness; // 设置粗糙度。
    uboData.numSamples = 64; // 设置采样数。
}

GenIBLPass::GenIBLPass(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width)
    : device(device_)
    , iLoader(example)
{
    // 构造函数：初始化 IBL 渲染通道。
    const int32_t dim = static_cast<int32_t>(width); // 设置立方体贴图尺寸。
    const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT; // 设置图像格式为高精度浮点。

    numMips = static_cast<uint32_t>(floor(log2(dim))) + 1; // 计算 Mipmap 级别数。

    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo(); // 初始化图像创建信息。
    imageCI.imageType = VK_IMAGE_TYPE_2D; // 设置图像类型为 2D。
    imageCI.format = format; // 设置图像格式。
    imageCI.extent.width = dim; // 设置宽度。
    imageCI.extent.height = dim; // 设置高度。
    imageCI.extent.depth = 1; // 设置深度为 1。
    imageCI.mipLevels = numMips; // 设置 Mipmap 级别数。
    imageCI.arrayLayers = 6; // 设置 6 层（立方体贴图）。
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL; // 设置最优平铺。
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 设置用途为采样和颜色附件。
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT; // 标记为立方体贴图兼容。

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo(); // 初始化图像视图创建信息。
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE; // 设置视图类型为立方体。
    viewCI.format = format; // 设置格式。
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 设置颜色方面。
    viewCI.subresourceRange.levelCount = numMips; // 设置 Mipmap 级别数。
    viewCI.subresourceRange.layerCount = 6; // 设置 6 层。

    // 创建辐照度立方体贴图。
    {
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &irradianceImage)); // 创建辐照度图像。
        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo(); // 初始化内存分配信息。
        VkMemoryRequirements memReqs; // 初始化内存需求。
        vkGetImageMemoryRequirements(device->logicalDevice, irradianceImage, &memReqs); // 获取内存需求。
        memAlloc.allocationSize = memReqs.size; // 设置分配大小。
        memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // 获取设备本地内存类型。
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &irradianceMemory)); // 分配内存。
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, irradianceImage, irradianceMemory, 0)); // 绑定内存。

        viewCI.image = irradianceImage; // 设置图像为辐照度图像。
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &irradianceView)); // 创建辐照度图像视图。
    }

    // 创建预过滤立方体贴图。
    {
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &prefilteredImage)); // 创建预过滤图像。
        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo(); // 初始化内存分配信息。
        VkMemoryRequirements memReqs; // 初始化内存需求。
        vkGetImageMemoryRequirements(device->logicalDevice, prefilteredImage, &memReqs); // 获取内存需求。
        memAlloc.allocationSize = memReqs.size; // 设置分配大小。
        memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // 获取设备本地内存类型。
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &prefilteredMemory)); // 分配内存。
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, prefilteredImage, prefilteredMemory, 0)); // 绑定内存。

        viewCI.image = prefilteredImage; // 设置图像为预过滤图像。
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &prefilteredView)); // 创建预过滤图像视图。
    }

    irradiance.resize(numMips); // 为辐照度贴图分配 Mipmap 级别。
    prefiltered.resize(numMips); // 为预过滤贴图分配 Mipmap 级别。
    for (uint32_t i = 0; i < numMips; ++i)
    {
        auto w = static_cast<float>(dim * std::pow(0.5f, i)); // 计算当前 Mipmap 级别的宽度。
        auto h = static_cast<float>(dim * std::pow(0.5f, i)); // 计算当前 Mipmap 级别的高度。

        irradiance[i].reset(new GenIrradianceCubeMip(device, example, irradianceImage, format, i, w, h)); // 创建辐照度 Mipmap 生成器。
        irradiance[i]->Prepare(); // 准备辐照度 Mipmap 资源。

        float roughness = (float)i / (float)(numMips - 1); // 计算当前 Mipmap 的粗糙度。
        prefiltered[i].reset(new GenPrefilterEnvMapMip(device, example, prefilteredImage, format, i, w, h, roughness)); // 创建预过滤 Mipmap 生成器。
        prefiltered[i]->Prepare(); // 准备预过滤 Mipmap 资源。
    }
}

GenIBLPass::~GenIBLPass()
{
    // 析构函数：清理 IBL 渲染通道资源。
    if (irradianceImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, irradianceImage, nullptr); // 销毁辐照度图像。
    }

    if (irradianceImage != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, irradianceView, nullptr); // 销毁辐照度图像视图。
    }

    if (irradianceMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, irradianceMemory, nullptr); // 释放辐照度图像内存。
    }

    if (prefilteredImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, prefilteredImage, nullptr); // 销毁预过滤图像。
    }

    if (prefilteredMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, prefilteredMemory, nullptr); // 释放预过滤图像内存。
    }

    if (prefilteredView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, prefilteredView, nullptr); // 销毁预过滤图像视图。
    }
}

void GenIBLPass::SetModel(const std::shared_ptr<vkglTF::Model>&model_)
{
    // 设置渲染模型。
    model = model_; // 存储模型指针。
}

void GenIBLPass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    // 设置立方体贴图并更新所有 Mipmap 级别的贴图。
    cubemap = cube; // 存储立方体贴图。
    for (uint32_t i = 0; i < numMips; ++i)
    {
        irradiance[i]->SetCubeMap(cube); // 为辐照度 Mipmap 设置贴图。
        prefiltered[i]->SetCubeMap(cube); // 为预过滤 Mipmap 设置贴图。
    }
}

void GenIBLPass::Draw(VkCommandBuffer cmd)
{
    // 执行 IBL 渲染。
    for (uint32_t i = 0; i < numMips; ++i)
    {
        irradiance[i]->Draw(cmd, *model); // 绘制辐照度 Mipmap。
        prefiltered[i]->Draw(cmd, *model); // 绘制预过滤 Mipmap。
    }
}

void GenIBLPass::Generate(VkQueue queue)
{
    // 生成 IBL 贴图。
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true); // 创建主命令缓冲区。

    Draw(cmdBuf); // 调用 Draw 方法执行渲染。

    device->flushCommandBuffer(cmdBuf, queue); // 提交并刷新命令缓冲区。
}

void GenIBLPass::FeedIrradianceMap(VkDescriptorImageInfo& descriptor)
{
    // 提供辐照度贴图的描述符信息。
    descriptor.imageView = irradianceView; // 设置图像视图。
    descriptor.sampler = irradiance[0]->GetDefaultSampler(); // 设置采样器。
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 设置图像布局为着色器只读。
}

void GenIBLPass::FeedPrefilteredMap(VkDescriptorImageInfo& descriptor)
{
    // 提供预过滤贴图的描述符信息。
    descriptor.imageView = prefilteredView; // 设置图像视图。
    descriptor.sampler = irradiance[0]->GetDefaultSampler(); // 设置采样器。
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 设置图像布局为着色器只读。
}


RenderAttachment::RenderAttachment(vks::VulkanDevice* device_, VkImageType t, VkFormat fmt, VkImageUsageFlags usage, uint32_t w, uint32_t h, uint32_t l)
    : device(device_)
    , width(w)
    , height(h)
    , layer(l)
    , format(fmt)
    , type(t)
{
    // 创建帧缓冲区，用于 BRDF 查找表渲染。
    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo(); // 初始化图像创建信息。
    imageCI.imageType = type; // 设置图像类型
    imageCI.format = format; // 设置图像格式。
    imageCI.extent.width = width; // 设置图像宽度。
    imageCI.extent.height = height; // 设置图像高度。
    imageCI.extent.depth = 1; // 设置图像深度为 1。
    imageCI.mipLevels = 1; // 设置单级 Mipmap。
    imageCI.arrayLayers = layer; // 设置单层。
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL; // 设置图像平铺为最优。
    imageCI.usage = usage; // 设置图像用途为颜色附件和采样。
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &image)); // 创建图像。

    VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo(); // 初始化内存分配信息。
    VkMemoryRequirements memReqs = {}; // 初始化内存需求。
    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs); // 获取图像内存需求。
    memAlloc.allocationSize = memReqs.size; // 设置分配大小。
    memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); // 获取设备本地内存类型。
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &deviceMemory)); // 分配内存。
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0)); // 绑定图像内存。
}

RenderAttachment::~RenderAttachment()
{
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device->logicalDevice, image, nullptr);
    }
    if (deviceMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
    }
}

DepthStencil::DepthStencil(vks::VulkanDevice* device_, VkFormat format, uint32_t width, uint32_t height, uint32_t layer)
    : RenderAttachment(device_, VK_IMAGE_TYPE_2D, format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, width, height, layer)
{
}

RenderTarget2D::RenderTarget2D(vks::VulkanDevice* device_, VkFormat fmt, uint32_t width, uint32_t height, uint32_t layer)
    : RenderAttachment(device_, VK_IMAGE_TYPE_2D, fmt, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, width, height, layer)
{
}

RenderTargetCube::RenderTargetCube(vks::VulkanDevice* device_, VkFormat fmt, uint32_t width, uint32_t height)
    : RenderTarget2D(device_, fmt, width, height, 6)
{
}

ResourceView::ResourceView(const std::shared_ptr<RenderAttachment>& att, VkImageViewType type, uint32_t firstSlice, uint32_t sliceCount, VkImageAspectFlags flags)
    : attachment(att)
    , viewCI{vks::initializers::imageViewCreateInfo()}
{
    viewCI.viewType = type;
    viewCI.format = attachment->GetFormat();
    viewCI.subresourceRange.aspectMask = flags;
    viewCI.subresourceRange.baseMipLevel = 0;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.baseArrayLayer = firstSlice;
    viewCI.subresourceRange.layerCount = sliceCount;
    viewCI.image = attachment->GetImage();

    CreateView();
}

ResourceView::~ResourceView()
{
    if (attachment && imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(attachment->device->logicalDevice, imageView, nullptr);
    }
}

void ResourceView::CreateView()
{
    VK_CHECK_RESULT(vkCreateImageView(attachment->device->logicalDevice, &viewCI, nullptr, &imageView));

}