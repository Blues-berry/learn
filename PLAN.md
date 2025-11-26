# 最终目标，实现使用Cornellbox场景的relighting
完整遍历有关cornelbox的所有资料，包括shader，模型，光照，渲染，完整遍历有关previewmodel的所有资料，包括shader，模型，光照，渲染

# 1、在勾选enbable lighting后，自动取消勾选usesh,并在着色器中更换着色逻辑，在previewmodel上显示光源颜色。

# 2、在previewmodel的中心位置放置一个光源，后续了跟随previewmodel的旋转，光源颜色改变时，previewmodel的颜色同步改变并在ui界面设置可开关


# 3、查找Cornell模型对应的着色器并修改，使其上受光照影响，可以着色，发生颜色变化

# 4、在旋转previewmodel和对应的光源，实现relighting，使得Cornell模型在变化的光源场景下业可以着色

# 5、实现基于PRT的relighting，使用球谐函数，预计算出光照和非光照信息，同时预计算光源旋转信息。

# 6、应用预计算的信息为Cornell模型着色

列一个tudolilst,先实现前四个目标


