@echo off
REM Compile PRT Relighting Shaders
REM This script compiles the fixed PRT relighting shaders

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
echo Compiling PRT Relighting Shaders...
echo.

REM Compile vertex shader
echo Compiling prt_relight.vert...
"%GLSLC%" -O prt_relight.vert -o prt_relight.vert.spv
if errorlevel 1 (
    echo Error: Failed to compile prt_relight.vert
    pause
    exit /b 1
)
echo OK

REM Compile fragment shader
echo Compiling prt_relight.frag...
"%GLSLC%" -O prt_relight.frag -o prt_relight.frag.spv
if errorlevel 1 (
    echo Error: Failed to compile prt_relight.frag
    pause
    exit /b 1
)
echo OK

echo.
echo All shaders compiled successfully!
echo.
echo Generated files:
echo   - prt_relight.vert.spv
echo   - prt_relight.frag.spv
echo.
pause

