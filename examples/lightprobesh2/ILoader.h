#pragma once

class IExampleInterfasce
{
public:
	IExampleInterfasce() = default;
	virtual ~IExampleInterfasce() = default;

	virtual VkPipelineShaderStageCreateInfo LoadShader(const std::string& path, VkShaderStageFlagBits stage) = 0;
};