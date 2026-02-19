@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo Staging end-user documentation...
powershell -ExecutionPolicy Bypass -File "%~dp0..\tools\stage_enduser_docs.ps1" -BuildBin "%~dp0..\build\bin"
if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo Signing executable...
powershell -ExecutionPolicy Bypass -File "%~dp0..\tools\sign_build.ps1"
if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo Building installer artifact...
powershell -ExecutionPolicy Bypass -File "%~dp0..\tools\build_installer.ps1"
if %errorlevel% neq 0 exit /b %errorlevel%
