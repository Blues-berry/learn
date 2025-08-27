/*
* Vulkan Example - Compute shader image processing
*
* This sample uses a compute shader to apply different filters to an image
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

// ����Vulkanʾ������ͷ�ļ����ṩ������Vulkan���ú͹�������
#include "vulkanexamplebase.h"

// ���嶥��ṹ�壬������Ⱦ�ı�������ʾͼ��
struct Vertex {
	float pos[3];  // 3Dλ������ (x, y, z)
	float uv[2];   // �������� (u, v)����������ӳ��
};

// ��Ҫ��Vulkanʾ���࣬�̳���VulkanExampleBase
class VulkanExample : public VulkanExampleBase
{
public:
	// Input image - ����ͼ��������������ɫ����������д���
	vks::Texture2D textureColorMap;
	// Storage image - �洢ͼ�񣬼�����ɫ�����˾�Ч��Ӧ�õ���ͼ����
	vks::Texture2D storageImage;

	// ͼ����Ⱦ���ֵ���Դ�ṹ��
	struct Graphics {
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };	// ͼ����ʾ��ɫ���󶨲���
		VkDescriptorSet descriptorSetPreCompute{ VK_NULL_HANDLE };		// ������ɫ������ǰ��ͼ����ʾ��ɫ����
		VkDescriptorSet descriptorSetPostCompute{ VK_NULL_HANDLE };		// ������ɫ���������ͼ����ʾ��ɫ����
		VkPipeline pipeline{ VK_NULL_HANDLE };							// ͼ����ʾ����
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };				// ͼ�ι��ߵĲ���
		VkSemaphore semaphore{ VK_NULL_HANDLE };						// �����ͼ���ύ֮���ִ�������ź���

		// ���ݸ�ͼ����ɫ�������ݽṹ
		struct UniformData {
			glm::mat4 projection;  // ͶӰ����
			glm::mat4 modelView;   // ģ����ͼ����
		} uniformData;
		vks::Buffer uniformBuffer;  // �洢uniform���ݵĻ�����
	} graphics;

	// ������ɫ�����ֵ���Դ�ṹ��
	struct Compute {
		VkQueue queue{ VK_NULL_HANDLE };								// ��������Ķ������У������������ͼ�β�ͬ��
		VkCommandPool commandPool{ VK_NULL_HANDLE };					// ����������أ������������ͼ�β�ͬ��
		VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };				// �洢������������ϵ��������
		VkSemaphore semaphore{ VK_NULL_HANDLE };						// �����ͼ���ύ֮���ִ�������ź���
		VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };	// ������ɫ���󶨲���
		VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };				// ������ɫ����
		VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };				// ������ߵĲ���
		std::vector<VkPipeline> pipelines{};							// ͼ���˾��ļ�����߼���
		int32_t pipelineIndex{ 0 };										// ��ǰͼ���˾������������
	} compute;

	// �����������������������Ⱦ��ʾͼ����ı���
	vks::Buffer vertexBuffer;    // ���㻺����
	vks::Buffer indexBuffer;     // ����������
	uint32_t indexCount{ 0 };    // ��������
	uint32_t vertexBufferSize{ 0 }; // ���㻺������С

	// �洢�˾����Ƶ�����������UI��ʾ
	std::vector<std::string> filterNames{};


	VkQueue computeQueue = VK_NULL_HANDLE;

	// ���캯������ʼ����������
	VulkanExample() : VulkanExampleBase()
	{
		title = "Compute shader image load/store";  // ���ڱ���
		camera.type = Camera::CameraType::lookat;   // �����������Ϊ�۲����
		camera.setPosition(glm::vec3(0.0f, 0.0f, -2.0f)); // �������λ��
		camera.setRotation(glm::vec3(0.0f)); // ���������ת�Ƕ�
		// ����͸��ͶӰ����Ұ��60�ȣ����߱�Ϊ���ڿ���һ����߶ȵı�ֵ����ƽ��1.0��Զƽ��256.0
		camera.setPerspective(60.0f, (float)width * 0.5f / (float)height, 1.0f, 256.0f);
	}

	// ������������������Vulkan��Դ
	~VulkanExample()
	{
		if (device) {
			// ����ͼ����Դ
			vkDestroyPipeline(device, graphics.pipeline, nullptr);              // ����ͼ�ι���
			vkDestroyPipelineLayout(device, graphics.pipelineLayout, nullptr);  // ���ٹ��߲���
			vkDestroyDescriptorSetLayout(device, graphics.descriptorSetLayout, nullptr); // ����������������
			vkDestroySemaphore(device, graphics.semaphore, nullptr);            // �����ź���
			graphics.uniformBuffer.destroy();                                   // ����uniform������

			// ����������Դ
			for (auto& pipeline : compute.pipelines)
			{
				vkDestroyPipeline(device, pipeline, nullptr);  // ����ÿ���������
			}
			vkDestroyPipelineLayout(device, compute.pipelineLayout, nullptr);      // ���ټ�����߲���
			vkDestroyDescriptorSetLayout(device, compute.descriptorSetLayout, nullptr); // ���ټ���������������
			vkDestroySemaphore(device, compute.semaphore, nullptr);                // ���ټ����ź���
			vkDestroyCommandPool(device, compute.commandPool, nullptr);            // ���ټ��������

			// ����������������
			vertexBuffer.destroy();    // ���ٶ��㻺����
			indexBuffer.destroy();     // ��������������
			textureColorMap.destroy(); // ������������
			storageImage.destroy();    // ���ٴ洢ͼ��
		}
	}

	// ׼�����ڴ洢������ɫ���˾�����Ĵ洢ͼ��
	void prepareStorageImage()
	{
		const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM; // ʹ��8λ�޷��ű�׼��RGBA��ʽ

		VkFormatProperties formatProperties;
		// ��ȡ�豸������������ʽ������֧�����
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
		// ��������ͼ���ʽ�Ƿ�֧�ִ洢ͼ�������������ɫ���洢�������裩
		assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		// ׼���洢Ŀ��������ʹ�������������ߴ���ͬ
		storageImage.width = textureColorMap.width;
		storageImage.height = textureColorMap.height;

		// ����ͼ��Ļ�����Ϣ�ṹ
		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;         // 2Dͼ������
		imageCreateInfo.format = format;                       // ͼ���ʽ
		imageCreateInfo.extent = { storageImage.width, storageImage.height, 1 }; // ͼ��ߴ�
		imageCreateInfo.mipLevels = 1;                        // mip�㼶����
		imageCreateInfo.arrayLayers = 1;                      // ���������
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;      // ��������
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;     // ����ƽ��ģʽ
		// ͼ����Ƭ����ɫ���в��������ڼ�����ɫ���������洢Ŀ��
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.flags = 0; // �������־

		// ��������ͼ�ζ�����������ͬ�������������߼乲����ͼ��
		// ����ܱȶ�ռ����ģʽ�����Բ��Ϊ�˱���ʾ���򵥣�����ʡȥһЩͬ��
		std::vector<uint32_t> queueFamilyIndices;
		if (vulkanDevice->queueFamilyIndices.graphics != vulkanDevice->queueFamilyIndices.compute) {
			queueFamilyIndices = {
				vulkanDevice->queueFamilyIndices.graphics,  // ͼ�ζ���������
				vulkanDevice->queueFamilyIndices.compute    // �������������
			};
			imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT; // ��������ģʽ
			imageCreateInfo.queueFamilyIndexCount = 2;                // ����������
			imageCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data(); // ��������������
		}
		// ����ͼ�����
		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &storageImage.image));

		// ����ͼ���ڴ�
		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, storageImage.image, &memReqs); // ��ȡ�ڴ�����
		memAllocInfo.allocationSize = memReqs.size; // �����С
		// ��ȡ�豸�����ڴ���������
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &storageImage.deviceMemory)); // �����ڴ�
		VK_CHECK_RESULT(vkBindImageMemory(device, storageImage.image, storageImage.deviceMemory, 0)); // ���ڴ�

		// ��ͼ��ת��Ϊͨ�ò��֣��Ա��ڼ�����ɫ���������洢ͼ��
		VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		storageImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // ����Ϊͨ�ò���
		// ִ�в���ת������δ���岼��ת��Ϊͨ�ò���
		vks::tools::setImageLayout(layoutCmd, storageImage.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, storageImage.imageLayout);
		vulkanDevice->flushCommandBuffer(layoutCmd, queue, true); // ����ִ�в��ȴ����

		// ����������
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_LINEAR;                    // �Ŵ�ʱʹ�������˲�
		sampler.minFilter = VK_FILTER_LINEAR;                    // ��Сʱʹ�������˲�
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;      // mipmapʹ�������˲�
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER; // U����ǯ�Ƶ��߽�
		sampler.addressModeV = sampler.addressModeU;             // V������U��ͬ
		sampler.addressModeW = sampler.addressModeU;             // W������U��ͬ
		sampler.mipLodBias = 0.0f;                              // mip LODƫ��
		sampler.maxAnisotropy = 1.0f;                           // ���������Բ���
		sampler.compareOp = VK_COMPARE_OP_NEVER;                // �Ƚϲ������Ӳ��Ƚϣ�
		sampler.minLod = 0.0f;                                  // ��СLOD
		sampler.maxLod = 1.0f;                                  // ���LOD
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // �߽���ɫΪ��͸����ɫ
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &storageImage.sampler));

		// ����ͼ����ͼ
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.image = VK_NULL_HANDLE;                            // ��ʼ��Ϊ�վ��
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;                  // 2Dͼ����ͼ����
		view.format = format;                                   // ��ͼ��ʽ
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }; // ����Դ��Χ����ɫͨ����mip�㼶0-1�������0-1��
		view.image = storageImage.image;                        // ����ͼ�����
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &storageImage.view));

		// ��ʼ���������Թ�����ʹ��
		storageImage.descriptor.imageLayout = storageImage.imageLayout; // ͼ�񲼾�
		storageImage.descriptor.imageView = storageImage.view;          // ͼ����ͼ
		storageImage.descriptor.sampler = storageImage.sampler;         // ������
		storageImage.device = vulkanDevice;                             // �����豸
	}

	// ������Դ�ļ��������ȣ�
	void loadAssets()
	{
		// ���ļ���������������֧�ֲ����ʹ洢������ʹ��ͨ�ò���
		textureColorMap.loadFromFile(
			getAssetPath() + "textures/vulkan_11_rgba.ktx",  // �����ļ�·��
			VK_FORMAT_R8G8B8A8_UNORM,                        // ������ʽ
			vulkanDevice,                                     // Vulkan�豸
			queue,                                           // ����
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, // ʹ�ñ�־������+�洢
			VK_IMAGE_LAYOUT_GENERAL                          // ͼ�񲼾֣�ͨ�ò���
		);
	}

	// ����ͼ����Ⱦ�������
	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		// �������ֵ����ɫ�����ģ��
		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor; // Ĭ�������ɫ
		clearValues[1].depthStencil = { 1.0f, 0 }; // ���1.0��ģ��0

		// ��Ⱦͨ����ʼ��Ϣ
		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;                    // ��Ⱦͨ��
		renderPassBeginInfo.renderArea.offset.x = 0;                   // ��Ⱦ����Xƫ��
		renderPassBeginInfo.renderArea.offset.y = 0;                   // ��Ⱦ����Yƫ��
		renderPassBeginInfo.renderArea.extent.width = width;           // ��Ⱦ�������
		renderPassBeginInfo.renderArea.extent.height = height;         // ��Ⱦ����߶�
		renderPassBeginInfo.clearValueCount = 2;                       // ���ֵ����
		renderPassBeginInfo.pClearValues = clearValues;                // ���ֵ����

		// Ϊÿ��֡�����������������
		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// ����Ŀ��֡������
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			// ��ʼ��¼�������
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			// ͼ���ڴ����ϣ�ȷ��������ɫ��д����ɺ����������в���
			VkImageMemoryBarrier imageMemoryBarrier = {};
			imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			// ���ǲ���ı�ͼ��Ĳ���
			imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;       // �ɲ��֣�ͨ��
			imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;       // �²��֣�ͨ��
			imageMemoryBarrier.image = storageImage.image;                // Ŀ��ͼ��
			imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }; // ����Դ��Χ
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; // Դ�������룺��ɫ��д��
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;  // Ŀ��������룺��ɫ����ȡ
			imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // ����Դ������
			imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // ����Ŀ�������

			// ����������ϣ��Ӽ�����ɫ���׶ε�Ƭ����ɫ���׶�
			vkCmdPipelineBarrier(
				drawCmdBuffers[i],
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,    // Դ�׶Σ�������ɫ��
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,   // Ŀ��׶Σ�Ƭ����ɫ��
				VK_FLAGS_NONE,                           // ������־
				0, nullptr,                              // �ڴ�����
				0, nullptr,                              // ����������
				1, &imageMemoryBarrier                   // ͼ������
			);

			// ��ʼ��Ⱦͨ��
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// �����ӿڣ�����Ϊ���ڿ��ȵ�һ�루�����ʾ��
			VkViewport viewport = vks::initializers::viewport((float)width * 0.5f, (float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			// ���òü�����
			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// �󶨶��㻺����
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &vertexBuffer.buffer, offsets);
			// ������������
			vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			// ��ࣺ��ʾ����ǰ��ԭʼͼ��
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, &graphics.descriptorSetPreCompute, 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);
			vkCmdDrawIndexed(drawCmdBuffers[i], indexCount, 1, 0, 0, 0); // ����������������

			// �Ҳࣺ��ʾ�����Ĵ���ͼ��
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipelineLayout, 0, 1, &graphics.descriptorSetPostCompute, 0, NULL);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics.pipeline);

			// �����ӿڵ��Ҳ�
			viewport.x = (float)width / 2.0f;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdDrawIndexed(drawCmdBuffers[i], indexCount, 1, 0, 0, 0); // �ٴλ���

			// ����UI���ǲ�
			drawUI(drawCmdBuffers[i]);

			// ������Ⱦͨ��
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			// ������������¼
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	// ����������ɫ���������
	void buildComputeCommandBuffer()
	{
		// ����ڹ��߸��ĺ����¹��������������ˢ�¶�����ȷ����ǰû����ʹ��
		vkQueueWaitIdle(compute.queue);

		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		// ��ʼ��¼�����������
		VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

		// �󶨵�ǰѡ��ļ������
		vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelines[compute.pipelineIndex]);
		// �󶨼�����ɫ������������
		vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayout, 0, 1, &compute.descriptorSet, 0, 0);

		// ���ɼ��㹤�����������СΪ16x16������ͼ��ߴ���㹤��������
		vkCmdDispatch(compute.commandBuffer, storageImage.width / 16, storageImage.height / 16, 1);

		// ��ɼ������������¼
		vkEndCommandBuffer(compute.commandBuffer);
	}

	// ����������ʾ��������ͼ��ĵ���UVӳ���ı��εĶ���
	void generateQuad()
	{
		// ������������������ɵĵ���UVӳ���ı��εĶ���
		std::vector<Vertex> vertices = {
			{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 1.0f } }, // ���Ͻǣ�λ��(1,1,0)����������(1,1)
			{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f } }, // ���Ͻǣ�λ��(-1,1,0)����������(0,1)
			{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } }, // ���½ǣ�λ��(-1,-1,0)����������(0,0)
			{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } }  // ���½ǣ�λ��(1,-1,0)����������(1,0)
		};

		// ������������������������
		std::vector<uint32_t> indices = { 0,1,2, 2,3,0 }; // ��һ�������Σ�0-1-2���ڶ��������Σ�2-3-0
		indexCount = static_cast<uint32_t>(indices.size()); // ��¼��������

		// �������������������ϴ���GPU

		// ��ʱ�ݴ滺�����ṹ��
		struct StagingBuffers {
			vks::Buffer vertices; // �����ݴ滺����
			vks::Buffer indices;  // �����ݴ滺����
		} stagingBuffers;

		// �����ɼ���Դ���������ݴ��ã�
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,                                    // ��������Դ
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // �����ɼ���һ��
			&stagingBuffers.vertices,                                           // ���������
			vertices.size() * sizeof(Vertex),                                   // ��������С
			vertices.data()                                                     // ��ʼ����
		));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,                                    // ��������Դ
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // �����ɼ���һ��
			&stagingBuffers.indices,                                            // ���������
			indices.size() * sizeof(uint32_t),                                  // ��������С
			indices.data()                                                      // ��ʼ����
		));

		// �豸����Ŀ�껺����
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, // ���㻺����+����Ŀ��
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,                                   // �豸�����ڴ�
			&vertexBuffer,                                                         // ���������
			vertices.size() * sizeof(Vertex)                                       // ��������С
		));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,   // ����������+����Ŀ��
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,                                   // �豸�����ڴ�
			&indexBuffer,                                                          // ���������
			indices.size() * sizeof(uint32_t)                                      // ��������С
		));

		// ���������Ƶ��豸
		vulkanDevice->copyBuffer(&stagingBuffers.vertices, &vertexBuffer, queue); // ���ƶ�������
		vulkanDevice->copyBuffer(&stagingBuffers.indices, &indexBuffer, queue);   // ������������

		// �����ݴ滺����
		stagingBuffers.vertices.destroy();
		stagingBuffers.indices.destroy();
	}

	// �������ؽ���ͼ�ι��ߺͼ������֮�乲��
	void setupDescriptorPool()
	{
		// �����������صĴ�С���ã�ָ��ÿ�����������͵�����
		std::vector<VkDescriptorPoolSize> poolSizes = {
			// ͼ�ι���ʹ�õ�ͳһ������������������2��
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2),
			// ͼ�ι���ʹ�õ����ͼ���������������ʾ�������ͼ�񣬷���2��
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2),
			// �������ʹ�õĴ洢ͼ������ͼ���д����������2��
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2),
		};
		// �����������أ����ɷ���3����������
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
	}

	// ׼��������ʾ������ɫ������׷�������ͼ����Դ
	void prepareGraphics()
	{
		// �������ڼ����ͼ��ͬ�����ź���
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &graphics.semaphore));

		// �����ź����źţ����ڳ�ʼ��ͬ��״̬
		VkSubmitInfo submitInfo = vks::initializers::submitInfo();
		submitInfo.signalSemaphoreCount = 1;  // Ҫ�����źŵ��ź�������
		submitInfo.pSignalSemaphores = &graphics.semaphore;  // ָ��Ҫ�����źŵ��ź���
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VK_CHECK_RESULT(vkQueueWaitIdle(queue));  // �ȴ�����������в���

		// ����������

		// ͼ�ι���ʹ���������ϣ�ÿ��������������
		// һ������������ʾ����ͼ����һ������������ʾӦ�ü����˾�������ͼ��
		// ��0��������ɫ��ͳһ������
		// ��1������ͼ��Ӧ�ü����˾�ǰ/��

		// ���������������ְ�
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// ��0��������ɫ���׶ε�ͳһ������
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			// ��1��Ƭ����ɫ���׶ε����ͼ�������
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
		};
		// ����������������
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &graphics.descriptorSetLayout));

		// ������������������Ϣ
		VkDescriptorSetAllocateInfo allocInfo =
			vks::initializers::descriptorSetAllocateInfo(descriptorPool, &graphics.descriptorSetLayout, 1);

		// ����ͼ�񣨼������ǰ������������
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &graphics.descriptorSetPreCompute));
		// ΪԤ����������������д�����
		std::vector<VkWriteDescriptorSet> baseImageWriteDescriptorSets = {
			// д��ͳһ����������������0
			vks::initializers::writeDescriptorSet(graphics.descriptorSetPreCompute, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &graphics.uniformBuffer.descriptor),
			// д��������ɫӳ�䵽��1��ԭʼͼ��
			vks::initializers::writeDescriptorSet(graphics.descriptorSetPreCompute, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textureColorMap.descriptor)
		};
		// ����Ԥ������������
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(baseImageWriteDescriptorSets.size()), baseImageWriteDescriptorSets.data(), 0, nullptr);

		// ����ͼ�񣨼�����ɫ�������󣩵���������
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &graphics.descriptorSetPostCompute));
		// Ϊ�����������������д�����
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// д��ͳһ����������������0
			vks::initializers::writeDescriptorSet(graphics.descriptorSetPostCompute, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &graphics.uniformBuffer.descriptor),
			// д��洢ͼ�񵽰�1���������ͼ��
			vks::initializers::writeDescriptorSet(graphics.descriptorSetPostCompute, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &storageImage.descriptor)
		};
		// ���º������������
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// ������ʾͼ���ͼ�ι��ߣ�Ӧ�ü���Ч��ǰ��

		// �������߲���
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&graphics.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &graphics.pipelineLayout));

		// ��������װ��״̬���������б�������������ʹ��ͼԪ����
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		// ���ù�դ��״̬�����ģʽ�����޳�����ʱ��Ϊ����
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		// ������ɫ��ϸ���״̬��д��������ɫͨ���������û��
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		// ������ɫ���״̬
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		// �������ģ��״̬��������Ȳ��Ժ�д�룬ʹ��С�ڵ��ڱȽ�
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		// �����ӿ�״̬��1���ӿڣ�1���ü�����
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		// ���ö��ز���״̬��1���������޶��ز���
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		// ���ö�̬״̬���ӿںͲü����οɶ�̬����
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		// ��ɫ���׶����飺�����Ƭ����ɫ��
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// ��ɫ������
		// ���ض�����ɫ��
		shaderStages[0] = loadShader(getShadersPath() + "computeshader/texture.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		// ����Ƭ����ɫ��
		shaderStages[1] = loadShader(getShadersPath() + "computeshader/texture.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		// ��������״̬����
		// ���嶥������󶨣��󶨵�0������ṹ���С��������������
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			vks::initializers::vertexInputBindingDescription(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)
		};
		// ���嶥������������λ�ú���������
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			// ����0��λ�ã�3��32λ��������ƫ��ΪVertex�ṹ����pos��ƫ��
			vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos)),
			// ����1���������꣬2��32λ��������ƫ��ΪVertex�ṹ����uv��ƫ��
			vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)),
		};
		// ������������״̬
		VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

		// ����ͼ�ι���
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(graphics.pipelineLayout, renderPass, 0);
		pipelineCreateInfo.pVertexInputState = &vertexInputState;        // ��������״̬
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;    // ����װ��״̬
		pipelineCreateInfo.pRasterizationState = &rasterizationState;    // ��դ��״̬
		pipelineCreateInfo.pColorBlendState = &colorBlendState;          // ��ɫ���״̬
		pipelineCreateInfo.pMultisampleState = &multisampleState;        // ���ز���״̬
		pipelineCreateInfo.pViewportState = &viewportState;              // �ӿ�״̬
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;      // ���ģ��״̬
		pipelineCreateInfo.pDynamicState = &dynamicState;                // ��̬״̬
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());  // ��ɫ���׶�����
		pipelineCreateInfo.pStages = shaderStages.data();                // ��ɫ���׶�����
		// ����ͼ�ι���
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &graphics.pipeline));
	}

	void prepareCompute()
	{
		// ���豸��ȡ�������

		vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.compute, 0, &computeQueue);

		// �����������
		// ������߶�����ͼ�ι��ߴ�������ʹ����ʹ����ͬ�Ķ���

		// ���������ɫ���������������ְ�
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			// ��0������ͼ��ֻ�������洢ͼ�����ͣ�������ɫ���׶�
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			// ��1�����ͼ��д�룩���洢ͼ�����ͣ�������ɫ���׶�
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		};

		// ����������ߵ�������������
		VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &compute.descriptorSetLayout));

		// ����������߲���
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&compute.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayout));

		// ���������ߵ���������
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &compute.descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &compute.descriptorSet));
		// ���ü�������������д�����
		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
			// ��0������ͼ��ԭʼ������
			vks::initializers::writeDescriptorSet(compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &textureColorMap.descriptor),
			// ��1�����ͼ�񣨴洢ͼ��
			vks::initializers::writeDescriptorSet(compute.descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImage.descriptor)
		};
		// ���¼�����������
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);

		// ����������ɫ������
		VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(compute.pipelineLayout, 0);

		// Ϊÿ�����õ�ͼ���˾�����һ������
		filterNames = { "emboss", "edgedetect", "sharpen" };  // ���񡢱�Ե��⡢��
		for (auto& shaderName : filterNames) {
			// ������ɫ���ļ�·��
			std::string fileName = getShadersPath() + "computeshader/" + shaderName + ".comp.spv";
			// ���ؼ�����ɫ��
			computePipelineCreateInfo.stage = loadShader(fileName, VK_SHADER_STAGE_COMPUTE_BIT);
			VkPipeline pipeline;
			// �����������
			VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &pipeline));
			// ���������ӵ��б���
			compute.pipelines.push_back(pipeline);
		}

		// ����������أ���Ϊ��������������ͼ�ζ����岻ͬ
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;  // �������������
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;      // ���������������
		VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &compute.commandPool));

		// Ϊ������������������
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(compute.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &compute.commandBuffer));

		// ���ڼ����ͼ��ͬ�����ź���
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &compute.semaphore));

		// ������������ַ�����ĵ����������
		buildComputeCommandBuffer();
	}

	void prepareUniformBuffers()
	{
		// ������ɫ��ͳһ��������
		// ����ͳһ������������ͳһ������ʹ�ã������ɼ�������
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &graphics.uniformBuffer, sizeof(Graphics::UniformData)));
		// �־�ӳ�仺����������Ƶ������
		VK_CHECK_RESULT(graphics.uniformBuffer.map());
	}

	void updateUniformBuffers()
	{
		// ������Ҫ����͸��ͶӰ����Ϊ��ʾ��������ʾ�����ӿ�
		camera.setPerspective(60.0f, (float)width * 0.5f / (float)height, 1.0f, 256.0f);
		// ����ͶӰ����
		graphics.uniformData.projection = camera.matrices.perspective;
		// ����ģ����ͼ����
		graphics.uniformData.modelView = camera.matrices.view;
		// �����ݿ�����ӳ��Ļ������ڴ�
		memcpy(graphics.uniformBuffer.mapped, &graphics.uniformData, sizeof(Graphics::UniformData));
	}

	void prepare()
	{
		// ���û����׼������
		VulkanExampleBase::prepare();
		loadAssets();           // ������Դ
		generateQuad();         // �����ı��μ�����
		prepareUniformBuffers();// ׼��ͳһ������
		prepareStorageImage();  // ׼���洢ͼ��
		setupDescriptorPool();  // ������������
		prepareGraphics();      // ׼��ͼ�ι���
		prepareCompute();       // ׼���������
		buildCommandBuffers();  // �����������
		prepared = true;        // ���Ϊ��׼�����
	}

	void draw()
	{
		// �ȴ���Ⱦ���
		// ����ȴ��׶����룺������ɫ���׶�
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		// �ύ��������
		VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
		computeSubmitInfo.commandBufferCount = 1;                        // �����������
		computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;      // �����������
		computeSubmitInfo.waitSemaphoreCount = 1;                        // �ȴ����ź�������
		computeSubmitInfo.pWaitSemaphores = &graphics.semaphore;         // �ȴ�ͼ���ź���
		computeSubmitInfo.pWaitDstStageMask = &waitStageMask;            // �ȴ��׶�����
		computeSubmitInfo.signalSemaphoreCount = 1;                      // �����źŵ��ź�������
		computeSubmitInfo.pSignalSemaphores = &compute.semaphore;        // ������ɺ󷢳����ź���
		VK_CHECK_RESULT(vkQueueSubmit(compute.queue, 1, &computeSubmitInfo, VK_NULL_HANDLE));

		// ׼��֡��������ͬ������
		VulkanExampleBase::prepareFrame();

		// ����ͼ�ι��ߵȴ��׶�����
		VkPipelineStageFlags graphicsWaitStageMasks[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		// ����ͼ�ι��ߵȴ����ź����������ź����ͳ�������ź���
		VkSemaphore graphicsWaitSemaphores[] = { compute.semaphore, semaphores.presentComplete };
		// ����ͼ�ι��߷������ź�����ͼ���ź�������Ⱦ����ź���
		VkSemaphore graphicsSignalSemaphores[] = { graphics.semaphore, semaphores.renderComplete };

		// �ύͼ������
		submitInfo.commandBufferCount = 1;                               // �����������
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];     // ��ǰ֡�Ļ����������
		submitInfo.waitSemaphoreCount = 2;                               // �ȴ�2���ź���
		submitInfo.pWaitSemaphores = graphicsWaitSemaphores;             // �ȴ����ź�������
		submitInfo.pWaitDstStageMask = graphicsWaitStageMasks;           // �ȴ��׶���������
		submitInfo.signalSemaphoreCount = 2;                             // ����2���ź���
		submitInfo.pSignalSemaphores = graphicsSignalSemaphores;         // �������ź�������
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		// �ύ֡���г���
		VulkanExampleBase::submitFrame();
	}

	virtual void render()
	{
		// ���δ׼����ϣ�ֱ�ӷ���
		if (!prepared)
		{
			return;
		}
		updateUniformBuffers();  // ����ͳһ������
		draw();                  // ִ�л���
	}

	// UI���Ǹ��º���
	virtual void OnUpdateUIOverlay(vks::UIOverlay* overlay)
	{
		// ���UI��"Settings"����ͷ
		if (overlay->header("Settings")) {
			// ������ɫ��ѡ����Ͽ򣬵�ѡ��ı�ʱ���¹��������������
			if (overlay->comboBox("Shader", &compute.pipelineIndex, filterNames)) {
				buildComputeCommandBuffer();  // ���¹����������������ʹ����ѡ�����ɫ��
			}
		}
	}
};

// Vulkanʾ��������ڵ��
VULKAN_EXAMPLE_MAIN()