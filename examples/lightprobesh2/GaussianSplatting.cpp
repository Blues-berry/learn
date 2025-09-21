#include "GaussianSplatting.h"
#include "VulkanglTFModel.h"
#include "glm/gtc/quaternion.hpp"
#include <unordered_map>
#include <array>

class IStreamArchive {
public:
	explicit IStreamArchive(std::istream& s) : stream(s) {}
	~IStreamArchive() = default;

	bool LoadRaw(char* data, size_t size)
	{
		return stream.rdbuf()->sgetn(data, static_cast<std::streamsize>(size)) == size;
	}

	template <typename T>
	bool Load(T& t)
	{
		return LoadRaw((char*)&t, sizeof(T));
	}

private:
	std::istream& stream;
};

class OStreamArchive {
public:
	explicit OStreamArchive(std::ostream& s) : stream(s) {}
	~OStreamArchive() = default;

	bool SaveRaw(const char* data, size_t size)
	{
		return stream.rdbuf()->sputn(data, static_cast<std::streamsize>(size)) == size;
	}

	template <typename T>
	bool Save(const T& t)
	{
		return SaveRaw((const char*)&t, sizeof(T));
	}
private:
	std::ostream& stream;
};


std::unordered_map<std::string, std::pair<size_t, size_t>> SH_MAP = {
	{"f_dc_0", { 0, 0} },
	{"f_dc_1", { 0, 1} },
	{"f_dc_2", { 0, 2} },

	{"f_rest_0", { 1, 0} },
	{"f_rest_1", { 1, 1} },
	{"f_rest_2", { 1, 2} },

	{"f_rest_3", { 2, 0} },
	{"f_rest_4", { 2, 1} },
	{"f_rest_5", { 2, 2} },

	{"f_rest_6", { 3, 0} },
	{"f_rest_7", { 3, 1} },
	{"f_rest_8", { 3, 2} },

	{"f_rest_9",  { 4, 0} },
	{"f_rest_10", { 4, 1} },
	{"f_rest_11", { 4, 2} },

	{"f_rest_12", { 5, 0} },
	{"f_rest_13", { 5, 1} },
	{"f_rest_14", { 5, 2} },

	{"f_rest_15", { 6, 0} },
	{"f_rest_16", { 6, 1} },
	{"f_rest_17", { 6, 2} },

	{"f_rest_18", { 7, 0} },
	{"f_rest_19", { 7, 1} },
	{"f_rest_20", { 7, 2} },

	{"f_rest_21", { 8, 0} },
	{"f_rest_22", { 8, 1} },
	{"f_rest_23", { 8, 2} },

	{"f_rest_24", { 9, 0} },
	{"f_rest_25", { 9, 1} },
	{"f_rest_26", { 9, 2} },

	{"f_rest_27", { 10, 0} },
	{"f_rest_28", { 10, 1} },
	{"f_rest_29", { 10, 2} },

	{"f_rest_30", { 11, 0} },
	{"f_rest_31", { 11, 1} },
	{"f_rest_32", { 11, 2} },

	{"f_rest_33", { 12, 0} },
	{"f_rest_34", { 12, 1} },
	{"f_rest_35", { 12, 2} },

	{"f_rest_36", { 13, 0} },
	{"f_rest_37", { 13, 1} },
	{"f_rest_38", { 13, 2} },

	{"f_rest_39", { 14, 0} },
	{"f_rest_40", { 14, 1} },
	{"f_rest_41", { 14, 2} },

	{"f_rest_42", { 15, 0} },
	{"f_rest_43", { 15, 1} },
	{"f_rest_44", { 15, 2} },
};

void GaussianSplattingElement::SetVetexCount(uint32_t count)
{
	vertexCount = count;
}

void GaussianSplattingElement::SetPosition(const vks::Buffer& src, VkDeviceSize size, VkCommandBuffer cmd)
{
	device->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &position, size);

	VkBufferCopy copy = {};
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = size;
	vkCmdCopyBuffer(cmd, src.buffer, position.buffer, 1, &copy);
}

