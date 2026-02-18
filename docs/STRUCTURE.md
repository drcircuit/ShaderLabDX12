# ShaderLab Project Structure

High-level layout of active directories and ownership.

## Repository Layout

```text
ShaderLab/
├── include/ShaderLab/            # Public headers by subsystem
├── src/
│   ├── app/                      # Entry points (editor/runtime players)
│   ├── audio/                    # Audio playback and beat timing
│   ├── core/                     # Build/export/packaging and project IO
│   ├── editor/                   # Editor-side logic and orchestration
│   ├── graphics/                 # D3D12 device, swapchain, render infra
│   ├── runtime/                  # Runtime-side systems and playback
│   ├── shader/                   # DXC integration and shader services
│   └── ui/                       # ImGui-based editor UI
├── creative/                     # Example shaders/projects and creative assets
├── docs/                         # Canonical documentation
│   └── archive/                  # Historical/deprecated notes
├── third_party/                  # External dependencies
├── tools/                        # Setup/build/check/sign utility scripts
└── templates/                    # Standalone/runtime build templates
```

## Build Artifacts (Generated)

- `build/`
- `build_player/`
- `build_selfcontained/`
- `build_selfcontained_ninja/`
- `build_selfcontained_pack/`

These are generated outputs and should not be treated as source directories.

## Runtime Target Policy

- Tiny presets use `MicroPlayer` and x86 output.
- Open/free preset uses full runtime and x64 output.

## Canonical Docs

- `docs/README.md`
- `docs/QUICKSTART.md`
- `docs/BUILD.md`
- `docs/ARCHITECTURE.md`
- `docs/CONTRIBUTING.md`
