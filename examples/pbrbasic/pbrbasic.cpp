/*
* Vulkan 示例 - 基于物理的着色基础
* 参考：http://graphicrants.blogspot.de/2013/08/specular-brdf-reference.html
* 版权所有 (C) 2017-2024 Sascha Willems - www.saschawillems.de
* MIT 许可证
*/
// 文件头部声明这是一个 Vulkan 示例程序，专注于基于物理的着色（PBR）。
// 引用了一个外部参考博客，提供了 PBR 镜面 BRDF 的理论基础。
// 作者 Sascha Willems，代码遵循 MIT 许可证。

#include "vulkanexamplebase.h"
// 包含 Vulkan 示例基类，提供 Vulkan 初始化、窗口管理等通用功能。

#include "VulkanglTFModel.h"
// 包含 glTF 模型加载工具，用于加载 3D 模型（如球体、茶壶等）。

const int maxnumLights = 64; // 最大光源数量
// 定义场景中支持的最大光源数量为 64，用于点光源模拟。

// 集群维度定义
const uint32_t CLUSTER_SIZE_X = 8;  // 屏幕宽度方向集群数
const uint32_t CLUSTER_SIZE_Y = 8;  // 屏幕高度方向集群数
const uint32_t CLUSTER_SIZE_Z = 8;  // 深度方向集群数
const uint32_t TOTAL_CLUSTERS = CLUSTER_SIZE_X * CLUSTER_SIZE_Y * CLUSTER_SIZE_Z;
const uint32_t lightIndexListnum = maxnumLights * TOTAL_CLUSTERS;
// 定义集群光照（Clustered Shading）的网格划分参数：
// - CLUSTER_SIZE_X/Y/Z：屏幕空间划分为 8x8x1 的集群网格。
// - TOTAL_CLUSTERS：总集群数 = 8 * 8 * 1 = 64。
// - lightIndexListnum：光源索引列表大小 = 64 光源 * 64 集群 = 4096，用于存储每个集群影响的光源索引。

// 材质定义
struct Material {
    struct PushBlock {
        float roughness; // 粗糙度
        float metallic;  // 金属度
        float r, g, b;   // RGB 颜色
    } params{};
    std::string name;
    Material() {}
    Material(std::string n, glm::vec3 c, float r, float m) : name(n) {
        params.roughness = r;
        params.metallic = m;
        params.r = c.r;
        params.g = c.g;
        params.b = c.b;
    }
};
// 定义材质结构体 Material，用于存储 PBR 材质属性：
// - PushBlock：包含粗糙度、金属度和 RGB 颜色，传递给着色器。
// - name：材质名称（如 "Gold"）。
// - 默认构造函数：初始化空材质。
// - 参数构造函数：根据名称、颜色、粗糙度、金属度初始化材质。
// - params{} 使用默认初始化，确保未显式初始化的成员为 0。

// 光源结构体
struct Light {
    glm::vec4 position;       // 位置
    glm::vec4 colorAndRadius; // 颜色和半径
    glm::vec4 direction;      // 方向
    glm::vec4 cutOff;         // 截止角度等参数
};
// 定义光源结构体 Light，存储点光源或聚光灯的参数：
// - position：光源的世界空间位置 (x, y, z, w)，w 通常为 1.0。
// - colorAndRadius：RGB 颜色和光源影响半径 (r, g, b, radius)。
// - direction：光源方向 (x, y, z, w)，用于聚光灯。
// - cutOff：截止角度等参数 (innerAngle, outerAngle, 0, 0)，用于聚光灯的衰减。

// 修正后的集群数据结构
struct ClusterCountsandOffsets {
    struct Cluster {
        uint32_t count;    // 4 字节
        uint32_t offset;   // 4 字节
        float padding[2];  // 8 字节，确保 16 字节对齐
    };
    Cluster cluster[TOTAL_CLUSTERS];
};
// 定义集群计数和偏移结构体，用于集群光照：
// - Cluster 子结构体：
//   - count：该集群影响的光源数量。
//   - offset：该集群在全局光源索引列表中的起始偏移。
//   - padding[2]：填充 8 字节，确保结构体大小为 16 字节，满足 Vulkan 的内存对齐要求（std140 布局）。
// - cluster 数组：存储 TOTAL_CLUSTERS（64）个集群的计数和偏移。

// 分离后的 uniform buffer 数据结构
struct UBOParams {
    Light lights[maxnumLights]; // 光源数组
};
// 定义 Uniform Buffer 对象 UBOParams，用于存储所有光源数据：
// - lights：包含 maxnumLights（64）个 Light 结构体，传递给着色器。

struct ClusterIndexList {
    struct Indices {
        uint32_t clusterIndexList; // 4 字节
        float padding[3];          // 12 字节，确保 16 字节对齐
    };
    Indices indices[maxnumLights * TOTAL_CLUSTERS]; // 全局光源索引列表
};
// 定义集群光源索引列表结构体：
// - Indices 子结构体：
//   - clusterIndexList：存储单个光源索引（指向 lights 数组）。
//   - padding[3]：填充 12 字节，确保 16 字节对齐（std140 布局）。
// - indices 数组：大小为 maxnumLights * TOTAL_CLUSTERS（4096），存储每个集群的光源索引。

class VulkanExample : public VulkanExampleBase {
public:
    // 继承 VulkanExampleBase，提供 Vulkan 渲染管线、窗口管理等基础功能。

    // 模型结构体
    struct Meshes {
        std::vector<vkglTF::Model> objects;
        int32_t objectIndex = 0;
    };
    // 定义 Meshes 结构体，管理加载的 glTF 模型：
    // - objects：存储多个 glTF 模型（如球体、茶壶等）。
    // - objectIndex：当前选中的模型索引，默认为 0（球体）。

    Meshes models; // 模型实例
    // 声明 models 成员变量，存储场景中使用的模型。

