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
#include "VulkanTexture.h"
#include "PreviewModel.h"
#include "gltfload.h"
class LightProbe {
public:
    VkDescriptorBufferInfo shCoeffs;
    LightProbe(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width_ = 512, uint32_t height_ = 512);
    ~LightProbe();
    void SetPosition(const glm::vec3& position_) { position = position_; }
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

    // ✅ 修复: 添加Draw方法来渲染探针为球体
    void Draw(VkCommandBuffer cmd, VkDescriptorSet descriptorSet, ETechnique technique) {
        if (previewModel) {
            previewModel->Draw(cmd, descriptorSet, technique, position);
        }
    }

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

