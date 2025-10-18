#include "gltfload.h"



VulkanglTFModel::VulkanglTFModel(vks::VulkanDevice* dev, IExampleInterfasce* example): vulkanDevice(dev), iLoader(example){
	// 初始化shaderData.buffer
	shaderData.buffer.buffer = VK_NULL_HANDLE;
	shaderData.buffer.memory = VK_NULL_HANDLE;
	shaderData.buffer.mapped = nullptr;
}
VulkanglTFModel::~VulkanglTFModel()
	{
		// 释放节点
		for (auto node : nodes) {
			delete node;
		}
		// Release all Vulkan resources allocated for the model
		// 销毁顶点缓冲区
		vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
		// 销毁索引缓冲区
		vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);
		// 销毁uniform buffer
		if (shaderData.buffer.buffer != VK_NULL_HANDLE) {
			shaderData.buffer.destroy();
		}
		// 销毁管线布局
		if (pipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
		}
		// 销毁描述符池
		if (descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
		}
		// 销毁描述符集布局
		if (descriptorSetLayouts.matrices != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayouts.matrices, nullptr);
		}
		if (descriptorSetLayouts.textures != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayouts.textures, nullptr);
		}
		// 销毁管线
		if (pipelines.solid != VK_NULL_HANDLE) {
			vkDestroyPipeline(vulkanDevice->logicalDevice, pipelines.solid, nullptr);
		}
		if (pipelines.wireframe != VK_NULL_HANDLE) {
			vkDestroyPipeline(vulkanDevice->logicalDevice, pipelines.wireframe, nullptr);
		}
		// 销毁所有图像资源
		for (Image image : images) {
			vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
			vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
			vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, image.texture.deviceMemory, nullptr);
		}
	}
void VulkanglTFModel::loadImages(tinygltf::Model& input)
	{
		// Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
		// loading them from disk, we fetch them from the glTF loader and upload the buffers
		std::cout << "Loading " << input.images.size() << " images..." << std::endl;
		// 调整图像数组大小
		images.resize(input.images.size());
		// 初始化所有图像对象
		for (size_t i = 0; i < images.size(); i++) {
			images[i].texture.sampler = VK_NULL_HANDLE;
			images[i].texture.view = VK_NULL_HANDLE;
			images[i].texture.image = VK_NULL_HANDLE;
			images[i].texture.deviceMemory = VK_NULL_HANDLE;
			images[i].descriptorSet = VK_NULL_HANDLE;
		}
		// 检查输入图像是否有效
		if (input.images.empty()) {
			std::cerr << "Warning: No images in input model" << std::endl;
			return;
		}
		// 遍历所有图像
		for (size_t i = 0; i < input.images.size(); i++) {
			// 用引用获取临时变量，减少开销
			tinygltf::Image& glTFImage = input.images[i];
			// Get the image data from the glTF loader
			// 创建临时变量以及标志
			unsigned char* buffer = nullptr;
			VkDeviceSize bufferSize = 0;
			bool deleteBuffer = false;
			// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
			// 如果是 RGB 格式，转换为 RGBA
			if (glTFImage.component == 3) {
				// 容量扩充
				bufferSize = glTFImage.width * glTFImage.height * 4;
				buffer = new unsigned char[bufferSize];
				unsigned char* rgba = buffer;
				unsigned char* rgb = &glTFImage.image[0];
				for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
					memcpy(rgba, rgb, sizeof(unsigned char) * 3);
					rgba += 4;
					rgb += 3;
				}
				deleteBuffer = true;
			}
			// 否则直接使用原缓冲
			else {
				buffer = &glTFImage.image[0];
				bufferSize = glTFImage.image.size();
			}
			// Load texture from image buffer
			// 从缓冲加载纹理到 Vulkan
			try {
				images[i].texture.fromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vulkanDevice, copyQueue);
				std::cout << "Loaded image " << i << " successfully" << std::endl;
			} catch (const std::exception& e) {
				std::cerr << "Error loading image " << i << ": " << e.what() << std::endl;
				// 继续处理下一个图像
				continue;
			}
			
			// fromBuffer函数已经创建了sampler和view，并调用了updateDescriptor
			// 所以我们不需要在这里再次创建它们
			
			// 初始化描述符集为空句柄，将在setupDescriptors中分配
			images[i].descriptorSet = VK_NULL_HANDLE;
			// 如果转换了缓冲，释放它
			if (deleteBuffer) {
				delete[] buffer;
				buffer = nullptr;
			}
		}
	}

	// 加载纹理引用