    struct {
        vks::Buffer object;         // 变换矩阵和相机位置
        vks::Buffer params;         // 光源数据
        vks::Buffer clusterData;    // 集群计数和偏移数据
        vks::Buffer clusterIndexList; // 全局光源索引列表
        vks::Buffer sphereVertex;   // 光源球体顶点缓冲区
        vks::Buffer sphereIndex;    // 光源球体索引缓冲区
        vks::Buffer sphereNormal;   // 光源球体法线缓冲区
    } uniformBuffers;
    // 定义 uniformBuffers 结构体，存储 Vulkan 缓冲区对象：
    // - object：存储投影、模型、视图矩阵和相机位置。
    // - params：存储光源数据（UBOParams）。
    // - clusterData：存储集群计数和偏移（ClusterCountsandOffsets）。
    // - clusterIndexList：存储光源索引列表（ClusterIndexList）。
    // - sphereVertex/Index/Normal：存储光源球体的顶点、索引和法线缓冲区，用于可视化光源。

    struct UBOMatrices {
        glm::mat4 projection; // 投影矩阵
        glm::mat4 model;      // 模型矩阵
        glm::mat4 view;       // 视图矩阵
        glm::vec3 camPos;     // 相机位置
        float padding;        // 填充以对齐 16 字节
    } uboMatrices;
    // 定义 UBOMatrices 结构体，存储相机和变换矩阵：
    // - projection：透视投影矩阵。
    // - model：模型变换矩阵。
    // - view：视图矩阵（相机方向）。
    // - camPos：相机世界空间位置。
    // - padding：填充 4 字节，确保结构体满足 16 字节对齐（std140 布局）。

    UBOParams uboParams;
    ClusterCountsandOffsets clusterData;
    ClusterIndexList clusterIndexList;
    // 声明 Uniform Buffer 数据实例：
    // - uboParams：存储光源数据。
    // - clusterData：存储集群计数和偏移。
    // - clusterIndexList：存储光源索引列表。

    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
    VkPipeline pipeline{ VK_NULL_HANDLE };
    VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    // 声明 Vulkan 管线相关对象：
    // - pipelineLayout：管线布局，定义着色器资源绑定。
    // - pipeline：图形管线，定义渲染流程。
    // - descriptorSetLayout：描述符集布局，定义 Uniform Buffer 绑定。
    // - descriptorSet：描述符集，绑定实际缓冲区。

    std::vector<Material> materials;
    int32_t materialIndex = 0;
    std::vector<std::string> materialNames;
    std::vector<std::string> objectNames;
    // 声明材质和模型选择相关变量：  std::vector<Material>模板类
    // - materials：存储所有材质（如金、铜等）。
    // - materialIndex：当前选中的材质索引，默认为 0（金）。
    // - materialNames：材质名称列表，用于 UI 显示。
    // - objectNames：模型名称列表（如 "Sphere", "Teapot"），用于 UI 选择。

    // 光源球体几何体数据
    uint32_t sphereIndexCount = 0;
    // 存储光源球体的索引数量，用于绘制光源位置的可视化球体。

    VulkanExample() : VulkanExampleBase() {
        // 构造函数，初始化示例。
        title = "Physical based shading basics";
        // 设置窗口标题为 "Physical based shading basics"。
        camera.type = Camera::CameraType::firstperson;
        // 设置相机为第一人称模式，用户可通过鼠标/键盘控制。
        camera.setPosition(glm::vec3(10.0f, 13.0f, 1.8f));
        // 设置相机初始位置为 (10.0, 13.0, 1.8)：
        // - X=10.0：右侧。
        // - Y=13.0：上方。
        // - Z=1.8：略向前。
        camera.setRotation(glm::vec3(-62.5f, 90.0f, 0.0f));
        // 设置相机初始旋转（欧拉角，单位：度）：
        // - Pitch=-62.5°：向下倾斜 62.5°。
        // - Yaw=90.0°：向右旋转 90°。
        // - Roll=0.0°：无滚转。
        camera.movementSpeed = 8.0f;
        // 设置相机移动速度为 4.0 单位/秒。
        camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);
        // 设置透视投影：
        // - FOV：60°。
        // - 宽高比：窗口宽度/高度。
        // - 近裁剪面：0.1。
        // - 远裁剪面：256.0。
        camera.rotationSpeed = 0.25f;
        // 设置相机旋转速度为 0.25 度/像素。
        timerSpeed *= 0.25f;
        // 减慢时间流逝速度（用于动画），乘以 0.25。

