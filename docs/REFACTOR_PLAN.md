# ShaderLab Refactor Plan: Component Separation

Date: 2026-02-21
Status: Draft v3 (M6 prebuilt packaging spike implemented and validated)

## Recent Progress (2026-02-20)

Additional progress (2026-02-21):

- Completed a milestone-sized behavior-preserving decomposition of `src/core/Serializer.cpp` packaging flow:
  - introduced file-scope helpers for stream reads, project asset-path traversal, directory blob creation, output directory creation, and final packed executable writing.
  - introduced `ExecutablePackAccumulator` to centralize packed-entry dedupe/indexing and asset ingestion (`project.json`, project assets, extra files).
  - reduced `Serializer::PackExecutable` to orchestration-only flow (read/load -> accumulate -> directory blob -> write).
- Validated the decomposition with full workspace CMake builds and `tools/check.ps1` guardrails after each refactor slice.
- During the larger extraction pass, one namespace regression (`PackedExtraFile` type scope) was intentionally tolerated, diagnosed, fixed immediately, and revalidated to green.

Completed runtime refactor slices in `src/app/runtime` with behavior-preserving changes and successful runtime target builds:

- Extracted window/presentation helpers into:
  - `include/ShaderLab/Runtime/RuntimeWindowPolicy.h`
  - `src/app/runtime/RuntimeWindowPolicy.cpp`
- Extracted startup/system helpers into:
  - `include/ShaderLab/Runtime/RuntimeStartupPolicy.h`
  - `src/app/runtime/RuntimeStartupPolicy.cpp`
- Refactored `PlayerApp.cpp` to use the policy modules for:
  - fullscreen/windowed transitions
  - title/FPS updates and initial black frame present
  - single-instance mutex + runtime error log
  - debug console setup + cursor lifecycle
- Consolidated player runtime state and resources in `PlayerApp.cpp`:
  - `RuntimeAppState` (flags/timing/title/window/cursor)
  - `RuntimeResources` (device/queue/swapchain/player)
- Deduplicated teardown path via `ShutdownRuntimeResources()` used by both startup-failure and normal shutdown.
- Added non-breaking CMake groundwork targets for separation:
  - `ShaderLabCoreApi`
  - `ShaderLabDevKit`
  - `ShaderLabEditorLib`
  while preserving existing executable linkage through `ShaderLabCore`.
- Migrated `ShaderLabBuildCli` linkage to the split graph (`ShaderLabDevKit` + `ShaderLabCoreApi`) and validated build.
- Tightened concern boundaries in CMake target split:
  - `ShaderLabDevKit` is now player-focused (runtime player sources only).
  - build/export orchestration moved to `ShaderLabDevKitBuildTools`.
  - `ShaderLabPlayer` / `ShaderLabScreenSaver` now link entrypoint-only executables against `ShaderLabDevKit` + `ShaderLabCoreApi`.
  - `ShaderLabMicroPlayer` CMake wiring now follows the same entrypoint-only pattern (when `SHADERLAB_BUILD_MICRO_PLAYER=ON`).
  - `ShaderLabBuildCli` now links `ShaderLabDevKitBuildTools` + `ShaderLabCoreApi`.
  - `ShaderLabEditor` now links split libs (`ShaderLabEditorLib` + `ShaderLabDevKitBuildTools` + `ShaderLabCoreApi`) instead of monolithic `ShaderLabCore`.
- Removed monolithic `ShaderLabCore` target from the main workspace `CMakeLists.txt`; split targets are now the authoritative build graph.
- Migrated `templates/Standalone_CMakeLists.txt` to split targets (`ShaderLabCoreApi` + `ShaderLabDevKit`) and entrypoint-only player executable linkage.
- Completed runtime include boundary cleanup:
  - added `include/ShaderLab/Runtime/RuntimeStartupPolicy.h`
  - added `include/ShaderLab/Runtime/RuntimeWindowPolicy.h`
  - switched runtime sources to include the new Runtime headers
  - removed temporary compatibility forwarders under `include/ShaderLab/App`
- Began Dev Kit include boundary migration for build/export APIs:
  - added `include/ShaderLab/DevKit/BuildPipeline.h`
  - added `include/ShaderLab/DevKit/RuntimeExporter.h`
  - switched editor/build-cli/build-tools includes to `ShaderLab/DevKit/*`
  - removed temporary compatibility forwarders under `include/ShaderLab/Core`
