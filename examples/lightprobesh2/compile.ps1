# PowerShell compile script for lightprobesh2
# Setup VS 2022 developer environment

# Set VS 2022 paths
$vsPath = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$vcvarsPath = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"

if (-not (Test-Path $vcvarsPath)) {
    Write-Host "ERROR: Cannot find vcvars64.bat" -ForegroundColor Red
    Write-Host "Path: $vcvarsPath" -ForegroundColor Red
    exit 1
}

Write-Host "Setting up MSVC environment..." -ForegroundColor Green
Write-Host "vcvars64.bat path: $vcvarsPath" -ForegroundColor Cyan

# Run vcvars64.bat and capture environment variables
$env = @{}
cmd /c "call `"$vcvarsPath`" && set" | ForEach-Object {
    if ($_ -match '=') {
        $name, $value = $_.split('=', 2)
        $env[$name] = $value
    }
}

# Apply environment variables to current PowerShell session
$env.GetEnumerator() | ForEach-Object {
    [Environment]::SetEnvironmentVariable($_.Name, $_.Value)
}

Write-Host "MSVC environment ready" -ForegroundColor Green

# Navigate to project directory
$projectDir = "c:\Users\Bluesky\Desktop\SKY\Learn\Vulkan"
Set-Location $projectDir

Write-Host "Project directory: $projectDir" -ForegroundColor Cyan
Write-Host "Building..." -ForegroundColor Green

# Build
cmake --build build --config Debug --target lightprobesh2 -j 4

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!" -ForegroundColor Green
} else {
    Write-Host "Build failed with exit code: $LASTEXITCODE" -ForegroundColor Red
}

exit $LASTEXITCODE

