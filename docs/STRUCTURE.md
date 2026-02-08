# ShaderLab Project Structure

```
ShaderLab/
│
├── include/                       # Public headers
│   └── ShaderLab/
│       ├── App/                   # App-facing interfaces
│       ├── Audio/                 # Audio and beat synchronization
│       ├── Core/                  # Data model and build/export services
│       ├── Graphics/              # Direct3D 12 abstraction
│       ├── Shader/                # Shader compilation system
│       └── UI/                    # Editor UI interfaces
│
├── src/                           # Implementation
│   ├── app/
│   │   ├── editor/                # Editor application
│   │   │   ├── main.cpp           # Win32 entry point
│   │   │   └── EditorApp.cpp      # Application implementation
│   │   └── runtime/               # Standalone demo runtime
│   │       ├── main_player.cpp    # Runtime entry point
│   │       └── DemoPlayer.cpp     # Player implementation
│   │
│   ├── engine/                    # Core engine systems (minimal, for future use)
│   │
│   ├── graphics/                  # Direct3D 12 abstraction
│   │   ├── Device.cpp             # D3D12 device management
│   │   ├── CommandQueue.cpp       # Command list and GPU sync
│   │   └── Swapchain.cpp          # Swapchain and backbuffers
│   │
│   ├── shader/                    # Shader compilation system
│   │   └── ShaderCompiler.cpp     # DXC wrapper, live/build modes
│   │
│   ├── audio/                     # Audio and beat synchronization
│   │   ├── AudioSystem.cpp        # miniaudio wrapper
│   │   └── BeatClock.cpp          # BPM-based timing
│   │
│   ├── core/                      # Data model and build/export services
│   │   ├── Serializer.cpp         # Project IO and packing
│   │   ├── PackageManager.cpp     # Packed executable reader
│   │   ├── BuildPipeline.cpp      # Self-contained build pipeline
│   │   └── RuntimeExporter.cpp    # Runtime export helpers
│   │
│   └── ui/                        # User interface
│       └── UISystem.cpp           # Dear ImGui integration, panels
│
├── creative/                      # Creative assets (CC BY-NC-SA 4.0)
│   ├── shaders/                   # Example shaders
│   │   ├── example_gradient.hlsl
│   │   └── example_tunnel.hlsl
│   │
│   ├── examples/                  # Demo projects
│   │   └── README.md
│   │
│   └── LICENSE.md                 # CC BY-NC-SA 4.0 license
│
├── docs/                          # Documentation
│   ├── ARCHITECTURE.md            # System architecture overview
│   ├── BUILD.md                   # Build instructions
│   ├── QUICKSTART.md              # Quick start guide
│   ├── STRUCTURE.md               # Project structure guide
│   ├── feature_time_offset.md     # Feature notes
│   └── CONTRIBUTING.md            # Contribution guidelines
│
├── third_party/                   # External dependencies
│   ├── imgui/                     # Dear ImGui (to be downloaded)
│   ├── miniaudio/                 # miniaudio (to be downloaded)
│   ├── json/                      # nlohmann/json (to be downloaded)
│   ├── stb/                       # stb_image (to be downloaded)
│   ├── CMakeLists.txt             # Third-party build config
│   └── README.md                  # Dependency instructions
│
├── tools/                         # Build and utility scripts
│
├── build/                         # Build output (gitignored)
│   ├── bin/                       # Compiled executables
│   └── lib/                       # Static libraries
├── build_player/                  # Runtime-only build output (gitignored)
├── build_selfcontained/           # Self-contained build output (gitignored)
├── build_selfcontained_ninja/     # Ninja self-contained build output (gitignored)
├── build_selfcontained_pack/      # Packaged assets output (gitignored)
│
├── CMakeLists.txt                 # Top-level CMake configuration
├── .gitignore                     # Git ignore rules
├── README.md                      # Project overview
├── LICENSE-COMMUNITY.md           # Community license (non-commercial)
└── LICENSE-COMMERCIAL.md          # Commercial license template
```

## Module Dependencies

```
EditorApp
    ├── Device
    ├── CommandQueue
    │   └── Device
    ├── Swapchain
    │   ├── Device
    │   └── CommandQueue
    ├── UISystem
    │   ├── Device
    │   └── Swapchain
    ├── AudioSystem
    ├── BeatClock
    └── ShaderCompiler
```

## Key Files

### Entry Points
- `src/app/editor/main.cpp` - Editor Win32 application entry
- `src/app/runtime/main_player.cpp` - Standalone demo runtime entry

### Core Systems
- `src/graphics/Device.cpp` - D3D12 initialization
- `src/shader/ShaderCompiler.cpp` - DXC shader compilation
- `src/audio/BeatClock.cpp` - Beat synchronization logic
- `src/ui/UISystem.cpp` - ImGui panels and rendering

### Build Configuration
- `CMakeLists.txt` - Main build configuration
- `third_party/CMakeLists.txt` - ImGui build setup

### Documentation
- `README.md` - Project overview and getting started
- `docs/QUICKSTART.md` - 5-minute setup guide
- `docs/BUILD.md` - Detailed build instructions
- `docs/ARCHITECTURE.md` - System design and philosophy

## File Naming Conventions

- **C++ Headers**: `.h` extension
- **C++ Source**: `.cpp` extension
- **Shaders**: `.hlsl` extension
- **Documentation**: `.md` (Markdown)
- **Configuration**: CMakeLists.txt, .gitignore

## Code Organization

- **RAII pattern**: Resources managed via constructors/destructors
- **Smart pointers**: std::unique_ptr for ownership
- **Namespaces**: Everything in `ShaderLab` namespace
- **Include guards**: `#pragma once` for all headers
- **Forward declarations**: Used to minimize includes

## Build Outputs

After building, the structure includes:

```
build/
├── bin/
│   └── ShaderLabEditor.exe       # Main editor executable
└── lib/
    └── ShaderLabCore.lib          # Core library
```

## Future Additions

Planned structure expansions:

```
src/engine/
├── Scene.h/.cpp                   # Scene management
├── PassManager.h/.cpp             # Multi-pass rendering
└── ParamAutomation.h/.cpp         # Parameter keyframes

creative/
├── projects/                      # User projects
│   └── example_demo/              # Example full demo
└── textures/                      # Image resources

tools/
├── build_demo.py                  # Export script
└── shader_validator.py            # Offline shader checker
```

---

This structure emphasizes:
- **Separation of concerns**: Graphics, audio, UI, shaders
- **Minimal coupling**: Each module is independent
- **Clear ownership**: Unique pointers for resources
- **Community assets**: Separate creative/ directory with permissive license