- Completed docs synchronization for split architecture:
  - `docs/ARCHITECTURE.md` updated with split target graph and ownership boundaries
  - `docs/STRUCTURE.md` updated with include/layer ownership map
  - `docs/BUILD.md` updated with linkage graph and layering check flow
- Updated editor dev_kit post-build assembly paths to copy only DevKit/player-relevant include/src/third_party content.
- Added startup/relaunch matrix validation script and VS Code task:
  - `tools/validate_startup_relaunch_matrix.ps1`
  - task label: `Startup Relaunch Validation`

Current remaining follow-up:
- Decide whether M6 prebuilt packaging becomes the default Dev Kit distribution mode (spike completed).

Guardrail status:
- Configure-time layering checks are now active in both:
  - main workspace `CMakeLists.txt`
  - `templates/Standalone_CMakeLists.txt`
- Current checks fail fast if runtime/player targets include build-export (`BuildPipeline`/`RuntimeExporter`) or UI source paths.
- `tools/check.ps1` now runs a dedicated CMake configure pass under `vcvars64.bat` to validate layering assertions in the normal local validation flow.
- `tools/check.ps1` now also enforces the MicroPlayer compact-track packaging contract via `ShaderLabBuildCli` log assertion.
- Release workflow now runs `tools/check.ps1` after Release build to keep guardrails active on clean-machine CI.

Notes:
- Existing Pylance ANSI/Unicode Win32 signature diagnostics in `PlayerApp.cpp` remain unchanged from baseline and are non-blocking for current build config.

## Near-Term Execution Plan (Next Steps)

1. **M1 target split groundwork (CMake-only, no behavior change)**
  - Introduce explicit target groupings (`CoreApi`, `DevKit`, `EditorLib` naming may remain provisional).
  - Keep link outputs identical while moving source lists behind new target boundaries.
  - Verify Debug + Release + runtime executables.

2. **Runtime include boundary cleanup** ✅
  - runtime policy headers moved to `include/ShaderLab/Runtime`.
  - temporary compatibility includes under `include/ShaderLab/App` removed.
  - main + standalone CMake runtime source/header lists updated and validated.

3. **Dev Kit ownership migration for build/export orchestration** ✅
  - `BuildPipeline` / `RuntimeExporter` include boundaries moved to `ShaderLab/DevKit` with compatibility forwards.
  - compatibility forwards removed; canonical includes now `ShaderLab/DevKit/*`.
  - editor orchestration remains API-driven.
  - standalone build-cli and editor flows validated in workspace build.

4. **Editor isolation and integration** (in progress)
  - continue enforcing editor boundary as entrypoint + editor-lib orchestration only.
  - add explicit CMake layering assertion for `ShaderLabEditor` executable target.
  - add `tools/check.ps1` include-surface gate to prevent editor/UI code from including runtime-internal headers.
  - enforce the same guard in CI on pull requests/pushes via `.github/workflows/validate.yml`.

5. **Layering guardrails + smoke checks** (in progress)
  - Add lightweight checks preventing `src/ui/*` from leaking into runtime/core-only targets.
  - Add launch/relaunch smoke matrix script hooks for runtime outputs.
  - keep local/CI validation gates aligned (`tools/check.ps1`, `.github/workflows/validate.yml`, release guard step).
  - enforce no-exclusive-fullscreen policy via `tools/check.ps1` runtime API scan.
  - Keep “no exclusive fullscreen” policy validated.

6. **Docs sync for architecture boundaries** ✅
  - `docs/ARCHITECTURE.md`, `docs/STRUCTURE.md`, and build notes updated.
  - docs now reflect split target graph and ownership rules.

## Confirmed Decisions (2026-02-20)

- Core API migration will be **static-first**, with optional DLL conversion later.
- `BuildPipeline` and `RuntimeExporter` are **fully Dev Kit-owned**; Editor orchestrates only.
- API boundaries are **not frozen yet** (no stability lock milestone currently).

## Goal

Refactor the solution into three explicit deliverables with clear ownership and minimal coupling:

