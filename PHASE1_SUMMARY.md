# 阶段1 完成总结：Cornell Box 和 Preview Model 资料遍历

## 执行时间
- 开始: 2025-11-27
- 完成: 2025-11-27

## 任务完成情况

✅ 查找Cornell box相关的shader文件
✅ 查找Cornell box的模型文件和光照配置
✅ 查找Preview model相关的shader文件
✅ 查找Preview model的模型文件和光照配置
✅ 分析Preview model颜色改变的实现机制

---

## 关键发现

### 1. Cornell Box 架构

**模型**:
- 位置: `assets/models/cornell/cornell.gltf`
- 顶点数: 1834
- 索引数: 5976
- 材质: 8个（红墙、绿墙、白墙等）

**着色器**:
- 主着色器: `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag`
- 支持: 球谐函数(SH)光照、IBL反射、Multiview渲染

**光照配置**:
- 固定位置: (0.0f, 5.5f, -9.0f)
- 可旋转: 绕Y轴旋转，旋转半径0.3倍
- 默认颜色: 白光 (1.0, 1.0, 1.0)

### 2. Preview Model 架构

**类**: `PreviewModel` (examples/lightprobesh2/PreviewModel.cpp)

**功能**: 渲染光源预览球体

**材质结构**:
```cpp
struct MaterialBuffer {
    float roughness = 1.f;
    float metallic = 0.5;
    float specular = 0.5;
    int32_t useLighting = 1;
    glm::vec4 elbedo;  // 颜色
    int32_t useSH = 1;
    int32_t useReflection = 0;
};
```

**关键方法**:
- `SetLightColor(const glm::vec3& color)` - 设置光源颜色
- `Draw()` - 渲染光源球体

### 3. 颜色改变流程

**UI → Preview Model → Cornell Box**

1. 用户在UI中选择颜色
2. `main.cpp` 调用 `previewModel->SetLightColor(color)`
3. Preview Model更新材质缓冲区
4. Preview Model着色器使用新颜色渲染
5. **问题**: Cornell Box着色器未收到颜色信息

---

## 问题分析

### 颜色不对应的根本原因

1. **光源颜色未传递**
   - Preview Model: 颜色在 `material.albedo` 中
   - Cornell Box: 使用固定的 `lightDir = normalize(vec3(1.0, 1.0, 1.0))`
   - 缺失: `lightColor` 未在Global UBO中传递

2. **Global UBO不完整**
   - 当前: 只有 `mainLight`, `exposure`, `gamma`
   - 需要: `lightColor`, `lightIntensity`, `lightPosition`

3. **着色计算不一致**
   - Preview Model: 直接使用颜色
   - Cornell Box: 使用 `albedo * NdotL * 0.5`

---

## 关键文件清单

| 文件 | 用途 | 行数 |
|------|------|------|
| `shaders/glsl/lightprobesh2/gltfmesh_mvr.frag` | Cornell Box主着色器 | 111 |
| `shaders/glsl/lightprobesh2/light_source.frag` | 光源着色器 | 44 |
| `examples/lightprobesh2/PreviewModel.cpp` | 光源预览类 | 367 |
| `examples/lightprobesh2/main.cpp` | 主程序 | 1240 |
| `examples/lightprobesh2/Pass.h` | 渲染通道定义 | 347 |
| `assets/models/cornell/cornell.gltf` | Cornell Box模型 | - |

---

## 建议的修复方案

### 短期修复 (阶段2)
1. 在Pass.h中扩展Global UBO结构
2. 修改gltfmesh_mvr.frag使用lightColor
3. 修改main.cpp传递光源参数
4. 测试颜色对应

### 长期改进 (阶段3-4)
1. 实现PRT预计算系统
2. 使用球谐函数表示光照
3. 支持动态光源旋转
4. 导出/导入预计算数据

---

## 下一步行动

**阶段2**: 修复Preview model颜色改变时的光源对应问题
- 任务: 定位Cornell model的着色器代码
- 任务: 修改着色器以匹配Preview model的颜色
- 任务: 测试颜色对应修复

**预计工作量**: 2-3小时

---

## 附件

- `RESEARCH_REPORT.md` - 详细研究报告
- `COLOR_MISMATCH_ANALYSIS.md` - 颜色不对应问题详细分析

