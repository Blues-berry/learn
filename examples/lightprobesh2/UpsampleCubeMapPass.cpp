#include "UpsampleCubeMapPass.h"
#include <stdexcept>

// CaptureScenePass::GetCubeMap 实现放到文件底部，保证类定义已知
// =================== CaptureScenePass::GetCubeMap ===================
#include "VulkanTexture.h"
std::shared_ptr<vks::TextureCubeMap> CaptureScenePass::GetCubeMap() const {
    if (!cube) return nullptr;
    return cube->GetTextureCubeMap();
}


CaptureScenePass::CaptureScenePass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkFormat format, uint32_t w, uint32_t h)
    : device(device_)
    , iLoader(example)
    , width(w)
    , height(h)
    , beginInfo(vks::initializers::renderPassBeginInfo())
{
    cube = std::make_shared<RenderTargetCube>(device_, format, w, h);
    depthStencil = std::make_shared<DepthStencil>(device_, VK_FORMAT_D32_SFLOAT, w, h, 6);

    colorView = std::make_shared<ResourceView>(cube, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 6, VK_IMAGE_ASPECT_COLOR_BIT);
    dsView = std::make_shared<ResourceView>(depthStencil, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 0, 6, VK_IMAGE_ASPECT_DEPTH_BIT);

    clearValue.resize(2); // 设置两个清除值（颜色和深度）。

    clearValue[0].color.float32[0] = 0.2f; // 设置颜色清除值（灰色）。
    clearValue[0].color.float32[1] = 0.2f;
    clearValue[0].color.float32[2] = 0.2f;
    clearValue[0].color.float32[3] = 0.f;

    clearValue[1].depthStencil.depth = 1.0; // 设置深度清除值为 1.0。
    clearValue[1].depthStencil.stencil = 0; // 设置模板清除值为 0。

    beginInfo.clearValueCount = 2; // 设置清除值数量。
    beginInfo.pClearValues = clearValue.data(); // 指定清除值数组。

    PrepareFrameBuffer();
    PreparePerPassResource();
}

CaptureScenePass::~CaptureScenePass()
{
    globalBuffer.destroy();

    if (cubeSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device->logicalDevice, cubeSampler, nullptr);
        cubeSampler = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device->logicalDevice, renderPass, nullptr);
    }

    if (framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device->logicalDevice, framebuffer, nullptr);
    }

    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr); // 销毁描述符集布局。
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr); // 销毁描述符池。
        descriptorPool = VK_NULL_HANDLE;
    }
}

void CaptureScenePass::PrepareFrameBuffer()
{
    VkAttachmentDescription attachments[2] = {};

    // 创建渲染通道。
    VkAttachmentDescription &attDesc = attachments[0]; // 初始化附件描述。
    attDesc.format = cube->GetFormat(); // 设置附件格式。
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 加载时清除附件。
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 存储附件内容。
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 模板加载不关心。
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 模板存储不关心。
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 初始布局未定义。
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 最终布局为着色器只读。
    VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }; // 颜色附件引用。

    VkAttachmentDescription &dsDesc = attachments[1]; // 初始化附件描述。
    dsDesc.format = depthStencil->GetFormat(); // 设置附件格式。
    dsDesc.samples = VK_SAMPLE_COUNT_1_BIT; // 设置单采样。
    dsDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 加载时清除附件。
    dsDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // 存储附件内容。
    dsDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // 模板加载不关心。
    dsDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 模板存储不关心。
    dsDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // 初始布局未定义。
    dsDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL; // 最终布局为着色器只读。
    VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL }; // 颜色附件引用。

    VkSubpassDescription subpassDescription = {}; // 初始化子通道描述。
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // 设置为图形管线。
    subpassDescription.colorAttachmentCount = 1; // 一个颜色附件。
    subpassDescription.pColorAttachments = &colorReference; // 指定颜色附件引用。
    subpassDescription.pDepthStencilAttachment = &depthReference;

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
    renderPassCI.attachmentCount = 2; // 一个附件。
    renderPassCI.pAttachments = attachments; // 指定附件描述。
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

    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassCI, nullptr, &renderPass)); // 创建渲染通道并检查结果。

    std::vector<VkImageView> views = {
        colorView->GetView(),
        dsView->GetView()
    };

    // 创建 IBL 帧缓冲区。
    VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo(); // 初始化帧缓冲区创建信息。
    framebufferCI.renderPass = renderPass; // 设置渲染通道。
    framebufferCI.attachmentCount = static_cast<uint32_t>(views.size()); // 一个附件。
    framebufferCI.pAttachments = views.data(); // 指定图像视图。
    framebufferCI.width = width; // 设置宽度。
    framebufferCI.height = height; // 设置高度。
    framebufferCI.layers = 6; // 设置单层。

    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &framebufferCI, nullptr, &framebuffer)); // 创建帧缓冲区。

    // 为采样创建 CUBE 视图（供后续 IBL/其他Pass 采样使用）
    cubeSampleView = std::make_shared<ResourceView>(cube, VK_IMAGE_VIEW_TYPE_CUBE, 0, 6, VK_IMAGE_ASPECT_COLOR_BIT);
}

