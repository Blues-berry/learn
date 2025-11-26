#pragma once
#include "VulkanDevice.h"
#include "VulkanTexture.h"
#include "VulkanglTFModel.h"
#include "glm/glm.hpp"
#include "ILoader.h"
#include <functional>

namespace vks
{
    struct VulkanDevice;
}

struct Technique {
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pso = VK_NULL_HANDLE;
};

enum class ETechnique : uint32_t {
    MAIN = 0,
    CAPTURE_SCENE = 1,
    NUM
};

class RenderAttachment {
public:
    RenderAttachment(vks::VulkanDevice* device_, VkImageType type, VkFormat format, VkImageUsageFlags usage, uint32_t width, uint32_t height, uint32_t layer, VkImageCreateFlags flags = 0);
    virtual ~RenderAttachment();

    VkImage GetImage() const { return image; }
    VkFormat GetFormat() const { return format; }
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }

    vks::VulkanDevice* device;
private:
    uint32_t width;
    uint32_t height;
    uint32_t layer;
    VkFormat format;
    VkImageType type;
    VkImage image;
    VkDeviceMemory deviceMemory;
};

class DepthStencil : public RenderAttachment {
public:
    DepthStencil(vks::VulkanDevice* device_, VkFormat format, uint32_t width, uint32_t height, uint32_t layer = 1);
    ~DepthStencil() override = default;
};

class RenderTarget2D : public RenderAttachment {
public:
    RenderTarget2D(vks::VulkanDevice* device_, VkFormat format, uint32_t width, uint32_t height, uint32_t layer = 1);
    ~RenderTarget2D() override = default;
};

class RenderTargetCube : public RenderTarget2D {
public:
    RenderTargetCube(vks::VulkanDevice* device_, VkFormat format, uint32_t width, uint32_t height);
    ~RenderTargetCube() override = default;

    std::shared_ptr<vks::TextureCubeMap> GetTextureCubeMap();
private:
    std::shared_ptr<vks::TextureCubeMap> cubeMap;
};

class StorageCubeMap : public RenderAttachment {
public:
    StorageCubeMap(vks::VulkanDevice* device_, VkFormat format, uint32_t width, uint32_t height);
    ~StorageCubeMap() override;

    std::shared_ptr<vks::TextureCubeMap> GetTextureCubeMap();
private:
    std::shared_ptr<vks::TextureCubeMap> cubeMap;
};

class ResourceView {
public:
    ResourceView(const std::shared_ptr<RenderAttachment>& attachment, VkImageViewType type, uint32_t firstSlice, uint32_t sliceCount, VkImageAspectFlags flags);
    ~ResourceView();

    VkImageView GetView() const { return imageView; }
private:
    void CreateView();

    std::shared_ptr<RenderAttachment> attachment;
    VkImageViewCreateInfo viewCI;
    VkImageView imageView = VK_NULL_HANDLE;
};

class ComputePass
{
public:
    explicit ComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    virtual ~ComputePass();

    void Draw(VkCommandBuffer cmd);

protected:
    virtual void Dispatch(VkCommandBuffer cmd) = 0;

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

class GenSHComputePass : public ComputePass
{
public:
    explicit GenSHComputePass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    ~GenSHComputePass();

    struct SHCoefficients {
        glm::vec4 shCoeffs[9];
    };

    void SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube);

    void Generate(VkQueue queue);

    void FeedSH(VkDescriptorBufferInfo& descriptor);

    vks::Buffer shCoeffBuffer;
private:
    void Dispatch(VkCommandBuffer cmd) override;

    std::shared_ptr<vks::TextureCubeMap> cubemap;
};

struct Evnironmemt
{
    VkDescriptorImageInfo brdfView;
    VkDescriptorImageInfo irradianceCube;
    VkDescriptorImageInfo prefilteredCube;
    VkDescriptorBufferInfo shCoeffs;
};

class MainPass
{
public:
    explicit MainPass(vks::VulkanDevice* device_);
    ~MainPass();

    struct GlobalUbo {
        glm::mat4 projection;  // ✅ 修复：改为分开的 projection 和 view，与着色器匹配
        glm::mat4 view;
        glm::vec4 light[4];
        glm::vec4 cameraPos;
        float exposure = 4.5f;
        float gamma = 2.2f;
        int useLightSource = 0;
        float lightIntensity = 50.0f;
        glm::vec3 lightPosition = glm::vec3(0.0f, 1.5f, 0.0f);
        glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
    };

    void UpdateGlobal(const GlobalUbo& ubo);

    void SetUp(VkRenderPass renderPass);
    
    void Draw(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width, uint32_t height, std::function<void(VkCommandBuffer)> &&encoder);

    void UpdateBindings();

    VkDescriptorSet descriptorSet;
    VkDescriptorSetLayout descriptorSetLayout;

    Evnironmemt environmemts = {};

protected:
    void PreparePerPassResource();
    vks::VulkanDevice* device;

    std::vector<VkClearValue> clearValue;
    VkDescriptorPool descriptorPool;

