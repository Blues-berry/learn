#include "LightProbe.h"

// 使用现有的 CubeMap
void LightProbe::SetExternalCubeMap(std::shared_ptr<vks::TextureCubeMap>& cubemap_)
{
    cubemap = cubemap_;
}

// 使用探针位置抓取
void LightProbe::CaptureCubeMap(VkFormat format, VkQueue queue)
{
    // TODO
}

void LightProbe::GenSH(VkCommandBuffer cmdBuffer, VkQueue queue)
{



}
