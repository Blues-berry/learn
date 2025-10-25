# 三个任务详细分析

## 任务1: 目前捕获后出现错误，preview和gltfmodel均变成黑色，解决这个问题

### 问题描述
- 捕获立方体贴图后，preview模型和gltfmodel都变成黑色
- 这表明光照信息（SH或IBL）没有正确应用

### 根本原因分析
1. **LightProbe.cpp 第69-76行**: 视图矩阵配置
   - 当前使用的up向量: +Y面(0,0,1), -Y面(0,0,-1)
   - 这与着色器中的Y坐标翻转可能不匹配

2. **着色器Y坐标翻转**:
   - irradiancecube.frag 第38行: `sampleVector.y = -sampleVector.y;`
   - prefilterenvmap.frag 第97行: `L.y = -L.y;`
   - lightprobesh.frag 第84, 178行: 采样前翻转Y

3. **可能的问题**:
   - 视图矩阵的up向量与着色器Y翻转不一致
   - 导致立方体贴图采样方向错误
   - 最终导致SH系数或IBL贴图全为黑色

### 解决方案
需要找到正确的视图矩阵up向量配置，使其与着色器Y翻转配合正确

## 任务2: cubemap生成后应用usereflect贴图位置不正确

### 问题描述
- 立方体贴图的上下面（±Y）贴图位置反了
- 六个贴图之间有明显的界限/接缝

### 根本原因分析
1. **立方体贴图面顺序**: Vulkan标准为 +X, -X, +Y, -Y, +Z, -Z
2. **视图矩阵up向量**: 影响立方体贴图的方向
3. **着色器采样**: Y坐标翻转可能导致方向反转

### 解决方案
- 调整LightProbe.cpp中的视图矩阵up向量
- 确保±Y面的方向正确

## 任务3: 为gltfmodel增加纹理信息，并在capture中捕获纹理和光照信息

### 问题描述
- gltfloading.cpp中有完整的纹理加载逻辑
- 需要将这些纹理信息集成到lightprobesh2的GltfModel中
- 需要在capture过程中正确处理纹理

### 当前状态
- GltfModel已有基本框架
- 但纹理绑定被注释掉了（gltfload.cpp 第193-200行）
- 需要启用纹理支持

### 解决方案
1. 在GltfModel中启用纹理绑定
2. 从vkglTF::Model中提取纹理信息
3. 在描述符集中正确绑定纹理
4. 在capture过程中保持纹理一致性

