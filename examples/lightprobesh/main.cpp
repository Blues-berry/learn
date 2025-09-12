#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "vulkanexamplebase.h"  // 寮曞叆Vulkan鍩虹绀轰緥绫?
#include "VulkanglTFModel.h"    // 寮曞叆glTF妯″瀷鍔犺浇绫?
#include <fstream>
// SH 绯绘暟 (2闃?SH, 9 涓?vec3 绯绘暟锛屽搴?RGB 閫氶亾)
struct SHCoefficients {
    glm::vec4 l00, l1m1, l10, l1p1, l2m2, l2m1, l20, l2p1, l2p2;
} shCoeffs;

// UI 鏂板锛氬姣旀ā寮忓紑鍏?
bool compareMode = false;  // 榛樿鍏抽棴

// 鏉愯川瀹氫箟缁撴瀯浣?
struct Material {
    // 鏉愯川鍙傛暟鍧?
    struct PushBlock {
        float roughness = 0.0f;  // 绮楃硻搴?
        float metallic = 0.0f;   // 閲戝睘搴?
        float specular = 0.0f;   // 闀滈潰鍙嶅皠寮哄害
        float r, g, b;           // RGB棰滆壊鍒嗛噺
    } params;
    
    std::string name;  // 鏉愯川鍚嶇О
    
    Material() {};  // 榛樿鏋勯€犲嚱鏁?
    
    // 甯﹀弬鏁扮殑鏋勯€犲嚱鏁?
    Material(std::string n, glm::vec3 c) : name(n) {
        params.r = c.r;  // 璁剧疆绾㈣壊鍒嗛噺
        params.g = c.g;  // 璁剧疆缁胯壊鍒嗛噺
        params.b = c.b;  // 璁剧疆钃濊壊鍒嗛噺
    };
};

/**
 * 鍩轰簬鍥惧儚鐨勭墿鐞嗘覆鏌?PBR)绀轰緥绫?
 * 瀹炵幇浜嗗熀浜庡浘鍍忕殑鐓ф槑(IBL)鍜岀悆璋愬嚱鏁版覆鏌?
 */
class VulkanExample : public VulkanExampleBase
{
public:

    // 澶╃┖鐩掔浉鍏虫垚鍛?
	std::vector<std::string> skyboxNames;  // 澶╃┖鐩掑悕绉板垪琛?
	int32_t skyboxIndex = 3;              // 褰撳墠閫変腑鐨勫ぉ绌虹洅绱㈠紩
	
	// 娓叉煋妯″紡锛?=IBL, 1=鐞冭皭鍑芥暟
	int32_t renderMode = 1;
	std::vector<std::string> renderModeNames = {"IBL", "harmonics"};

    // 绾圭悊璧勬簮缁撴瀯浣?
	struct Textures {
		vks::TextureCubeMap environmentCube;     // 鐜璐村浘
		vks::TextureCubeMap environmentCube2;     // 绗簩鐜璐村浘
        vks::TextureCubeMap environmentCube3;     // 绗笁鐜璐村浘

		
		vks::Texture2D lutBrdf;                  // BRDF鏌ユ壘琛?
		vks::TextureCubeMap irradianceCube;      // 杈愬皠搴﹁创鍥?
		vks::TextureCubeMap prefilteredCube;     // 棰勮繃婊よ创鍥?
	} textures;

    // 妯″瀷璧勬簮缁撴瀯浣?
	struct Meshes {
		vkglTF::Model skybox;                    // 澶╃┖鐩掓ā鍨?
		std::vector<vkglTF::Model> objects;     // 鐗╀綋妯″瀷鍒楄〃
		int32_t objectIndex = 0;                // 褰撳墠閫変腑鐨勭墿浣撶储寮?
	} models;									// 澹版槑 models 鎴愬憳锛屽瓨鍌ㄦ墍鏈夋ā鍨嬭祫婧愩€?

    // Uniform缂撳啿鍖?
	struct {

		vks::Buffer object;      // 鐗╀綋uniform缂撳啿鍖?
		vks::Buffer skybox;      // 澶╃┖鐩抲niform缂撳啿鍖?
		vks::Buffer params;      // 鍙傛暟uniform缂撳啿鍖?
		vks::Buffer sh;			// SH绯绘暟Uniform缂撳啿鍖猴紝瀛樺偍鐞冭皭鍏夌収绯绘暟銆?
	} uniformBuffers;			// 澹版槑 uniformBuffers 鎴愬憳锛屽瓨鍌ㄦ墍鏈塙niform缂撳啿鍖?

    // 鐭╅樀uniform缁撴瀯浣?
	struct UBOMatrices {
		glm::mat4 projection;    // 鎶曞奖鐭╅樀
		glm::mat4 model;        // 妯″瀷鐭╅樀
		glm::mat4 view;         // 瑙嗗浘鐭╅樀
		glm::vec3 camPos;       // 鐩告満浣嶇疆
	} uboMatrices;				// 澹版槑 uboMatrices 鎴愬憳锛屽瓨鍌ㄧ煩闃垫暟鎹€?

    // 鍙傛暟uniform缁撴瀯浣?瀹氫箟 UBOParams 缁撴瀯浣擄紝瀛樺偍鍥涗釜鍏夋簮浣嶇疆銆佹洕鍏夊害鍜屼冀椹€笺€?
	// 缁撴瀯浣撲綔涓轰竴涓嫭绔嬪璞℃椂锛屾湯灏句笉闇€瑕侀澶栫殑濉厖锛岄櫎闈炲畠鍦ㄦ暟缁勬垨宓屽涓婁笅鏂囦腑闇€瑕佺‘淇濅笅涓€涓厓绱犵殑瀵归綈銆?
	//72 瀛楄妭琚涓烘槸鈥滄弧瓒?16 瀛楄妭瀵归綈鈥濈殑锛屽洜涓猴細鎵€鏈夋垚鍛樼殑鍋忕Щ閲忛兘绗﹀悎 std140 鐨勫榻愯鍒欍€?
	struct UBOParams {
		glm::vec4 lights[4];    // 鍏夋簮鍙傛暟
		float exposure = 4.5f;   // 鏇濆厜搴?
		float gamma = 2.2f;     // 浼介┈鍊?
	} uboParams;
	
    // 绠￠亾瀵硅薄 瀹氫箟娓叉煋绠＄嚎缁撴瀯浣擄紝鍖呭惈澶╃┖鐩掑拰 PBR 瀵硅薄鐨勭绾裤€?
	struct {
		VkPipeline skybox{ VK_NULL_HANDLE };    // 澶╃┖鐩掓覆鏌撶閬?
		VkPipeline pbr{ VK_NULL_HANDLE };      // PBR娓叉煋绠￠亾
		VkPipeline sh{ VK_NULL_HANDLE };      // 鐞冭皭鍑芥暟锛圫H锛夋覆鏌撶閬?
	} pipelines;

