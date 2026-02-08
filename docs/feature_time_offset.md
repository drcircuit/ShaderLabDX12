# Time Offset Feature Implementation

Implemented the "Time Offset" feature to allow scenes to have discontinuous time relative to the global transport. This fixes the issue where scenes would reset to t=0 or have incorrect phase alignment during transitions.

## Changes:
1.  **Data Structure**: Added `float timeOffset` to `TrackerRow` (in beats).
2.  **Serialization**: Added JSON support for "offset" field.
3.  **UI**: Added `InputFloat` column in Tracker to edit the offset in beats.
4.  **Runtime**:
    -   `DemoPlayer` now tracks `m_activeSceneOffset`.
    -   During scene activation, the offset is loaded from the tracker row.
    -   In `RenderScene`, the time passed to the shader is `globalTime + (offsetBeats * 60 / BPM)`.
    -   Transition rendering logic now correctly calculates offsets for both Source and Destination scenes to ensure smooth visual continuity if desired, or hard resets.

## Logic Details:
-   **Main Loop**: Uses `m_activeSceneOffset` set during `Update()`.
-   **Transitions**:
    -   Source scene uses the active offset (since it was the active scene).
    -   Destination scene offset is looked up dynamically during the transition phase by scanning the tracker rows for the target scene index and row ID.

## Verification:
-   Build passes.
-   Logic covers both normal playback and cross-fade transitions.
