# 最终目标，实现使用Cornellbox场景的relighting

项目目录在C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\examples\lightprobesh2
着色器目录在C:\Users\Bluesky\Desktop\SKY\Learn\Vulkan\shaders\glsl\lightprobesh2
完整遍历有关cornelbox的所有资料，包括shader，模型，光照，渲染，完整遍历有关previewmodel的所有资料，包括shader，模型，光照，渲染



# 1、实现基于PRT的relighting，使用球谐函数，预计算出光照和非光照信息，同时预计算光源旋转信息，导出为txt文件，目前代码已经有了这一部分。 目前代码的问题是：预计算不在GPU端进行，需要转移到GPU上。  且第3步: 预计算Light Transport (物体表面对光照的响应)应该采用逐个顶点计算，这部分也需要转移到GPU上。转移后进行导出测试，确认本地有文件且可以正常打开读取。在UI界面增加一个导出选项，点击后进行预计算，并导出txt文件。


# 2、最后需要使用导出的txt文件，UI能够读取对应的TXT文件应用预计算的信息为Cornell模型着色


列一个tudolilst,实现目标,先实现第一个，完成后写一个总结文档，并测试，期间不要写其他文档，全程中文回答。实现代码的过程中，增加调试内容，尽可能输出所有调试细节，方便定位问题


