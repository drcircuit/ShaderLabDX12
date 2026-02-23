# ShaderLab UI Refactor Plan (Entropy Reduction)

Date: 2026-02-21  
Status: Active v2 (amended 2026-02-22 to match implementation reality)

Update (2026-02-21): feature-first organization is now the preferred refactor direction.

Update (2026-02-22): the refactor execution moved faster than the original phase ordering. The codebase now follows a hybrid of vertical mode views and feature modules, with `ShaderLabIDE.cpp` reduced to an orchestration-focused composition root.

## Reality Check (2026-02-22)

Implemented direction (behavior-preserving):

- `ShaderLabIDE.cpp` is now orchestration-only (constructor/lifecycle/project naming + delegation).
- Major responsibilities were extracted into `src/ui/Features/*` modules (Render/Frame, Transport, MainMenuBar, Theme, BuildSettings, About, Project state, Snippets, CodeEditor).
- Mode composition remains explicit under `src/ui/ShaderLabIDE{DemoMode,SceneMode,PostFXMode}.cpp` and view modules under `src/ui/ShaderLabIDEView/*`.
- Cross-cutting helpers live under `src/ui/ShaderLabIDECore/*` and `src/ui/Helpers/*`.
- App-side UI boundary folder was renamed from `src/app/editor/*` to `src/app/ShaderLabMain/*`.

Conclusion: the original goals are being met, but physical foldering is currently hybrid rather than a strict one-shot move into only `ShaderLabIDECore/ShaderLabIDEView/ShaderLabEditorWindows`.

## Why This Refactor

The current UI layer is feature-rich but increasingly hard to evolve safely because responsibilities are spread across large files and mixed concerns (layout, state mutation, rendering, styling, and platform behavior).

Primary outcomes:

1. Reduce maintenance cost and onboarding time.
2. Make behavior-preserving changes faster and safer.
3. Keep visual/theming consistency under explicit, testable boundaries.

## Scope and Constraints

In scope:

- `src/ui/*`
- editor-side UI integration points (`src/app/ShaderLabMain/*` where UI boundary touches Win32)
- existing theme system and UI panel composition

Out of scope (for this refactor wave):

- Runtime player rendering architecture
- Dev Kit packaging flow
- Broad UX redesign beyond behavior-preserving cleanup

Rules:

- Prefer behavior-preserving decomposition over redesign.
- Keep each slice compilable.
- Validate after each slice with existing build/check scripts.
- Organize primarily by feature concern (vertical slices), not by generic UI type.
- Local duplication between feature slices is acceptable when it improves clarity and ownership.
- Share only true cross-cutting primitives (themes, constants, transport, platform helpers).

## Target UI Architecture

## Canonical Vertical Structure (Requested)

The refactor target is explicitly organized into these verticals:

1. **ShaderLabIDECore**
2. **ShaderLabIDEView**
  - **DemoModeView**
  - **SceneModeView**
  - **PostFXModeView**
3. **ShaderLabEditorWindows**
  - **ThemeEditorWindow**
  - **BuildSettingsWindow**
  - **AboutWindow**

This structure is the primary organization rule for new extraction work.

## 1) Feature-First Vertical Slices (Primary Organization)

Purpose: each user-facing feature owns its own UI composition, state mutations, and local helpers.

Preferred feature slices:

- Transport + mode switch
- Demo metadata + tracker
- Shader editor + diagnostics
- Scene library + preview
- Build settings + dependency checks
- Theme editor
- Titlebar/menu chrome

Mapping to requested verticals:

- `ShaderLabIDECore`
  - shared editor orchestration state
  - theme/token application
  - global constants/layout metrics
  - transport primitives used by all views
  - platform boundary helpers (window actions, non-client policy)

- `ShaderLabIDEView`
  - `DemoModeView`: demo metadata, tracker, demo transport-facing interactions
  - `SceneModeView`: scene library, scene preview, scene-specific editing flows
  - `PostFXModeView`: post FX chain editing, post FX diagnostics, compile interactions

- `ShaderLabEditorWindows`
  - `ThemeEditorWindow`: theme selection/editor popup and persistence interactions
  - `BuildSettingsWindow`: build config/dependency checks/log presentation
  - `AboutWindow`: about content and branding panel

Within each feature slice, it is acceptable to have:

- local helper duplication
- feature-local mini view-models
- feature-local style mapping helpers

This is preferred over forcing premature generic abstractions.

## 2) Shared Core Primitives (Cross-Cutting Only)

Purpose: centralize only what is truly global and stable.

Shared by all features:

- `UIThemeColors` and theme application pipeline
- global UI constants/layout metrics
- transport/time primitives used across features
- platform boundary helpers (Win32 actions, non-client policy)

Must stay out of shared core unless proven reusable by multiple features.

## 3) Thin Frame Orchestrator

Purpose: top-level flow stays minimal.

- `UISystem::BeginFrame/ShowModeWindows/EndFrame` remains orchestration-only.
- Delegates to `ShaderLabIDEView` mode views and `ShaderLabEditorWindows` windows.
- Avoids deep feature logic.

## 4) Platform Boundary Layer

Purpose: keep Win32-specific behavior isolated from general feature UI logic.