1. **Core API**
   - Shared runtime/engine API (bootstrapping, sync tracker, serializer/compiler primitives, graphics/audio/shader abstractions).
2. **Dev Kit (SDK)**
   - Player source, sync-track runtime integration, bundled runtime assets (transitions/post FX/examples), build + pack pipeline.
3. **Editor**
   - Authoring tool on top of Dev Kit + Core API: UI, project authoring, shader/snippet management, sequencing, export orchestration.

## Current State (from repo)

- `ShaderLabCore` currently mixes concerns:
  - Runtime/core systems (`src/graphics`, `src/audio`, `src/shader`, `src/core/Serializer.cpp`, `src/core/PackageManager.cpp`)
  - Editor-only systems (`src/ui/*`, `src/core/BuildPipeline.cpp`, `src/core/RuntimeExporter.cpp`) when `SHADERLAB_BUILD_EDITOR=ON`.
- Editor and players both link the same `ShaderLabCore` target.
- Dev kit deployment is generated from editor post-build copy (`dev_kit` folder).
- Runtime policy now stabilized on DX12 borderless + independent flip behavior.

## Target Architecture

## 1) Core API

**Owns**
- `src/graphics/*`
- `src/audio/*` (sync/clock, audio runtime primitives)
- `src/shader/*`
- `src/core/Serializer.*`, `src/core/PackageManager.*`
- Public API headers under `include/ShaderLab/{Graphics,Audio,Shader,Core}` (core-only subset)

**Must not own**
- Editor UI (`src/ui/*`)
- Build/export orchestration for deliverables (`BuildPipeline`, `RuntimeExporter`)
- Runtime player app wiring (`src/app/runtime/*`)

## 2) Dev Kit (SDK)

**Owns**
- Player app/runtime entry + orchestration (`src/app/runtime/*`)
- Runtime playback layer (`src/runtime/*` / `DemoPlayer` integration)
- Export/build/pack tooling (`src/core/BuildPipeline.*`, `src/core/RuntimeExporter.*`, CLI wiring)
- Runtime creative assets/templates and standalone CMake template

**Consumes**
- Core API as dependency

## 3) Editor

**Owns**
- `src/app/editor/*`
- `src/editor/*`
- `src/ui/*`
- Editor assets/workflows

**Consumes**
- Dev Kit services for export/build orchestration
- Core API for preview/runtime subsystems

## Milestones and Checkpoints

## M0 — Baseline Lock + Safety Gates (0.5–1 day)

**Tasks**
- Freeze behavioral baseline for player startup and relaunch stability.
- Record current build matrix commands and expected artifacts.

**Checkpoints**
- Gate A: packaged/packed/self-contained/micro/crinkled all launch with no second-run regression.
- Gate B: fullscreen policy remains borderless/eFSE (no exclusive mode path).

## M1 — Build Target Separation in CMake (1–2 days)

**Tasks**
- Split monolithic `ShaderLabCore` into explicit targets (names tentative):
  - `ShaderLabCoreApi`
  - `ShaderLabDevKit`
  - `ShaderLabEditorLib`
- Move editor-only source lists out of core target.
- Wire executables:
  - `ShaderLabPlayer`, `ShaderLabScreenSaver`, `ShaderLabMicroPlayer` -> Dev Kit + Core API
  - `ShaderLabEditor` -> EditorLib + Dev Kit + Core API
  - `ShaderLabBuildCli` -> Dev Kit + Core API

**Checkpoints**
- Build succeeds Debug + Release.
- Link graph confirms no `src/ui/*` in Core API target.

## M2 — Core API Boundary Hardening (1–2 days)

**Tasks**
- Define stable Core API header set.
- Remove/relocate headers that couple editor/devkit concerns into core include surface.
- Normalize compile defs so runtime/editor toggles do not leak through Core API unnecessarily.

**Checkpoints**
- Core API builds independently.
- No dependency from Core API to editor modules.

## M3 — Dev Kit Consolidation (1–2 days)

**Tasks**
- Move `BuildPipeline` + `RuntimeExporter` under explicit Dev Kit ownership (code + include paths).
- Consolidate runtime player policy helpers (startup/window/swapchain policy) into shared Dev Kit/runtime utility module.
- Ensure templates/dev kit package reflect separated ownership.

