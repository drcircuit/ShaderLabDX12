# Third Party Dependencies

This directory contains third-party libraries used by ShaderLab.

## Required Libraries

### Dear ImGui
- **License**: MIT
- **Source**: https://github.com/ocornut/imgui
- **Usage**: UI framework
- **Integration**: Copy `imgui/` directory here

### miniaudio
- **License**: MIT / Public Domain
- **Source**: https://github.com/mackron/miniaudio
- **Usage**: Audio playback
- **Integration**: Copy `miniaudio.h` here

### nlohmann/json
- **License**: MIT
- **Source**: https://github.com/nlohmann/json
- **Usage**: JSON serialization
- **Integration**: Copy `json/include/` directory here

### stb_image
- **License**: MIT / Public Domain
- **Source**: https://github.com/nothings/stb
- **Usage**: Image loading
- **Integration**: Copy `stb_image.h` here

## Setup Instructions

1. Download each library from the sources above
2. Place them in this directory according to the structure:
   ```
   third_party/
   ├── imgui/
   │   ├── imgui.h
   │   ├── imgui.cpp
   │   └── ...
   ├── miniaudio/
   │   └── miniaudio.h
   ├── json/
   │   └── include/
   │       └── nlohmann/
   │           └── json.hpp
   └── stb/
       └── stb_image.h
   ```

3. Run CMake to configure the build

## Notes

- These libraries are header-only or require minimal integration
- All are permissively licensed (MIT or Public Domain)
- Versions will be documented in release notes
