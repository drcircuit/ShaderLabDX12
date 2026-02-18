@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake --build build --config Debug

if %errorlevel% neq 0 exit /b %errorlevel%

echo.
echo Signing executable...
powershell -ExecutionPolicy Bypass -File "%~dp0..\tools\sign_build.ps1"