void GaussianSplattingElement::SetExtra(const vks::Buffer& src, VkDeviceSize size, VkCommandBuffer cmd)
{
	device->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &gsExtra, size);
	VkBufferCopy copy = {};
	copy.srcOffset = 0;
	copy.dstOffset = 0;
	copy.size = size;

	vkCmdCopyBuffer(cmd, src.buffer, gsExtra.buffer, 1, &copy);
}

void GaussianSplattingElement::Draw(VkCommandBuffer cmd, VkPipelineLayout pipelineLayout, VkDescriptorSet globalSet)
{
	std::vector<VkDescriptorSet> sets = {
		globalSet
	};

	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, NULL);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &position.buffer, &offset);
	vkCmdDraw(cmd, vertexCount, 1, 0, 0);
}

GaussianSplattingItem::GaussianSplattingItem(vks::VulkanDevice* device_, IExampleInterfasce* example)
	: device(device_), iLoader(example)
{
	PreparePerBatchResource();
}

GaussianSplattingItem::~GaussianSplattingItem()
{
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
		descriptorPool = VK_NULL_HANDLE;
	}

	if (descriptorSetLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
		descriptorSetLayout = VK_NULL_HANDLE;
	}

	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
		pipelineLayout = VK_NULL_HANDLE;
	}

	if (pso != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device->logicalDevice, pso, nullptr);
		pso = VK_NULL_HANDLE;
	}
}

void GaussianSplattingItem::PreparePerBatchResource()
{
	//std::vector<VkDescriptorPoolSize> poolSizes = {
	//	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	//	{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
	//};
	//VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
	//VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

	//std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
	//	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
	//	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
	//};
	//VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	//VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	//VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	//VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));
}

bool GaussianSplattingCacheFile::LoadFromFile(const std::string& path)
{
	std::fstream stream(path, std::ios::in | std::ios::binary);

	if (!stream.is_open()) {
		return false;
	}

	IStreamArchive archive(stream);
	bool res = true;

	uint32_t count = 0;
	res &= archive.Load(count);

	elementData.resize(count);

	for (uint32_t i = 0; i < count; ++i) {

		res &= archive.Load(elementData[i].vertexCount);

		std::vector<GaussianSplattingVertex> &gsPos = elementData[i].position;
		std::vector<GaussianSplattingVertexInfo> &gsInfo = elementData[i].extra;

		gsPos.resize(elementData[i].vertexCount);
		gsInfo.resize(elementData[i].vertexCount);

		res &= archive.LoadRaw((char*)gsPos.data(), sizeof(GaussianSplattingVertex) * gsPos.size());
		res &= archive.LoadRaw((char*)gsInfo.data(), sizeof(GaussianSplattingVertexInfo) * gsInfo.size());
	}

	return res;
}

void GaussianSplattingItem::LoadFromCache(std::shared_ptr<GaussianSplattingCacheFile>& cache, VkQueue queue)
{
	std::vector<vks::Buffer> stagingBuffers;

	VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	for (const auto& element : cache->elementData) {
		const std::vector<GaussianSplattingVertex> &gsPos = element.position;
		const std::vector<GaussianSplattingVertexInfo> &gsInfo = element.extra;

		elements.emplace_back(std::make_unique<GaussianSplattingElement>(device, iLoader));

		auto* back = elements.back().get();
		vks::Buffer staging1 = {};
		device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging1, sizeof(GaussianSplattingVertex) * gsPos.size());
		staging1.map();
		staging1.copyTo((void*)(gsPos.data()), sizeof(GaussianSplattingVertex) * gsPos.size());

		vks::Buffer staging2 = {};
		device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging2, sizeof(GaussianSplattingVertexInfo) * gsInfo.size());
		staging2.map();
		staging2.copyTo((void*)(gsInfo.data()), sizeof(GaussianSplattingVertexInfo) * gsInfo.size());

		back->SetPosition(staging1, sizeof(GaussianSplattingVertex) * gsPos.size(), cmdBuf);
		back->SetExtra(staging2, sizeof(GaussianSplattingVertexInfo) * gsInfo.size(), cmdBuf);

		stagingBuffers.emplace_back(staging1);
		stagingBuffers.emplace_back(staging2);

		back->SetVetexCount(element.vertexCount);
	}

	device->flushCommandBuffer(cmdBuf, queue);

	for (auto& staging : stagingBuffers) {
		staging.destroy();
	}
}

