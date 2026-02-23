# ShaderLab v2.0.0 Ship Checklist

Release target date: 2026-02-23

Use this as the final pre-release and release execution checklist for `v2.0.0`.

## 1) Metadata and Notes

- [ ] Confirm `metadata/app_metadata.json` has final `version`, `productName`, `companyName`, `publisher`, and `description`.
- [ ] Confirm release notes are final in `RELEASE_NOTES_v2.0.0.md`.
- [ ] Confirm no pending scope/version mismatch remains in notes or docs.

## 2) Clean State

- [ ] Ensure working tree is clean (or only intentional release edits).
- [ ] Ensure no temporary/debug toggles are left enabled.
- [ ] Ensure runtime debug flags remain OFF by default:
  - [ ] `SHADERLAB_RUNTIME_DEBUG_LOG`
  - [ ] `SHADERLAB_COMPACT_TRACK_DEBUG`

## 3) Build and Validation

Recommended VS Code task sequence:

- [ ] Run `Build ShaderLab (Release, No Installer)`
- [ ] Run `Layering Check`
- [ ] Run `Startup Relaunch Validation`
- [ ] Run `Crinkled Matrix Summary` (or `Crinkled Matrix Validation` when needed)

Optional deeper packaging check:

- [ ] Run `M6 Prebuilt Packaging Spike`

## 4) Packaging

- [ ] Build installer via `Build ShaderLab (Release + Installer)` (or `tools/build_installer.ps1`).
- [ ] Verify output appears under `artifacts/`.
- [ ] Verify expected artifact type:
  - [ ] Inno Setup installer (`ShaderLabSetup-x64-...exe`) when Inno Setup is available
  - [ ] Portable zip fallback when Inno Setup is unavailable
- [ ] Verify end-user docs are staged/included from `docs/enduser/`.

## 5) Windows Metadata Verification

For built binaries (at minimum `ShaderLabIIDE.exe`, `ShaderLabPlayer.exe`, `ShaderLabScreenSaver.scr`):

- [ ] Right-click → Properties → Details shows expected version/product/company metadata.
- [ ] VERSIONINFO fields match `metadata/app_metadata.json`.

## 6) Runtime Sanity

- [ ] Launch editor and verify startup path.
- [ ] Open at least one representative project and verify preview renders.
- [ ] Validate transport controls and timeline playback behavior.
- [ ] Validate PostFX/Compute chain preview behavior on at least one scene.

## 7) Publish

- [ ] Create/push release tag for `v2.0.0`.
- [ ] Draft GitHub release using `RELEASE_NOTES_v2.0.0.md`.
- [ ] Attach release artifacts from `artifacts/`.
- [ ] Publish release.

## 8) Post-Ship

- [ ] Smoke-download installer/zip from release page.
- [ ] Verify install/run on a clean machine profile (or VM) when available.
- [ ] Record any follow-up issues as `v2.0.1` candidates.
