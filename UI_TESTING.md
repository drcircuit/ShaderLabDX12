# UI Redesign Testing Checklist

This document outlines the testing steps to verify the UI redesign functionality.

## Prerequisites
- Windows 10/11
- Built ShaderLab editor executable
- `editor_assets/fonts/` directory with font files in the same directory as the executable

## Visual Verification

### 1. Font Loading
- [ ] Launch the editor
- [ ] Verify no font loading errors in the log
- [ ] Check that the UI text appears in Orbitron font (clean, futuristic geometric font)
- [ ] Logo text "SHADERLAB" in menu bar should use Hacked font (technical monospace style)

### 2. Color Scheme
- [ ] Window backgrounds should be very dark blue-black (not pure black)
- [ ] Active title bars and accents should be bright cyan/teal
- [ ] Borders should have subtle cyan tint
- [ ] Buttons should have cyan hover states
- [ ] Sliders and checkmarks should be bright cyan

### 3. Menu Bar Logo
- [ ] "SHADERLAB" text appears in the top-left of menu bar
- [ ] Logo text is bright cyan color
- [ ] Logo uses larger Hacked font
- [ ] A separator line appears after the logo before "File" menu

### 4. UI Elements

#### Text Display
- [ ] All menu items use Orbitron font
- [ ] Button labels use Orbitron font
- [ ] Window titles use Orbitron font
- [ ] General UI text is clear and readable

#### Numerical Fields
- [ ] BPM input fields display numbers
- [ ] Offset/duration fields display numbers
- [ ] All numerical inputs should ideally use Erbos Draco font (monospace technical)

### 5. Theme Consistency
- [ ] All windows follow the dark cyan theme
- [ ] Hover states show cyan tinting
- [ ] Active/selected items show bright cyan
- [ ] Inactive elements are appropriately dimmed

## Functional Testing

### 1. Editor Functionality
- [ ] Create a new project
- [ ] Load/save project files
- [ ] Edit shader code
- [ ] Compile shaders
- [ ] Play/pause timeline
- [ ] All functionality works as before (no regressions)

### 2. UI Interactions
- [ ] Click buttons
- [ ] Use sliders
- [ ] Toggle checkboxes
- [ ] Navigate menus
- [ ] Dock/undock windows
- [ ] Resize windows

### 3. Performance
- [ ] UI responsiveness is not degraded
- [ ] Font rendering is smooth
- [ ] No visible lag or stuttering

## Known Limitations

- Fonts are loaded from `editor_assets/fonts/` relative to the executable
- If fonts fail to load, the UI falls back to ImGui's default font
- Fonts are editor-only and NOT included in runtime builds

## Reporting Issues

If you encounter any issues:
1. Note the specific font or UI element affected
2. Check if fonts loaded successfully (check console/logs)
3. Verify `editor_assets/fonts/` directory exists and contains .ttf files
4. Take screenshots showing the issue
5. Report in the GitHub issue tracker
