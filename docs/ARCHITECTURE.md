# ShaderLab Architecture

This document provides an overview of ShaderLab's architecture and design decisions.

## Core Principles

ShaderLab follows these design principles:

1. **Minimalism**: Only include what's necessary for demoscene production
2. **Performance**: Direct3D 12 with minimal overhead
3. **Live Feedback**: Instant shader recompilation for creative flow
4. **Beat-Driven**: Music and rhythm at the core of the experience
5. **Community**: Open development with shared creative assets

## System Architecture

### Graphics Module (`src/graphics/`)

The graphics module wraps Direct3D 12 with a thin RAII-based abstraction:

- **Device**: D3D12 device initialization with debug layer support
- **CommandQueue**: Command list management and GPU synchronization
- **Swapchain**: Present queue and backbuffer management

Design decisions:
- Minimal abstraction overhead
- RAII for resource lifetime management
- No global state
- Validation layers enabled in debug builds

### Shader Module (`src/shader/`)

The shader system uses Microsoft's DXC compiler for HLSL 6.x support:

- **ShaderCompiler**: DXC wrapper with two compilation modes
  - **Live Mode**: Fast compilation, debug symbols, no optimization
  - **Build Mode**: Full optimization (O3), stripped for final demo

Features:
- Real-time compilation from source or file
- Structured diagnostic parsing (file, line, column)
- Include file support via DXC's include handler

### Audio Module (`src/audio/`)

Lightweight audio system built on miniaudio:

- **AudioSystem**: Audio file loading and playback
- **BeatClock**: BPM-based timing system

The beat clock provides:
- Quarter/eighth/sixteenth note counting
- Bar tracking (based on time signature)
- Hit flags for beat detection
- Normalized phase values (0.0 to 1.0) for smooth animation

Design decisions:
- Single audio track at a time (sufficient for demos)
- No mixing or effects (keep it simple)
- High-precision timing for beat synchronization

### UI Module (`src/ui/`)

Dear ImGui-based interface with demoscene aesthetic:

- **UISystem**: ImGui integration with D3D12
- **Demo View**: Timeline, transport controls, playlist
- **Scene View**: Realtime shader preview
- **Effect View**: Shader editor and parameters

Features:
- Dockable panels
- Dark theme optimized for long coding sessions
- Win32 and D3D12 backend integration

### Editor Application (`src/app/editor/`)

The editor ties everything together:

- **EditorApp**: Main application class
- **main.cpp**: Win32 entry point and message loop

The editor uses a simple single-threaded architecture:
1. Process Win32 messages
2. Update audio and beat clock
3. Render scene
4. Present frame

## Data Flow

```
Audio File → AudioSystem → playback time
                              ↓
                         BeatClock → beat counts, phases
                              ↓
                         Shader Constants → GPU
                              ↓
                         Scene Rendering
```

## Build System

CMake-based build with these targets:

- **ShaderLabCore**: Static library with all core systems
- **ShaderLabEditor**: Editor executable
- **ShaderLabRuntime**: Standalone runtime (optional)

Third-party dependencies:
- Dear ImGui (UI framework)
- miniaudio (audio playback)
- nlohmann/json (project serialization)
- stb_image (texture loading)

## Future Extensions

Planned additions (without over-engineering):

1. **Multi-pass rendering**: Render-to-texture for buffer passes
2. **Parameter automation**: Animate shader uniforms on timeline
3. **Playlist system**: Sequence effects with beat-anchored transitions
4. **Project serialization**: Save/load shader setups as JSON
5. **Texture support**: Load images as shader inputs
6. **Export pipeline**: Build standalone runtime executable

## Performance Considerations

- Command list recording happens every frame (simple approach)
- GPU sync after each frame (no frame pipelining yet)
- ImGui rendering uses dedicated descriptor heap
- Shader hot-reload with minimal overhead

For final demos, the runtime can be optimized further by:
- Pre-compiling all shaders in Build mode
- Removing debug/validation layers
- Frame pipelining for higher throughput
- Stripping editor UI code

## Code Style

- C++20 with modern features (concepts, ranges, etc. where appropriate)
- RAII for resource management
- Smart pointers for ownership
- Raw pointers for non-owning references
- Minimal use of exceptions (favor return codes for expected errors)
- Clear naming: no Hungarian notation
- Comments only where intent isn't obvious

## License Architecture

The project uses a dual-license model:

- **Engine/Editor**: Custom community license (non-commercial)
- **Creative Assets**: CC BY-NC-SA 4.0
- **Commercial Use**: Separate commercial license

This allows community growth while providing a path for commercial applications.