void VulkanglTFModel::loadTextures(tinygltf::Model& input)
	{
		// 调整纹理数组大小
		textures.resize(input.textures.size());
		// 遍历纹理，设置图像索引
		for (size_t i = 0; i < input.textures.size(); i++) {
			textures[i].imageIndex = input.textures[i].source;
		}
	}

	// 加载材质
void VulkanglTFModel::loadMaterials(tinygltf::Model& input)
	{
		// 调整材质数组大小
		materials.resize(input.materials.size());
		// 遍历材质
		for (size_t i = 0; i < input.materials.size(); i++) {
			// We only read the most basic properties required for our sample
			tinygltf::Material glTFMaterial = input.materials[i];
			// Get the base color factor
			// 获取基础颜色因子
			if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
				materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
			}
			// Get base color texture index
			// 获取基础颜色纹理索引
			if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
				materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
			}
		}
	}

	// 递归加载节点
void VulkanglTFModel::loadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, VulkanglTFModel::Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<VulkanglTFModel::Vertex>& vertexBuffer)
	{
		// 创建新节点
		VulkanglTFModel::Node* node = new VulkanglTFModel::Node{};
		// 初始化矩阵为单位矩阵
		node->matrix = glm::mat4(1.0f);
		node->parent = parent;

		// Get the local node matrix
		// It's either made up from translation, rotation, scale or a 4x4 matrix
		// 应用平移
		if (inputNode.translation.size() == 3) {
			node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
		}
		// 应用旋转
		if (inputNode.rotation.size() == 4) {
			glm::quat q = glm::make_quat(inputNode.rotation.data());
			node->matrix *= glm::mat4(q);
		}
		// 应用缩放
		if (inputNode.scale.size() == 3) {
			node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
		}
		// 直接应用矩阵
		if (inputNode.matrix.size() == 16) {
			node->matrix = glm::make_mat4x4(inputNode.matrix.data());
		};

		// Load node's children
		// 加载子节点
		if (inputNode.children.size() > 0) {
			for (size_t i = 0; i < inputNode.children.size(); i++) {
				loadNode(input.nodes[inputNode.children[i]], input , node, indexBuffer, vertexBuffer);
			}
		}

		// If the node contains mesh data, we load vertices and indices from the buffers
		// In glTF this is done via accessors and buffer views
		// 如果节点有网格
		if (inputNode.mesh > -1) {
			const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
			// Iterate through all primitives of this node's mesh
			// 遍历网格的所有图元
			for (size_t i = 0; i < mesh.primitives.size(); i++) {
				const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
				// 计算索引偏移和顶点起始
				uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
				uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
				uint32_t indexCount = 0;
				// Vertices
				// 处理顶点数据
				{
					const float* positionBuffer = nullptr;
					const float* normalsBuffer = nullptr;
					const float* texCoordsBuffer = nullptr;
					size_t vertexCount = 0;

					// Get buffer data for vertex positions
					// 获取位置数据
					if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
						const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
						const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
						positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
						vertexCount = accessor.count;
					}
					// Get buffer data for vertex normals
					// 获取法线数据
					if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
						const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
						const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
						normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}
					// Get buffer data for vertex texture coordinates
					// glTF supports multiple sets, we only load the first one
					// 获取纹理坐标（仅第一个集）
					if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
						const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
						const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
						texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					}

					// Append data to model's vertex buffer
					// 将顶点数据追加到缓冲
					for (size_t v = 0; v < vertexCount; v++) {
						Vertex vert{};
						vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
						vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
						vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
						vert.color = glm::vec3(1.0f);
						vertexBuffer.push_back(vert);
					}
				}
				// Indices
				// 处理索引数据
				{
					const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
					const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

					indexCount += static_cast<uint32_t>(accessor.count);

					// glTF supports different component types of indices
					// 根据组件类型处理索引
					switch (accessor.componentType) {
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
						const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
						const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						break;
					}
					case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
						const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
						for (size_t index = 0; index < accessor.count; index++) {
							indexBuffer.push_back(buf[index] + vertexStart);
						}
						break;
					}
					default:
						std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
						return;
					}
				}
				// 创建图元并添加到网格
				Primitive primitive{};
				primitive.firstIndex = firstIndex;
				primitive.indexCount = indexCount;
				primitive.materialIndex = glTFPrimitive.material;
				node->mesh.primitives.push_back(primitive);
			}
		}

		// 如果有父节点，添加到子列表，否则添加到顶级节点
		if (parent) {
			parent->children.push_back(node);
		}
		else {
			nodes.push_back(node);
		}
	}

	/*
		glTF rendering functions
	*/

	// Draw a single node including child nodes (if present)
	// 绘制单个节点，包括子节点
