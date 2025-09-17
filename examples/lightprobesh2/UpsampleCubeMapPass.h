#pragma once
#include "Pass.h"
#include <memory>

namespace vks {
    struct VulkanDevice;
    struct TextureCubeMap;
}

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