void GaussianSplattingItem::SetData(const std::shared_ptr<PlyObject>& object, VkQueue queue, const std::string& cacheFile)
{
	const auto& data = object->GetData();

	std::vector<vks::Buffer> stagingBuffers;


	std::fstream stream(cacheFile, std::ios::out | std::ios::binary);
	bool saveCache = stream.is_open();
	OStreamArchive archive(stream);

	if (saveCache) {
		archive.Save((uint32_t)data.elements.size());
	}

	VkCommandBuffer cmdBuf = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	for (auto& item : data.elements) {
		std::vector<GaussianSplattingVertex> gsPos;
		std::vector<GaussianSplattingVertexInfo> gsInfo;
		std::vector<glm::quat> gsRotations;

		gsPos.resize(item.count, GaussianSplattingVertex{ glm::vec3(0.f, 0.f, 0.f) });
		gsInfo.resize(item.count);
		gsRotations.resize(item.count);

		for (auto& prop : item.properties) {

			if (prop->name == "nx" || prop->name == "ny" || prop->name == "nz") {
				continue;
			}

			for (uint32_t i = 0; i < item.count; ++i) {

				if (prop->name == "x") {
					gsPos[i].position.x = *prop->GetAsFloat(i);
				}
				else if (prop->name == "y") {
					gsPos[i].position.y = *prop->GetAsFloat(i);
				}
				else if (prop->name == "z") {
					gsPos[i].position.z = *prop->GetAsFloat(i);
				}
				else if (prop->name == "opacity") {
					gsInfo[i].scale.w = *prop->GetAsFloat(i);
				}
				else if (prop->name == "scale_0") {
					gsInfo[i].scale.x = *prop->GetAsFloat(i);
				}
				else if (prop->name == "scale_1") {
					gsInfo[i].scale.y = *prop->GetAsFloat(i);
				}
				else if (prop->name == "scale_2") {
					gsInfo[i].scale.z = *prop->GetAsFloat(i);
				}
				else if (prop->name == "rot_0") {
					gsRotations[i].x = *prop->GetAsFloat(i);
				}
				else if (prop->name == "rot_1") {
					gsRotations[i].y = *prop->GetAsFloat(i);
				}
				else if (prop->name == "rot_2") {
					gsRotations[i].z = *prop->GetAsFloat(i);
				}
				else if (prop->name == "rot_3") {
					gsRotations[i].w = *prop->GetAsFloat(i);
				}
				else if (StartsWith(prop->name, "f_"))
				{
					const auto& [sh, idx] = SH_MAP[prop->name];
					float* ptr = &gsInfo[i].sh[sh].r;
					ptr[idx] = *prop->GetAsFloat(i);
				}
			}
		}

		for (uint32_t i = 0; i < item.count; ++i) {
			gsInfo[i].rotation = static_cast<glm::mat4>(gsRotations[i]);
		}

		elements.emplace_back(std::make_unique<GaussianSplattingElement>(device, iLoader));

		auto* back = elements.back().get();


		vks::Buffer staging1 = {};
		device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging1, sizeof(GaussianSplattingVertex) * gsPos.size());
		staging1.map();
		staging1.copyTo((void*)(gsPos.data()), sizeof(GaussianSplattingVertex) * gsPos.size());

		vks::Buffer staging2 = {};
		device->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging2, sizeof(GaussianSplattingVertexInfo) * gsInfo.size());
		staging2.map();
		staging2.copyTo((void*)(gsInfo.data()), sizeof(GaussianSplattingVertexInfo) * gsInfo.size());

		back->SetPosition(staging1, sizeof(GaussianSplattingVertex) * gsPos.size(), cmdBuf);
		back->SetExtra(staging2, sizeof(GaussianSplattingVertexInfo) * gsInfo.size(), cmdBuf);

		stagingBuffers.emplace_back(staging1);
		stagingBuffers.emplace_back(staging2);

		back->SetVetexCount(item.count);

		if (saveCache) {
			archive.Save((uint32_t)item.count);
			archive.SaveRaw((char*)(gsPos.data()), sizeof(GaussianSplattingVertex) * gsPos.size());
			archive.SaveRaw((char*)(gsInfo.data()), sizeof(GaussianSplattingVertexInfo) * gsInfo.size());
		}
	}

	device->flushCommandBuffer(cmdBuf, queue);

	for (auto& staging : stagingBuffers) {
		staging.destroy();
	}
}