void CaptureScenePass::PreparePerPassResource()
{
    // 准备主渲染通道的资源。
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, // 两个统一缓冲区描述符。
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1); // 初始化描述符池。
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool)); // 创建描述符池。

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
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

    UpdateBindings();

    // 创建默认采样器（线性过滤，clamp）
    VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 0.0f;
    VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerCI, nullptr, &cubeSampler));
}

void CaptureScenePass::UpdateGlobal(const GlobalUbo& ubo)
{
    // 更新全局统一缓冲区对象（UBO）数据。
    memcpy(globalBuffer.mapped, &ubo, sizeof(GlobalUbo)); // 复制 UBO 数据到缓冲区。
}

void CaptureScenePass::Draw(VkCommandBuffer cmd, std::function<void(VkCommandBuffer)>&& encoder)
{
    // 执行主渲染通道的绘制。
    beginInfo.renderArea.extent.width = width; // 设置渲染区域宽度。
    beginInfo.renderArea.extent.height = height; // 设置渲染区域高度。
    beginInfo.renderPass = renderPass;
    beginInfo.framebuffer = framebuffer; // 设置帧缓冲区。

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE); // 开始渲染通道。

    VkViewport viewport = vks::initializers::viewport((float)beginInfo.renderArea.extent.width, (float)beginInfo.renderArea.extent.height, 0.0f, 1.0f); // 设置视口。
    vkCmdSetViewport(cmd, 0, 1, &viewport); // 应用视口设置。

    VkRect2D scissor = vks::initializers::rect2D(static_cast<int32_t>(viewport.width), static_cast<int32_t>(viewport.height), 0, 0); // 设置剪刀矩形。
    vkCmdSetScissor(cmd, 0, 1, &scissor); // 应用剪刀矩形设置。

    encoder(cmd); // 调用用户提供的编码函数执行绘制。

    vkCmdEndRenderPass(cmd); // 结束渲染通道。
}

void CaptureScenePass::UpdateBindings()
{
    // 更新描述符集绑定。
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &globalBuffer.descriptor), // 绑定 0：全局 UBO。
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL); // 更新描述符集。
}

void CaptureScenePass::FeedCubeDescriptor(VkDescriptorImageInfo& descriptor)
{
    descriptor.sampler = cubeSampler;
    descriptor.imageView = cubeSampleView ? cubeSampleView->GetView() : VK_NULL_HANDLE;
    descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

UpsampleCubeMapPass::UpsampleCubeMapPass(vks::VulkanDevice* device_, IExampleInterfasce* example)
    : ComputePass(device_, example), lowResWidth(0), lowResHeight(0), highResWidth(0), highResHeight(0)
{
    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }, // 低分辨率cubemap
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }           // 高分辨率cubemap
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1)
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    // 创建管线布局
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 创建计算管线
    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = iLoader->LoadShader("lightprobesh2/upsample_cubemap.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &pipeline));
}

UpsampleCubeMapPass::~UpsampleCubeMapPass() {
    // 清理在基类中已处理
}

void UpsampleCubeMapPass::SetCubeMaps(const std::shared_ptr<vks::TextureCubeMap>& lowResCube, 
                                      const std::shared_ptr<vks::TextureCubeMap>& highResCube) {
    lowResCubemap = lowResCube;
    highResCubemap = highResCube;

    // 更新描述符集
    VkDescriptorImageInfo lowResInfo = lowResCubemap->descriptor;
    lowResInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDescriptorImageInfo highResInfo = {};
    highResInfo.imageView = highResCubemap->view;
    highResInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &lowResInfo),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &highResInfo)
    };
    vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void UpsampleCubeMapPass::Generate(VkQueue queue, uint32_t lowWidth, uint32_t lowHeight, uint32_t highWidth, uint32_t highHeight) {
    lowResWidth = lowWidth;
    lowResHeight = lowHeight;
    highResWidth = highWidth;
    highResHeight = highHeight;

    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // 布局转换
    VkImageMemoryBarrier barrier = vks::initializers::imageMemoryBarrier();
    barrier.image = lowResCubemap->image;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    barrier.image = highResCubemap->image;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    Draw(cmdBuf);

    // 转换高分辨率cubemap到采样布局
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 创建栅栏用于同步
    VkFence fence;
    VkFenceCreateInfo fenceInfo = vks::initializers::fenceCreateInfo();
    vkCreateFence(device->logicalDevice, &fenceInfo, nullptr, &fence);
    
    // 提交命令缓冲区（使用设备特定的flushCommandBuffer方法）
    device->flushCommandBuffer(cmdBuf, queue);
    
    // 提交一个空的提交信息到队列，附加栅栏，用于等待命令缓冲区完成
    VkSubmitInfo submitInfo = vks::initializers::submitInfo();
    submitInfo.commandBufferCount = 0;  // 不提交新的命令缓冲区
    submitInfo.pCommandBuffers = nullptr;
    
    vkQueueSubmit(queue, 1, &submitInfo, fence);
    vkWaitForFences(device->logicalDevice, 1, &fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(device->logicalDevice, fence, nullptr);
}


void UpsampleCubeMapPass::Dispatch(VkCommandBuffer cmd) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    // 假设工作组大小为16x16，覆盖高分辨率cubemap的6个面
    uint32_t groupCountX = (highResWidth + 15) / 16;
    uint32_t groupCountY = (highResHeight + 15) / 16;
    vkCmdDispatch(cmd, groupCountX, groupCountY, 6);
}