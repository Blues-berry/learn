#pragma once

#include "Pass.h"

class GBufferPass : public MainPass
{
public:
    explicit GBufferPass(vks::VulkanDevice* device_);
    ~GBufferPass();

    void SetUp(uint32_t width, uint32_t height);
    void Draw(VkCommandBuffer cmd, std::function<void(VkCommandBuffer)> &&encoder);

    std::shared_ptr<vks::Texture2D> GetPositionAttachment() { return position; }
    std::shared_ptr<vks::Texture2D> GetNormalAttachment() { return normal; }
    std::shared_ptr<vks::Texture2D> GetAlbedoAttachment() { return albedo; }

private:
    void PrepareRenderPass();
    void PrepareFramebuffer();

    std::shared_ptr<vks::Texture2D> position;
    std::shared_ptr<vks::Texture2D> normal;
    std::shared_ptr<vks::Texture2D> albedo;
    std::shared_ptr<vks::Texture2D> depth;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
};

