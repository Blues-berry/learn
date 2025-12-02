@echo off
REM 编译脚本 - 使用VS开发者命令提示符环境

REM 设置MSVC环境
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM 导航到项目目录
cd /d c:\Users\Bluesky\Desktop\SKY\Learn\Vulkan

REM 编译
cmake --build build --config Debug --target lightprobesh2 -j 4

pause