**Checkpoints**
- Standalone dev_kit export builds player successfully.
- Clean-solution export path resolves templates/source root correctly.

## M4 — Editor Isolation and Integration (1–2 days)

**Tasks**
- Keep editor orchestration thin; editor calls Dev Kit APIs for build/export.
- Remove editor direct reach into internals that should live in Dev Kit.
- Keep editor-specific assets and UI logic isolated from runtime player code.

**Checkpoints**
- Editor authoring + export flows function unchanged.
- Runtime outputs produced by editor match baseline behavior.

## M5 — Packaging, Docs, and CI Gates (1 day)

**Tasks**
- Update docs (`ARCHITECTURE`, `STRUCTURE`, build docs) to reflect new boundaries.
- Add CI/build checks to enforce layering constraints (at least by target source lists + grep guards).
- Add “no exclusive fullscreen” guard check.

**Checkpoints**
- Docs and target graph consistent.
- Build/test gates pass on clean machine path.

## M6 — Dev Kit Prebuilt Packaging Spike (0.5–1 day)

Purpose: resolve whether Dev Kit should support a practical prebuilt distribution that includes player binaries + precompiled shaders + compact track/sync payloads.

**Tasks**
- Define candidate bundle layout (binaries, `assets/shaders/*.cso`, compact track payload, manifest). ✅
- Validate runtime loader expectations against bundled structure. ✅
- Produce one reference prebuilt package and run launch matrix. ✅

Implementation notes:

- Added `tools/validate_devkit_prebuilt_spike.ps1`:
  - builds a reference packaged zip via `ShaderLabBuildCli` with `--target packaged --restricted-compact-track`
  - validates extracted layout (`project.json`, runtime exe, `assets/track.bin`, precompiled shader outputs)
  - validates manifest-driven loader contract (relative asset paths + resolved precompiled files)
  - runs startup/relaunch matrix by default (can be skipped with `-SkipLaunchMatrix`)
- Added VS Code task: `M6 Prebuilt Packaging Spike`.
- Wired a lightweight M6 gate into `tools/check.ps1` (runs validator with `-SkipLaunchMatrix` for routine checks).
- Wired full M6 spike into `.github/workflows/validate.yml` as a dedicated job for nightly schedule and optional manual dispatch (`run_full_m6=true`).

**Checkpoints**
- Package builds and runs without editor present.
- No shader runtime compilation dependency in packaged run.
- Sync/track payload loads from packaged layout without path hacks.

## Work Breakdown Backlog (Initial)

- [x] Create target split in `CMakeLists.txt`
- [x] Introduce `DevKit` include namespace or folder boundary
- [x] Move `BuildPipeline` / `RuntimeExporter` under Dev Kit ownership
- [x] Remove editor source files from core target
- [x] Introduce runtime startup/presentation policy utility (shared by all players)
- [x] Update editor post-build dev_kit assembly paths
- [ ] Update docs and add architecture diagram
- [x] Add regression scripts for startup/relaunch matrix
- [x] Wire layering/micro packaging guard checks into release CI

## Definition of Done (Program-Level)

- Three deliverables have explicit target boundaries and ownership docs.
- Core API can be built without editor or dev kit UI/tooling modules.
- Dev Kit can build runtime deliverables without editor binaries.
- Editor depends on Dev Kit/Core API via explicit APIs only.
- Startup/relaunch stability matrix remains green for all runtime output modes.

## Risks and Mitigations

- **Risk:** Refactor reintroduces runtime startup regressions.
  - **Mitigation:** Run launch matrix gate at every milestone.
- **Risk:** Include-path churn breaks dev_kit template exports.
  - **Mitigation:** Add template smoke build gate in M3.
- **Risk:** CMake target split causes duplicated compile defs/options.
  - **Mitigation:** Centralize common compile options in helper CMake function.

## Open Decisions / Ambiguities (Remaining)

1. **Dev Kit packaging model default**:
  - You prefer prebuilt in principle, but current uncertainty is bundling player + precompiled shaders + binary sync track reliably.
  - This is now scoped to milestone **M6** as an implementation spike with pass/fail criteria.

## Tracking Cadence

- Milestone status updates in this file.
- Every milestone closes with:
  - completed checklist
  - known deviations
  - next milestone start criteria.
