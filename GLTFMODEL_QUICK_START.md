# 🚀 gltfModel快速启动指南

## 问题

gltfModel消失了，无法被渲染。

---

## 解决方案

### 修复1: 简化着色器光照计算

**文件**: `shaders/glsl/lightprobesh2/gltfmesh.frag` (第151-192行)

**改动**: 替换复杂的PBR光照为简单的光照模型

```glsl
void main()
{
    vec3 N = normalize(inNormal);
    vec3 V = normalize(global.cameraPos[gl_ViewIndex].xyz - inWorldPos);
    
    // ✅ 使用material.elbedo作为基础颜色
    vec3 albedo = ALBEDO;
    
    // 简单的光照计算
    vec3 N_normalized = normalize(N);
    vec3 diffuse = albedo * 0.5;  // 基础环境光
    
    // 添加方向光
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float NdotL = max(dot(N_normalized, lightDir), 0.0);
    diffuse += albedo * NdotL * 0.5;
    
    // 简单的镜面反射
    vec3 H = normalize(V + lightDir);
    float NdotH = max(dot(N_normalized, H), 0.0);
    float specular = pow(NdotH, 32.0) * 0.5;
    
    vec3 color = diffuse + vec3(specular);
    color = max(color, vec3(0.1));  // 最小亮度
    
    // Tone mapping和Gamma correction
    color = Uncharted2Tonemap(color * global.exposure);
    color = color * (1.0f / Uncharted2Tonemap(vec3(11.2f)));
    color = pow(color, vec3(1.0f / global.gamma));
    
    outColor = vec4(color, 1.0) * pc.tint;
}
```

### 修复2: 修复Multiview版本

**文件**: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` (第144-182行)

**改动**: 同样的简化光照计算

---

## 编译步骤

### 第1步: 编译着色器

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn\shaders\glsl
python ./compileshaders.py --project lightprobesh2
```

### 第2步: 编译C++代码

```bash
cd c:\Users\Bluesky\Desktop\graphic\learn
cmake --build build --config Release
```

### 第3步: 运行程序

```bash
./build/Release/lightprobesh2.exe
```

---

## 验证

- [ ] gltfModel可见（灰白色）
- [ ] 有基本的光照效果
- [ ] 固定在世界坐标系中
- [ ] 移动鼠标时不跟随

---

## 预期效果

✅ gltfModel显示为灰白色
✅ 有漫反射和镜面反射光照
✅ 可以正常交互

---

## 后续改进

添加纹理支持（参考 `gltfloading.cpp`）：
1. 加载模型纹理
2. 创建纹理描述符集
3. 修改着色器采样纹理
4. 在Draw函数中绑定纹理


