#include "Pass.h"
#include "glm/gtc/matrix_transform.hpp"
#include <array>

/**
 * @brief ComputePass类的构造函数
 * @param device_ 指向VulkanDevice对象的指针，用于管理Vulkan设备相关资源
 * @param example 指向IExampleInterface接口的指针，提供示例功能的访问接口
 */
ComputePass::ComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example) : device(device_), iLoader(example) // 使用成员初始化列表初始化device和iLoader成员变量
{
}

ComputePass::~ComputePass()
{
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    }

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
    }
}

/**
 * @brief ComputePass类的Draw方法，用于执行计算着色器的绘制命令
 * 
 * @param cmd VkCommand类型的命令缓冲区，用于记录命令
 */
void ComputePass::Draw(VkCommandBuffer cmd)
{
    // 调用Dispatch方法，将命令缓冲区作为参数传递
    Dispatch(cmd);
}

FullScreenPass::FullScreenPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkFormat format_) : device(device_), iLoader(example), format(format_)
{
    clearValue.color.float32[0] = 0.f;
    clearValue.color.float32[1] = 0.f;
    clearValue.color.float32[2] = 0.f;
    clearValue.color.float32[3] = 0.f;

    beginInfo = vks::initializers::renderPassBeginInfo();
    beginInfo.clearValueCount = 1;
    beginInfo.pClearValues = &clearValue;

    GenerateSampler();
}

/**
 * 全屏渲染通道的析构函数
 * 用于释放和清理与全屏渲染相关的所有 Vulkan 资源
 * 包括采样器、管线、图像、图像视图、内存、渲染通道、帧缓冲区、描述符集合布局、管线布局和描述符池等
 */
FullScreenPass::~FullScreenPass()
{
    // 检查并销毁采样器对象
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device->logicalDevice, sampler, nullptr);
    }

    // 检查并销毁图形管线对象
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    }

    // 检查并销毁图像对象
    if (image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, image, nullptr);
    }

    // 检查并销毁图像视图对象
    if (view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, view, nullptr);
    }

    // 检查并释放设备内存
    if (deviceMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, deviceMemory, nullptr);
    }

    // 检查并销毁渲染通道对象
    if (renderpass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(device->logicalDevice, renderpass, nullptr);
    }

    // 检查并销毁帧缓冲区对象
    if (fbo != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(device->logicalDevice, fbo, nullptr);
    }

    // 检查并销毁描述符集合布局对象
    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    }

    // 检查并销毁管线布局对象
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
    }

    // 检查并销毁描述符池对象
    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
    }
}

// 生成全屏渲染通道所需的采样器对象
void FullScreenPass::GenerateSampler()
{
    // 初始化采样器创建信息结构体
    VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
    // 设置放大和缩小过滤方式为线性过滤
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    // 设置mipmap模式为线性
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    // 设置U、V、W方向的寻址模式为边缘裁剪
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    // 设置最小和最大LOD
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    // 设置边界颜色为不透明白色
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    // 创建采样器对象，并检查结果
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCI, nullptr, &sampler));
}

/**
 * @brief 准备全屏渲染通道所需的所有资源
 * 
 * 该函数用于初始化全屏渲染所需的各个组件，包括渲染通道、管线、帧缓冲和数据。
 */
void FullScreenPass::Prepare()
{
    // 准备渲染通道，设置渲染目标等
    PrepareRenderPass();
    // 准备渲染管线，包括着色器、状态设置等
    PreparePipeline();
    // 准备帧缓冲，用于渲染目标
    PrepareFrameBuffer();
    // 准备渲染所需的数据，如顶点数据、纹理等
    PrepareData();
}

