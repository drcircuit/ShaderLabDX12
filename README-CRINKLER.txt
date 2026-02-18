ShaderLab - Release Crinkled Setup
==================================

Release Crinkled is the in-app "smallest binary" build mode for final demos.
It is intended for tiny intro-style outputs and uses Crinkler as linker.

Required tools
--------------
1) Crinkler (crinkler.exe)
2) Ninja (ninja.exe on PATH)
3) Visual Studio 2022 C++ build tools
4) Windows SDK 10/11

How to install Crinkler
-----------------------
1) Download Crinkler from:
   https://crinkler.net/
2) Extract it to a stable folder, for example:
   C:\tools\crinkler23\Win64\

Configure ShaderLab to find Crinkler
------------------------------------
Use ONE of these options:

A) Environment variable SHADERLAB_CRINKLER
   - Set to crinkler.exe OR the folder containing crinkler.exe

B) Environment variable CRINKLER_PATH
   - Set to crinkler.exe OR the folder containing crinkler.exe

C) Put crinkler.exe on PATH

Examples:
- SHADERLAB_CRINKLER=C:\tools\crinkler23\Win64\crinkler.exe
- SHADERLAB_CRINKLER=C:\tools\crinkler23\Win64\

Tip: restart ShaderLab after changing environment variables.

Ninja setup
-----------
Install Ninja and ensure `ninja.exe` is available on PATH.

Common options:
- winget install Ninja-build.Ninja
- choco install ninja
- scoop install ninja

Expected output sizes
---------------------
Targets like "<1k" or "<4k" are content-dependent and not guaranteed.
Shader complexity, transition usage, runtime features, and packed assets all matter.
Release Crinkled is optimized for small size, but actual size depends on the demo.

In-app behavior
---------------
- "Release": standard optimized self-contained build
- "Release Crinkled": requires Crinkler + Ninja
  If missing, build fails and points to this file.
- "Build Settings...": in-app configuration window for build mode, explicit size
   targets, dependency detection, and setup shortcuts.

Size target presets
-------------------
ShaderLab supports optional budget presets and reports whether the final packed
EXE hits or misses the chosen target:

- 1K
- 2K
- 4K
- 16K
- 32K
- 64K

The report is based on the final self-contained executable size after all packed
assets and manifests are baked into the binary.

Restricted compact track mode
-----------------------------
An optional restricted optimization writes track data to `assets/track.bin`
and strips verbose track/editor-facing fields from `project.json` before
packing.

Current compact timeline format is v2 only (non-backward-compatible):
- Header: 10 bytes (`TKR2` magic + BPM Q8 + length beats + row count)
- Row: 9 bytes (row beat, scene index, transition, stop flag,
  transition duration Q4, time offset Q4, music index)

Notes:
- `oneShotIndex` and `isBeat` are not stored in compact v2.
- Compact timeline decoding now expects v2 only.
- Projects packed with old compact timeline binaries must be rebuilt.
