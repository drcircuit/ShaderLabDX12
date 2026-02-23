@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if exist build_debug (
	echo Removing existing build_debug folder...
	rmdir /s /q build_debug
)

cmake -S . -B build_debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
if %errorlevel% neq 0 exit /b %errorlevel%

cmake --build build_debug --config Debug --clean-first --target ShaderLabIIDE ShaderLabBuildCli ShaderLabPlayer ShaderLabScreenSaver

if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo Signing executable...
powershell -ExecutionPolicy Bypass -File "%~dp0..\tools\sign_build.ps1" -ExePath "%~dp0..\build_debug\bin\ShaderLabIIDE.exe"
