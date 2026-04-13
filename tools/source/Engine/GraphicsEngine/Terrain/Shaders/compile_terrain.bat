@echo off
setlocal EnableDelayedExpansion

set "FXC="
set "WINDOWS_KITS_ROOT=%ProgramFiles(x86)%\Windows Kits\10\bin"
if defined WindowsSdkVerBinPath if exist "%WindowsSdkVerBinPath%x64\fxc.exe" set "FXC=%WindowsSdkVerBinPath%x64\fxc.exe"
if not defined FXC if exist "!WINDOWS_KITS_ROOT!\x64\fxc.exe" set "FXC=!WINDOWS_KITS_ROOT!\x64\fxc.exe"
if not defined FXC (
    for /f "delims=" %%i in ('dir /b /ad "!WINDOWS_KITS_ROOT!\10.*" 2^>nul ^| sort /r') do (
        if exist "!WINDOWS_KITS_ROOT!\%%i\x64\fxc.exe" (
            set "FXC=!WINDOWS_KITS_ROOT!\%%i\x64\fxc.exe"
            goto :fxc_found
        )
    )
)
for %%i in (fxc.exe) do if not defined FXC set "FXC=%%~$PATH:i"

:fxc_found
if not defined FXC (
    echo Could not locate fxc.exe. Install the Windows 10 SDK or launch this script from a developer prompt.
    exit /b 1
)

set SHADERS=%~dp0
set OUT=%~dp0..\..\..\..\..\EngineResources\Assets\Shaders

"!FXC!" /T ps_5_0 /E main /Fo "%OUT%\TerrainPS.cso" "%SHADERS%TerrainPS.hlsl"
if %ERRORLEVEL% neq 0 ( echo FAILED: TerrainPS & pause & exit /b 1 )
echo OK: TerrainPS.cso

"!FXC!" /T ps_5_0 /E main /Fo "%OUT%\TerrainReflectionPS.cso" "%SHADERS%TerrainReflectionPS.hlsl"
if %ERRORLEVEL% neq 0 ( echo FAILED: TerrainReflectionPS & pause & exit /b 1 )
echo OK: TerrainReflectionPS.cso

echo Done.
pause