- Keep titlebar hit/click and non-client behavior behind explicit UI boundary helpers.
- Keep minimize/close/window policy outside feature rendering details.

## Phased Migration Plan (Amended)

## Phase P0 — Baseline + Safety (0.5 day) ✅

- Capture baseline behavior checklist (titlebar controls, mode switches, compile flow, theme switching).
- Confirm build + smoke workflow used for every slice.

Exit criteria:

- Debug build clean.
- Manual smoke list agreed.

## Phase P1 — Styling Consolidation (1 day) 🟡

- Introduce style helper functions for common semantic intents:
  - success/warning/error/info text
  - preview backdrop
  - control-surface background
- Replace remaining hardcoded panel colors with theme token usage.

Exit criteria:

- No hardcoded control-surface colors in `src/ui/*` except intentionally fixed branding assets.

## Phase P2 — Shader Editor Feature Slice (1–2 days) ✅

- Split large shader editor draw flow into feature-local sections:
  - toolbar/status
  - search/replace
  - editor render
  - diagnostics interactions
- Keep helper logic local to Shader Editor unless clearly reused elsewhere.
- Keep existing behavior and shortcuts.

Vertical destination:

- `ShaderLabIDEView/PostFXModeView`

Exit criteria:

- `ShowShaderEditor()` reduced to orchestration-level readability.

## Phase P3 — Demo/Tracker Feature Slice (1–2 days) 🟡

- Extract beat-row rendering and tracker editing helpers.
- Isolate metadata panel controls and validation.
- Allow tracker-specific helper duplication if it avoids leaky abstractions.
- Keep transport/beat interactions behavior-identical.

Vertical destination:

- `ShaderLabIDEView/DemoModeView`

Exit criteria:

- Tracker row rendering and editing logic separated from top-level panel flow.

## Phase P4 — Titlebar/Menu Feature Slice + Platform Boundary (1 day) ✅

- Encapsulate custom title strip draw + interaction into a dedicated unit.
- Keep Win32 action dispatch (`minimize/close`) behind a narrow helper API.
- Keep menu construction separate from strip chrome rendering.

Vertical destination:

- UI composition in `ShaderLabIDECore`
- mode-specific actions delegated to `ShaderLabIDEView`

Exit criteria:

- Titlebar/menu code no longer mixed with unrelated panel orchestration.

## Phase P5 — Guardrails + Docs (0.5–1 day) 🟡

- Add lightweight static checks for UI style hygiene:
  - alert on new hardcoded control backgrounds in `src/ui/*`
- Update docs with module boundaries and coding rules.

Exit criteria:

- Refactor rules documented and enforceable.

## Coding Guidelines for the UI Refactor

1. No new one-off styling literals for controls; use theme tokens/helpers.
2. Organize by feature concern first; avoid broad utility extraction too early.
3. Duplication between features is acceptable when it improves local clarity.
4. Keep functions short and single-purpose where practical.
5. Keep UI-state mutations close to event handling.
6. Extract to shared core only after repeated multi-feature reuse is clear.

## Validation Matrix (per slice)

- Build: `Build ShaderLab (Debug)`
- Basic manual checks:
  - titlebar controls
  - mode switching
  - shader compile + diagnostics
  - tracker edit/scrub
  - theme switch + opacity sliders

## Immediate Next Slice (Recommended)

Given current progress, the highest-value next slice is **P5 Guardrails + Docs completion** plus **targeted P1 cleanup**.

Concrete next tasks:

1. Add a UI-style guard in `tools/check.ps1` for new hardcoded control-surface color literals under `src/ui/*`.
2. Finish a lightweight styling sweep to replace remaining one-off status/control colors with theme tokens.
3. Document the adopted hybrid structure (`Features` + `ShaderLabIDEView` + `ShaderLabIDECore`) as the canonical interim architecture.
4. Keep extracting only when it improves ownership clarity; avoid churn-only moves.

## Vertical Foldering Direction (Incremental, Updated)

Current accepted structure is hybrid. Prefer incremental moves only when they reduce entropy:

- `src/ui/Features/*` for feature-owned behavior and rendering
- `src/ui/ShaderLabIDEView/*` for mode-oriented composition views
- `src/ui/ShaderLabIDECore/*` for shared editor-core primitives
- `src/ui/Helpers/*` for small orchestration helpers

If/when consolidation is needed, use this structure as the destination:

- `src/ui/ShaderLabIDECore/*`
- `src/ui/ShaderLabIDEView/DemoModeView/*`
- `src/ui/ShaderLabIDEView/SceneModeView/*`
- `src/ui/ShaderLabIDEView/PostFXModeView/*`
- `src/ui/ShaderLabEditorWindows/ThemeEditorWindow/*`
- `src/ui/ShaderLabEditorWindows/BuildSettingsWindow/*`
- `src/ui/ShaderLabEditorWindows/AboutWindow/*`

Shared cross-cutting utilities stay in a narrow common area:

- `src/ui/ShaderLabIDECore/theme/*`
- `src/ui/ShaderLabIDECore/constants/*`
- `src/ui/ShaderLabIDECore/transport/*`
- `src/ui/ShaderLabIDECore/platform/*`

This foldering is a direction, not a big-bang migration requirement.
