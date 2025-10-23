#pragma once
#include "Pass.h"
#include <memory>

namespace vks {
    struct VulkanDevice;
    class TextureCubeMap;
}

class CaptureScenePass {
public:
    explicit CaptureScenePass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkFormat format, uint32_t width, uint32_t height);
    ~CaptureScenePass();

    // ????????????
    std::shared_ptr<vks::TextureCubeMap> GetCubeMap() const;

    struct GlobalUbo {
        glm::mat4 viewproj[6];
        glm::vec4 cameraPos[6];
        glm::vec4 mainLight;
        float exposure = 4.5f;
        float gamma = 2.2f;
    };

    void UpdateGlobal(const GlobalUbo& ubo);

    void Draw(VkCommandBuffer cmd, std::function<void(VkCommandBuffer)>&& encoder);

    VkRenderPass renderPass;
    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    // Л�7(�cubemap��&�,�o
    void FeedCubeDescriptor(VkDescriptorImageInfo& descriptor);
    VkImage GetCubeImage() const { return cube ? cube->GetImage() : VK_NULL_HANDLE; }
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
private:
    void PreparePerPassResource();
    void PrepareFrameBuffer();
    void UpdateBindings();

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    uint32_t width;
    uint32_t height;

    std::shared_ptr<RenderTargetCube> cube;
    std::shared_ptr<DepthStencil> depthStencil;

    std::shared_ptr<ResourceView> colorView;
    std::shared_ptr<ResourceView> dsView;
    std::shared_ptr<ResourceView> cubeSampleView; // CUBE 视图用于采样
    VkSampler cubeSampler = VK_NULL_HANDLE;       // 采样器

    std::vector<VkClearValue> clearValue;

    VkDescriptorPool descriptorPool;

    VkFramebuffer framebuffer;
    VkRenderPassBeginInfo beginInfo;
    vks::Buffer globalBuffer;
};

class UpsampleCubeMapPass : public ComputePass {
public:
    explicit UpsampleCubeMapPass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    ~UpsampleCubeMapPass() override;

    void SetCubeMaps(const std::shared_ptr<vks::TextureCubeMap>& lowResCube, 
                     const std::shared_ptr<vks::TextureCubeMap>& highResCube);
    void Generate(VkQueue queue, uint32_t lowResWidth, uint32_t lowResHeight, uint32_t highResWidth, uint32_t highResHeight);

private:
    void Dispatch(VkCommandBuffer cmd) override;

    std::shared_ptr<vks::TextureCubeMap> lowResCubemap;
    std::shared_ptr<vks::TextureCubeMap> highResCubemap;
    uint32_t lowResWidth, lowResHeight, highResWidth, highResHeight;
};