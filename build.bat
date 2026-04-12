@echo off
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\MSBuild.exe" "E:\Projekt\Relevant\TerrainEditor\local\Tools.sln" /p:Configuration=Debug /p:Platform=x64 /t:Build /m /nologo /v:minimal > E:\Projekt\Relevant\TerrainEditor\build_log.txt 2>&1
echo EXIT:%ERRORLEVEL% >> E:\Projekt\Relevant\TerrainEditor\build_log.txt