void VulkanglTFModel::drawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, VulkanglTFModel::Node* node)
	{
		// 如果节点有图元
		if (node->mesh.primitives.size() > 0) {
			// Pass the node's matrix via push constants
			// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
			// 计算最终变换矩阵
			glm::mat4 nodeMatrix = node->matrix;
			VulkanglTFModel::Node* currentParent = node->parent;
			while (currentParent) {
				nodeMatrix = currentParent->matrix * nodeMatrix;
				currentParent = currentParent->parent;
			}
			// Pass the final matrix to the vertex shader using push constants
			// 使用 push constant 传递矩阵
			vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
			// 遍历图元
			for (VulkanglTFModel::Primitive& primitive : node->mesh.primitives) {
				if (primitive.indexCount > 0) {
					// Get the texture index for this primitive
					// 获取纹理
					VulkanglTFModel::Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
					
					// 检查纹理索引是否有效
					if (texture.imageIndex >= images.size()) {
						std::cerr << "Error: Invalid texture index: " << texture.imageIndex << std::endl;
						continue;
					}
					
					// 检查描述符集是否已准备好
					if (images[texture.imageIndex].descriptorSet == VK_NULL_HANDLE) {
						std::cerr << "Error: Descriptor set not ready for texture index: " << texture.imageIndex << std::endl;
						continue;
					}
					
					// Bind the descriptor for the current primitive's texture
					// 绑定纹理描述符集
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[texture.imageIndex].descriptorSet, 0, nullptr);
					// 绘制索引
					vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
				}
			}
		}
		// 递归绘制子节点
		for (auto& child : node->children) {
			drawNode(commandBuffer, pipelineLayout, child);
		}
	}

	// Draw the glTF scene starting at the top-level-nodes
	// 绘制整个场景
void VulkanglTFModel::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
	{
		// All vertices and indices are stored in single buffers, so we only need to bind once
		// 绑定顶点和索引缓冲
		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Render all nodes at top-level
		// 绘制所有顶级节点
		for (auto& node : nodes) {
			drawNode(commandBuffer, pipelineLayout, node);
		}
	}
void VulkanglTFModel::buildCommandBuffers(VkCommandBuffer cmd)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.25f, 0.25f, 0.25f, 1.0f } };;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = pass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		const VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
		const VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);
			// Bind scene matrices descriptor to set 0
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.solid);
			this->draw(drawCmdBuffers[i], pipelineLayout);
			
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}


