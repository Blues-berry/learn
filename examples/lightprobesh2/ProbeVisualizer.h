#pragma once

#include "VulkanglTFModel.h"
#include "Pass.h"
#include "ILoader.h"
#include "glm/glm.hpp"
#include <vector>
#include <memory>

namespace vks {
    struct VulkanDevice;
}

// 探针可视化类 - 用于显示探针位置和插值效果
class ProbeVisualizer {
public:
    explicit ProbeVisualizer(vks::VulkanDevice* dev, IExampleInterfasce* example);
    ~ProbeVisualizer() = default;

    // 材质缓冲区
    struct MaterialBuffer {
        float roughness = 0.8f;
        float metallic = 0.0f;
        float specular = 0.5f;
        float padding = 0.f;
        glm::vec4 elbedo = glm::vec4(0.2f, 0.8f, 0.2f, 1.f);  // 绿色

        int32_t useSH = 0;
        int32_t useReflection = 0;
    };

    // 本地变换缓冲区
    struct LocalBuffer {
        glm::mat4 transform;
    };

    // 初始化可视化器
    void Initialize();

    // 销毁资源
    void Destroy();

    // 准备 PSO
    void PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout, ETechnique technique);

    // 设置球体模型（使用已加载的模型）
    void SetSphereModel(const std::shared_ptr<vkglTF::Model>& model) { sphereModel = model; }

    // 绘制单个探针
    void DrawProbe(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech,
                   const glm::vec3& position, const glm::vec4& color = glm::vec4(0.2f, 0.8f, 0.2f, 1.f),
                   bool bindPipeline = true);

    // 绘制多个探针
    void DrawProbes(VkCommandBuffer cmd, VkDescriptorSet globalSet, ETechnique tech,
                    const std::vector<glm::vec3>& positions);

    // 设置探针大小（缩放因子）
    void SetProbeScale(float scale) { probeScale = scale; }

    // 获取球体模型
    std::shared_ptr<vkglTF::Model> GetSphereModel() const { return sphereModel; }

private:
    void PreparePerBatchResource();
    void UpdateSet();
    void DrawNodeDirect(vkglTF::Node* node, VkCommandBuffer cmd, VkPipelineLayout pipelineLayout);

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;
    std::shared_ptr<vkglTF::Model> sphereModel;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::array<Technique, (uint32_t)ETechnique::NUM> techniques;

    LocalBuffer localData;
    vks::Buffer localBuffer;

    MaterialBuffer materialData;
    vks::Buffer materialBuffer;

    bool materialDirty = false;
    float probeScale = 0.2f;  // 探针球体的缩放因子
};

