#pragma once
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "Pass.h"
#include "UpsampleCubeMapPass.h"
#include "Skybox.h"
#include <glm/glm.hpp>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include "VulkanTexture.h"
#include "PreviewModel.h"
#include "gltfload.h"
#include "VulkanUIOverlay.h"

// 探针网格配置
struct ProbeGridConfig {
    glm::vec3 minBounds{ -5.0f, 0.0f, -5.0f };
    glm::vec3 maxBounds{ 5.0f, 4.0f, 5.0f };
    glm::ivec3 dimensions{ 2, 2, 2 };
    uint32_t resolution{ 16 }; // 默认16x16分辨率用于多探针
    bool enabled{ false };
};

class LightProbe {
public:
    VkDescriptorBufferInfo shCoeffs;
    LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width_ = 512, uint32_t height_ = 512);
    ~LightProbe();
    void SetPosition(const glm::vec3& position_) { position = position_; }
    glm::vec3 GetPosition() const { return position; }
     // 获取内部的立方体贴图
    std::shared_ptr<vks::TextureCubeMap> GetCubemap() const { return cubemap; }
    void setmodel(std::shared_ptr<vkglTF::Model> model_) { model = model_; }
    // 新增：专门用于 GltfModel 指针
    void SetGltfModel(GltfModel* model_) { gltfModel = model_; }
    // void SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_);
    void setSkybox(Skybox* skybox_);
    void setPreviewModel(PreviewModel* previewModel_) { previewModel = previewModel_; }
    void CaptureCubeMap(VkQueue queue, VkCommandBuffer cmd = VK_NULL_HANDLE);
    void GenSH(VkCommandBuffer cmdBuffer, VkQueue queue);
    // 获取 capturePass 以便外部访问
    CaptureScenePass* GetCapturePass() { return capturePass.get(); }
    // 保存立方体贴图的六个面为单独的图片
    void SaveCubeMapFaces(VkQueue queue, const std::string& basePath);

    // ✅ 探针可视化：渲染探针为球体
    void Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, ETechnique technique) {
        if (previewModel) {
            previewModel->Draw(cmd, descriptorSet, technique, position);
        }
    }
    
    // ✅ UI支持：显示探针配置UI
    static void ShowProbeGridUI(vks::UIOverlay* overlay, ProbeGridConfig& config, bool& showProbes);
    
    // ✅ 探针管理：自动生成探针网格
    static std::vector<std::unique_ptr<LightProbe>> GenerateProbeGrid(
        vks::VulkanDevice* device,
        IExampleInterfasce* example,
        const ProbeGridConfig& config,
        Skybox* skybox,
        PreviewModel* previewModel,
        GltfModel* gltfModel
    );
    
    // ✅ 批量捕获：捕获所有探针的立方体贴图
    static void CaptureAllProbes(
        std::vector<std::unique_ptr<LightProbe>>& probes,
        VkQueue queue,
        std::vector<std::shared_ptr<vks::TextureCubeMap>>& cubeMaps,
        std::vector<std::string>& cubemapNames
    );

private:
   
    void drawScene(VkCommandBuffer cmdBuf);

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;
    glm::vec3 position;
    uint32_t width, height;
    std::shared_ptr<vks::TextureCubeMap> cubemap;
    std::shared_ptr<vkglTF::Model> model; // 假设场景模型
    std::shared_ptr<vkglTF::Model> gltfmodel; // 假设场景模型
    Skybox* skybox = nullptr; // 可选的天空盒对象
    PreviewModel* previewModel = nullptr; // 可选的预览模型对象
    GltfModel* gltfModel=nullptr;
    std::unique_ptr<CaptureScenePass> capturePass;
};