    // 鎻忚堪绗﹂泦 瀹氫箟鎻忚堪绗﹂泦缁撴瀯浣擄紝鍖呭惈瀵硅薄鍜屽ぉ绌虹洅鐨勬弿杩扮闆?
	struct {
		VkDescriptorSet object{ VK_NULL_HANDLE };     // 鐗╀綋鎻忚堪绗﹂泦
		VkDescriptorSet skybox{ VK_NULL_HANDLE };    // 澶╃┖鐩掓弿杩扮闆?
	
	} descriptorSets;
	// 瀹氫箟绠＄嚎甯冨眬鍜屾弿杩扮闆嗗竷灞€锛岀敤浜庣鐞嗙潃鑹插櫒璧勬簮缁戝畾
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };         // 绠￠亾甯冨眬
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE }; // 鎻忚堪绗﹂泦甯冨眬

	


    // 鏉愯川鐩稿叧鎴愬憳
	std::vector<Material> materials;           // 鏉愯川鍒楄〃
	int32_t materialIndex = 0;                 // 褰撳墠閫変腑鐨勬潗璐ㄧ储寮?

	// 瀹氫箟鏉愯川鍚嶇О鍜屽璞″悕绉板垪琛紝鐢ㄤ簬 UI 鏄剧ず銆?
	std::vector<std::string> materialNames;     // 鏉愯川鍚嶇О鍒楄〃
	std::vector<std::string> objectNames;      // 鐗╀綋鍚嶇О鍒楄〃

    /**
     * 鏋勯€犲嚱鏁?
     * 鍒濆鍖栫ず渚嬬殑鍩烘湰璁剧疆
     */
	VulkanExample() : VulkanExampleBase()
	{
		title = "IBL and SH lighting";  // 璁剧疆绐楀彛鏍囬

        // 璁剧疆鐩告満鍙傛暟
		camera.type = Camera::CameraType::firstperson;//璁剧疆鐩告満涓虹涓€浜虹О妯″紡銆?
		camera.movementSpeed = 4.0f;//璁剧疆鐩告満绉诲姩閫熷害涓?.0鍗曚綅/绉掋€?
		camera.setPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);//璁剧疆閫忚鎶曞奖锛岃鍦鸿60搴︼紝瀹介珮姣斿熀浜庣獥鍙ｅ昂瀵革紝杩戣鍓潰0.1锛岃繙瑁佸壀闈?56.0銆?
		camera.rotationSpeed = 0.25f;//璁剧疆鐩告満鏃嬭浆閫熷害涓?.25銆?

        // 璁剧疆鐩告満鍒濆浣嶇疆鍜屾湞鍚?
		camera.setRotation({ -3.75f, 180.0f, 0.0f });//璁剧疆鐩告満鍒濆鏃嬭浆瑙掑害锛堟鎷夎锛氬亸鑸?3.75搴︼紝
		camera.setPosition({ 0.55f, 0.85f, 12.0f });//璁剧疆鐩告満鍒濆浣嶇疆涓?0.55, 0.85, 12.0)銆?

		
        // 娣诲姞棰勫畾涔夌殑閲戝睘鏉愯川 "push_back" 鏄竴涓紪绋嬫湳璇紝鍦?C++ 涓壒鎸囧悜瀹瑰櫒鏈熬娣诲姞鍏冪礌鐨勬搷浣溿€?
		materials.push_back(Material("Gold", glm::vec3(1.0f, 0.765557f, 0.336057f)));
		materials.push_back(Material("Copper", glm::vec3(0.955008f, 0.637427f, 0.538163f)));
		materials.push_back(Material("Chromium", glm::vec3(0.549585f, 0.556114f, 0.554256f)));
		materials.push_back(Material("Nickel", glm::vec3(0.659777f, 0.608679f, 0.525649f)));
		materials.push_back(Material("Titanium", glm::vec3(0.541931f, 0.496791f, 0.449419f)));
		materials.push_back(Material("Cobalt", glm::vec3(0.662124f, 0.654864f, 0.633732f)));
		materials.push_back(Material("Platinum", glm::vec3(0.672411f, 0.637331f, 0.585456f)));
		materials.push_back(Material("White", glm::vec3(1.0f)));
		materials.push_back(Material("Dark", glm::vec3(0.1f)));
		materials.push_back(Material("Black", glm::vec3(0.0f)));
		materials.push_back(Material("Red", glm::vec3(1.0f, 0.0f, 0.0f)));
		materials.push_back(Material("Blue", glm::vec3(0.0f, 0.0f, 1.0f)));

		for (auto material : materials) {
			materialNames.push_back(material.name);
		}
		objectNames = { "Sphere", "Teapot", "Torusknot", "Venus" };
		// 璁剧疆榛樿鏉愯川绱㈠紩涓?8锛堝搴斺€淏lack鈥濇潗璐級銆?
		materialIndex = 8;
		// 鍒濆鍖栧ぉ绌虹洅鍚嶇О鍒楄〃锛堟棤澶╃┖鐩掋€丳isa銆丟rand Canyon銆乁ffizi锛夛紝榛樿閫夋嫨绱㈠紩 1锛圥isa锛夈€?
		skyboxNames = {"NO Skybox", "Pisa", "Grand Canyon","uffizi_cube"};
		
	}
		// 鏋愭瀯鍑芥暟锛岄噴鏀?Vulkan 璧勬簮锛屽寘鎷绾裤€佺绾垮竷灞€銆佹弿杩扮闆嗗竷灞€銆佺粺涓€缂撳啿鍖哄拰绾圭悊銆?
	~VulkanExample()
	{
		if (device) {
			vkDestroyPipeline(device, pipelines.skybox, nullptr);
			vkDestroyPipeline(device, pipelines.pbr, nullptr);
			vkDestroyPipeline(device, pipelines.sh, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
			uniformBuffers.object.destroy();
			uniformBuffers.skybox.destroy();
			uniformBuffers.params.destroy();
			uniformBuffers.sh.destroy();
			textures.environmentCube.destroy();
			textures.environmentCube2.destroy();
            textures.environmentCube3.destroy();
            
            
			textures.irradianceCube.destroy();
			textures.prefilteredCube.destroy();
			textures.lutBrdf.destroy();
		}
	}
	// 鑾峰彇鍚敤鐨勫姛鑳斤紝妫€鏌ヨ澶囨槸鍚︽敮鎸侀噰鏍峰櫒鍚勫悜寮傛€с€?
	virtual void getEnabledFeatures()
	{
		if (deviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
	}
	// 瀹氫箟鍛戒护缂撳啿鍖哄紑濮嬩俊鎭紝鐢ㄤ簬鍒濆鍖栧懡浠ょ紦鍐插尯褰曞埗銆?
	void buildCommandBuffers()
	{
		// 鍒濆鍖栧懡浠ょ紦鍐插尯寮€濮嬩俊鎭€?
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.1f, 0.1f, 0.1f, 1.0f } };  // 棰滆壊缂撳啿鍖烘竻闄や负娣辩伆鑹诧紙0.1, 0.1, 0.1, 1.0锛?
		clearValues[1].depthStencil = { 1.0f, 0 };             	// 娣卞害缂撳啿鍖烘竻闄や负 1.0锛屾ā鏉跨紦鍐插尯娓呴櫎涓?0

		// 閰嶇疆娓叉煋閫氶亾寮€濮嬩俊鎭紝鎸囧畾娓叉煋閫氶亾銆佹覆鏌撳尯鍩燂紙鍏ㄥ睆锛夈€佹竻闄ゅ€肩瓑銆?
		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;              
		renderPassBeginInfo.renderArea.offset.x = 0;            
		renderPassBeginInfo.renderArea.offset.y = 0;            
		renderPassBeginInfo.renderArea.extent.width = width;    
		renderPassBeginInfo.renderArea.extent.height = height;  
		renderPassBeginInfo.clearValueCount = 2;                
		renderPassBeginInfo.pClearValues = clearValues;         

		
		for (size_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// 閬嶅巻鎵€鏈夌粯鍒跺懡浠ょ紦鍐插尯锛岃缃綋鍓嶅抚缂撳啿鍖恒€?
			// 灏嗗綋鍓嶅抚鐨勬覆鏌撶洰鏍囩粦瀹氬埌娓叉煋閫氶亾 drawCmdBuffers[i] 鏄綍鍒舵覆鏌撴寚浠ょ殑瀹瑰櫒锛屼笌 frameBuffers[i] 涓€涓€缁戝畾
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			// 寮€濮嬪綍鍒跺懡浠ょ紦鍐插尯锛屾鏌?Vulkan API 璋冪敤鏄惁鎴愬姛銆?
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));
			// 寮€濮嬫覆鏌撻€氶亾锛屾寚瀹氭覆鏌撳懡浠ゅ唴鑱旀墽琛?
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			// 璁剧疆璁剧疆瑙嗗彛锛岃鐩栨暣涓獥鍙ｏ紝鎸囧畾瀹藉害銆侀珮搴﹀拰娣卞害鑼冨洿锛?.0 鍒?1.0锛?
			VkViewport viewport = vks::initializers::viewport((float)width,	(float)height, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			// 璁剧疆瑁佸壀鐭╁舰锛岃鐩栨暣涓獥鍙?
			VkRect2D scissor = vks::initializers::rect2D(width,	height,	0, 0);
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

			// 濡傛灉閫夋嫨浜嗗ぉ绌虹洅锛坰kyboxIndex > 0锛夛紝缁戝畾澶╃┖鐩掓弿杩扮闆嗗拰绠＄嚎锛岀粯鍒跺ぉ绌虹洅妯″瀷
			if (skyboxIndex > 0) {
				// 缁戝畾瀵硅薄鐨勬弿杩扮闆嗭紝鐢ㄤ簬娓叉煋 3D 瀵硅薄
				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.skybox, 0, NULL);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
				models.skybox.draw(drawCmdBuffers[i]);
			}

			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets.object, 0, NULL);
			// 鏍规嵁娓叉煋妯″紡锛圛BL 鎴栫悆璋愬嚱鏁帮級缁戝畾 PBR 绠＄嚎锛堝綋鍓嶄唬鐮佷腑涓ょ妯″紡閮戒娇鐢ㄧ浉鍚岀殑 PBR 绠＄嚎锛屾槸寰呭疄鐜扮殑閫昏緫锛夈€?
			if (renderMode == 0) {
				// IBL娓叉煋妯″紡
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
			} else {
				// 鐞冭皭鍑芥暟娓叉煋妯″紡
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.sh); 
			}
			// vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);

			// 寰幆娓叉煋 10 涓璞★紝娌?X 杞存帓鍒楋紝鍔ㄦ€佽皟鏁寸矖绯欏害鍜岄噾灞炲害锛堜粠宸﹀埌鍙抽€愭笎鍙樺寲锛夈€?
			Material mat = materials[materialIndex];
			const uint32_t objcount = 10;
			for (uint32_t x = 0; x < objcount; x++) {
				glm::vec3 pos = glm::vec3(float(x - (objcount / 2.0f)) * 2.15f, 0.0f, 0.0f);
				mat.params.roughness = 1.0f-glm::clamp((float)x / (float)objcount, 0.005f, 1.0f);
				mat.params.metallic = glm::clamp((float)x / (float)objcount, 0.005f, 1.0f);
				// 閫氳繃鎺ㄩ€佸父閲忓皢瀵硅薄浣嶇疆浼犻€掔粰椤剁偣鐫€鑹插櫒锛屾潗璐ㄥ弬鏁颁紶閫掔粰鐗囨鐫€鑹插櫒銆?
				// 鎺ㄩ€佸父閲忥紙Push Constants锛夋満鍒?
				// 浣滅敤锛氶珮鏁堜紶閫掑皬鍧楁暟鎹埌鐫€鑹插櫒锛屾棤闇€鎻忚堪绗﹂泦锛圖escriptor Sets锛夋垨 Uniform 缂撳啿鍖恒€?
				// 浼樺娍锛氫綆寮€閿€锛岄€傚悎姣忓抚棰戠箒鏇存柊鐨勬暟鎹紙濡傛ā鍨嬩綅缃€佹潗璐ㄥ弬鏁帮級銆?
				vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec3), &pos);
				vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::vec3), sizeof(Material::PushBlock), &mat);
				// 缁樺埗褰撳墠閫変腑鐨勫璞℃ā鍨嬨€?
				models.objects[models.objectIndex].draw(drawCmdBuffers[i]);

			}
			// 缁樺埗鐢ㄦ埛鐣岄潰锛圲I锛夛紝鍖呭惈鏉愯川閫夋嫨銆佸璞￠€夋嫨绛夋帶浠躲€?
			drawUI(drawCmdBuffers[i]);
			// 缁撴潫娓叉煋閫氶亾
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void loadAssets()
	{
		// 瀹氫箟 glTF 妯″瀷鍔犺浇鏍囧織锛岄鍙樻崲椤剁偣骞剁炕杞?Y 杞达紙閫傞厤 Vulkan 鍧愭爣绯伙級銆?
		uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::FlipY;
		// 鍔犺浇澶╃┖鐩掓ā鍨嬶紙绔嬫柟浣?glTF 鏂囦欢锛夈€?
		models.skybox.loadFromFile(getAssetPath() + "models/cube.gltf", vulkanDevice, queue, glTFLoadingFlags);
		// 鍔犺浇瀵硅薄妯″瀷锛堢悆浣撱€佽尪澹躲€佺幆闈㈢粨銆侀噾鏄熼洉鍍忥級銆?
		std::vector<std::string> filenames = { "sphere.gltf", "teapot.gltf", "torusknot.gltf", "venus.gltf" };
		models.objects.resize(filenames.size());
		for (size_t i = 0; i < filenames.size(); i++) {
			models.objects[i].loadFromFile(getAssetPath() + "models/" + filenames[i], vulkanDevice, queue, glTFLoadingFlags);
		}
		// HDR cubemap 鍔犺浇鐜绔嬫柟浣撹创鍥撅紙Pisa銆丟rand Canyon銆乁ffizi锛夛紝浣跨敤 16 浣嶆诞鐐规牸寮?
		textures.environmentCube.loadFromFile(getAssetPath() + "textures/hdr/pisa_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		textures.environmentCube2.loadFromFile(getAssetPath() + "textures/hdr/gcanyon_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
        textures.environmentCube3.loadFromFile(getAssetPath() + "textures/hdr/uffizi_cube.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		// textures.environmentCube4.loadFromFile(getAssetPath() + "textures/hdr/grace_cross.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);
		// textures.environmentCube5.loadFromFile(getAssetPath() + "textures/hdr/rnl_cross.ktx", VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice, queue);

	}

	void setupDescriptors()
	{
		// Descriptor Pool 瀹氫箟鎻忚堪绗︽睜澶у皬锛屽垎閰?4 涓粺涓€缂撳啿鍖哄拰 6 涓粍鍚堝浘鍍忛噰鏍峰櫒銆?
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)
		};
		// 鍒涘缓鎻忚堪绗︽睜锛屽垎閰?3 涓弿杩扮闆嗐€?
		// 4 涓粺涓€缂撳啿鍖哄拰 6 涓粍鍚堝浘鍍忛噰鏍峰櫒琚垎閰嶇粰涓嬮潰涓や釜绗﹂泦
		// 鐗╀綋娓叉煋鎻忚堪绗﹂泦 ( descriptorSets.object )
		// 缁戝畾澶╃┖鐩掔汗鐞嗗拰閲囨牱鍣ㄣ€?
		VkDescriptorPoolCreateInfo descriptorPoolInfo =	vks::initializers::descriptorPoolCreateInfo(poolSizes, 3);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Descriptor set layout 
		// 瀹氫箟鎻忚堪绗﹂泦甯冨眬缁戝畾锛屽寘鎷?3 涓粺涓€缂撳啿鍖猴紙鐭╅樀鍜屽弬鏁帮級鍜?3 涓浘鍍忛噰鏍峰櫒锛堣緪鐓у害銆丅RDF銆侀杩囨护璐村浘锛夈€?
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),  // SH coefficients
		};
		// 鍒涘缓鎻忚堪绗﹂泦甯冨眬
		VkDescriptorSetLayoutCreateInfo descriptorLayout = 	vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

		// Descriptor sets 涓哄璞″垎閰嶆弿杩扮闆嗐€傚垵濮嬪寲鎻忚堪绗﹂泦鍒嗛厤淇℃伅锛屽垎閰?涓弿杩扮闆嗐€?
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

		// Objects 閰嶇疆瀵硅薄鎻忚堪绗﹂泦锛岀粦瀹氱粺涓€缂撳啿鍖哄拰绾圭悊璧勬簮锛屽苟鏇存柊鎻忚堪绗﹂泦
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.object));
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.object.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.irradianceCube.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &textures.lutBrdf.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &textures.prefilteredCube.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.object, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &uniformBuffers.sh.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		// Sky box 涓哄ぉ绌虹洅鍒嗛厤鎻忚堪绗﹂泦锛岀粦瀹氬ぉ绌虹洅缁熶竴缂撳啿鍖哄拰鐜璐村浘锛屽苟鏇存柊鎻忚堪绗﹂泦銆?
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.skybox));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.skybox, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.skybox.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.skybox, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &uniformBuffers.params.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.skybox, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.environmentCube.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void preparePipelines()
	{
		// 閰嶇疆杈撳叆缁勮鐘舵€侊紝浣跨敤涓夎褰㈠垪琛ㄦ嫇鎵戙€?
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		// 閰嶇疆鍏夋爡鍖栫姸鎬侊紝濉厖妯″紡锛屾棤鑳岄潰鍓旈櫎锛岄€嗘椂閽堜负姝ｉ潰銆?
		VkPipelineRasterizationStateCreateInfo rasterizationState =
			vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		// 閰嶇疆棰滆壊娣峰悎鐘舵€侊紝绂佺敤娣峰悎	
		VkPipelineColorBlendAttachmentState blendAttachmentState =
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		// 閰嶇疆棰滆壊娣峰悎鐘舵€侊紝鎸囧畾涓€涓檮浠躲€?
		VkPipelineColorBlendStateCreateInfo colorBlendState =
			vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		// 閰嶇疆娣卞害鍜屾ā鏉跨姸鎬侊紝鍒濆绂佺敤娣卞害娴嬭瘯鍜屽啓鍏ャ€?
		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		// 閰嶇疆瑙嗗彛鐘舵€侊紝鎸囧畾涓€涓鍙ｅ拰瑁佸壀鐭╁舰銆?
		VkPipelineViewportStateCreateInfo viewportState =
			vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		// 閰嶇疆澶氶噸閲囨牱鐘舵€侊紝绂佺敤澶氶噸閲囨牱锛堝崟閲囨牱锛夈€?
		VkPipelineMultisampleStateCreateInfo multisampleState =
			vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		// 瀹氫箟鍔ㄦ€佺姸鎬侊紝鍖呮嫭瑙嗗彛鍜岃鍓煩褰€?
		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		// 閰嶇疆鍔ㄦ€佺姸鎬併€?
		VkPipelineDynamicStateCreateInfo dynamicState =
			vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

		// Pipeline layout 鍒濆鍖栫绾垮竷灞€鍒涘缓淇℃伅锛屾寚瀹氭弿杩扮闆嗗竷灞€銆?
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		// Push constant ranges 瀹氫箟鎺ㄩ€佸父閲忚寖鍥达細
		std::vector<VkPushConstantRange> pushConstantRanges = {
			// 绗竴涓寖鍥达細椤剁偣鐫€鑹插櫒锛屼紶杈?glm::vec3锛堢墿浣撲綅缃級锛屽亸绉?銆?
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::vec3), 0),
			// 绗簩涓寖鍥达細鐗囨鐫€鑹插櫒锛屼紶杈?Material::PushBlock锛堟潗璐ㄥ弬鏁帮級锛屽亸绉?sizeof(glm::vec3)銆?
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Material::PushBlock), sizeof(glm::vec3)),

		};
		// 鎸囧畾涓や釜鎺ㄩ€佸父閲忚寖鍥淬€?
		pipelineLayoutCreateInfo.pushConstantRangeCount = 2; 
		// 璁剧疆鎺ㄩ€佸父閲忚寖鍥淬€?
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		// 鍒涘缓绠＄嚎甯冨眬銆?
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		// 瀹氫箟涓や釜鐫€鑹插櫒闃舵锛堥《鐐瑰拰鐗囨锛夈€?
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		// Pipelines
		// 鍒濆鍖栧浘褰㈢绾垮垱寤轰俊鎭紝鎸囧畾绠＄嚎甯冨眬鍜屾覆鏌撻€氶亾銆?
		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		// 璁剧疆杈撳叆缁勮鐘舵€併€?
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		// 璁剧疆鍏夋爡鍖栫姸鎬併€?
		pipelineCI.pRasterizationState = &rasterizationState;
		// 璁剧疆棰滆壊娣峰悎鐘舵€併€?
		pipelineCI.pColorBlendState = &colorBlendState;	
		// 璁剧疆娣卞害鍜屾ā鏉跨姸鎬併€?
		pipelineCI.pMultisampleState = &multisampleState;
		// 璁剧疆瑙嗗彛鐘舵€併€?
		pipelineCI.pViewportState = &viewportState;
		// 璁剧疆娣卞害鍜屾ā鏉跨姸鎬併€?
		pipelineCI.pDepthStencilState = &depthStencilState;
		// 璁剧疆鍔ㄦ€佺姸鎬併€?
		pipelineCI.pDynamicState = &dynamicState;
		// 璁剧疆鐫€鑹插櫒闃舵鏁伴噺锛?锛夈€?
		pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
		// 璁剧疆鐫€鑹插櫒闃舵鏁扮粍銆?
		pipelineCI.pStages = shaderStages.data();
		// 璁剧疆椤剁偣杈撳叆鐘舵€侊紝鍖呮嫭浣嶇疆銆佹硶绾垮拰UV鍧愭爣銆?
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		// Skybox pipeline (background cube)

		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.skybox));

		// PBR pipeline using IBL
		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/lightprobesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.pbr));
		// SH pipeline (used for the light probe)
		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/lightprobesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/lightprobesh_sh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		// Enable depth test and write
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthTestEnable = VK_TRUE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.sh));
			
			
	}

	// Generate a BRDF integration map used as a look-up-table (stores roughness / NdotV)
	// 鐢熸垚BRDF鏌ユ壘琛紙LUT锛夛紝鐢ㄤ簬PBR闀滈潰鍙嶅皠璁＄畻銆?
	void generateBRDFLUT()
	{
		// 璁板綍寮€濮嬫椂闂达紝鐢ㄤ簬璁＄畻鐢熸垚鏃堕棿銆?
		auto tStart = std::chrono::high_resolution_clock::now();
		// 瀹氫箟BRDF LUT鐨勬牸寮忎负16浣嶆诞鐐筊G锛堢孩缁块€氶亾锛夈€?
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;	// R16G16 is supported pretty much everywhere
		// 瀹氫箟BRDF LUT鐨勫昂瀵镐负512x512鍍忕礌
		const int32_t dim = 512;
		// Image 鍒涘缓BRDF LUT绾圭悊銆?
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();// 鍒濆鍖栧浘鍍忓垱寤轰俊鎭€?
		imageCI.imageType = VK_IMAGE_TYPE_2D;// 璁剧疆鍥惧儚绫诲瀷涓?D銆?
		imageCI.format = format;	// 璁剧疆鍥惧儚鏍煎紡涓篟16G16_SFLOAT銆?
		imageCI.extent.width = dim;	// 璁剧疆鍥惧儚瀹藉害涓?12銆?
		imageCI.extent.height = dim;// 璁剧疆鍥惧儚楂樺害涓?12銆?
		imageCI.extent.depth = 1;	// 璁剧疆鍥惧儚娣卞害涓?銆?
		imageCI.mipLevels = 1;		// 璁剧疆鍥惧儚Mipmap绾у埆涓?銆傛棤MIP鏄犲皠
		imageCI.arrayLayers = 1;	// 璁剧疆鍥惧儚鏁扮粍灞備负1銆?
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT; // 璁剧疆鍗曢噰鏍凤紙鏃犲閲嶉噰鏍凤級銆?
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;// 璁剧疆鍥惧儚骞抽摵鏂瑰紡涓烘渶浣筹紙GPU浼樺寲锛夈€?
		imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;	// 璁剧疆鍥惧儚鐢ㄩ€斾负棰滆壊闄勪欢鍜岄噰鏍枫€?
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.lutBrdf.image)); // 鍒涘缓BRDF LUT鍥惧儚銆?
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo(); // 鍒濆鍖栧唴瀛樺垎閰嶄俊鎭?
		VkMemoryRequirements memReqs;											 // 瀹氫箟鍙橀噺瀛樺偍鍥惧儚鍐呭瓨闇€姹傘€?
		vkGetImageMemoryRequirements(device, textures.lutBrdf.image, &memReqs);	 // 鑾峰彇鍥惧儚鍐呭瓨闇€姹傘€?
		memAlloc.allocationSize = memReqs.size;									 // 璁剧疆鍐呭瓨鍒嗛厤澶у皬涓哄浘鍍忓唴瀛橀渶姹傚ぇ灏忋€?
		// 閫夋嫨閫傚悎鐨勫唴瀛樼被鍨嬶紙璁惧鏈湴锛夈€?
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		// 鍒嗛厤鍐呭瓨銆?
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.lutBrdf.deviceMemory));
		// 灏嗗唴瀛樼粦瀹氬埌BRDF LUT鍥惧儚銆?
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.lutBrdf.image, textures.lutBrdf.deviceMemory, 0));
		// Image view 鍒濆鍖栧浘鍍忚鍥惧垱寤轰俊鎭€?
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;// 璁剧疆瑙嗗浘绫诲瀷涓?D銆?
		viewCI.format = format;					// 璁剧疆瑙嗗浘鏍煎紡涓篟16G16_SFLOAT銆?
		viewCI.subresourceRange = {};			// 鍒濆鍖栧瓙璧勬簮鑼冨洿銆?
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;// 璁剧疆瀛愯祫婧愪负棰滆壊闄勪欢銆?
		viewCI.subresourceRange.levelCount = 1;//  璁剧疆MIP绾у埆鏁颁负1銆?
		viewCI.subresourceRange.layerCount = 1;//  璁剧疆灞傛暟涓?銆?
		viewCI.image = textures.lutBrdf.image;//   鍏宠仈BRDF LUT鍥惧儚銆?
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.lutBrdf.view));// 鍒涘缓BRDF LUT鍥惧儚瑙嗗浘銆?
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();// 鍒濆鍖栭噰鏍峰櫒鍒涘缓淇℃伅銆?
		samplerCI.magFilter = VK_FILTER_LINEAR;// 璁剧疆鏀惧ぇ杩囨护涓虹嚎鎬с€?
		samplerCI.minFilter = VK_FILTER_LINEAR;// 璁剧疆缂╁皬杩囨护涓虹嚎鎬с€?
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;// 璁剧疆MIP鏄犲皠妯″紡涓虹嚎鎬с€?
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 璁剧疆U鏂瑰悜绾圭悊瀵诲潃涓鸿竟缂樺す绱с€?
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 璁剧疆V鏂瑰悜绾圭悊瀵诲潃涓鸿竟缂樺す绱с€?
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;// 璁剧疆W鏂瑰悜绾圭悊瀵诲潃涓鸿竟缂樺す绱с€?
		samplerCI.minLod = 0.0f;// 璁剧疆鏈€灏廘OD涓?銆?
		samplerCI.maxLod = 1.0f;// 璁剧疆鏈€澶OD涓?銆?
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;// 璁剧疆杈圭晫棰滆壊涓轰笉閫忔槑鐧借壊銆傘€?
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.lutBrdf.sampler));// 鍒涘缓BRDF LUT閲囨牱鍣ㄣ€?

		textures.lutBrdf.descriptor.imageView = textures.lutBrdf.view;// 璁剧疆BRDF LUT鎻忚堪绗︾殑鍥惧儚瑙嗗浘銆?
		textures.lutBrdf.descriptor.sampler = textures.lutBrdf.sampler;// 璁剧疆BRDF LUT鎻忚堪绗︾殑閲囨牱鍣ㄣ€?
		textures.lutBrdf.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// 璁剧疆鍥惧儚甯冨眬涓虹潃鑹插櫒鍙銆?
		textures.lutBrdf.device = vulkanDevice;// 鍏宠仈Vulkan璁惧銆?

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};// 鍒濆鍖栭檮浠舵弿杩般€?
		// Color attachment
		attDesc.format = format; // 璁剧疆闄勪欢鏍煎紡涓篟16G16_SFLOAT銆?
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;// 璁剧疆鍗曢噰鏍枫€?
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;// 鍔犺浇鏃舵竻闄ら檮浠躲€?
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;// 瀛樺偍闄勪欢鏁版嵁銆?
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;// 蹇界暐妯℃澘鍔犺浇銆?
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;// 蹇界暐妯℃澘瀛樺偍銆?
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;// 鍒濆甯冨眬涓烘湭瀹氫箟銆?
		attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;// 鏈€缁堝竷灞€涓虹潃鑹插櫒鍙銆?
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };// 瀹氫箟棰滆壊闄勪欢寮曠敤锛岀储寮?锛屽竷灞€涓洪鑹查檮浠躲€?

		VkSubpassDescription subpassDescription = {};// 鍒濆鍖栧瓙閫氶亾鎻忚堪
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;// 璁剧疆瀛愰€氶亾涓哄浘褰㈢绾裤€?
		subpassDescription.colorAttachmentCount = 1; // 璁剧疆涓€涓鑹查檮浠躲€?
		subpassDescription.pColorAttachments = &colorReference;// 鍏宠仈棰滆壊闄勪欢寮曠敤銆?

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;// 瀹氫箟涓や釜瀛愰€氶亾渚濊禆銆?
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create the actual renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();

		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		VkFramebufferCreateInfo framebufferCI = vks::initializers::framebufferCreateInfo();
		framebufferCI.renderPass = renderpass;
		framebufferCI.attachmentCount = 1;
		framebufferCI.pAttachments = &textures.lutBrdf.view;
		framebufferCI.width = dim;
		framebufferCI.height = dim;
		framebufferCI.layers = 1;

		VkFramebuffer framebuffer;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCI, nullptr, &framebuffer));

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));

		// Pipeline layout
		VkPipelineLayout pipelinelayout;
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.pVertexInputState = &emptyInputState;

		// Look-up-table (from BRDF) pipeline
		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/genbrdflut.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/genbrdflut.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;
		renderPassBeginInfo.framebuffer = framebuffer;

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);
		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
		vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdDraw(cmdBuf, 3, 1, 0, 0);
		vkCmdEndRenderPass(cmdBuf);
		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkQueueWaitIdle(queue);

		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
	}

	// Generate an irradiance cube map from the environment cube map
	void generateIrradianceCube()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		const int32_t dim = 64;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.irradianceCube.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.irradianceCube.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.irradianceCube.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.irradianceCube.image, textures.irradianceCube.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = textures.irradianceCube.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.irradianceCube.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.irradianceCube.sampler));

		textures.irradianceCube.descriptor.imageView = textures.irradianceCube.view;
		textures.irradianceCube.descriptor.sampler = textures.irradianceCube.sampler;
		textures.irradianceCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.irradianceCube.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();
		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offscreen framebuffer
		{
			// Color attachment
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent.width = dim;
			imageCreateInfo.extent.height = dim;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = format;
			colorImageView.flags = 0;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreen.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

			VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::tools::setImageLayout(
				layoutCmd,
				offscreen.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.environmentCube.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Pipeline layout
		struct PushBlock {
			glm::mat4 mvp;
			// Sampling deltas
			float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
			float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
		} pushBlock;

		VkPipelineLayout pipelinelayout;
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderpass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		shaderStages[0] = loadShader(getShadersPath() + "lightprobesh/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "lightprobesh/irradiancecube.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vks::tools::setImageLayout(
			cmdBuf,
			textures.irradianceCube.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

				models.skybox.draw(cmdBuf);

				vkCmdEndRenderPass(cmdBuf);

				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					textures.irradianceCube.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back
				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}

		vks::tools::setImageLayout(
			cmdBuf,
			textures.irradianceCube.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
		vkFreeMemory(device, offscreen.memory, nullptr);
		vkDestroyImageView(device, offscreen.view, nullptr);
		vkDestroyImage(device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating irradiance cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
	}

	// Prefilter environment cubemap
	// See https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
	void generatePrefilteredCube()
	{
		auto tStart = std::chrono::high_resolution_clock::now();

		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI = vks::initializers::imageCreateInfo();
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageCI, nullptr, &textures.prefilteredCube.image));
		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, textures.prefilteredCube.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &textures.prefilteredCube.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, textures.prefilteredCube.image, textures.prefilteredCube.deviceMemory, 0));
		// Image view
		VkImageViewCreateInfo viewCI = vks::initializers::imageViewCreateInfo();
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = textures.prefilteredCube.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewCI, nullptr, &textures.prefilteredCube.view));
		// Sampler
		VkSamplerCreateInfo samplerCI = vks::initializers::samplerCreateInfo();
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerCI, nullptr, &textures.prefilteredCube.sampler));

		textures.prefilteredCube.descriptor.imageView = textures.prefilteredCube.view;
		textures.prefilteredCube.descriptor.sampler = textures.prefilteredCube.sampler;
		textures.prefilteredCube.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		textures.prefilteredCube.device = vulkanDevice;

		// FB, Att, RP, Pipe, etc.
		VkAttachmentDescription attDesc = {};
		// Color attachment
		attDesc.format = format;
		attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
		attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpassDescription = {};
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorReference;

		// Use subpass dependencies for layout transitions
		std::array<VkSubpassDependency, 2> dependencies;
		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Renderpass
		VkRenderPassCreateInfo renderPassCI = vks::initializers::renderPassCreateInfo();
		renderPassCI.attachmentCount = 1;
		renderPassCI.pAttachments = &attDesc;
		renderPassCI.subpassCount = 1;
		renderPassCI.pSubpasses = &subpassDescription;
		renderPassCI.dependencyCount = 2;
		renderPassCI.pDependencies = dependencies.data();
		VkRenderPass renderpass;
		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

		struct {
			VkImage image;
			VkImageView view;
			VkDeviceMemory memory;
			VkFramebuffer framebuffer;
		} offscreen;

		// Offfscreen framebuffer
		{
			// Color attachment
			VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = format;
			imageCreateInfo.extent.width = dim;
			imageCreateInfo.extent.height = dim;
			imageCreateInfo.extent.depth = 1;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

			VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
			memAlloc.allocationSize = memReqs.size;
			memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
			VK_CHECK_RESULT(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

			VkImageViewCreateInfo colorImageView = vks::initializers::imageViewCreateInfo();
			colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
			colorImageView.format = format;
			colorImageView.flags = 0;
			colorImageView.subresourceRange = {};
			colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			colorImageView.subresourceRange.baseMipLevel = 0;
			colorImageView.subresourceRange.levelCount = 1;
			colorImageView.subresourceRange.baseArrayLayer = 0;
			colorImageView.subresourceRange.layerCount = 1;
			colorImageView.image = offscreen.image;
			VK_CHECK_RESULT(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = renderpass;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.pAttachments = &offscreen.view;
			fbufCreateInfo.width = dim;
			fbufCreateInfo.height = dim;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

			VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::tools::setImageLayout(
				layoutCmd,
				offscreen.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);
		}

		// Descriptors
		VkDescriptorSetLayout descriptorsetlayout;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

		// Descriptor Pool
		std::vector<VkDescriptorPoolSize> poolSizes = { vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };
		VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		VkDescriptorPool descriptorpool;
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

		// Descriptor sets
		VkDescriptorSet descriptorset;
		VkDescriptorSetAllocateInfo allocInfo =	vks::initializers::descriptorSetAllocateInfo(descriptorpool, &descriptorsetlayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &textures.environmentCube.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		// Pipeline layout
		struct PushBlock {
			glm::mat4 mvp;
			float roughness;
			uint32_t numSamples = 32u;
		} pushBlock;

		VkPipelineLayout pipelinelayout;
		std::vector<VkPushConstantRange> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PushBlock), 0),
		};
		VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorsetlayout, 1);
		pipelineLayoutCI.pushConstantRangeCount = 1;
		pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

		// Pipeline
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::pipelineCreateInfo(pipelinelayout, renderpass);
		pipelineCI.pInputAssemblyState = &inputAssemblyState;
		pipelineCI.pRasterizationState = &rasterizationState;
		pipelineCI.pColorBlendState = &colorBlendState;
		pipelineCI.pMultisampleState = &multisampleState;
		pipelineCI.pViewportState = &viewportState;
		pipelineCI.pDepthStencilState = &depthStencilState;
		pipelineCI.pDynamicState = &dynamicState;
		pipelineCI.stageCount = 2;
		pipelineCI.pStages = shaderStages.data();
		pipelineCI.renderPass = renderpass;
		pipelineCI.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::UV });

		shaderStages[0] = loadShader(getShadersPath() + "pbribl/filtercube.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "pbribl/prefilterenvmap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VkPipeline pipeline;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

		// Render

		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = renderpass;
		renderPassBeginInfo.framebuffer = offscreen.framebuffer;
		renderPassBeginInfo.renderArea.extent.width = dim;
		renderPassBeginInfo.renderArea.extent.height = dim;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		std::vector<glm::mat4> matrices = {
			// POSITIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_X
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Y
			glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// POSITIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
			// NEGATIVE_Z
			glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
		};

		VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport = vks::initializers::viewport((float)dim, (float)dim, 0.0f, 1.0f);
		VkRect2D scissor = vks::initializers::rect2D(dim, dim, 0, 0);

		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = numMips;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vks::tools::setImageLayout(
			cmdBuf,
			textures.prefilteredCube.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t m = 0; m < numMips; m++) {
			pushBlock.roughness = (float)m / (float)(numMips - 1);
			for (uint32_t f = 0; f < 6; f++) {
				viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
				viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
				vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

				// Render scene from cube face's point of view
				vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				// Update shader push constant block
				pushBlock.mvp = glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[f];

				vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushBlock), &pushBlock);

				vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
				vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0, NULL);

				models.skybox.draw(cmdBuf);

				vkCmdEndRenderPass(cmdBuf);

				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

				// Copy region for transfer from framebuffer to cube face
				VkImageCopy copyRegion = {};

				copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.srcSubresource.baseArrayLayer = 0;
				copyRegion.srcSubresource.mipLevel = 0;
				copyRegion.srcSubresource.layerCount = 1;
				copyRegion.srcOffset = { 0, 0, 0 };

				copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				copyRegion.dstSubresource.baseArrayLayer = f;
				copyRegion.dstSubresource.mipLevel = m;
				copyRegion.dstSubresource.layerCount = 1;
				copyRegion.dstOffset = { 0, 0, 0 };

				copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
				copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
				copyRegion.extent.depth = 1;

				vkCmdCopyImage(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					textures.prefilteredCube.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&copyRegion);

				// Transform framebuffer color attachment back
				vks::tools::setImageLayout(
					cmdBuf,
					offscreen.image,
					VK_IMAGE_ASPECT_COLOR_BIT,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			}
		}

		vks::tools::setImageLayout(
			cmdBuf,
			textures.prefilteredCube.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->flushCommandBuffer(cmdBuf, queue);

		vkDestroyRenderPass(device, renderpass, nullptr);
		vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
		vkFreeMemory(device, offscreen.memory, nullptr);
		vkDestroyImageView(device, offscreen.view, nullptr);
		vkDestroyImage(device, offscreen.image, nullptr);
		vkDestroyDescriptorPool(device, descriptorpool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		std::cout << "Generating pre-filtered enivornment cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Object vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.object,
			sizeof(uboMatrices)));

		// Skybox vertex shader uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.skybox,
			sizeof(uboMatrices)));

		// Shared parameter uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.params,
			sizeof(uboParams)));
		// SH uniform buffer
		VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.sh,
			sizeof(SHCoefficients))); // 鏄惧紡鎸囧畾 144 瀛楄妭
		
		// Map persistent
		VK_CHECK_RESULT(uniformBuffers.object.map());
		VK_CHECK_RESULT(uniformBuffers.skybox.map());
		VK_CHECK_RESULT(uniformBuffers.params.map());
		VK_CHECK_RESULT(uniformBuffers.sh.map());
		updateUniformBuffers();
		updateParams();
	}

	void updateUniformBuffers()
	{
		// 3D object
		uboMatrices.projection = camera.matrices.perspective;
		uboMatrices.view = camera.matrices.view;
		uboMatrices.model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f + (models.objectIndex == 1 ? 45.0f : 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
		uboMatrices.camPos = camera.position * -1.0f;
		memcpy(uniformBuffers.object.mapped, &uboMatrices, sizeof(uboMatrices));
		// Skybo-x
		uboMatrices.model = glm::mat4(glm::mat3(camera.matrices.view));

		memcpy(uniformBuffers.skybox.mapped, &uboMatrices, sizeof(uboMatrices));
	}

	void updateParams()
	{
		const float p = 15.0f;
		uboParams.lights[0] = glm::vec4(-p, -p*0.5f, -p, 1.0f);
		uboParams.lights[1] = glm::vec4(-p, -p*0.5f,  p, 1.0f);
		uboParams.lights[2] = glm::vec4( p, -p*0.5f,  p, 1.0f);
		uboParams.lights[3] = glm::vec4( p, -p*0.5f, -p, 1.0f);

		memcpy(uniformBuffers.params.mapped, &uboParams, sizeof(uboParams));
	}

	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		generateBRDFLUT();
		generateIrradianceCube();
		generatePrefilteredCube();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		loadSkyboxTexture();
		prepared = true;
	}

	virtual void render()
	{
		if (!prepared)
			return;
		updateUniformBuffers();
		draw();
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	virtual void loadSkyboxTexture() {
		// 鏍规嵁 skyboxIndex 閫夋嫨姝ｇ‘鐨勭幆澧冭创鍥?
		VkDescriptorImageInfo* currentSkyboxDescriptor = nullptr;
		switch(skyboxIndex) {
        case 0: currentSkyboxDescriptor = nullptr; break;
        case 1: currentSkyboxDescriptor = &textures.environmentCube.descriptor; break;
        case 2: currentSkyboxDescriptor = &textures.environmentCube2.descriptor; break;
        case 3: currentSkyboxDescriptor = &textures.environmentCube3.descriptor; break;
    }
		if (currentSkyboxDescriptor) {
			// 鏇存柊鎻忚堪绗﹂泦
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,  // 缁戝畾鐐?
				currentSkyboxDescriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}

	
		if (currentSkyboxDescriptor) {
			// 鏇存柊鎻忚堪绗﹂泦
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::writeDescriptorSet(
				descriptorSets.skybox,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				2,  // 缁戝畾鐐?
				currentSkyboxDescriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

		}
		
		// 鏇存柊 uniform buffer 骞堕噸寤哄懡浠ょ紦鍐插尯
		
		generateSHCoefficients();  // 鏇存柊 SH 绯绘暟
		updateUniformBuffers();
		buildCommandBuffers();
	}

	void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			if (overlay->comboBox("Material", &materialIndex, materialNames)) {
				buildCommandBuffers();
			}
			if (overlay->comboBox("Object type", &models.objectIndex, objectNames)) {
				updateUniformBuffers();
				buildCommandBuffers();
			}
			if (overlay->inputFloat("Exposure", &uboParams.exposure, 0.1f, 2)) {
				updateParams();
			}
			if (overlay->inputFloat("Gamma", &uboParams.gamma, 0.1f, 2)) {
				updateParams();
			}
			if (overlay->comboBox("Skybox", &skyboxIndex, skyboxNames)) {
				loadSkyboxTexture(); // 宸插寘鍚玝uildCommandBuffers璋冪敤
			}
			// 娣诲姞娓叉煋妯″紡閫夋嫨
			if (overlay->comboBox("Render Mode", &renderMode, renderModeNames)) {
				// 鍒囨崲娓叉煋妯″紡鏃堕噸寤哄懡浠ょ紦鍐插尯
				buildCommandBuffers();
			}
		}
	}
	// 杈呭姪鍑芥暟锛氳幏鍙?SH basis (鍖呭惈 normalization锛屽父閲忓尮閰?LearnOpenGL)
	std::vector<float> getSHBasis(const glm::vec3& dir) {
		float x = dir.x, y = dir.y, z = dir.z;
		float x2 = x * x, y2 = y * y, z2 = z * z;
		return {
			0.282095f,                  // l00
			0.488603f * y,              // l1m1
			0.488603f * z,              // l10
			0.488603f * x,              // l1p1
			1.092548f * x * y,          // l2m2
			1.092548f * y * z,          // l2m1
			0.315392f * (3.0f * z2 - 1.0f), // l20
			1.092548f * x * z,          // l2p1
			0.546274f * (x2 - y2)       // l2p2
		};
	}


