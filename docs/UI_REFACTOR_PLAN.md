# ShaderLab UI Refactor Plan (Entropy Reduction)

Date: 2026-02-21  
Status: Draft v1 (planning stage)

Update (2026-02-21): feature-first organization is now the preferred refactor direction.

## Why This Refactor

The current UI layer is feature-rich but increasingly hard to evolve safely because responsibilities are spread across large files and mixed concerns (layout, state mutation, rendering, styling, and platform behavior).

Primary outcomes:

1. Reduce maintenance cost and onboarding time.
2. Make behavior-preserving changes faster and safer.
3. Keep visual/theming consistency under explicit, testable boundaries.

## Scope and Constraints

In scope:

- `src/ui/*`
- editor-side UI integration points (`src/app/editor/*` where UI boundary touches Win32)
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

1. **ShaderLabEditorCore**
2. **ShaderLabEditorView**
  - **DemoModeView**
  - **SceneModeView**
  - **PostEffectsView**
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

- `ShaderLabEditorCore`
  - shared editor orchestration state
  - theme/token application
  - global constants/layout metrics
  - transport primitives used by all views
  - platform boundary helpers (window actions, non-client policy)

- `ShaderLabEditorView`
  - `DemoModeView`: demo metadata, tracker, demo transport-facing interactions
  - `SceneModeView`: scene library, scene preview, scene-specific editing flows
  - `PostEffectsView`: post FX chain editing, post FX diagnostics, compile interactions

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
- Delegates to `ShaderLabEditorView` mode views and `ShaderLabEditorWindows` windows.
- Avoids deep feature logic.

## 4) Platform Boundary Layer

Purpose: keep Win32-specific behavior isolated from general feature UI logic.

- Keep titlebar hit/click and non-client behavior behind explicit UI boundary helpers.
- Keep minimize/close/window policy outside feature rendering details.

## Phased Migration Plan

## Phase P0 — Baseline + Safety (0.5 day)

- Capture baseline behavior checklist (titlebar controls, mode switches, compile flow, theme switching).
- Confirm build + smoke workflow used for every slice.

Exit criteria:

- Debug build clean.
- Manual smoke list agreed.

## Phase P1 — Styling Consolidation (1 day)

- Introduce style helper functions for common semantic intents:
  - success/warning/error/info text
  - preview backdrop
  - control-surface background
- Replace remaining hardcoded panel colors with theme token usage.

Exit criteria:

- No hardcoded control-surface colors in `src/ui/*` except intentionally fixed branding assets.

## Phase P2 — Shader Editor Feature Slice (1–2 days)

- Split large shader editor draw flow into feature-local sections:
  - toolbar/status
  - search/replace
  - editor render
  - diagnostics interactions
- Keep helper logic local to Shader Editor unless clearly reused elsewhere.
- Keep existing behavior and shortcuts.

Vertical destination:

- `ShaderLabEditorView/PostEffectsView`

Exit criteria:

- `ShowShaderEditor()` reduced to orchestration-level readability.

## Phase P3 — Demo/Tracker Feature Slice (1–2 days)

- Extract beat-row rendering and tracker editing helpers.
- Isolate metadata panel controls and validation.
- Allow tracker-specific helper duplication if it avoids leaky abstractions.
- Keep transport/beat interactions behavior-identical.

Vertical destination:

- `ShaderLabEditorView/DemoModeView`

Exit criteria:

- Tracker row rendering and editing logic separated from top-level panel flow.

## Phase P4 — Titlebar/Menu Feature Slice + Platform Boundary (1 day)

- Encapsulate custom title strip draw + interaction into a dedicated unit.
- Keep Win32 action dispatch (`minimize/close`) behind a narrow helper API.
- Keep menu construction separate from strip chrome rendering.

Vertical destination:

- UI composition in `ShaderLabEditorCore`
- mode-specific actions delegated to `ShaderLabEditorView`

Exit criteria:

- Titlebar/menu code no longer mixed with unrelated panel orchestration.

## Phase P5 — Guardrails + Docs (0.5–1 day)

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

Start with **P1 Styling Consolidation** because it is low-risk and gives immediate entropy reduction across all panels.

Concrete first tasks:

1. Add semantic color helpers in `UISystem` (success/warn/error/info text).
2. Replace remaining hardcoded status text colors in build-settings UI with those helpers.
3. Add a simple grep guard in `tools/check.ps1` for new hardcoded `ImGuiCol_FrameBg/ChildBg/PopupBg` literals under `src/ui/*`.

## Vertical Foldering Direction (Incremental)

When moving code physically, prefer an incremental structure such as:

- `src/ui/ShaderLabEditorCore/*`
- `src/ui/ShaderLabEditorView/DemoModeView/*`
- `src/ui/ShaderLabEditorView/SceneModeView/*`
- `src/ui/ShaderLabEditorView/PostEffectsView/*`
- `src/ui/ShaderLabEditorWindows/ThemeEditorWindow/*`
- `src/ui/ShaderLabEditorWindows/BuildSettingsWindow/*`
- `src/ui/ShaderLabEditorWindows/AboutWindow/*`

Shared cross-cutting utilities stay in a narrow common area:

- `src/ui/ShaderLabEditorCore/theme/*`
- `src/ui/ShaderLabEditorCore/constants/*`
- `src/ui/ShaderLabEditorCore/transport/*`
- `src/ui/ShaderLabEditorCore/platform/*`

This foldering is a direction, not a big-bang migration requirement.