void FullScreenPass::FeedDescriptor(VkDescriptorImageInfo& descriptor)
{
    descriptor.sampler = sampler;
    descriptor.imageView = view;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void FullScreenPass::Draw(VkCommandBuffer cmd)
{
    if (pipeline == VK_NULL_HANDLE)
    {
        return;
    }

    beginInfo.renderPass = renderpass;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.framebuffer = fbo;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

/**
 * @brief 准备渲染通道(Render Pass)的函数
 * 该函数用于创建和配置Vulkan渲染通道，包括附件描述、子通道描述和依赖关系
 */
/**
 * @brief 准备渲染通道(Render Pass)的函数
 * 该函数用于创建一个完整的渲染通道，包括附件描述、子通道描述和依赖关系
 */
void FullScreenPass::PrepareRenderPass()
{
    // FB, Att, RP, Pipe, etc. (帧缓冲、附件、渲染通道、管道等的缩写)
    VkAttachmentDescription attDesc = {};  // Vulkan附件描述结构体，用于定义渲染通道的附件属性
    // Color attachment - 颜色附件设置
    attDesc.format = format;               // 设置附件的图像格式
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;  // 设置采样数量为1，表示不使用多重采样
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // 设置加载操作为清除操作
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // 设置存储操作为存储操作
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // 模板加载操作设为不关心
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 模板存储操作设为不关心
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 初始图像布局设为未定义
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // 最终图像布局设为着色器读取最优
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };  // 颜色附件引用，索引为0，布局为颜色附件最优

    VkSubpassDescription subpassDescription = {};  // 子通道描述结构体，用于定义渲染通道中的子通道
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 管道绑定点设置为图形管道
    subpassDescription.colorAttachmentCount = 1;  // 颜色附件数量为1
    subpassDescription.pColorAttachments = &colorReference;  // 指向颜色附件引用的指针

    // Use subpass dependencies for layout transitions - 使用子通道依赖进行布局转换
    std::array<VkSubpassDependency, 2> dependencies;  // 创建一个包含两个子通道依赖的数组

    // 第一个依赖：从外部子通道到我们子通道的转换
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;  // 源子通道为外部
    dependencies[0].dstSubpass = 0;  // 目标子通道为第一个子通道
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // 源阶段为管道底部阶段
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // 目标阶段为颜色附件输出阶段
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;  // 源访问掩码为内存读取
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // 目标访问掩码为颜色附件读写
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;  // 依赖标志为按区域依赖



    // 第二个依赖：从我们的子通道到外部子通道的转换
    dependencies[1].srcSubpass = 0;  // 源子通道为第一个子通道
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;  // 目标子通道为外部
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // 源阶段为颜色附件输出阶段
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;  // 目标阶段为管道底部阶段
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;  // 源访问掩码为颜色附件读写
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;  // 目标访问掩码为内存读取
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;  // 依赖标志为按区域依赖

    // Create the actual renderpass - 创建实际的渲染通道
    VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();  // 创建渲染通道创建信息结构体
    renderPassCI.attachmentCount = 1;  // 附件数量为1
    renderPassCI.pAttachments = &attDesc;  // 指向附件描述的指针
    renderPassCI.subpassCount = 1;  // 子通道数量为1
    renderPassCI.pSubpasses = &subpassDescription;  // 指向子通道描述的指针
    renderPassCI.dependencyCount = 2;  // 依赖数量为2
    renderPassCI.pDependencies = dependencies.data();  // 指向依赖数组的指针

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderpass));  // 创建渲染通道并检查结果
}

GenBRDFLutPass::GenBRDFLutPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : FullScreenPass(device_, example, VK_FORMAT_R16G16_SFLOAT)
{
    width = 512;
    height = 512;
}

GenBRDFLutPass::~GenBRDFLutPass()
{

}

/**
 * 准备渲染管线
 * 该函数用于创建并配置BRDF查找表(LUT)的渲染管线
 */
void GenBRDFLutPass::PreparePipeline()
{
    // 创建管线布局，不使用任何描述符集
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(nullptr, 0);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 配置输入装配状态，使用三角形图元
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    // 配置光栅化状态，填充多边形，禁用背面剔除，使用逆时针为正面
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    // 配置颜色混合附件状态，启用所有颜色通道
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    // 配置颜色混合状态，使用单个附件
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    // 配置深度模板状态，禁用深度测试和模板测试
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    // 配视口状态，使用单个视口和剪刀
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
    // 配置多重采样状态，禁用多重采样
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    // 配置动态状态，启用视口和剪刀的动态设置
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    // 配置顶点输入状态，不使用顶点数据
    VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    // 创建着色器阶段数组，包含顶点和片段着色器
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    // 创建图形管线信息
    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderpass);
    // 设置管线各个状态
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    // Look-up-table (from BRDF) pipeline
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
}

void GenBRDFLutPass::PrepareFrameBuffer()
{
    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = width;
    imageCI.extent.height = height;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &image));


    VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
    VkMemoryRequirements memReqs = {};
    vkGetImageMemoryRequirements(device->logicalDevice, image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &deviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, image, deviceMemory, 0));


    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.layerCount = 1;
    viewCI.image = image;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &view));


    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &view;
    framebufferCI.width = width;
    framebufferCI.height = height;
    framebufferCI.layers = 1;

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &fbo));
}


