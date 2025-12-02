@echo off
REM Compile G-Buffer Shaders

setlocal enabledelayedexpansion

REM Find Vulkan SDK
for /f "tokens=2*" %%A in ('reg query "HKEY_LOCAL_MACHINE\SOFTWARE\LunarG\VulkanSDK" /v InstallDir 2^>nul') do set "VULKAN_SDK=%%B"

if not defined VULKAN_SDK (
    echo Error: Vulkan SDK not found in registry
    echo Please install Vulkan SDK or set VULKAN_SDK environment variable
    pause
    exit /b 1
)

set "GLSLC=%VULKAN_SDK%\Bin\glslc.exe"

if not exist "%GLSLC%" (
    echo Error: glslc.exe not found at %GLSLC%
    pause
    exit /b 1
)

echo Vulkan SDK found at: %VULKAN_SDK%
echo Compiling G-Buffer Shaders...
echo.

REM Compile vertex shader
echo Compiling gbuffer.vert...
"%GLSLC%" -O gbuffer.vert -o gbuffer.vert.spv
if errorlevel 1 (
    echo Error: Failed to compile gbuffer.vert
    pause
    exit /b 1
)
echo OK

REM Compile fragment shader
echo Compiling gbuffer.frag...
"%GLSLC%" -O gbuffer.frag -o gbuffer.frag.spv
if errorlevel 1 (
    echo Error: Failed to compile gbuffer.frag
    pause
    exit /b 1
)
echo OK

echo.
echo All shaders compiled successfully!
echo.
echo Generated files:
echo   - gbuffer.vert.spv
echo   - gbuffer.frag.spv
echo.
pause

