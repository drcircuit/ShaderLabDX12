# ShaderLab Quick Start

Fast path from clone to first editor run.

## 1) Clone

```powershell
git clone https://github.com/drcircuit/ShaderLabDX12.git
cd ShaderLab
```

## 2) Ensure Dependencies

Confirm third-party files are present under `third_party/`.

If needed, run:

```powershell
.\tools\setup.ps1
```

Then verify with:

```powershell
.\tools\check.ps1
```

## 3) Build (Recommended: VS Code Task)

Run the `Build ShaderLab (Debug)` task.

Equivalent command-line flow:

```powershell
.\tools\dev_env.ps1
cmd /c .\.vscode\build-debug.bat
```

## 4) Run Editor

From repository root:

```powershell
.\build\bin\ShaderLabEditor.exe
```

## 5) Build Preset Behavior (Important)

- Tiny presets (`Micro 1K`-`Micro 64K`) target `MicroPlayer` on x86.
- Open/free preset (`None`) targets full runtime on x64.

## 6) Size-Sensitive Defaults

Debug logging flags used by runtime/compact track code are OFF by default.
Only enable them for troubleshooting, not for final tiny outputs.

## 7) Clean Solution Shader Sources

When you export/build with a clean solution folder:

- Shader source is written to `assets/shaders/hlsl/`
- `project.json` stores shader links (`codePath` and `code` as `@file:<relative-path>`)
- Full shader source is not embedded inline in clean-solution `project.json`

This is the expected format for recompiling shaders directly from the exported solution.

## Next

- Build and packaging details: `docs/BUILD.md`
- System overview: `docs/ARCHITECTURE.md`
- Repository layout: `docs/STRUCTURE.md`
