@echo off
REM Compile Per-Pixel LT Compute Shader

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
echo Compiling Per-Pixel LT Compute Shader...
echo.

REM Compile compute shader
echo Compiling prt_lt_pixel.comp...
"%GLSLC%" -O prt_lt_pixel.comp -o prt_lt_pixel.comp.spv
if errorlevel 1 (
    echo Error: Failed to compile prt_lt_pixel.comp
    pause
    exit /b 1
)
echo OK

echo.
echo Shader compiled successfully!
echo.
echo Generated file:
echo   - prt_lt_pixel.comp.spv
echo.
pause

