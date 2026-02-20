# ShaderLab Refactor Plan: Component Separation

Date: 2026-02-20
Status: Draft v2 (decisions captured, one packaging ambiguity remains)

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
- Define candidate bundle layout (binaries, `assets/shaders/*.cso`, compact track payload, manifest).
- Validate runtime loader expectations against bundled structure.
- Produce one reference prebuilt package and run launch matrix.

**Checkpoints**
- Package builds and runs without editor present.
- No shader runtime compilation dependency in packaged run.
- Sync/track payload loads from packaged layout without path hacks.

## Work Breakdown Backlog (Initial)

- [ ] Create target split in `CMakeLists.txt`
- [ ] Introduce `DevKit` include namespace or folder boundary
- [ ] Move `BuildPipeline` / `RuntimeExporter` under Dev Kit ownership
- [ ] Remove editor source files from core target
- [ ] Introduce runtime startup/presentation policy utility (shared by all players)
- [ ] Update editor post-build dev_kit assembly paths
- [ ] Update docs and add architecture diagram
- [ ] Add regression scripts for startup/relaunch matrix

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