        // 初始化材质
        materials.push_back(Material("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f), 0.1f, 1.0f));
        materials.push_back(Material("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f), 0.1f, 1.0f));
        materials.push_back(Material("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f), 0.1f, 1.0f));
        materials.push_back(Material("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f), 0.1f, 1.0f));
        materials.push_back(Material("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f), 0.1f, 1.0f));
        materials.push_back(Material("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f), 0.1f, 1.0f));
        materials.push_back(Material("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f), 0.1f, 1.0f));
        materials.push_back(Material("planematerial", glm::vec3(0.955008f, 0.654864f, 0.336057f), 0.1f, 1.0f));
        materials.push_back(Material("White", glm::vec3(1.0f), 0.1f, 1.0f));
        materials.push_back(Material("Red", glm::vec3(1.0f, 0.0f, 0.0f), 0.1f, 1.0f));
        materials.push_back(Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f), 0.1f, 1.0f));
        materials.push_back(Material("Black", glm::vec3(0.0f), 0.1f, 1.0f));
        // 初始化材质列表，添加 12 种材质：
        // - 每种材质指定名称、RGB 颜色、粗糙度（0.1）、金属度（1.0）。
        // - 材质包括金、铜、铬等金属，以及纯色（白、红、蓝、黑）。

        for (auto material : materials) {
            materialNames.push_back(material.name);
        }
        // 遍历材质列表，将材质名称添加到 materialNames，用于 UI 显示。

        objectNames = { "Sphere", "Teapot", "Torusknot", "Venus", "plane", "plane_circle" };
        // 初始化模型名称列表，包含 6 种模型：球体、茶壶、环面结、金星、平面、圆形平面。

        materialIndex = 0;
        // 设置初始材质索引为 0（金）。
    }

    ~VulkanExample() {
        // 析构函数，释放 Vulkan 资源。
        if (device) {
            // 检查 Vulkan 设备是否有效。
            vkDestroyPipeline(device, pipeline, nullptr);
            // 销毁图形管线。
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            // 销毁管线布局。
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
            // 销毁描述符集布局。
            uniformBuffers.object.destroy();
            uniformBuffers.params.destroy();
            uniformBuffers.clusterData.destroy();
            uniformBuffers.clusterIndexList.destroy();
            uniformBuffers.sphereVertex.destroy();
            uniformBuffers.sphereIndex.destroy();
            uniformBuffers.sphereNormal.destroy();
            // 销毁所有 Uniform Buffer 和光源球体缓冲区。
        }
    }

    // 生成球体几何体
    void generateSphereGeometry(std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<uint32_t>& indices, uint32_t sectors = 64, uint32_t stacks = 64) {
        // 函数生成球体几何体数据，用于绘制光源位置的可视化球体：
        // - vertices：存储顶点位置。
        // - normals：存储顶点法线。
        // - indices：存储三角形索引。
        // - sectors：经度细分数，默认 32。
        // - stacks：纬度细分数，默认 32。
        vertices.clear();
        normals.clear();
        indices.clear();
        // 清空输入容器，确保无旧数据。
        const float PI = 3.14159265359f;
        // 定义 π 常量。
        float sectorStep = 2 * PI / sectors;
        // 计算经度步长：2π / 细分数。
        float stackStep = PI / stacks;
        // 计算纬度步长：π / 细分数。

        // 生成顶点和法线（半径为1）
        for (uint32_t i = 0; i <= stacks; ++i) {
            // 遍历纬度（从北极到南极）。
            float stackAngle = PI / 2 - i * stackStep;
            // 计算当前纬度角度：从 π/2（北极）到 -π/2（南极）。
            float xy = cosf(stackAngle);
            // 计算 xy 平面投影：cos(纬度角)。
            float z =  sinf(stackAngle);
            // 计算 z 坐标：sin(纬度角)。

            for (uint32_t j = 0; j <= sectors; ++j) {
                // 遍历经度（绕 Z 轴旋转）。
                float sectorAngle = j * sectorStep;
                // 计算当前经度角度：0 到 2π。
                glm::vec3 vertex;
                vertex.x = xy * cosf(sectorAngle);
                // X 坐标：xy * cos(经度角)。
                vertex.y = xy * sinf(sectorAngle);
                // Y 坐标：xy * sin(经度角)。
                vertex.z = z;
                // Z 坐标：z。
                vertices.push_back(vertex);
                // 添加顶点到 vertices。
                normals.push_back(glm::normalize(vertex));
                // 法线为归一化的顶点位置（球体半径为 1，说明已经归一化了，不使用归一化也是可以的）。
            }
        }

        // 生成索引
        for (uint32_t i = 0; i < stacks; ++i) {
            // 遍历纬度带。
            uint32_t k1 = i * (sectors + 1);
            // 当前纬度带的起始顶点索引。
            uint32_t k2 = k1 + sectors + 1;
            // 下一纬度带的起始顶点索引。

            for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
                // 遍历经度，生成三角形。
                if (i != 0) {
                    // 非北极，生成上三角形。
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }
                if (i != (stacks - 1)) {
                    // 非南极，生成下三角形。
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }
    }

    // 创建球体缓冲区
    void prepareSphereBuffers() {
        // 函数创建 Vulkan 缓冲区，存储光源球体的几何数据。
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<uint32_t>  indices;
        // 定义临时容器，存储顶点、法线和索引。
        generateSphereGeometry(vertices, normals, indices);
        // 调用 generateSphereGeometry 生成球体数据。
        sphereIndexCount = static_cast<uint32_t>(indices.size());
        // 存储索引数量，用于绘制。static_cast<uint32_t>为强制类型转换

        // 创建顶点缓冲区（位置）
        VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
        // 计算顶点缓冲区大小：顶点数 * 每个顶点大小。
        vks::Buffer stagingVertexBuffer;
        // 创建临时暂存缓冲区，用于 CPU 到 GPU 数据传输。是CPU内存到GPU显存的中间通道，增加传输效率
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingVertexBuffer,
            vertexBufferSize,
            vertices.data()));
        // 创建暂存缓冲区：
        // - 用法：VK_BUFFER_USAGE_TRANSFER_SRC_BIT 传输源。
        // - 内存属性：VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  主机可见
        //             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 可一致性映射、缓存一致。
        // - 缓冲区大小：vertexBufferSize。
        // - 数据来源：vertices 的指针。
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uniformBuffers.sphereVertex,
            vertexBufferSize));
        // 创建目标顶点缓冲区：
        // - 用法：VK_BUFFER_USAGE_VERTEX_BUFFER_BIT           顶点缓冲区
        //         VK_BUFFER_USAGE_TRANSFER_DST_BIT            传输目标。
        // - 内存属性：VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT     设备本地（GPU 优化）。
        // - 大小：vertexBufferSize                            缓冲区大小       
        vulkanDevice->copyBuffer(&stagingVertexBuffer, &uniformBuffers.sphereVertex, queue);
        // 将数据从暂存缓冲区复制到目标缓冲区，使用命令队列。
        stagingVertexBuffer.destroy();
        // 销毁暂存缓冲区，释放内存。

        // 创建法线缓冲区
        VkDeviceSize normalBufferSize = normals.size() * sizeof(glm::vec3);
        // 计算法线缓冲区大小。
        vks::Buffer stagingNormalBuffer;
        // 创建法线暂存缓冲区。
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingNormalBuffer,
            normalBufferSize,
            normals.data()));
        // 创建法线暂存缓冲区，类似顶点缓冲区。
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uniformBuffers.sphereNormal,
            normalBufferSize));
        // 创建法线目标缓冲区。
        vulkanDevice->copyBuffer(&stagingNormalBuffer, &uniformBuffers.sphereNormal, queue);
        // 复制法线数据。
        stagingNormalBuffer.destroy();
        // 销毁法线暂存缓冲区。

        // 创建索引缓冲区
        VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
        // 计算索引缓冲区大小。
        vks::Buffer stagingIndexBuffer;
        // 创建索引暂存缓冲区。
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingIndexBuffer,
            indexBufferSize,
            indices.data()));
        // 创建索引暂存缓冲区。
        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            &uniformBuffers.sphereIndex,
            indexBufferSize));
        // 创建索引目标缓冲区：
        // - 用法：索引缓冲区、传输目标。
        vulkanDevice->copyBuffer(&stagingIndexBuffer, &uniformBuffers.sphereIndex, queue);
        // 复制索引数据。
        stagingIndexBuffer.destroy();
        // 销毁索引暂存缓冲区。
    }

