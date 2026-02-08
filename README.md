# ShaderLab

A Windows-native demoscene SDK and realtime shader authoring environment.

## Overview

ShaderLab is a minimalist, high-performance toolkit for creating demoscene productions and visual experiments. Built on Direct3D 12 and DXC, it provides a tracker-inspired interface for authoring shader-driven visuals synchronized to music.

### Core Features

- **Realtime Shader Authoring**: Live HLSL compilation via DXC with instant feedback
- **Beat-Synchronized Playback**: BPM-based timeline with quarter-note precision
- **Multi-Pass Rendering**: Pixel buffer passes with post-processing chains
- **Playlist System**: Beat-anchored scene transitions for demo sequencing
- **Tracker Interface**: Demoscene-inspired UI built with Dear ImGui
- **Optimized Output**: Debug mode for editing, release mode for final demos

### Editor UI Design

The editor features a custom futuristic cyberpunk aesthetic:
- **Custom Fonts**: Hacked (logo/headings), Orbitron (UI text), Erbos Draco (numbers)
- **Color Scheme**: Dark blue-black backgrounds with bright cyan/teal accents
- **Sharp Geometry**: Angular, industrial design with no rounded corners
- **High Contrast**: Optimized for extended coding sessions in dark environments

*Note: Custom fonts are editor-only and not included in runtime builds.*

## Technology Stack

- **Language**: C++20
- **Graphics**: Direct3D 12
- **Shaders**: HLSL 6.x with DXC compiler
- **UI**: Dear ImGui
- **Audio**: miniaudio
- **Build**: CMake 3.20+ (Ninja optional)
- **JSON**: nlohmann/json
- **Image Loading**: stb_image

## Architecture

```
src/
├── engine/     - Core runtime systems
├── editor/     - Editor application
├── runtime/    - Standalone demo runtime
├── graphics/   - D3D12 abstraction
├── shader/     - DXC wrapper and shader system
├── audio/      - Audio playback and beat clock
└── ui/         - ImGui integration and panels

creative/
├── shaders/    - Example shaders
└── examples/   - Demo projects

docs/           - Documentation
third_party/    - External dependencies
tools/          - Build and utility scripts
```

### Module Breakdown

**Graphics Module** (`src/graphics/`)
- D3D12 device and resource management
- Swapchain and present logic
- Command list abstraction
- Minimal overhead, RAII-based

**Shader Module** (`src/shader/`)
- DXC compiler integration
- Live compilation (debug, unoptimized)
- Build compilation (O3, stripped)
- Diagnostic parsing (file, line, column)

**Audio Module** (`src/audio/`)
- Audio file loading and playback
- Playback time tracking
- Beat clock with BPM sync
- Quarter/eighth/sixteenth note tracking

**UI Module** (`src/ui/`)
- ImGui integration with D3D12
- Demo View: Timeline and playlist
- Scene View: Realtime preview
- Effect View: Shader editor and parameters

## Building

### Prerequisites

- Windows 10/11
- Visual Studio 2022 (with C++20 support)
- CMake 3.20+
- Ninja (optional but recommended)
- Windows SDK (for D3D12 and DXC)

### Build Instructions

```powershell
# Clone the repository
git clone https://github.com/drcircuit/ShaderLabDX12.git
cd ShaderLab

# Open a VS Developer PowerShell or load the toolchain
.\tools\dev_env.ps1

# Configure with CMake
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build

# Run the editor from the repository root
build\bin\ShaderLabEditor.exe
```

Third-party dependencies are listed in [third_party/README.md](third_party/README.md).

### Build Configurations

- **Debug**: Live shader compilation, validation layers, full diagnostics
- **Release**: Optimized shaders, minimal overhead for final demo output

## Usage

1. **Launch Editor**: Run `build\bin\ShaderLabEditor.exe` from the repo root
2. **Load Audio**: Import a music track (WAV, MP3, OGG)
3. **Set BPM**: Configure tempo for beat synchronization
4. **Create Effect**: Write HLSL pixel shaders in the Effect View
5. **Arrange Timeline**: Build a demo sequence in the Demo View
6. **Export**: Compile optimized standalone executable

## Licensing

ShaderLab uses a dual-license model:

- **Engine/Editor Code**: Custom non-commercial Community License (see [LICENSE-COMMUNITY.md](LICENSE-COMMUNITY.md))
- **Creative Assets**: CC BY-NC-SA 4.0 (see [creative/LICENSE.md](creative/LICENSE.md))
- **Commercial Use**: Separate commercial license available (see [LICENSE-COMMERCIAL.md](LICENSE-COMMERCIAL.md))

### Community License Summary

- ✅ Personal, educational, and non-commercial use
- ✅ Creating and sharing demoscene productions
- ✅ Contributing back to the project
- ❌ Commercial products or services
- ❌ Redistribution as standalone SDK

For commercial licensing, please contact the maintainers.

## Philosophy

ShaderLab embraces demoscene principles:

- **Minimalism**: Small footprint, no bloat
- **Performance**: Direct3D 12, optimized compilation
- **Live Coding**: Instant shader feedback
- **Music-Driven**: Beat synchronization at the core
- **Community**: Open development, shared learning

We avoid over-engineering. The codebase prioritizes clarity, performance, and extensibility over feature bloat.

## Contributing

Contributions are welcome! Please:

1. Open an issue to discuss major changes
2. Follow the existing code style
3. Test your changes in Debug and Release
4. Submit a pull request with clear description

All contributions must be licensed under the Community License.

## Roadmap

- [x] Core D3D12 rendering
- [x] DXC shader compilation
- [x] Beat clock and audio sync
- [x] Basic ImGui interface
- [ ] Multi-pass rendering system
- [ ] Shader parameter automation
- [ ] Playlist and transitions
- [ ] Export to standalone runtime
- [ ] Shader library and presets

## Credits

Created by the ShaderLab community.

Inspired by:
- ShaderToy (Inigo Quilez and contributors)
- Demoscene tracker tools (Renoise, Buzz)
- Notch Builder, TouchDesigner

## Contact

- **Issues**: [GitHub Issues](https://github.com/drcircuit/ShaderLabDX12/issues)
- **Discussions**: [GitHub Discussions](https://github.com/drcircuit/ShaderLabDX12/discussions)
- **Commercial Licensing**: Open an issue with tag [commercial-license]

---

**ShaderLab** - Realtime shader authoring for the demoscene  
Copyright (c) 2026 ShaderLab Contributors