float half_to_float(uint16_t half) {
    uint32_t sign = (half >> 15) & 0x1;
    uint32_t exp = (half >> 10) & 0x1F;
    uint32_t mant = half & 0x3FF;
    if (exp == 0x1F) {
        if (mant == 0) return sign ? -INFINITY : INFINITY;
        return NAN;
    }
    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        float val = mant / 1024.0f * powf(2.0f, -14.0f);
        return sign ? -val : val;
    }
    exp = exp - 15 + 127;
    uint32_t result = (sign << 31) | (exp << 23) | (mant << 13);
    return *reinterpret_cast<float*>(&result);
}

void saveCubemapToPPM(const char* filename, uint16_t* data, uint32_t width, uint32_t height) {
    std::ofstream file(filename);
    file << "P3\n" << width << " " << height * 6 << "\n255\n";
    for (uint32_t i = 0; i < width * height * 6 * 4; i += 4) {
        int r = std::min(255, int(half_to_float(data[i]) * 255.0f));
        int g = std::min(255, int(half_to_float(data[i + 1]) * 255.0f));
        int b = std::min(255, int(half_to_float(data[i + 2]) * 255.0f));
        file << r << " " << g << " " << b << "\n";
    }
    file.close();
}
// 鐢ㄩ€旓細锛屾牴鎹粰瀹氱殑 3D 鏂瑰悜鍚戦噺浠庣珛鏂逛綋璐村浘涓噰鏍烽鑹层€?
glm::vec3 sampleCubemap(uint16_t* data, uint32_t width, uint32_t height, glm::vec3 dir) {
	// maxAxis锛氬瓨鍌ㄦ柟鍚戝悜閲忎腑缁濆鍊兼渶澶х殑鍒嗛噺锛岀敤浜庣‘瀹氶噰鏍峰摢涓珛鏂逛綋璐村浘闈€?
	// u, v锛氱汗鐞嗗潗鏍囷紙鑼冨洿 [0, 1]锛夛紝鐢ㄤ簬鍦ㄩ€夊畾闈笂閲囨牱銆?
	// face锛氳〃绀虹珛鏂逛綋璐村浘鐨勫叚涓潰锛? 鍒?5锛岄€氬父瀵瑰簲 +X, -X, +Y, -Y, +Z, -Z锛夈€?
    float maxAxis, u, v;
	uint32_t face;
	// 妫€鏌ユ柟鍚戝悜閲忕殑 x 鍒嗛噺鏄惁鍏锋湁鏈€澶х粷瀵瑰€硷紝浠ョ‘瀹氭槸鍚﹂噰鏍?X 杞寸浉鍏抽潰锛?X 鎴?-X锛夈€?
    if (std::abs(dir.x) >= std::abs(dir.y) && std::abs(dir.x) >= std::abs(dir.z)) {
	// maxAxis = std::abs(dir.x)锛氬皢 x 鍒嗛噺鐨勭粷瀵瑰€煎瓨鍌ㄤ负鏈€澶ц酱銆?
    // face = dir.x > 0 ? 0 : 1锛氬鏋?x 涓烘锛岄€夋嫨 +X 闈紙face=0锛夛紱鍚﹀垯閫夋嫨 -X 闈紙face=1锛夈€?
	// u = dir.x > 0 ? -dir.z : dir.z锛氭牴鎹?x 鐨勬璐燂紝璁＄畻 u 鍧愭爣锛堝搴?z 鍒嗛噺锛岃€冭檻绔嬫柟浣撹创鍥惧潗鏍囩郴锛夈€?
 	// v = -dir.y锛歷 鍧愭爣涓?y 鍒嗛噺鐨勮礋鍊硷紙鑰冭檻绔嬫柟浣撹创鍥剧殑鍧愭爣绯绘柟鍚戯級銆?
        maxAxis = std::abs(dir.x);
        face = dir.x > 0 ? 0 : 1;
        u = dir.x > 0 ? -dir.z : dir.z;
        v = -dir.y;
    } else if (std::abs(dir.y) >= std::abs(dir.x) && std::abs(dir.y) >= std::abs(dir.z)) {
        maxAxis = std::abs(dir.y);
        face = dir.y > 0 ? 2 : 3;
        u = dir.x;
        v = dir.y > 0 ? dir.z : -dir.z;
    } else {
        maxAxis = std::abs(dir.z);
        face = dir.z > 0 ? 4 : 5;
        u = dir.z > 0 ? dir.x : -dir.x;
        v = -dir.y;
    }
	// 鐢ㄩ€旓細灏?u 鍜?v 鍧愭爣褰掍竴鍖栧埌 [0, 1] 鑼冨洿锛?
    u = (u / maxAxis + 1.0f) * 0.5f;
    v = (v / maxAxis + 1.0f) * 0.5f;
	// 灏嗗綊涓€鍖栫殑 u, v 鍧愭爣杞崲涓哄儚绱犲潗鏍?
    uint32_t x = std::min((uint32_t)(u * width), width - 1);
    uint32_t y = std::min((uint32_t)(v * height), height - 1);
	// 澹版槑闈欐€佸彉閲?sampleCount锛岀敤浜庤褰曢噰鏍锋鏁帮紙浠呯敤浜庤皟璇曟棩蹇楋級銆?
    static int sampleCount = 0;
    if (sampleCount < 10) {
        std::ofstream logFile("../../examples/lightprobesh/lightprobeshsh_coefficients.log", std::ios::app);
        logFile << "SampleCubemap " << sampleCount << ": face=" << face << ", u=" << u << ", v=" << v 
                << ", x=" << x << ", y=" << y << "\n";
        logFile.close();
        sampleCount++;
    }
	// 璁＄畻鍗曚釜绔嬫柟浣撹创鍥鹃潰鐨勫儚绱犳€绘暟锛堝 脳 楂橈級銆?
    uint32_t pixelCount = width * height;
	// 璁＄畻閲囨牱鍍忕礌鍦ㄧ珛鏂逛綋璐村浘涓殑鍋忕Щ閲忥紙faceOffset锛夊拰鍍忕礌鍦ㄦ暟鎹暟缁勪腑鐨勫亸绉婚噺锛坧ixelOffset锛夈€?
    uint32_t faceOffset = face * pixelCount;
    uint32_t pixelOffset = faceOffset + y * width + x;
    uint32_t offset = pixelOffset * 4;
	// 妫€鏌ュ亸绉绘槸鍚﹁秴鍑烘暟鎹寖鍥达紙6 闈?脳 姣忛潰鍍忕礌鏁?脳 4 閫氶亾锛夈€?
    if (offset + 3 >= 6 * pixelCount * 4) {
		// 濡傛灉鍋忕Щ瓒婄晫锛岃繑鍥為粦鑹诧紙RGB = 0, 0, 0锛変互閬垮厤闈炴硶璁块棶銆?
        return glm::vec3(0.0f);
    }
	// 浠庢暟鎹腑璇诲彇 RGB 閫氶亾鍊硷紙鍗婄簿搴︽诞鐐规牸寮忥紝uint16_t锛夛紝骞惰浆鎹负娴偣鏁帮細
    float r = half_to_float(data[offset]);
    float g = half_to_float(data[offset + 1]);
    float b = half_to_float(data[offset + 2]);
    return glm::vec3(r, g, b);
}