void VulkanglTFModel::loadglTFFile(VkQueue queue,std::string filename)
	{
		tinygltf::Model glTFInput;
		tinygltf::TinyGLTF gltfContext;
		std::string error, warning;
	
		// Pass some Vulkan resources required for setup and rendering to the glTF model loading class

		this->copyQueue = queue;
#if defined(__ANDROID__)
		// On Android all assets are packed with the apk in a compressed form, so we need to open them using the asset manager
		// We let tinygltf handle this, by passing the asset manager of our app
		tinygltf::asset_manager = androidApp->activity->assetManager;
#endif
		// 使用 tinygltf 加载文件
		bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);



		std::vector<uint32_t> indexBuffer;
		std::vector<VulkanglTFModel::Vertex> vertexBuffer;

		// 如果加载成功
		if (fileLoaded) {
			std::cout << "Loading images..." << std::endl;
			this->loadImages(glTFInput);
			std::cout << "Loaded " << this->images.size() << " images" << std::endl;
			this->loadMaterials(glTFInput);
			this->loadTextures(glTFInput);
			const tinygltf::Scene& scene = glTFInput.scenes[0];
			for (size_t i = 0; i < scene.nodes.size(); i++) {
				const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
				this->loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
			}
		}
		else {
			vks::tools::exitFatal("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
			return;
		}

		// Create and upload vertex and index buffer
		// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
		// Primitives (of the glTF model) will then index into these using index offsets

		// 计算缓冲大小
		size_t vertexBufferSize = vertexBuffer.size() * sizeof(VulkanglTFModel::Vertex);
		size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
		this->indices.count = static_cast<uint32_t>(indexBuffer.size());

		// 暂存缓冲结构
		struct StagingBuffer {
			VkBuffer buffer;
			VkDeviceMemory memory;
		} vertexStaging, indexStaging;

		// Create host visible staging buffers (source)
		// 创建顶点暂存缓冲
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vertexBufferSize,
			&vertexStaging.buffer,
			&vertexStaging.memory,
			vertexBuffer.data()));
		// Index data
		// 创建索引暂存缓冲
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			indexBufferSize,
			&indexStaging.buffer,
			&indexStaging.memory,
			indexBuffer.data()));

		// Create device local buffers (target)
		// 创建设备本地顶点缓冲
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			vertexBufferSize,
			&this->vertices.buffer,
			&this->vertices.memory));
		// 创建设备本地索引缓冲
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			indexBufferSize,
			&this->indices.buffer,
			&this->indices.memory));

		// Copy data from staging buffers (host) do device local buffer (gpu)
		// 创建命令缓冲进行复制
		VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		VkBufferCopy copyRegion = {};

		copyRegion.size = vertexBufferSize;
		vkCmdCopyBuffer(
			copyCmd,
			vertexStaging.buffer,
			this->vertices.buffer,
			1,
			&copyRegion);

		copyRegion.size = indexBufferSize;
		vkCmdCopyBuffer(
			copyCmd,
			indexStaging.buffer,
			this->indices.buffer,
			1,
			&copyRegion);

		// 刷新命令缓冲
		vulkanDevice->flushCommandBuffer(copyCmd, queue, true);

		// Free staging resources
		// 释放暂存资源
		vkDestroyBuffer(*vulkanDevice, vertexStaging.buffer, nullptr);
		vkFreeMemory(*vulkanDevice, vertexStaging.memory, nullptr);
		vkDestroyBuffer(*vulkanDevice, indexStaging.buffer, nullptr);
		vkFreeMemory(*vulkanDevice, indexStaging.memory, nullptr);
	}

	// 准备管道
void VulkanglTFModel::preparePipelines(VkRenderPass renderPass,VkPipelineCache pipelineCache)
	{
		// Layout
		// The pipeline layout uses both descriptor sets (set 0 = matrices, set 1 = material)
		std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
		// We will use push constants to push the local matrices of a primitive to the vertex shader
		VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
		// Push constant ranges are part of the pipeline layout
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
		VK_CHECK_RESULT(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pipelineLayoutCI, nullptr, &pipelineLayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		// Vertex input bindings and attributes
		// 顶点输入绑定和属性
		const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(VulkanglTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, pos)),	// Location 0: Position
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, normal)),// Location 1: Normal
			vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, uv)),	// Location 2: Texture coordinates
			vks::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VulkanglTFModel::Vertex, color)),	// Location 3: Color
		};
		VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// 加载着色器
		const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            iLoader->LoadShader("gltfloading/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
            iLoader->LoadShader("gltfloading/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)

		};
		// 创建管道信息
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
		pipelineCI.pVertexInputState = &vertexInputStateCI;
		pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
		pipelineCI.pRasterizationState = &rasterizationStateCI;
		pipelineCI.pColorBlendState = &colorBlendStateCI;
		pipelineCI.pMultisampleState = &multisampleStateCI;
		pipelineCI.pViewportState = &viewportStateCI;
		pipelineCI.pDepthStencilState = &depthStencilStateCI;
		pipelineCI.pDynamicState = &dynamicStateCI;
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCI.pStages = shaderStages.data();

		// Solid rendering pipeline
		// 创建实心管道
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid));

	}

	// 设置描述符