void GaussianSplattingItem::Draw(VkCommandBuffer cmd, VkDescriptorSet globalSet)
{
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pso);
	for (auto& element : elements) {
		element->Draw(cmd, pipelineLayout, globalSet);
	}
}

void GaussianSplattingItem::PreparePSO(VkRenderPass renderPass, VkDescriptorSetLayout passLayout)
{
	VkDevice rawDevice = device->logicalDevice;

	// 配置输入组装状态，使用三角形列表拓扑。
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_POINT_LIST, 0, VK_FALSE);
	// 配置光栅化状态，填充模式，无背面剔除，逆时针为正面。
	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	// 配置颜色混合状态，禁用混合	
	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
	// 配置颜色混合状态，指定一个附件。
	VkPipelineColorBlendStateCreateInfo colorBlendState =
		vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	// 配置深度和模板状态，初始禁用深度测试和写入。
	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	// 配置视口状态，指定一个视口和裁剪矩形。
	VkPipelineViewportStateCreateInfo viewportState =
		vks::initializers::pipelineViewportStateCreateInfo(1, 1);
	// 配置多重采样状态，禁用多重采样（单采样）。
	VkPipelineMultisampleStateCreateInfo multisampleState =
		vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
	// 定义动态状态，包括视口和裁剪矩形。
	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	// 配置动态状态。
	VkPipelineDynamicStateCreateInfo dynamicState =
		vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

	std::vector<VkDescriptorSetLayout> setLayotus = {
		passLayout,
	};

	// Pipeline layout 初始化管线布局创建信息，指定描述符集布局。
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(setLayotus.data(), static_cast<uint32_t>(setLayotus.size()));
	// 创建管线布局。
	VK_CHECK_RESULT(vkCreatePipelineLayout(rawDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	// 定义两个着色器阶段（顶点和片段）。
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	// Pipelines
	// 初始化图形管线创建信息，指定管线布局和渲染通道。
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
	// 设置输入组装状态。
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	// 设置光栅化状态。
	pipelineCI.pRasterizationState = &rasterizationState;
	// 设置颜色混合状态。
	pipelineCI.pColorBlendState = &colorBlendState;
	// 设置深度和模板状态。
	pipelineCI.pMultisampleState = &multisampleState;
	// 设置视口状态。
	pipelineCI.pViewportState = &viewportState;
	// 设置深度和模板状态。
	pipelineCI.pDepthStencilState = &depthStencilState;
	// 设置动态状态。
	pipelineCI.pDynamicState = &dynamicState;
	// 设置着色器阶段数量（2）。
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	// 设置着色器阶段数组。
	pipelineCI.pStages = shaderStages.data();
	// 设置顶点输入状态，包括位置、法线和UV坐标。
	pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position });

	shaderStages[0] = iLoader->LoadShader("lightprobesh2/gaussian_point.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = iLoader->LoadShader("lightprobesh2/gaussian_point.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(rawDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pso));
}