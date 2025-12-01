# PRT Relighting 修复 - 验证清单

## 修复前准备

- [ ] 备份原始代码（如需要）
- [ ] 确认 Vulkan SDK 已安装
- [ ] 确认 CMake 已安装
- [ ] 确认编译器可用（MSVC 或 GCC）

## 代码修改验证

### 文件检查
- [ ] `main.cpp` 已修改
- [ ] 第 714-780 行包含新的 PRT 渲染代码
- [ ] 调试输出已添加

### 代码结构验证
- [ ] `if (usePRTRelighting && pipelinePRT != VK_NULL_HANDLE)` 分支存在
- [ ] `vkCmdBindPipeline()` 调用正确
- [ ] `vkCmdBindDescriptorSets()` 使用 `pipelineLayoutPRT`
- [ ] `gltfModel->getModel()->bindBuffers()` 被调用
- [ ] 节点遍历逻辑正确
- [ ] Push Constants 正确传递

### 编译验证
- [ ] 无编译错误
- [ ] 无编译警告（或仅有预期的警告）
- [ ] 生成的可执行文件有效

## 运行时验证

### 程序启动
- [ ] 程序正常启动
- [ ] 无崩溃或异常
- [ ] UI 正常显示

### 模型加载
- [ ] Cornell 模型成功加载
- [ ] 模型在屏幕上可见
- [ ] 模型顶点数据正确

### PRT Relighting 启用
- [ ] "PRT Relighting" 复选框可用
- [ ] 勾选后启用 PRT 模式
- [ ] 控制台输出调试信息：
  ```
  [DEBUG PRT] Rendering Cornell model with PRT pipeline (frame 0)
  [DEBUG PRT]   - pipelinePRT: 0x...
  [DEBUG PRT]   - pipelineLayoutPRT: 0x...
  [DEBUG PRT]   - descriptorSetPRT: 0x...
  [DEBUG PRT]   - mainPass->descriptorSet: 0x...
  [DEBUG PRT]   - Drew X primitives
  ```

### Light Rotation 测试
- [ ] "Light Rotation" 滑块可用
- [ ] 拖动滑块时场景不消失
- [ ] 场景平滑渲染
- [ ] 光照随旋转而改变
- [ ] 无闪烁或抖动

### 光照效果验证
- [ ] 光源旋转到红色墙壁时，场景泛红光
- [ ] 光源旋转到绿色墙壁时，场景泛绿光
- [ ] 光源旋转到白色天花板时，场景变亮
- [ ] 光源旋转到黑色地板时，场景变暗
- [ ] 光照变化平滑连续

## 性能验证

### 帧率测试
- [ ] 启用 PRT Relighting 时帧率稳定
- [ ] 帧率与标准 PBR 渲染相近
- [ ] 无明显的帧率波动

### 内存使用
- [ ] 内存使用正常
- [ ] 无内存泄漏
- [ ] 无异常的内存增长

## 错误处理验证

### Vulkan 验证层
- [ ] 启用验证层时无错误
- [ ] 无描述符集绑定错误
- [ ] 无管线布局不匹配错误
- [ ] 无其他 Vulkan 错误

### 边界情况
- [ ] 快速拖动 Light Rotation 滑块不崩溃
- [ ] 多次启用/禁用 PRT Relighting 不崩溃
- [ ] 切换模型后 PRT 仍然正常工作

## 功能验证

### 标准渲染模式
- [ ] 禁用 PRT Relighting 后标准 PBR 渲染正常
- [ ] 其他模型仍然可以正常渲染
- [ ] 其他功能不受影响

### PRT 特定功能
- [ ] 光照 SH 系数正确更新
- [ ] 每顶点 LT 系数正确应用
- [ ] 旋转光照系数正确插值

## 文档验证

- [ ] `CHANGES_SUMMARY.md` 已创建
- [ ] `TECHNICAL_ANALYSIS_PRT_BUG.md` 已创建
- [ ] `PRT_ROTATION_BUG_FIX.md` 已创建
- [ ] `PRT_TESTING_GUIDE.md` 已创建
- [ ] `QUICK_FIX_REFERENCE.md` 已创建
- [ ] `FIX_IMPLEMENTATION_COMPLETE.md` 已创建
- [ ] 所有文档内容准确完整

## 最终验证

### 完整测试流程
1. [ ] 启动程序
2. [ ] 加载 Cornell 模型
3. [ ] 启用 PRT Relighting
4. [ ] 缓慢拖动 Light Rotation（0 → 6.28）
5. [ ] 快速拖动 Light Rotation（多次）
6. [ ] 禁用 PRT Relighting
7. [ ] 切换其他模型
8. [ ] 重新启用 PRT Relighting
9. [ ] 验证场景仍然正常渲染

### 预期结果
- [ ] 场景始终可见
- [ ] 光照平滑变化
- [ ] 无错误或警告
- [ ] 性能稳定

## 签字确认

| 项目 | 状态 | 备注 |
|------|------|------|
| 代码修改 | ✅ | 已完成 |
| 编译测试 | ⏳ | 待执行 |
| 运行时测试 | ⏳ | 待执行 |
| 功能验证 | ⏳ | 待执行 |
| 性能验证 | ⏳ | 待执行 |
| 文档完成 | ✅ | 已完成 |

## 问题记录

如在验证过程中发现问题，请记录在此：

```
问题 1：
  描述：
  位置：
  解决方案：

问题 2：
  描述：
  位置：
  解决方案：
```

## 修复完成确认

- [ ] 所有检查项已完成
- [ ] 所有测试已通过
- [ ] 修复已验证有效
- [ ] 文档已完善
- [ ] 代码已提交

**验证完成日期**：_______________

**验证人员**：_______________

**备注**：

---

## 快速参考

### 如果测试失败

1. **场景仍然消失**
   - [ ] 检查着色器文件是否存在
   - [ ] 检查 `pipelinePRT` 是否有效
   - [ ] 启用 Vulkan 验证层查看错误

2. **帧率下降**
   - [ ] 检查是否有其他渲染通道被激活
   - [ ] 检查 GPU 使用率

3. **光照效果不对**
   - [ ] 检查 `lightingSHBuffer` 是否正确更新
   - [ ] 检查 `UpdatePRTLighting()` 是否被调用

### 调试命令

```bash
# 启用 Vulkan 验证层
set VK_LAYER_PATH=C:\VulkanSDK\1.3.xxxx\Bin

# 运行程序并捕获输出
bin\lightprobesh2.exe > output.log 2>&1

# 查看调试输出
grep "DEBUG PRT" output.log
```

## 相关资源

- 修复代码：`main.cpp` 第 714-780 行
- 着色器：`shaders/glsl/lightprobesh2/prt_relight.vert/frag`
- 技术文档：`TECHNICAL_ANALYSIS_PRT_BUG.md`
- 测试指南：`PRT_TESTING_GUIDE.md`

