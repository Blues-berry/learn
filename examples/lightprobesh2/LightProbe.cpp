#include "LightProbe.h"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <stdexcept>



LightProbe::LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width_, uint32_t height_)
    : device(device_), iLoader(example), width(width_), height(height_)
{
}
LightProbe::~LightProbe() {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    }
    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
    }
    if (depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(device->logicalDevice, depthImage, nullptr);
    }
    if (depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(device->logicalDevice, depthView, nullptr);
    }
    if (depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device->logicalDevice, depthMemory, nullptr);
    }
    if (cubemap) {
        if (cubemap->image != VK_NULL_HANDLE) {
            vkDestroyImage(device->logicalDevice, cubemap->image, nullptr);
        }
        if (cubemap->view != VK_NULL_HANDLE) {
            vkDestroyImageView(device->logicalDevice, cubemap->view, nullptr);
        }
        if (cubemap->deviceMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device->logicalDevice, cubemap->deviceMemory, nullptr);
        }
        if (cubemap->sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device->logicalDevice, cubemap->sampler, nullptr);
        }
    }
    uboBuffer.destroy();
}
void LightProbe::SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_) {
    cubemap = cubemap_;
    // UpdateBindings(); // 更新描述符集
}

void LightProbe::prepare() {
    // 创建描述符池
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },// 一个统一缓冲区描述符。
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } // 一个图像采样器描述符。
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 创建描述符集布局
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);// 初始化描述符集布局。
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));// 创建描述符集布局。

    // 分配描述符集
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));

    // 创建UBO缓冲区
    device->createBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,// 创建统一缓冲区。
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,// 主机可见且一致性内存。
        &uboBuffer,
        sizeof(UBO));
    uboBuffer.map();// 映射缓冲区。
}
void LightProbe::UpdateBindings()
{
    if (!cubemap || !cubemap->descriptor.sampler || !cubemap->descriptor.imageView|| !cubemap->descriptor.imageLayout) {
            throw std::runtime_error("Cubemap texture not properly initialized!");
        }
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uboBuffer.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &cubemap->descriptor)
        };
        vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
void LightProbe::updateUBO(const UBO& ubo) {
    memcpy(uboBuffer.mapped, &ubo, sizeof(UBO));
}

void LightProbe::drawScene(VkCommandBuffer cmdBuf) {
    if (model) {
        model->draw(cmdBuf);
    }
}