GenSHComputePass::GenSHComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example)
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    VkPipelineLayoutCreateInfo pplInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pplInfo, nullptr, &pipelineLayout));

    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/sh_compute.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &shCoeffBuffer,
        sizeof(SHCoefficients));
}
   
GenSHComputePass::~GenSHComputePass()
{
    shCoeffBuffer.destroy();
}

void GenSHComputePass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    cubemap = cube;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &cubemap->descriptor),
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &shCoeffBuffer.descriptor),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GenSHComputePass::FeedSH(VkDescriptorBufferInfo& descriptor)
{
    descriptor = shCoeffBuffer.descriptor;
}

void GenSHComputePass::Generate(VkQueue queue)
{
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Draw(cmdBuf);

    device->flushCommandBuffer(cmdBuf, queue);
}

void GenSHComputePass::Dispatch(VkCommandBuffer cmd)
{
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDispatch(cmd, 1, 1, 1);
}

MainPass::MainPass(vks::VulkanDevice* device_)
    : device(device_)
    , beginInfo(vks::initializers::renderPassBeginInfo())
{
    clearValue.resize(2);

    clearValue[0].color.float32[0] = 0.2f;
    clearValue[0].color.float32[1] = 0.2f;
    clearValue[0].color.float32[2] = 0.2f;
    clearValue[0].color.float32[3] = 0.f;

    clearValue[1].depthStencil.depth = 1.0;
    clearValue[1].depthStencil.stencil = 0;

    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues = clearValue.data();

    PreparePerPassResource();
}

MainPass::~MainPass()
{
    globalBuffer.unmap();
    globalBuffer.destroy();

    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
}

void MainPass::SetUp(VkRenderPass pass)
{
    beginInfo.renderPass = pass;
}

void MainPass::UpdateGlobal(const GlobalUbo& ubo)
{
    memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo));
}

void MainPass::Draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width, uint32_t height, std::function<void(VkCommandBuffer)> &&encoder)
{
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.framebuffer = framebuffer;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = vks::initializers::viewport((float)beginInfo.renderArea.extent.width, (float)beginInfo.renderArea.extent.height, 0.0f, 1.0f);
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = vks::initializers::rect2D(viewport.width, viewport.height, 0, 0);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    encoder(cmd);

    vkCmdEndRenderPass(cmd);
}

void MainPass::UpdateBinngs()
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalBuffer.descriptor),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &environmemts.shCoeffs),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &environmemts.brdfView),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &environmemts.irradianceCube),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &environmemts.prefilteredCube),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void MainPass::PreparePerPassResource()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &globalBuffer,
        sizeof(GlobalUbo));
    globalBuffer.map();
}


GenIBLCubeMipPass::GenIBLCubeMipPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap_, VkFormat format, uint32_t mip_, uint32_t width_, uint32_t height_)
    : FullScreenPass(device_, example, format)
    , cubemap(cubemap_)
    , mipmap(mip_)
{
    width = width_;
    height = height_;

    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &ubo,
        sizeof(IBLGenUBO));
    ubo.map();

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.baseMipLevel = mipmap;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.baseArrayLayer = 0;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = cubemap;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &view));
}

GenIBLCubeMipPass::~GenIBLCubeMipPass()
{
    ubo.destroy();
}

void GenIBLCubeMipPass::Draw(VkCommandBuffer cmd, vkglTF::Model& model)
{
    if (pipeline == VK_NULL_HANDLE)
    {
        return;
    }

    beginInfo.renderPass = renderpass;
    beginInfo.renderArea.extent.width = width;
    beginInfo.renderArea.extent.height = height;
    beginInfo.framebuffer = fbo;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
    
    model.draw(cmd);

    vkCmdEndRenderPass(cmd);
}

void GenIBLCubeMipPass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo.descriptor),
    vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cube->descriptor),
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void GenIBLCubeMipPass::PrepareRenderPass()
{
    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    const uint32_t viewMask = 0b00111111;
    const uint32_t correlationMask = 0b00111111;

    VkRenderPassMultiviewCreateInfo renderPassMultiviewCI{};
    renderPassMultiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
    renderPassMultiviewCI.subpassCount = 1;
    renderPassMultiviewCI.pViewMasks = &viewMask;
    renderPassMultiviewCI.correlationMaskCount = 1;
    renderPassMultiviewCI.pCorrelationMasks = &correlationMask;

    renderPassCI.pNext = &renderPassMultiviewCI;

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderpass));
}

void GenIBLCubeMipPass::PreparePipeline()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

    FeeShader(shaderStages);

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
}

