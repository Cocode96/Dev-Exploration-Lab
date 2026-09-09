@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
msbuild Containment.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /nologo
if errorlevel 1 exit /b 1
pushd build\Release
cl /nologo /std:c++20 /EHsc /O2 /MD /utf-8 /LD /I ..\..\Public /I ..\..\..\dx11-wboit-benchmark\Engine\Public ..\..\Private\Arena.cpp ..\..\Private\NeuralPolicy.cpp ..\..\Private\TrainingBridge.cpp /Fe:ArenaTraining.dll
set "buildResult=%errorlevel%"
popd
exit /b %buildResult%
