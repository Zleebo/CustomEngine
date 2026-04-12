@echo off
set FXC="C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe"
set SHADERS=%~dp0
set OUT=%~dp0..\..\..\..\..\EngineResources\Assets\Shaders

%FXC% /T ps_5_0 /E main /Fo "%OUT%\TerrainPS.cso" "%SHADERS%TerrainPS.hlsl"
if %ERRORLEVEL% neq 0 ( echo FAILED: TerrainPS & pause & exit /b 1 )
echo OK: TerrainPS.cso

%FXC% /T ps_5_0 /E main /Fo "%OUT%\TerrainReflectionPS.cso" "%SHADERS%TerrainReflectionPS.hlsl"
if %ERRORLEVEL% neq 0 ( echo FAILED: TerrainReflectionPS & pause & exit /b 1 )
echo OK: TerrainReflectionPS.cso

echo Done.
pause