    void buildCommandBuffers() {
        // 函数构建命令缓冲区，记录渲染命令。
        VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
        // 初始化命令缓冲区开始信息，设置默认标志。
        VkClearValue clearValues[2];
        clearValues[0].color = defaultClearColor;
        clearValues[1].depthStencil = { 1.0f, 0 };
        // 定义清空值：
        // - clearValues[0]：颜色缓冲区清空为默认颜色（通常黑色）。
        // - clearValues[1]：深度缓冲区清空为 1.0（最远），模板值为 0。

        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
        renderPassBeginInfo.renderPass = renderPass;
        renderPassBeginInfo.renderArea.offset.x = 0;
        renderPassBeginInfo.renderArea.offset.y = 0;
        renderPassBeginInfo.renderArea.extent.width = width;
        renderPassBeginInfo.renderArea.extent.height = height;
        renderPassBeginInfo.clearValueCount = 2;
        renderPassBeginInfo.pClearValues = clearValues;
        // 初始化渲染通道开始信息：
        // - renderPass：使用的渲染通道。
        // - renderArea：渲染区域为整个窗口（偏移 0,0，宽高为窗口尺寸）。
        // - clearValueCount：2 个清空值（颜色和深度）。
        // - pClearValues：指向 clearValues 数组。

        for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
            // 遍历所有命令缓冲区（通常与帧缓冲区数量相同）。
            renderPassBeginInfo.framebuffer = frameBuffers[i];
            // 设置当前帧缓冲区。
            VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
            // 开始记录命令缓冲区。
            vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            // 开始渲染通道：
            // - 使用 renderPassBeginInfo 配置。
            // - 子通道内容为内联（直接记录命令）。

            VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
            vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
            // 设置视口：
            // - 大小：窗口宽高。
            // - Z 范围：0.0 到 1.0（标准深度范围）。

            VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
            vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
            // 设置裁剪矩形：
            // - 覆盖整个窗口（偏移 0,0，宽高为窗口尺寸）。

            vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            // 绑定图形管线，指定渲染使用的管线对象。

            vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);
            // 绑定描述符集：
            // - 绑定到图形管线。
            // - 使用 pipelineLayout。
            // - 绑定第 0 个描述符集（descriptorSet）。
            // - 无动态偏移。

            // 绘制场景几何体
            Material mat = materials[materialIndex];
            // 获取当前选中的材质。
            const uint32_t gridSize = 7;
            // 定义网格大小为 7x7，用于排列模型。