void VulkanglTFModel::prepareUniformBuffers()
{
	// 创建uniform buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(shaderData.values);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;
	
	// 创建buffer
	VK_CHECK_RESULT(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferInfo, nullptr, &shaderData.buffer.buffer));

	// 获取内存需求
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, shaderData.buffer.buffer, &memRequirements);
	allocInfo.allocationSize = memRequirements.size;
	
	// 查找合适的内存类型
	VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	allocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memRequirements.memoryTypeBits, properties);
	
	// 分配内存
	VK_CHECK_RESULT(vkAllocateMemory(vulkanDevice->logicalDevice, &allocInfo, nullptr, &shaderData.buffer.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(vulkanDevice->logicalDevice, shaderData.buffer.buffer, shaderData.buffer.memory, 0));
	
	// 映射内存
	VK_CHECK_RESULT(vkMapMemory(vulkanDevice->logicalDevice, shaderData.buffer.memory, 0, memRequirements.size, 0, &shaderData.buffer.mapped));
}

void VulkanglTFModel::updateUniformBuffers()
{
	if (shaderData.buffer.mapped) {
		// 更新uniform数据
		memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
	}
}

void VulkanglTFModel::setupDescriptors(VkRenderPass renderPass)
	{
		/*
			This sample uses separate descriptor sets (and layouts) for the matrices and materials (textures)
		*/

		// 检查图像容器是否有效
		if (!this->images.data()) {
			std::cerr << "Error: images vector is not properly initialized" << std::endl;
			return;
		}
		
		// 描述符池大小
		uint32_t imageCount = static_cast<uint32_t>(this->images.size());
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
			// One combined image sampler per model image/texture
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount),
		};
		// One set for matrices and one per model image/texture
		const uint32_t maxSetCount = imageCount + 1;
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
		VK_CHECK_RESULT(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Descriptor set layout for passing matrices
		// 矩阵描述符集布局
		VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));
		// Descriptor set layout for passing material textures
		// 纹理描述符集布局
		setLayoutBinding = vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

		// Descriptor set for scene matrices
		// 分配矩阵描述符集
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &descriptorSet));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
		vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
		// Descriptor sets for materials
		// 为每个图像分配纹理描述符集
		// 检查图像容器是否为空
		// if (this->images.empty()) {
		// 	std::cerr << "Warning: No images to process" << std::endl;
		// 	return;
		// }
		// 检查图像容器是否已分配
		// if (!this->images.data()) {
		// 	std::cerr << "Error: images vector is not properly allocated" << std::endl;
		// 	return;
		// }
		
		// std::cout << "Processing " << imageCount << " images" << std::endl;
		for (size_t i = 0; i < this->images.size(); i++) {
			if (i >= this->images.size()) {
				std::cerr << "Error: Index out of bounds" << std::endl;
				break;
			}
			auto& image = this->images[i];
			// 检查纹理是否已准备好
			if (image.texture.sampler == VK_NULL_HANDLE || image.texture.view == VK_NULL_HANDLE) {
				std::cerr << "Error: Texture not properly loaded for image " << i << std::endl;
				continue;
			}
			
			const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
			VkResult result = vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &image.descriptorSet);
			if (result != VK_SUCCESS) {
				std::cerr << "Error: Failed to allocate descriptor set for texture: " << result << std::endl;
				continue;
			}
			
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(image.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &image.texture.descriptor);
			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
		}
	}

void VulkanglTFModel::prepare(VkRenderPass renderPass,VkPipelineCache pipelineCache,VkCommandBuffer cmd)
	{
		// 设置渲染通道
		pass = renderPass;
		
		// 准备uniform buffers
		prepareUniformBuffers();

		// 设置描述符
		setupDescriptors(renderPass);
		preparePipelines(renderPass,pipelineCache);
		// buildCommandBuffers(cmd); // 不再需要，因为我们将在主渲染循环中绘制
		// prepared = true;
	}	