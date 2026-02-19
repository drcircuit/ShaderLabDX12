# Building ShaderLab

This document is the source of truth for local builds and packaged outputs.

## Prerequisites

- Windows 10/11
- Visual Studio 2022 with C++ Desktop workload
- CMake and Ninja (available in typical VS dev setups)
- Third-party dependencies present under `third_party/` (see `third_party/README.md`)

## Recommended Workflow (VS Code Tasks)

From VS Code, use the workspace tasks:

- `Build ShaderLab (Debug)`
- `Build ShaderLab (Release)`
- `Reconfigure CMake`
- `Run ShaderLabEditor (Dev Env)`

These tasks automatically load `tools/dev_env.ps1` before configuring/building/running.

## Command-Line Workflow

From repository root in PowerShell:

```powershell
.\tools\dev_env.ps1
cmd /c .\.vscode\build-debug.bat
```

For release:

```powershell
.\tools\dev_env.ps1
cmd /c .\.vscode\build-release.bat
```

## Build Modes and Runtime Targets

ShaderLab supports two practical runtime paths:

- Tiny preset range (`Micro 1K` through `Micro 64K`):
  - Uses `MicroPlayer`
  - Targets x86 (Win32)
- Open/free preset (`None`):
  - Uses full runtime player
  - Targets x64

The build pipeline logs selected architecture and runtime path in build output.

## Size-Sensitive Logging Flags

The following CMake options are default OFF and intended only for diagnostics:

- `SHADERLAB_RUNTIME_DEBUG_LOG`
- `SHADERLAB_COMPACT_TRACK_DEBUG`

These are exposed in the editor Build Settings and should remain OFF for tiny/release size runs.

## Crinkler Notes

- Crinkler integration depends on a valid MSVC environment.
- If toolset pinning fails, the build pipeline retries with default toolset resolution.
- Validate your local `SHADERLAB_CRINKLER` path in environment setup when using crinkled outputs.

## Build Outputs

Typical outputs:

- Editor: `build/bin/ShaderLabEditor.exe`
- Runtime/player outputs: under active build directory `bin/`
- Packaged assets: `build_selfcontained_pack/` when packaging is used

## Clean Solution Export Format

For all build modes/targets, the exported clean solution folder uses linked shader sources:

- `project.json` stores shader references (`codePath` and `code` as `@file:<relative-path>`)
- Scene and post-FX HLSL files are written under `assets/shaders/hlsl/`
- Shader source is not embedded inline in clean-solution `project.json`

This keeps the clean solution recompile-friendly when you edit and rebuild shaders directly from source files.

## Troubleshooting

- Missing compiler headers (`cstdint`, `string`, `excpt.h`) usually means MSVC environment is not loaded correctly.
- Re-run environment setup in the same shell: `.\tools\dev_env.ps1`.
- Reconfigure CMake if switching major toolchain/runtime options.

## Related Docs

- `docs/QUICKSTART.md`
- `docs/ARCHITECTURE.md`
- `docs/STRUCTURE.md`
- `third_party/README.md`