            for (uint32_t y = 0; y < gridSize; y++) {
                // 遍历网格 Y 轴。
                for (uint32_t x = 0; x < gridSize; x++) {
                    // 遍历网格 X 轴。
                    glm::vec3 pos = glm::vec3(float(x - (gridSize / 2.0f)) * 2.5f, 0.0f, float(y - (gridSize / 2.0f)) * 2.5f);
                    // 计算模型位置：
                    // - X：(x - 3.5) * 2.5，范围 [-8.75, 8.75]。
                    // - Y：0.0（地面）。
                    // - Z：(y - 3.5) * 2.5，范围 [-8.75, 8.75]。
                    vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
                    // 推送顶点着色器的推送常量：
                    // - 偏移 0，大小为 glm::vec3。
                    // - 数据：模型位置 pos。
                    vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &mat.params);
                    // 推送片段着色器的推送常量：
                    // - 偏移 sizeof(glm::vec3)，大小为 PushBlock。
                    // - 数据：材质参数（粗糙度、金属度、颜色）。
                    models.objects[models.objectIndex].draw(drawCmdBuffers[i]);
                    // 绘制当前选中的模型（由 objectIndex 指定）。
                }
            }

            // 绘制光源球体
            VkDeviceSize offsets[] = { 0, 0 };
            // 定义顶点缓冲区偏移，均为 0。
            VkBuffer vertexBuffers[] = { uniformBuffers.sphereVertex.buffer, uniformBuffers.sphereNormal.buffer };
            // 定义顶点缓冲区数组：位置和法线。
            vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 2, vertexBuffers, offsets);
            // 绑定顶点缓冲区：
            // - 绑定点 0 和 1（位置和法线）。
            // - 使用 vertexBuffers 和 offsets。
            vkCmdBindIndexBuffer(drawCmdBuffers[i], uniformBuffers.sphereIndex.buffer, 0, VK_INDEX_TYPE_UINT32);
            // 绑定索引缓冲区：
            // - 使用 sphereIndex.buffer。
            // - 偏移 0，索引类型为 uint32。

            for (int j = 0; j < maxnumLights; ++j) {
                // 遍历所有光源（64 个）。
                glm::vec3 pos = glm::vec3(uboParams.lights[j].position);
                // 获取光源位置（忽略 w 分量）。
                vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
                // 推送光源球体位置到顶点着色器。
                Material::PushBlock dummyMat = { 0.5f, 0.1f, uboParams.lights[j].colorAndRadius.x,uboParams.lights[j].colorAndRadius.y,uboParams.lights[j].colorAndRadius.z };
                // 创建虚拟材质（全，颜色为光源颜色），避免未定义行为。

                vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &dummyMat);
                // 推送虚拟材质到片段着色器（可能用于禁用 PBR 着色）。


                models.objects[models.objectIndex].draw(drawCmdBuffers[i]);
                // 绘制当前选中的模型（由 objectIndex 指定）


                vkCmdDrawIndexed(drawCmdBuffers[i], sphereIndexCount, 1, 0, 0, j);
                // 绘制光源球体：
                // - 索引数：sphereIndexCount。
                // - 实例数：1。
                // - 索引偏移：0。
                // - 顶点偏移：0。
                // - 实例 ID：j（可能用于着色器中）。
            }

            drawUI(drawCmdBuffers[i]);
            // 绘制用户界面（由基类提供）。
            vkCmdEndRenderPass(drawCmdBuffers[i]);
            // 结束渲染通道。
            VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
            // 结束命令缓冲区记录。
        }
    }

    void loadAssets() {
        // 函数加载 glTF 模型资产。
        std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf", "plane.gltf", "plane_circle.gltf" };
        // 定义模型文件名列表。
        models.objects.resize(filenames.size());
        // 调整 models.objects 大小以匹配文件数量（6）。
        for (size_t i = 0; i < filenames.size(); i++) {
            // 遍历文件名。
            models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue,
                vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY);
            // 加载 glTF 模型：
            // - 路径：资产路径 + "models/" + 文件名。
            // - 设备：vulkanDevice。
            // - 队列：queue。
            // - 标志：预变换顶点（应用模型变换）、翻转 Y 轴（适配 Vulkan 坐标系）。
        }
    }

    void setupDescriptors() {
        // 函数设置描述符池、描述符集布局和描述符集。
        std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
        };
        // 定义描述符池大小：
        // - 类型：Uniform Buffer。
        // - 数量：4（对应 4 个缓冲区）。
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
        // 初始化描述符池创建信息：
        // - 池大小：poolSizes。
        // - 最大描述符集数：2。
        VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
        // 创建描述符池，存储到 descriptorPool。

        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
        };
        // 定义描述符集布局绑定：
        // - 绑定 0：Uniform Buffer（矩阵），用于顶点和片段着色器。
        // - 绑定 1：Uniform Buffer（光源数据），用于片段着色器。
        // - 绑定 2：Uniform Buffer（光源索引列表），用于片段着色器。
        // - 绑定 3：Uniform Buffer（集群计数和偏移），用于片段着色器。
        VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        // 初始化描述符集布局创建信息，使用 setLayoutBindings。
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));
        // 创建描述符集布局，存储到 descriptorSetLayout。

        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
        // 初始化描述符集分配信息：
        // - 池：descriptorPool。
        // - 布局：descriptorSetLayout。
        // - 数量：1。
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
        // 分配描述符集，存储到 descriptorSet。

        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.object.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &uniformBuffers.clusterIndexList.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &uniformBuffers.clusterData.descriptor),
        };
        // 定义描述符集写入操作：
        // - 绑定 0：矩阵缓冲区（uniformBuffers.object）。
        // - 绑定 1：光源缓冲区（uniformBuffers.params）。
        // - 绑定 2：光源索引列表（uniformBuffers.clusterIndexList）。
        // - 绑定 3：集群计数和偏移（uniformBuffers.clusterData）。

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
        // 更新描述符集，应用所有写入操作。
    }

    VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage) {
        // 函数加载着色器模块。
        VkPipelineShaderStageCreateInfo shaderStage = {};
        // 初始化着色器阶段信息。
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        // 设置结构体类型。
        shaderStage.stage = stage;
        // 设置着色器阶段（顶点或片段）。
        shaderStage.pName = "main";
        // 设置着色器入口点为 "main"。

        // 假设HLSL已预编译为SPIR-V
        std::string spirvFile = fileName;
        // 初始化 SPIR-V 文件名为输入文件名。
        if (fileName.ends_with(".hlsl")) {
            // 检查文件名是否以 ".hlsl" 结尾。
            spirvFile = fileName.substr(0, fileName.size() - 5) + ".spv";
            // 将扩展名替换为 ".spv"。
        }
        shaderStage.module = vks::tools::loadShader(spirvFile.c_str(), device);
        // 加载 SPIR-V 文件，创建着色器模块。

        return shaderStage;
        // 返回着色器阶段信息。
    }

    void preparePipelines() {
        // 函数创建图形管线。
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
        // 初始化管线布局创建信息：
        // - 描述符集布局：descriptorSetLayout。
        // - 数量：1。
        std::vector<VkPushConstantRange> pushConstantRanges = {
            vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0),
            vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),
        };
        // 定义推送常量范围：
        // - 顶点着色器：glm::vec3（模型位置），偏移 0。
        // - 片段着色器：Material::PushBlock（材质参数），偏移 sizeof(glm::vec3)。
        pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
        // 设置推送常量范围数量为 2。
        pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
        // 设置推送常量范围数组。
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
        // 创建管线布局，存储到 pipelineLayout。

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        // 初始化输入组装状态：
        // - 拓扑：三角形列表。
        // - 基元重启：禁用。
        VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
        // 初始化光栅化状态：
        // - 多边形模式：填充。
        // - 背面剔除：启用。
        // - 正面方向：逆时针。
        VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
        // 初始化颜色混合附件状态：
        // - 写掩码：RGBA 全写。
        // - 混合：禁用。
        VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
        // 初始化颜色混合状态：
        // - 附件数：1。
        // - 附件：blendAttachmentState。
        VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        // 初始化深度模板状态：
        // - 深度测试：启用。
        // - 深度写入：启用。
        // - 比较操作：小于等于。
        VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
        // 初始化视口状态：
        // - 视口数：1。
        // - 裁剪矩形数：1。
        VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        // 初始化多重采样状态：
        // - 采样数：1（无多重采样）。
        std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        // 定义动态状态：
        // - 视口。
        // - 裁剪矩形。
        VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
        // 初始化动态状态信息。
        VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
        // 初始化图形管线创建信息：
        // - 管线布局：pipelineLayout。
        // - 渲染通道：renderPass。

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
        // 定义着色器阶段数组，大小为 2（顶点和片段）。
        pipelineCI.pInputAssemblyState = &inputAssemblyState;
        pipelineCI.pRasterizationState = &rasterizationState;
        pipelineCI.pColorBlendState = &colorBlendState;
        pipelineCI.pMultisampleState = &multisampleState;
        pipelineCI.pViewportState = &viewportState;
        pipelineCI.pDepthStencilState = &depthStencilState;
        pipelineCI.pDynamicState = &dynamicState;
        pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineCI.pStages = shaderStages.data();
        // 设置管线状态：
        // - 输入组装、栅格化、颜色混合、多重采样、视口、深度模板、动态状态。
        // - 着色器阶段数和指针。
        pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal });
        // 设置顶点输入状态：
        // - 使用 glTF 顶点格式（位置和法线）。

        shaderStages[0] = loadShader(getShadersPath() + "pbrbasic/pbr.vert.hlsl", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "pbrbasic/pbr.frag.hlsl", VK_SHADER_STAGE_FRAGMENT_BIT);
        // 加载顶点和片段着色器：
        // - 顶点着色器：pbr.vert.hlsl。
        // - 片段着色器：pbr.frag.hlsl。
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));
        // 创建图形管线，存储到 pipeline。
    }

    void prepareUniformBuffers() {
        // 函数创建和映射 Uniform Buffer。
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        // 获取物理设备属性。
        VkDeviceSize minAlignment = properties.limits.minUniformBufferOffsetAlignment;
        // 获取 Uniform Buffer 的最小对齐要求。
        VkDeviceSize alignedSizeClusterIndexList = ((sizeof(clusterIndexList) + minAlignment - 1) / minAlignment) * minAlignment;
        // 计算对齐后的集群索引列表缓冲区大小：
        // - 使用 ceiling 公式确保对齐。

        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.object,
            sizeof(uboMatrices)));
        // 创建矩阵缓冲区：
        // - 用法：Uniform Buffer。
        // - 内存属性：主机可见、可一致性映射。
        // - 大小：UBOMatrices 结构体大小。

        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.params,
            sizeof(uboParams)));
        // 创建光源数据缓冲区，类似矩阵缓冲区。

        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.clusterData,
            sizeof(clusterData)));
        // 创建集群计数和偏移缓冲区。

        VK_CHECK_RESULT(vulkanDevice->createBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &uniformBuffers.clusterIndexList,
            alignedSizeClusterIndexList));
        // 创建光源索引列表缓冲区，使用对齐大小。

        VK_CHECK_RESULT(uniformBuffers.object.map());
        VK_CHECK_RESULT(uniformBuffers.params.map());
        VK_CHECK_RESULT(uniformBuffers.clusterData.map());
        VK_CHECK_RESULT(uniformBuffers.clusterIndexList.map());
        // 映射所有缓冲区，使 CPU 可直接写入数据。

        prepareSphereBuffers();
        // 创建光源球体缓冲区。
    }

    void updateUniformBuffers() {
        // 函数更新矩阵缓冲区数据。
        uboMatrices.projection = camera.matrices.perspective;
        // 设置投影矩阵为相机透视矩阵。
        uboMatrices.view = camera.matrices.view;
        // 设置视图矩阵为相机视图矩阵。
        float rotationAngle = -90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f);
        // 计算模型旋转角度：
        // - 基础角度：-90°。
        // - 如果是茶壶（objectIndex == 1），额外加 45°。
        uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(rotationAngle), glm::vec3(0.0f, 1.0f, 0.0f));
        // 设置模型矩阵为绕 Y 轴旋转的矩阵。
        uboMatrices.camPos = camera.position * -1.0f;
        // 设置相机位置为相反值（可能是坐标系调整）。
        memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
        // 将 uboMatrices 数据复制到映射的缓冲区。
    }

    void updateLights() {
        // 函数更新光源数据。
        const float p = 15.0f;
        // 定义光源网格范围参数：±15.0。
        const int gridSize = static_cast<int>(ceil(sqrt(static_cast<float>(maxnumLights))));
        // 计算光源网格大小：ceil(sqrt(64)) = 8。
        const float spacing = 2.0f * p / (gridSize - 1);
        // 计算光源间距：2 * 15.0 / (8 - 1) ≈ 4.2857。

        int lightIndex = 0;
        // 初始化光源索引。
        for (int y = 0; y < gridSize && lightIndex < maxnumLights; y++) {
            // 遍历网格 Y 轴。
            for (int x = 0; x < gridSize && lightIndex < maxnumLights; x++) {
                // 遍历网格 X 轴。
                float posX = -p + x * spacing;
                // 计算 X 坐标：-15.0 到 +15.0。
                float posZ = -p + y * spacing;
                // 计算 Z 坐标：-15.0 到 +15.0。
                float posY = -p * 0.5f;
                // 设置 Y 坐标：-7.5（固定高度）。

                uboParams.lights[lightIndex].position = glm::vec4(posX, posY, posZ, 1.0f);
                // 设置光源位置，w=1.0。

                glm::vec3 color;
                switch (lightIndex % 4) {
                case 0: color = glm::vec3(1.0f, 0.0f, 0.0f); break;
                case 1: color = glm::vec3(0.0f, 1.0f, 0.0f); break;
                case 2: color = glm::vec3(0.0f, 0.0f, 1.0f); break;
                case 3: color = glm::vec3(1.0f, 1.0f, 0.0f); break;
                }
                // 根据索引模 4 设置光源颜色：
                // - 0：红。
                // - 1：绿。
                // - 2：蓝。
                // - 3：黄。
                uboParams.lights[lightIndex].colorAndRadius = glm::vec4(color, 15.0f);
                // 设置光源颜色和半径（15.0）。
                glm::vec3 direction = glm::normalize(glm::vec3(0.0f, 0.0f, 0.0f) - glm::vec3(posX, posY, posZ));
                // 计算光源方向：从光源位置指向原点 (0,0,0)。
                uboParams.lights[lightIndex].direction = glm::vec4(direction, 1.0f);
                // 设置光源方向，w=1.0。
                uboParams.lights[lightIndex].cutOff = glm::vec4(12.5f, 18.5f, 0.0f, 0.0f);
                // 设置聚光灯截止角度：
                // - 内角：12.5°。
                // - 外角：18.5°。
                // - 填充：0.0。
                lightIndex++;
                // 递增光源索引。
            }
        }

        if (!paused) {
            // 如果未暂停，更新光源位置（动画效果）。
            for (int i = 0; i < maxnumLights; i++) {
                uboParams.lights[i].position.x += sin(glm::radians(timer * 360.0f)) * 0.1f;
                uboParams.lights[i].position.z += cos(glm::radians(timer * 360.0f)) * 0.1f;

                // 使光源沿椭圆路径移动：
                // - X：增加 sin(时间 * 360°) * 0.1。
                // - Z：增加 cos(时间 * 360°) * 0.1。
            }
        }

        memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
        // 将光源数据复制到映射的缓冲区。
    }

    void updateLightsCluster() {
        // 函数更新集群光照数据。
        memset(clusterIndexList.indices, 0, sizeof(clusterIndexList.indices));
        // 清空光源索引列表。
        memset(clusterData.cluster, 0, sizeof(clusterData.cluster));
        // 清空集群计数和偏移。
        glm::mat4 viewProj = uboMatrices.projection * uboMatrices.view;
        // 计算视图投影矩阵：投影 * 视图。
        float zNear = 0.1f;
        float zFar = 256.0f;
        // 定义近裁剪面和远裁剪面。

        std::vector<std::vector<bool>> assignedLights(TOTAL_CLUSTERS, std::vector<bool>(maxnumLights, false));
        // 创建布尔数组，记录每个集群已分配的光源：
        // - 尺寸：64 集群 x 64 光源。
        // - 初始值：false。

        for (int lightIdx = 0; lightIdx < maxnumLights; lightIdx++) {
            // 遍历所有光源。
            Light& light = uboParams.lights[lightIdx];
            // 获取当前光源。
            float radius = light.colorAndRadius.w;
            // 获取光源影响半径。

            glm::vec4 clipPos = viewProj * light.position;
            // 将光源位置变换到裁剪空间。
            float ndcX = clipPos.x / clipPos.w;
            float ndcY = clipPos.y / clipPos.w;
            float ndcZ = clipPos.z / clipPos.w;
            // 转换为 NDC 坐标（[-1, 1]）。

            float radiusNDC = radius / clipPos.w;
            // 计算 NDC 空间中的半径。

            float minX = glm::clamp(ndcX - radiusNDC, -1.0f, 1.0f);
            float maxX = glm::clamp(ndcX + radiusNDC, -1.0f, 1.0f);
            float minY = glm::clamp(ndcY - radiusNDC, -1.0f, 1.0f);
            float maxY = glm::clamp(ndcY + radiusNDC, -1.0f, 1.0f);
            float minZ = glm::clamp(ndcZ - radiusNDC, 0.0f, 1.0f);
            float maxZ = glm::clamp(ndcZ + radiusNDC, 0.0f, 1.0f);
            // 计算光源影响的 NDC 范围：
            // - X/Y：中心 ± 半径，限制在 [-1, 1]。
            // - Z：中心 ± 半径，限制在 [0, 1]。

            uint32_t minClusterX = static_cast<uint32_t>((minX * 0.5f + 0.5f) * CLUSTER_SIZE_X);
            uint32_t maxClusterX = static_cast<uint32_t>((maxX * 0.5f + 0.5f) * CLUSTER_SIZE_X);
            uint32_t minClusterY = static_cast<uint32_t>((minY * 0.5f + 0.5f) * CLUSTER_SIZE_Y);
            uint32_t maxClusterY = static_cast<uint32_t>((maxY * 0.5f + 0.5f) * CLUSTER_SIZE_Y);
            uint32_t minClusterZ = static_cast<uint32_t>((log(minZ * (zFar - zNear) + zNear) / log(zFar / zNear)) * CLUSTER_SIZE_Z);
            uint32_t maxClusterZ = static_cast<uint32_t>((log(maxZ * (zFar - zNear) + zNear) / log(zFar / zNear)) * CLUSTER_SIZE_Z);
            // 映射 NDC 范围到集群索引：
            // - X/Y：将 [-1, 1] 映射到 [0, 1]，再乘以 CLUSTER_SIZE_X/Y。
            // - Z：使用对数深度公式映射到 [0, CLUSTER_SIZE_Z]。

            minClusterX = glm::clamp(minClusterX, 0u, CLUSTER_SIZE_X - 1);
            maxClusterX = glm::clamp(maxClusterX, 0u, CLUSTER_SIZE_X - 1);
            minClusterY = glm::clamp(minClusterY, 0u, CLUSTER_SIZE_Y - 1);
            maxClusterY = glm::clamp(maxClusterY, 0u, CLUSTER_SIZE_Y - 1);
            minClusterZ = glm::clamp(minClusterZ, 0u, CLUSTER_SIZE_Z - 1);
            maxClusterZ = glm::clamp(maxClusterZ, 0u, CLUSTER_SIZE_Z - 1);
            // 限制集群索引在有效范围内：[0, CLUSTER_SIZE_X/Y/Z - 1]。

            for (uint32_t z = minClusterZ; z <= maxClusterZ; ++z) {
                // 遍历 Z 方向集群。
                for (uint32_t y = minClusterY; y <= maxClusterY; ++y) {
                    // 遍历 Y 方向集群。
                    for (uint32_t x = minClusterX; x <= maxClusterX; ++x) {
                        // 遍历 X 方向集群。
                        uint32_t clusterIdx = z * CLUSTER_SIZE_X * CLUSTER_SIZE_Y + y * CLUSTER_SIZE_X + x;
                        // 计算集群索引：z * 8 * 8 + y * 8 + x。
                        if (!assignedLights[clusterIdx][lightIdx] && clusterData.cluster[clusterIdx].count < maxnumLights) {
                            // 检查条件：
                            // - 该光源未分配到此集群。
                            // - 集群的光源计数未达上限（64）。
                            clusterData.cluster[clusterIdx].count++;
                            // 增加集群的光源计数。
                            assignedLights[clusterIdx][lightIdx] = true;
                            // 标记光源已分配。
                        }
                    }
                }
            }
        }

        uint32_t runningSum = 0;
        // 初始化偏移累加器。
        for (uint32_t i = 0; i < TOTAL_CLUSTERS; i++) {
            // 遍历所有集群。
            clusterData.cluster[i].offset = runningSum;
            // 设置当前集群的偏移为累加值。
            runningSum += clusterData.cluster[i].count;
            // 累加光源计数，更新下个偏移。
        }

        std::vector<uint32_t> tempOffsets(TOTAL_CLUSTERS, 0);
        // 创建临时偏移数组，初始化为 0，记录每个集群已分配的光源数。
        for (int lightIdx = 0; lightIdx < maxnumLights; lightIdx++) {
            // 再次遍历所有光源，填充索引列表。
            Light& light = uboParams.lights[lightIdx];
            float radius = light.colorAndRadius.w;

            glm::vec4 clipPos = viewProj * light.position;
            float ndcX = clipPos.x / clipPos.w;
            float ndcY = clipPos.y / clipPos.w;
            float ndcZ = clipPos.z / clipPos.w;

            float radiusNDC = radius / clipPos.w;

            float minX = glm::clamp(ndcX - radiusNDC, -1.0f, 1.0f);
            float maxX = glm::clamp(ndcX + radiusNDC, -1.0f, 1.0f);
            float minY = glm::clamp(ndcY - radiusNDC, -1.0f, 1.0f);
            float maxY = glm::clamp(ndcY + radiusNDC, -1.0f, 1.0f);
            float minZ = glm::clamp(ndcZ - radiusNDC, 0.0f, 1.0f);
            float maxZ = glm::clamp(ndcZ + radiusNDC, 0.0f, 1.0f);
            // 重复计算光源的 NDC 范围（与之前相同）。
            //minZ = glm::max(minZ, 0.0001f); // 避免 log(0)
            //maxZ = glm::max(maxZ, 0.0001f);
            uint32_t minClusterX = static_cast<uint32_t>((minX * 0.5f + 0.5f) * CLUSTER_SIZE_X);
            uint32_t maxClusterX = static_cast<uint32_t>((maxX * 0.5f + 0.5f) * CLUSTER_SIZE_X);
            uint32_t minClusterY = static_cast<uint32_t>((minY * 0.5f + 0.5f) * CLUSTER_SIZE_Y);
            uint32_t maxClusterY = static_cast<uint32_t>((maxY * 0.5f + 0.5f) * CLUSTER_SIZE_Y);
            uint32_t minClusterZ = static_cast<uint32_t>((log(minZ * (zFar - zNear) + zNear) / log(zFar / zNear)) * CLUSTER_SIZE_Z);
            uint32_t maxClusterZ = static_cast<uint32_t>((log(maxZ * (zFar - zNear) + zNear) / log(zFar / zNear)) * CLUSTER_SIZE_Z);
            // 重复映射到集群索引。

            minClusterX = glm::clamp(minClusterX, 0u, CLUSTER_SIZE_X - 1);
            maxClusterX = glm::clamp(maxClusterX, 0u, CLUSTER_SIZE_X - 1);
            minClusterY = glm::clamp(minClusterY, 0u, CLUSTER_SIZE_Y - 1);
            maxClusterY = glm::clamp(maxClusterY, 0u, CLUSTER_SIZE_Y - 1);
            minClusterZ = glm::clamp(minClusterZ, 0u, CLUSTER_SIZE_Z - 1);
            maxClusterZ = glm::clamp(maxClusterZ, 0u, CLUSTER_SIZE_Z - 1);
            // 重复限制索引范围。

            for (uint32_t z = minClusterZ; z <= maxClusterZ; ++z) {
                for (uint32_t y = minClusterY; y <= maxClusterY; ++y) {
                    for (uint32_t x = minClusterX; x <= maxClusterX; ++x) {
                        uint32_t clusterIdx = z * CLUSTER_SIZE_X * CLUSTER_SIZE_Y + y * CLUSTER_SIZE_X + x;
                        // 计算集群索引。
                        uint32_t offset = clusterData.cluster[clusterIdx].offset + tempOffsets[clusterIdx];
                        // 计算索引列表中的偏移：集群偏移 + 已分配光源数。
                        if (offset < lightIndexListnum && tempOffsets[clusterIdx] < clusterData.cluster[clusterIdx].count) {
                            // 检查条件：
                            // - 偏移未超出索引列表大小（4096）。
                            // - 集群未分配满。
                            clusterIndexList.indices[offset].clusterIndexList = lightIdx;
                            // 将光源索引存储到索引列表。
                            tempOffsets[clusterIdx]++;
                            // 增加集群的已分配光源计数。
                        }
                    }
                }
            }
        }

        //memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
        //// 更新光源数据缓冲区（可能是多余的，因为 updateLights 已更新）。
        memcpy(uniformBuffers.clusterData.mapped, &clusterData, sizeof(clusterData));
        // 更新集群计数和偏移缓冲区。
        memcpy(uniformBuffers.clusterIndexList.mapped, &clusterIndexList, sizeof(clusterIndexList));
        // 更新光源索引列表缓冲区。
    }

    void draw() {
        // 函数执行绘制操作。
        VulkanExampleBase::prepareFrame();
        // 准备帧（由基类实现，可能包括交换链操作）。
        submitInfo.commandBufferCount = 1;
        // 设置提交信息：1 个命令缓冲区。
        submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
        // 设置当前帧的命令缓冲区。
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
        // 提交命令缓冲区到队列，无围栏（fence）。
        VulkanExampleBase::submitFrame();
        // 提交帧（由基类实现，可能包括呈现）。
    }

    void prepare() {
        // 函数准备渲染环境。
        VulkanExampleBase::prepare();
        // 调用基类准备函数（初始化 Vulkan 设备、交换链等）。
        loadAssets();
        // 加载 glTF 模型。
        prepareUniformBuffers();
        // 创建 Uniform Buffer。
        setupDescriptors();
        // 设置描述符。
        preparePipelines();
        // 创建管线。
        buildCommandBuffers();
        // 构建命令缓冲区。
        prepared = true;
        // 标记准备完成。
    }

    virtual void render() {
        // 虚函数，执行每帧渲染。
        if (!prepared) return;
        // 如果未准备好，直接返回。
        updateUniformBuffers();
        // 更新矩阵缓冲区。
        if (!paused) {
            // 如果未暂停，更新光源和集群数据。
            updateLights();
            updateLightsCluster();
        }
        draw();
        // 执行绘制。
    }

    virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay) {
        // 虚函数，更新 UI 覆盖层。
        if (overlay->header("Settings")) {
            // 如果 UI 显示设置标题。
            if (overlay->comboBox("Material", &materialIndex, materialNames)) {
                // 显示材质选择下拉框：
                // - 当前值：materialIndex。
                // - 选项：materialNames。
                // - 返回 true 表示值已更改。
                buildCommandBuffers();
                // 重建命令缓冲区以应用新材质。
            }
            if (overlay->comboBox("Type", &models.objectIndex, objectNames)) {
                // 显示模型类型选择下拉框。
                updateUniformBuffers();
                // 更新矩阵缓冲区（因模型旋转可能变化）。
                buildCommandBuffers();
                // 重建命令缓冲区以应用新模型。
            }
        }
    }
};

VULKAN_EXAMPLE_MAIN()
// 定义主函数宏，创建 VulkanExample 实例并运行。