void GenIBLCubeMipPass::PrepareFrameBuffer()
{
    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &view;
    framebufferCI.width = width;
    framebufferCI.height = height;
    framebufferCI.layers = 1;

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &fbo));
}

void GenIBLCubeMipPass::PrepareData()
{
    auto project = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f);

    IBLGenUBO uboData = {};
    // POSITIVE_X
    uboData.mvp[0] = project * glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // NEGATIVE_X
    uboData.mvp[1] = project * glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // POSITIVE_Y
    uboData.mvp[2] = project * glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // NEGATIVE_Y
    uboData.mvp[3] = project * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // POSITIVE_Z
    uboData.mvp[4] = project * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    // NEGATIVE_Z
    uboData.mvp[5] = project * glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    FeedUBO(uboData);

    memcpy(ubo.mapped, &uboData, sizeof(IBLGenUBO));
}

void GenIrradianceCubeMip::FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shaderStages)
{
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
}

void GenIrradianceCubeMip::FeedUBO(IBLGenUBO & uboData)
{
    uboData.deltaPhi = (2.0f * float(M_PI)) / 180.0f;
    uboData.deltaTheta = (0.5f * float(M_PI)) / 64.0f;
}

void GenPrefilterEnvMapMip::FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shaderStages)
{
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
}

void GenPrefilterEnvMapMip::FeedUBO(IBLGenUBO& uboData)
{
    uboData.roughness = roughness;
    uboData.numSamples = 64;
}

GenIBLPass::GenIBLPass(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width)
    : device(device_)
    , iLoader(example)
{
    const int32_t dim = static_cast<int32_t>(width);
    const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;

    {
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &irradianceImage));
        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device->logicalDevice, irradianceImage, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &irradianceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, irradianceImage, irradianceMemory, 0));

        viewCI.image = irradianceImage;
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &irradianceView));
    }

    {
        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &imageCI, nullptr, &prefilteredImage));
        VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device->logicalDevice, prefilteredImage, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAlloc, nullptr, &prefilteredMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, prefilteredImage, prefilteredMemory, 0));

        viewCI.image = prefilteredImage;
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &viewCI, nullptr, &prefilteredView));
    }


    irradiance.resize(numMips);
    prefiltered.resize(numMips);
    for (uint32_t i = 0; i < numMips; ++i)
    {
        auto w = static_cast<float>(dim * std::pow(0.5f, i));
        auto h = static_cast<float>(dim * std::pow(0.5f, i));

        irradiance[i].reset(new GenIrradianceCubeMip(device, example, irradianceImage, format, i, w, h));
        irradiance[i]->Prepare();

        float roughness = (float)i / (float)(numMips - 1);
        prefiltered[i].reset(new GenPrefilterEnvMapMip(device, example, prefilteredImage, format, i, w, h, roughness));
        prefiltered[i]->Prepare();
    }
}

GenIBLPass::~GenIBLPass()
{
    if (irradianceImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, irradianceImage, nullptr);
    }

    if (irradianceImage != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, irradianceView, nullptr);
    }

    if (irradianceMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, irradianceMemory, nullptr);
    }

    if (prefilteredImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device->logicalDevice, prefilteredImage, nullptr);
    }

    if (prefilteredMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device->logicalDevice, prefilteredMemory, nullptr);
    }

    if (prefilteredView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device->logicalDevice, prefilteredView, nullptr);
    }
}

void GenIBLPass::SetModel(const std::shared_ptr<vkglTF::Model>&model_)
{
    model = model_;
}

void GenIBLPass::SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube)
{
    cubemap = cube;
    for (uint32_t i = 0; i < numMips; ++i)
    {
        irradiance[i]->SetCubeMap(cube);
        prefiltered[i]->SetCubeMap(cube);
    }
}

void GenIBLPass::Draw(VkCommandBuffer cmd)
{
    for (uint32_t i = 0; i < numMips; ++i)
    {
        irradiance[i]->Draw(cmd, *model);
        prefiltered[i]->Draw(cmd, *model);
    }
}

void GenIBLPass::Generate(VkQueue queue)
{
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    Draw(cmdBuf);

    device->flushCommandBuffer(cmdBuf, queue);
}

void GenIBLPass::FeedIrradianceMap(VkDescriptorImageInfo& descriptor)
{
    descriptor.imageView = irradianceView;
    descriptor.sampler = irradiance[0]->GetDefaultSampler();
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void GenIBLPass::FeedPrefilteredMap(VkDescriptorImageInfo& descriptor)
{
    descriptor.imageView = prefilteredView;
    descriptor.sampler = irradiance[0]->GetDefaultSampler();
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}