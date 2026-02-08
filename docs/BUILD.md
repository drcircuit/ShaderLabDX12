# Building ShaderLab

This guide covers building ShaderLab from source on Windows.

## Prerequisites

### Required Software

1. **Windows 10 or 11** (64-bit)
   - Windows SDK 10.0.19041.0 or later

2. **Visual Studio 2022** (or later)
   - Desktop development with C++
   - C++20 support
   - Windows 10/11 SDK
   - CMake tools

3. **CMake 3.20+**
   - Included with Visual Studio, or download from https://cmake.org

4. **Ninja** (optional but recommended)
   - Download from https://github.com/ninja-build/ninja/releases
   - Add to PATH

### Third-Party Dependencies

ShaderLab requires several header-only or small libraries. These must be manually downloaded and placed in the `third_party/` directory.

#### 1. Dear ImGui

Download from: https://github.com/ocornut/imgui

Required files:
```
third_party/imgui/
├── imgui.h
├── imgui.cpp
├── imgui_demo.cpp
├── imgui_draw.cpp
├── imgui_tables.cpp
├── imgui_widgets.cpp
├── imgui_internal.h
├── imconfig.h
├── imstb_rectpack.h
├── imstb_textedit.h
├── imstb_truetype.h
└── backends/
    ├── imgui_impl_win32.h
    ├── imgui_impl_win32.cpp
    ├── imgui_impl_dx12.h
    └── imgui_impl_dx12.cpp
```

#### 2. miniaudio

Download from: https://github.com/mackron/miniaudio

Required files:
```
third_party/miniaudio/
└── miniaudio.h
```

#### 3. nlohmann/json

Download from: https://github.com/nlohmann/json

Required files:
```
third_party/json/
└── include/
    └── nlohmann/
        └── json.hpp
```

#### 4. stb_image

Download from: https://github.com/nothings/stb

Required files:
```
third_party/stb/
└── stb_image.h
```

## Build Instructions

### Option 1: Command Line Build

1. Open "x64 Native Tools Command Prompt for VS 2022"

2. Navigate to the ShaderLab directory:
   ```cmd
   cd C:\path\to\ShaderLab
   ```

3. Configure with CMake (Debug):
   ```cmd
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
   ```

   Or for Release:
   ```cmd
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   ```

4. Build:
   ```cmd
   cmake --build build
   ```

5. Run the editor:
   ```cmd
   build\bin\ShaderLabEditor.exe
   ```

### Optional: Load a Developer Environment (PowerShell)

If you are not using the VS Developer Console, you can load the MSVC toolchain
and any extra PATH entries in a regular PowerShell session:

1. (Optional) Add custom PATH entries in:
   ```text
   tools\dev_env.cfg
   ```

2. Load the environment in the same terminal you will build or run from:
   ```powershell
   .\tools\dev_env.ps1
   ```

### Option 2: Visual Studio

1. Open Visual Studio 2022

2. Select "Open a local folder" and choose the ShaderLab directory

3. Visual Studio will automatically detect CMakeLists.txt and configure the project

4. Select build configuration (Debug or Release) from the toolbar

5. Build → Build All

6. Run → Start Without Debugging (or F5 to debug)

### Option 3: CMake GUI

1. Open CMake GUI

2. Set source directory to ShaderLab folder

3. Set build directory to `ShaderLab/build`

4. Click "Configure", select Visual Studio 2022

5. Click "Generate"

6. Click "Open Project" to open in Visual Studio

7. Build and run from Visual Studio

## Build Configurations

### Debug

- Validation layers enabled
- Debug symbols included
- Shader compilation in "Live" mode (fast, unoptimized)
- Suitable for development

```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

### Release

- No validation layers
- Optimized code
- Shader compilation can use "Build" mode (O3, stripped)
- Suitable for final demos

```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

## CMake Options

The following CMake options are available:

- `SHADERLAB_BUILD_EDITOR` (default: ON) - Build the editor application
- `SHADERLAB_BUILD_RUNTIME` (default: OFF) - Build standalone runtime

Example:
```cmd
cmake -B build -G Ninja -DSHADERLAB_BUILD_RUNTIME=ON
```

## Troubleshooting

### "Cannot find d3d12.lib"

Ensure Windows SDK is installed with Visual Studio. The SDK should be automatically found.

### "ImGui files not found"

Make sure you've downloaded and placed Dear ImGui in `third_party/imgui/` as described above.

### "DXC not found"

DXC is included with Windows SDK 10.0.19041.0 and later. Ensure you have an up-to-date SDK installed.

### Linker errors with miniaudio

Make sure `MINIAUDIO_IMPLEMENTATION` is defined only once. It's already defined in `AudioSystem.cpp`.

### CMake can't find Ninja

Add Ninja to your PATH, or use a different generator:
```cmd
cmake -B build -G "Visual Studio 17 2022"
```

## Running the Editor

After building, the executable is located at:
- `build/bin/ShaderLabEditor.exe`

The editor expects to be run from the repository root to find creative assets:
```cmd
cd C:\path\to\ShaderLab
build\bin\ShaderLabEditor.exe
```

## Next Steps

- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand the codebase
- Check [creative/examples/README.md](../creative/examples/README.md) for shader examples
- Start experimenting with realtime shader coding!
