# ShaderLab v2.0.0 — Release Notes

Release date: 2026-02-23

## Scope

These notes cover the complete `v2.0.0` release scope now being shipped.

## Highlights

- Major quality pass across editor rendering, transport timing, and runtime preview stability.
- New smooth music-sync shader uniforms for beat-reactive effects (`fBeat`, `fBarBeat`) with compatibility support for quantized timing (`fBarBeat16`).
- Performance overlay improvements, including clearer GPU/VRAM visibility and restored runtime toggles.
- Windows release metadata is now centralized and embedded in binaries using standard VERSIONINFO resources.
- Large UI feature split/refactor across Scene, Demo, PostFX, Theme, Transport, and core helper modules for maintainability.
- PostFX and Compute chain authoring flow expanded with dedicated library/source/chain windows and per-effect controls.

## What’s New

### Rendering and Shader Workflow

- Added continuous musical timing values to shader constants:
  - `fBeat` (continuous beat timeline)
  - `fBarBeat` (continuous beat inside current bar)
  - `fBarBeat16` retained for stepped 16th-note compatibility
- Propagated timing updates through both editor preview and runtime player paths.
- Improved compute/post-processing preview robustness around effect chains and device transitions.
- Added compute effect chain execution path with per-effect pipeline compilation, dispatch resources, history textures, and ping-pong output handling.
- Added dedicated scene compile path and texture I/O/resource helpers to improve render-path reliability and code ownership boundaries.

### Performance Overlay

- Overlay was refactored into a clearer componentized render path.
- Theme integration now controls overlay appearance consistently (color and font controls via theme system).
- True viewport DPI handling restored for overlay layout/scaling behavior.
- VRAM telemetry presentation improved for quick health checks.
- Overlay now reports mode context, preview resolution, frame timing, VRAM budget usage, and active compute count.

### Runtime and UX

- Transport/runtime diagnostics and playlist interaction updates for smoother live authoring workflows.
- Preview and fullscreen flows polished for consistency with aspect management and editor state.
- Additional UI modularization/refactor work across scene, post-FX, and helper views.
- Added screen-keys capture overlay (toggleable) for scene-mode key visibility and debugging.
- Expanded runtime log window and transport controls with better playback gating and status behavior.
- Added/expanded snippet management workflows (foldered snippets, draft editing popup, insert/overwrite flows).
- Added theme editor refinements, built-in style presets, background tiling support, and persisted UI theme settings.

### Authoring and Playlist Tooling

- Demo playlist UI now includes richer beat/scene/transition/music/oneshot authoring interactions.
- Added tighter playlist scrubbing/focus behavior and improved beat-follow interaction in the tracker grid.
- Added stronger transition timeline handling and transport state reset helpers.

## Build, Packaging, and Release Pipeline

### Single Metadata Source (Windows)

ShaderLab now uses one source for app identity/versioning:

- `metadata/app_metadata.json`

This metadata is consumed by:

- CMake project versioning
- Generated C++ metadata header
- Windows VERSIONINFO resources attached to binaries
- Installer versioning
- GitHub release workflow version resolution

### Embedded VERSIONINFO Resources

VERSIONINFO is now attached to key Windows outputs, including:

- `ShaderLabIIDE.exe`
- `ShaderLabPlayer.exe`
- `ShaderLabScreenSaver.scr`
- `ShaderLabBuildCli.exe` (when built)

### Installer Improvements

- Installer version resolves from `metadata/app_metadata.json`.
- Uninstall entry now provides explicit icon metadata (`UninstallDisplayIcon`).

## Notes for Maintainers

- To ship a new version, update only `metadata/app_metadata.json` (`version`, identity fields).
- Rebuild/repackage as normal; generated metadata/resources update automatically.
- `include/ShaderLab/Constants.h` remains as a compatibility wrapper and resolves to generated metadata values.

## Known Notes

- Existing script-lint warning about PowerShell verb naming in installer tooling is non-blocking for release artifacts.

---

Thanks to everyone testing and iterating quickly on rendering, timing sync, and packaging quality this cycle.
