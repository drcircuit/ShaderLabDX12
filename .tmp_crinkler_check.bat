@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.29 >nul
cmake -S . -B build_selfcontained_ninja_crinkled -G Ninja -DCMAKE_BUILD_TYPE=Release -DSHADERLAB_BUILD_EDITOR=OFF -DSHADERLAB_BUILD_RUNTIME=ON -DSHADERLAB_TINY_PLAYER=ON -DSHADERLAB_USE_CRINKLER=ON -DCRINKLER_PATH=C:/tools/crinkler23/Win32/Crinkler.exe -DCMAKE_LINKER=C:/tools/crinkler23/Win32/Crinkler.exe
cmake --build build_selfcontained_ninja_crinkled --target ShaderLabPlayer --config Release