    VkRenderPassBeginInfo beginInfo;
    vks::Buffer globalBuffer;
};

class ScenePass:public MainPass
{
public:
    
    
};
class FullScreenPass
{
public:
    FullScreenPass(vks::VulkanDevice* dev, IExampleInterfasce* example, VkFormat format);
    virtual ~FullScreenPass();

    void Prepare();
    virtual void Draw(VkCommandBuffer cmd);

    void FeedDescriptor(VkDescriptorImageInfo& descriptor);

    VkSampler GetDefaultSampler() const { return sampler; }
protected:
    void GenerateSampler();

    virtual void PrepareRenderPass();
    virtual void PrepareData() {}
    virtual void PreparePipeline() = 0;
    virtual void PrepareFrameBuffer() = 0;

    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;

    VkRenderPassBeginInfo beginInfo;
    VkClearValue clearValue = {};

    uint32_t width = 1;
    uint32_t height = 1;
    VkFormat format = VK_FORMAT_UNDEFINED;
    
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
    VkRenderPass renderpass = VK_NULL_HANDLE;
    VkFramebuffer fbo = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

class GenBRDFLutPass : public FullScreenPass
{
public:
    explicit GenBRDFLutPass(vks::VulkanDevice* device_, IExampleInterfasce* example);
    ~GenBRDFLutPass();

private:
    void PreparePipeline() override;
    void PrepareFrameBuffer() override;
};

struct IBLGenUBO {
    glm::mat4 mvp[6];
    float deltaPhi;
    float deltaTheta;
    float roughness;
    uint32_t numSamples;
};

class GenIBLCubeMipPass : public FullScreenPass
{
public:
    explicit GenIBLCubeMipPass(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap, VkFormat format, uint32_t mip, uint32_t width, uint32_t height);
    ~GenIBLCubeMipPass();

    void SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube);
    void Draw(VkCommandBuffer cmd, vkglTF::Model& model);

private:
    void PrepareRenderPass() override;
    void PreparePipeline() override;
    void PrepareFrameBuffer() override;
    void PrepareData() override;

    virtual void FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shader) {}
    virtual void FeedUBO(IBLGenUBO& ubo) {}

    uint32_t mipmap;
    VkImage cubemap; // weakRef
    VkImageView subView = VK_NULL_HANDLE;

    vks::Buffer ubo;
};

class GenIrradianceCubeMip : public GenIBLCubeMipPass
{
public:
    GenIrradianceCubeMip(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap, VkFormat format, uint32_t mip, uint32_t width, uint32_t height)
        : GenIBLCubeMipPass(device_, example, cubemap, format, mip, width, height)
    {
    }
    ~GenIrradianceCubeMip() = default;

private:
    void FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shader) override;
    void FeedUBO(IBLGenUBO& ubo) override;
};

class GenPrefilterEnvMapMip : public GenIBLCubeMipPass
{
public:
    GenPrefilterEnvMapMip(vks::VulkanDevice* device_, IExampleInterfasce* example, VkImage cubemap, VkFormat format, uint32_t mip, uint32_t width, uint32_t height, float roughness_)
        : GenIBLCubeMipPass(device_, example, cubemap, format, mip, width, height)
        , roughness(roughness_)
    {
    }
    ~GenPrefilterEnvMapMip() = default;

private:
    void FeeShader(std::array<VkPipelineShaderStageCreateInfo, 2>& shader) override;
    void FeedUBO(IBLGenUBO& ubo) override;

    float roughness;
};

class GenIBLPass
{
public:
    GenIBLPass(vks::VulkanDevice* device_, IExampleInterfasce* example, uint32_t width);
    ~GenIBLPass();

    void SetModel(const std::shared_ptr<vkglTF::Model>& model_);
    void SetCubeMap(const std::shared_ptr<vks::TextureCubeMap>& cube);
    void Draw(VkCommandBuffer cmd);
    void Generate(VkQueue queue);

    void FeedIrradianceMap(VkDescriptorImageInfo& descriptor);
    void FeedPrefilteredMap(VkDescriptorImageInfo& descriptor);

private:
    vks::VulkanDevice* device;
    IExampleInterfasce* iLoader;
    uint32_t numMips;

    std::shared_ptr<vkglTF::Model> model;
    std::shared_ptr<vks::TextureCubeMap> cubemap;
    std::vector<std::unique_ptr<GenIrradianceCubeMip>> irradiance;
    std::vector<std::unique_ptr<GenPrefilterEnvMapMip>> prefiltered;

    VkImage irradianceImage = VK_NULL_HANDLE;
    VkImageView irradianceView = VK_NULL_HANDLE;
    VkDeviceMemory irradianceMemory = VK_NULL_HANDLE;

    VkImage prefilteredImage = VK_NULL_HANDLE;
    VkImageView prefilteredView = VK_NULL_HANDLE;
    VkDeviceMemory prefilteredMemory = VK_NULL_HANDLE;
};



