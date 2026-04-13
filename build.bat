@echo off
setlocal

set "ROOT=%~dp0"
set "SOLUTION=%ROOT%Tools.sln"
set "LOG=%ROOT%build_log.txt"
set "MSBUILD="

if not exist "%SOLUTION%" (
    > "%LOG%" echo Missing solution: "%SOLUTION%"
    exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
        set "MSBUILD=%%i"
        goto :msbuild_found
    )
)

for %%i in (MSBuild.exe) do if not defined MSBUILD set "MSBUILD=%%~$PATH:i"

:msbuild_found
if not defined MSBUILD (
    > "%LOG%" echo Could not locate MSBuild.exe.
    >> "%LOG%" echo Install Visual Studio Build Tools 2022 or make MSBuild available on PATH.
    exit /b 1
)

> "%LOG%" echo Using MSBuild: %MSBUILD%
"%MSBUILD%" "%SOLUTION%" /p:Configuration=Debug /p:Platform=x64 /t:Build /m /nologo /v:minimal >> "%LOG%" 2>&1
set "EXITCODE=%ERRORLEVEL%"
>> "%LOG%" echo EXIT:%EXITCODE%
exit /b %EXITCODE%