void LightProbe::CaptureCubeMap(VkFormat format, VkQueue queue) {
    if (!device || !device->logicalDevice || !queue) {
        throw std::runtime_error("Invalid device or queue pointer");
    }

    // --- 创建低分辨率立方体贴图（128x128） ---
    std::shared_ptr<vks::TextureCubeMap> lowResCubemap = std::make_shared<vks::TextureCubeMap>();

    VkImageCreateInfo lowResImageInfo = vks::initializers::imageCreateInfo();
    lowResImageInfo.imageType = VK_IMAGE_TYPE_2D;// 设置图像类型为 2D。
    lowResImageInfo.format = format;// 设置图像格式。
    lowResImageInfo.extent.width = lowReswidth;// 设置宽度。
    lowResImageInfo.extent.height = lowResheight;// 设置高度。
    lowResImageInfo.extent.depth = 1;// 设置深度为 1
    lowResImageInfo.mipLevels = 1;// 设置 Mipmap 级别数。
    lowResImageInfo.arrayLayers = 6;// 设置 6 层（立方体贴图）。
    lowResImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;// 设置单采样。
    lowResImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;// 设置最优平铺。
    lowResImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;// 设置用途为采样和颜色附件。
    lowResImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;// 标记为立方体贴图兼容。

    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &lowResImageInfo, nullptr, &lowResCubemap->image));

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device->logicalDevice, lowResCubemap->image, &memReqs);
    VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
    memAllocInfo.allocationSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &lowResCubemap->deviceMemory));
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, lowResCubemap->image, lowResCubemap->deviceMemory, 0));

    VkImageViewCreateInfo lowResViewInfo = vks::initializers::imageViewCreateInfo();; // 初始化图像视图创建信息。
    lowResViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE; // 设置视图类型为立方体。
    lowResViewInfo.format = format;// 设置格式。
    lowResViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };// 设置颜色方面。// 设置 Mipmap 级别数。// 设置 6 层。
    lowResViewInfo.image = lowResCubemap->image;
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &lowResViewInfo, nullptr, &lowResCubemap->view));// 创建图像视图。

    // --- 创建高分辨率立方体贴图 ---

    if (!cubemap) {
        cubemap = std::make_shared<vks::TextureCubeMap>();
        VkImageCreateInfo highResImageInfo = vks::initializers::imageCreateInfo();
        highResImageInfo.imageType = VK_IMAGE_TYPE_2D;
        highResImageInfo.format = format;
        highResImageInfo.extent.width = width;
        highResImageInfo.extent.height = height;
        highResImageInfo.extent.depth = 1;
        highResImageInfo.mipLevels = 1;
        highResImageInfo.arrayLayers = 6;
        highResImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        highResImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        highResImageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        highResImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

        VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &highResImageInfo, nullptr, &cubemap->image));

        vkGetImageMemoryRequirements(device->logicalDevice, cubemap->image, &memReqs);
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &cubemap->deviceMemory));
        VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, cubemap->image, cubemap->deviceMemory, 0));

        VkImageViewCreateInfo highResViewInfo = vks::initializers::imageViewCreateInfo();
        highResViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        highResViewInfo.format = format;
        highResViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
        highResViewInfo.image = cubemap->image;
        VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &highResViewInfo, nullptr, &cubemap->view));// 创建图像视图。

        VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();// 初始化采样器创建信息。
        samplerInfo.magFilter = VK_FILTER_LINEAR;// 设置放大过滤为线性。
        samplerInfo.minFilter = VK_FILTER_LINEAR;// 设置缩小过滤为线性。
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR; // 设置 Mipmap 模式为线性。
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 寻址模式为边缘裁剪。
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;// 设置边界颜色为不透明白色。
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &cubemap->sampler));// 创建采样器并检查结果。

        // 设置描述符图像信息，用于着色器访问纹理。
        cubemap->descriptor.sampler = cubemap->sampler;// 指定采样器。
        cubemap->descriptor.imageView = cubemap->view;// 指定图像视图。
        cubemap->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// 设置图像布局为着色器只读。
    }

    lowResCubemap->descriptor.sampler = cubemap->sampler;
    lowResCubemap->descriptor.imageView = lowResCubemap->view;
    lowResCubemap->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    UpdateBindings(); // 更新描述符集

    // --- 创建渲染通行证（支持多视图和深度缓冲） ---
    std::vector<VkAttachmentDescription> attachmentDescriptions(2);
    // 颜色附件
    attachmentDescriptions[0].format = format;
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;// 加载时清除。
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;// 存储附件内容。
    attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;// 模板加载不关心。
    attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;// 模板存储不关心。
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// 初始布局未定义。
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;// 最终图像布局：优化为颜色附件布局，适合后续作为渲染目标或采样。

    // 深度附件
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;//深度附件使用 32 位浮点深度格式，适合高精度深度测试
    attachmentDescriptions[1].format = depthFormat;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// 初始布局未定义。
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;//渲染完成后，深度缓冲保持在适合深度/模板附件的布局，优化后续深度测试。
    // 定义附件引用：用于子通行证中指定哪些附件参与渲染，以及它们的布局。
    VkAttachmentReference colorAttachmentRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };// 颜色附件引用：附件索引0，在子通行证中使用颜色附件最优布局。
    VkAttachmentReference depthAttachmentRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };// 深度附件引用：附件索引1，在子通行证中使用深度/模板附件最优布局。
    // 定义子通行证：渲染通行证的一个阶段，这里只有一个子通行证，用于实际的图形渲染。
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;// 绑定点：图形管线（用于绘制命令，非计算管线）。
    subpass.colorAttachmentCount = 1;  // 颜色附件数量：1个。
    subpass.pColorAttachments = &colorAttachmentRef;  // 指针到颜色附件数组。
    subpass.pDepthStencilAttachment = &depthAttachmentRef;  // 指针到深度/模板附件。

    std::array<VkSubpassDependency, 2> dependencies;// 创建一个包含2个依赖的数组。
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;// 源子通行证：外部。
    dependencies[0].dstSubpass = 0;// 目标子通行证：0（第一个子通行证）。
    // srcAccess：规定了上一个SubPass的哪一个Stage的哪一个操作完成后才能执行。
    // 源管线阶段：管线底部（所有先前操作完成）。
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    // 目标管线阶段：颜色输出和早期片段测试（包括深度测试）。
    // dstAccess：规定了下一个SubPass阻塞在哪一个Stage的哪一个操作后。
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;// 源访问掩码：内存读取（外部可能读取附件）。
    // 目标访问掩码：写入颜色和深度附件。
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;// 依赖标志：按区域依赖（仅同步像素区域，提高并行性）。
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    // 源管线阶段：颜色输出和晚期片段测试（渲染完成）。
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;// 依赖标志：按区域依赖（仅同步像素区域，提高并行性）。
    // 创建渲染通行证对象。
    // 使用初始化器创建渲染通行证信息结构体。
    VkRenderPassCreateInfo renderPassInfo = vks::initializers::renderPassCreateInfo();
    renderPassInfo.attachmentCount = 2;  // 附件数量：2。
    renderPassInfo.pAttachments = attachmentDescriptions.data();  // 指针到附件数组。
    renderPassInfo.subpassCount = 1;  // 子通行证数量：1。
    renderPassInfo.pSubpasses = &subpass;  // 指针到子通行证数组。
    renderPassInfo.dependencyCount = 2;  // 依赖数量：2。
    renderPassInfo.pDependencies = dependencies.data();  // 指针到依赖数组。

    // 多视图支持：使用VK_KHR_multiview扩展，一次渲染生成多个视图（这里为6个立方体面）。
    const uint32_t viewMask = 0b00111111; // 视图掩码：二进制0b00111111（前6位为1），表示渲染到6个视图（立方体贴图的6个面）。
    const uint32_t correlationMask = 0b00111111;// 相关性掩码：指定视图间的相关性（这里全相关，确保所有视图同步）。
    VkRenderPassMultiviewCreateInfo multiviewCI = {};  // 初始化多视图创建信息。
    multiviewCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;  // 结构体类型：多视图渲染通行证。
    multiviewCI.subpassCount = 1;  // 子通行证数量：1。
    multiviewCI.pViewMasks = &viewMask;  // 指针到视图掩码数组。
    multiviewCI.correlationMaskCount = 1;  // 相关性掩码数量：1。
    multiviewCI.pCorrelationMasks = &correlationMask;  // 指针到相关性掩码数组。
    renderPassInfo.pNext = &multiviewCI;  // 将多视图信息链入渲染通行证创建信息（pNext链）。链接多视图信息。

    VkRenderPass renderPass;// 渲染通行证句柄。
    VK_CHECK_RESULT(vkCreateRenderPass(device->logicalDevice, &renderPassInfo, nullptr, &renderPass));

    // --- 创建管线布局和渲染管线 ---
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    {
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1); // 初始化管线布局。
    VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout)); // 创建管线布局。
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE); // 设置三角形列表拓扑。
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE); // 设置填充模式，背面剔除，逆时针为正面。
    VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE); // 设置颜色混合，启用所有通道。
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState); // 设置颜色混合状态。
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL); // 启用深度和模板测试。
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1); // 设置一个视口和剪刀。
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT); // 禁用多重采样。
    std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR }; // 启用动态视口和剪刀。
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables); // 设置动态状态。
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages; shaderStages[0] = iLoader->LoadShader("lightprobesh2/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT); // 加载顶点着色器。
    shaderStages[0] = iLoader->LoadShader("lightprobesh2/scene.vert.spv", VK_SHADER_STAGE_VERTEX_BIT); // 加载顶点着色器。
    shaderStages[1] = iLoader->LoadShader("lightprobesh2/scene.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT); // 加载片段着色器。
    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
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
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline));
    }


    // 创建深度缓冲：分配图像、内存和视图，用于多视图深度测试。
    VkImageCreateInfo depthImageInfo = vks::initializers::imageCreateInfo();  // 初始化图像创建信息。
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;  // 图像类型：2D（但结合arrayLayers=6和CUBE_COMPATIBLE_BIT，形成立方体贴图）。
    depthImageInfo.format = depthFormat;  // 格式：深度格式。
    depthImageInfo.extent.width = lowReswidth;  // 宽度：低分辨率宽度。
    depthImageInfo.extent.height = lowResheight;  // 高度：低分辨率高度。
    depthImageInfo.extent.depth = 1;  // 深度：1（2D图像）。
    depthImageInfo.mipLevels = 1;  // Mipmap级别：1（无多级纹理）。
    depthImageInfo.arrayLayers = 6;  // 数组层数：6（对应6个立方体面，支持多视图）。
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;  // 采样数：1。
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;  // 平铺：最优（GPU优化布局）。
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  // 使用标志：仅作为深度/模板附件（不支持采样或传输）。
    depthImageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;  // 标志：立方体兼容（允许作为立方体贴图使用）。
    VK_CHECK_RESULT(vkCreateImage(device->logicalDevice, &depthImageInfo, nullptr, &depthImage));  // 创建深度图像。

    // 分配内存：获取需求并分配设备本地内存。
    vkGetImageMemoryRequirements(device->logicalDevice, depthImage, &memReqs);  // 获取图像内存需求（memReqs假设已定义为VkMemoryRequirements）。
    memAllocInfo.allocationSize = memReqs.size;  // 分配大小：需求大小。
    memAllocInfo.memoryTypeIndex = device->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);  // 内存类型：设备本地（高速GPU内存）。
    VK_CHECK_RESULT(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &depthMemory));  // 分配内存（memAllocInfo假设已初始化）。
    VK_CHECK_RESULT(vkBindImageMemory(device->logicalDevice, depthImage, depthMemory, 0));  // 绑定内存到图像，偏移0。

    // 创建深度图像视图：允许管线访问图像的特定部分。
    VkImageViewCreateInfo depthViewInfo = vks::initializers::imageViewCreateInfo();  // 初始化图像视图创建信息。
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;  // 视图类型：立方体（访问6个层作为立方体面）。
    depthViewInfo.format = depthFormat;  // 格式：深度格式。
    depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 6 };  // 子资源范围：深度方面（aspect），Mipmap 0-1，层0-6。
    depthViewInfo.image = depthImage;  // 底层图像。
    VK_CHECK_RESULT(vkCreateImageView(device->logicalDevice, &depthViewInfo, nullptr, &depthView));  // 创建视图。

    // 创建帧缓冲（支持多视图）：将图像视图绑定到渲染通行证，用于实际渲染目标。
    VkFramebufferCreateInfo fbInfo = vks::initializers::framebufferCreateInfo();  // 初始化帧缓冲创建信息。
    fbInfo.renderPass = renderPass;  // 关联渲染通行证。
    fbInfo.attachmentCount = 2;  // 附件数量：2（颜色视图 + 深度视图）。
    // 帧缓冲的图像视图数组（用于帧缓冲）
    std::array<VkImageView, 2> imageViews = { lowResCubemap->view, depthView };  // 改名为 imageViews
    fbInfo.pAttachments = imageViews.data();  // 正确赋值：VkImageView * 类型
    // std::array<VkImageView, 2> attachments = { lowResCubemap->view, depthView };  // 附件数组：低分辨率立方体贴图视图（假设lowResCubemap已创建）和深度视图。
    // fbInfo.pAttachments = attachments.data();  // 指针到附件数组。
    fbInfo.width = lowReswidth;  // 宽度：低分辨率。
    fbInfo.height = lowResheight;  // 高度：低分辨率。
    fbInfo.layers = 6;  // 层数：6（多视图渲染到6个层）。
    VkFramebuffer framebuffer;  // 帧缓冲句柄。
    VK_CHECK_RESULT(vkCreateFramebuffer(device->logicalDevice, &fbInfo, nullptr, &framebuffer));  // 创建帧缓冲。

    // --- 渲染：捕获低分辨率cubemap（多视图） ---
    // 这个部分记录命令缓冲，执行多视图渲染到低分辨率立方体贴图，用于光探针（Light Probe）捕获。
    VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);  // 创建主命令缓冲，一次性使用（true表示开始记录）。

    // 设置视图和投影矩阵：为立方体贴图的6个面生成视图矩阵（从position位置看向每个方向），投影为90度透视（适合立方体）。
    std::array<glm::mat4, 6> viewMatrices = {
        glm::lookAt(position, position + glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // +X面
        glm::lookAt(position, position + glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // -X面
        glm::lookAt(position, position + glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),  // +Y面
        glm::lookAt(position, position + glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),  // -Y面
        glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),  // +Z面
        glm::lookAt(position, position + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))   // -Z面
    };
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);  // 投影矩阵：90度FOV，宽高比1（正方形），近裁剪0.1，远裁剪100。

    // 更新统一缓冲对象（UBO）：为每个面更新视图矩阵（假设updateUBO是自定义函数，更新描述符集中的UBO）。
    UBO ubo = {};  // 初始化UBO结构体（假设定义为包含projection和view的结构体）。
    ubo.projection = projection;  // 设置投影。
    for (uint32_t face = 0; face < 6; ++face) {
        ubo.view[face] = viewMatrices[face];  // 为每个面设置视图矩阵。
        
    }
    updateUBO(ubo);  // 更新UBO（涉及描述符更新）。

    VkRenderPassBeginInfo rpBeginInfo = vks::initializers::renderPassBeginInfo();  // 初始化渲染通行证开始信息。
    rpBeginInfo.renderPass = renderPass;  // 渲染通行证。
    rpBeginInfo.framebuffer = framebuffer;  // 帧缓冲。
    rpBeginInfo.renderArea.extent.width = lowReswidth;  // 渲染区域宽度。
    rpBeginInfo.renderArea.extent.height = lowResheight;  // 渲染区域高度。
    std::array<VkClearValue, 2> clearValues;
    clearValues[0].color.float32[0] = 0.0f; // 设置颜色清除值（
    clearValues[0].color.float32[1] = 0.0f;
    clearValues[0].color.float32[2] = 0.0f;
    clearValues[0].color.float32[3] = 1.0f;//表示完全不透明（Alpha = 1.0）黑色

    clearValues[1].depthStencil.depth = 1.0f; // 设置深度清除值为 1.0。清除深度缓冲区到 1.0f 表示将所有像素的深度初始化为最远值。
    clearValues[1].depthStencil.stencil = 0.0f; // 设置模板清除值为 0。

    rpBeginInfo.clearValueCount = 2;  // 清除值数量：2。
    rpBeginInfo.pClearValues = clearValues.data();  // 指针到清除值。

    vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);  // 开始渲染通行证：内联内容（直接记录命令，无二级命令缓冲）。
    VkViewport viewport = vks::initializers::viewport((float)lowReswidth, (float)lowResheight, 0.0f, 1.0f);  // 视口：全分辨率，深度范围[0,1]。
    VkRect2D scissor = vks::initializers::rect2D(lowReswidth, lowResheight, 0, 0);  // 剪刀矩形：全区域，从(0,0)开始。
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);  // 设置视口（动态状态）。
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);  // 设置剪刀。
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);  // 绑定图形管线。
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);  // 绑定描述符集（集0，1个描述符，假设descriptorSet已创建）。
    drawScene(cmdBuf);  // 绘制场景（自定义函数，记录绘制命令，多视图会自动渲染到6个层）。
    vkCmdEndRenderPass(cmdBuf);  // 结束渲染通行证。
    // 提交命令缓冲
    device->flushCommandBuffer(cmdBuf, queue);
    // --- 使用计算着色器上采样 ---
    // 上采样低分辨率立方体贴图到高分辨率，使用自定义计算通行证。
    UpsampleCubeMapPass upsamplePass(device, iLoader);  // 创建上采样通行证（自定义类，使用计算着色器）。
    upsamplePass.SetCubeMaps(lowResCubemap, cubemap);  // 设置输入（低分辨率）和输出（高分辨率）立方体贴图（cubemap假设已创建）。
    upsamplePass.Generate(queue, lowReswidth, lowResheight, width, height);  // 生成：提交到队列，从低分辨率尺寸上采样到高分辨率（width/height假设已定义）。

    // --- 清理 ---
    // 释放资源：帧缓冲、渲染通行证、低分辨率cubemap视图/内存/图像、深度视图/内存/图像。
    vkDestroyFramebuffer(device->logicalDevice, framebuffer, nullptr);  // 销毁帧缓冲。
    vkDestroyRenderPass(device->logicalDevice, renderPass, nullptr);  // 销毁渲染通行证。
    vkDestroyImageView(device->logicalDevice, lowResCubemap->view, nullptr);  // 销毁低分辨率cubemap视图。
    vkFreeMemory(device->logicalDevice, lowResCubemap->deviceMemory, nullptr);  // 释放cubemap内存。
    vkDestroyImage(device->logicalDevice, lowResCubemap->image, nullptr);  // 销毁cubemap图像。
    //vkDestroyImageView(device->logicalDevice, depthView, nullptr);  // 销毁深度视图。转移到析构函数了
    //vkFreeMemory(device->logicalDevice, depthMemory, nullptr);  // 释放深度内存。转移到析构函数了
    //vkDestroyImage(device->logicalDevice, depthImage, nullptr);  // 销毁深度图像。转移到析构函数了
}

void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue) {
    GenSHComputePass shPass(device, iLoader);
    shPass.SetCubeMap(cubemap);
    shPass.Generate(queue);
    shPass.FeedSH(shCoeffs);
}