// 浠庡綋鍓嶇珛鏂逛綋璐村浘璁＄畻鐞冭皭锛圫H锛夌郴鏁帮紝鐢ㄤ簬鐜鍏夌収璁＄畻銆?
void generateSHCoefficients() {
    // 鎵撳紑鏃ュ織鏂囦欢锛堣拷鍔犳ā寮忥級锛岃褰?SH 绯绘暟鐢熸垚杩囩▼
    std::ofstream logFile("../../examples/lightprobesh/lightprobeshsh_coefficients.log", std::ios::app);
    logFile << "\n" << "begin time(UTC): " << std::chrono::system_clock::now() << "\n";
    logFile << "Starting SH coefficient generation (GPU)\n";
    logFile << "skyboxIndex: " << skyboxIndex << "\n";

    // 閫夋嫨褰撳墠绔嬫柟浣撹创鍥?
    vks::TextureCubeMap* currentCube = nullptr;
    switch (skyboxIndex) {
        case 0:
            logFile << "Invalid skyboxIndex, setting SH coefficients to zero\n";
            shCoeffs = SHCoefficients{};
            memcpy(uniformBuffers.sh.mapped, &shCoeffs, sizeof(SHCoefficients));
            logFile.close();
            return;
        case 1: currentCube = &textures.environmentCube; break;
        case 2: currentCube = &textures.environmentCube2; break;
        case 3: currentCube = &textures.environmentCube3; break;
    }

    // 妫€鏌ョ珛鏂逛綋璐村浘鏄惁鏈夋晥
    if (!currentCube || !currentCube->image) {
        logFile << "Error: currentCube is null or not initialized\n";
        shCoeffs = SHCoefficients{};
        memcpy(uniformBuffers.sh.mapped, &shCoeffs, sizeof(SHCoefficients));
        logFile.close();
        return;
    }

    logFile << "Cubemap width: " << currentCube->width << ", height: " << currentCube->height << "\n";

    // 鍒涘缓瀛樺偍缂撳啿鍖猴紝鐢ㄤ簬瀛樺偍 SH 绯绘暟
    vks::Buffer shStorageBuffer;
    VkDeviceSize bufferSize = sizeof(SHCoefficients);
    VK_CHECK_RESULT(vulkanDevice->createBuffer(
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &shStorageBuffer,
        bufferSize));

    // 鍒涘缓鎻忚堪绗︽睜
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1);
    VkDescriptorPool descriptorPool;
    VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 鍒涘缓鎻忚堪绗﹂泦甯冨眬
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
        vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1)
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

    // 鍒嗛厤鎻忚堪绗﹂泦
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
    VkDescriptorSet descriptorSet;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

    // 鏇存柊鎻忚堪绗﹂泦
    VkDescriptorImageInfo imageInfo = currentCube->descriptor;
    VkDescriptorBufferInfo bufferInfo = shStorageBuffer.descriptor;
    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageInfo),
        vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &bufferInfo)
    };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

    // 鍒涘缓璁＄畻绠＄嚎甯冨眬
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
    VkPipelineLayout pipelineLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    // 加载计算着色器并创建管线
    VkPipelineShaderStageCreateInfo shaderStage = loadShader(getShadersPath() + "lightprobesh/sh_compute.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VkShaderModule shaderModule = shaderStage.module;
    VkComputePipelineCreateInfo computePipelineCI = vks::initializers::computePipelineCreateInfo(pipelineLayout);
    computePipelineCI.stage = shaderStage;
    VkPipeline computePipeline;
    VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCI, nullptr, &computePipeline));

    // 鍒涘缓鍛戒护缂撳啿鍖?
    VkCommandBuffer cmdBuf = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    // 缁戝畾绠＄嚎鍜屾弿杩扮闆?
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // 鍒嗘淳璁＄畻浠诲姟锛? 涓嚎绋嬶紝1 涓伐浣滅粍锛?
    vkCmdDispatch(cmdBuf, 1, 1, 1);

    // 娣诲姞鍐呭瓨灞忛殰锛岀‘淇濊绠楀畬鎴愬悗鏁版嵁鍙
    VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

    // 灏嗗瓨鍌ㄧ紦鍐插尯鐨勬暟鎹鍒跺埌 Uniform 缂撳啿鍖?
    VkBufferCopy copyRegion = {};
    copyRegion.size = sizeof(SHCoefficients);
    vkCmdCopyBuffer(cmdBuf, shStorageBuffer.buffer, uniformBuffers.sh.buffer, 1, &copyRegion);

    // 鎻愪氦鍛戒护缂撳啿鍖?
    vulkanDevice->flushCommandBuffer(cmdBuf, queue);

    // 璇诲彇 SH 绯绘暟鐢ㄤ簬鏃ュ織璁板綍
    SHCoefficients tempCoeffs;
    memcpy(&tempCoeffs, uniformBuffers.sh.mapped, sizeof(SHCoefficients));

    // 璁板綍 SH 绯绘暟
    logFile << "SH Coefficients for skyboxIndex " << skyboxIndex << ":\n";
    logFile << "l00: " << tempCoeffs.l00.x << ", " << tempCoeffs.l00.y << ", " << tempCoeffs.l00.z << "\n";
    logFile << "l1m1: " << tempCoeffs.l1m1.x << ", " << tempCoeffs.l1m1.y << ", " << tempCoeffs.l1m1.z << "\n";
    logFile << "l10: " << tempCoeffs.l10.x << ", " << tempCoeffs.l10.y << ", " << tempCoeffs.l10.z << "\n";
    logFile << "l1p1: " << tempCoeffs.l1p1.x << ", " << tempCoeffs.l1p1.y << ", " << tempCoeffs.l1p1.z << "\n";
    logFile << "l2m2: " << tempCoeffs.l2m2.x << ", " << tempCoeffs.l2m2.y << ", " << tempCoeffs.l2m2.z << "\n";
    logFile << "l2m1: " << tempCoeffs.l2m1.x << ", " << tempCoeffs.l2m1.y << ", " << tempCoeffs.l2m1.z << "\n";
    logFile << "l20: " << tempCoeffs.l20.x << ", " << tempCoeffs.l20.y << ", " << tempCoeffs.l20.z << "\n";
    logFile << "l2p1: " << tempCoeffs.l2p1.x << ", " << tempCoeffs.l2p1.y << ", " << tempCoeffs.l2p1.z << "\n";
    logFile << "l2p2: " << tempCoeffs.l2p2.x << ", " << tempCoeffs.l2p2.y << ", " << tempCoeffs.l2p2.z << "\n";

    // 娓呯悊璧勬簮
    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyShaderModule(device, shaderModule, nullptr);
    shStorageBuffer.destroy();

    logFile << "SH coefficient generation completed (GPU)\n";
    logFile << "end time(UTC): " << std::chrono::system_clock::now() << "\n";
    logFile.close();
}
// 浠庡綋鍓?environmentCube 璁＄畻 SH 绯绘暟
    // 浣跨敤 9 涓熀纭€鍑芥暟鎶曞奖锛堝熀浜?LearnOpenGL 鍜?jMonkeyEngine 鐨勬€濊矾锛?
    // 杩欓噷绠€鍖栧疄鐜帮細瀹為檯闇€閲囨牱 cubemap锛圕PU 鎴?GPU compute shader锛?
    // 绀轰緥鍏紡锛氭瘡涓郴鏁?= 鈭?L(蠅) * Y_lm(蠅) d蠅 (Y_lm 鏄?SH basis)
    // 涓虹畝鍖栵紝鐢ㄤ吉浠ｇ爜锛涘疄闄呭彲鍙傝€?https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf
    // shCoeffs = SHCoefficients{};
    
    // 绀轰緥锛氳绠?l00 (甯告暟椤? = (1 / 4蟺) * 鈭?L(蠅) d蠅
    // 鍋囪浠?cubemap 閲囨牱绉垎锛堥渶瀹炵幇閲囨牱寰幆锛岀被浼?generateIrradianceCube 涓殑閲囨牱锛?
    // const float pi = glm::pi<float>();
    // glm::vec3 basis[9] = { /* SH basis constants */ };  // 棰勫畾涔?basis 鍊?
    // for (int face = 0; face < 6; ++face) {
        // 閲囨牱姣忎釜闈紝绉垎...
        // shCoeffs.l00 += sample * basis[0] * weight;
    // }
    // 褰掍竴鍖?
    // shCoeffs.l00 *= 4.0f * pi / numSamples;

    
//     memcpy(uniformBuffers.sh.mapped, &shCoeffs, sizeof(SHCoefficients));
// }


};

VULKAN_EXAMPLE_MAIN()
