# ShaderLab UI Redesign - Visual Summary

## Overview
The ShaderLab editor has been redesigned with a futuristic cyberpunk aesthetic featuring custom fonts and a bright cyan/teal color scheme on dark backgrounds.

## Color Palette

### Primary Colors
- **Background**: RGB(2, 3, 4) - Very dark blue-black
- **Primary Accent**: RGB(0, 255, 255) - Bright cyan
- **Secondary Accent**: RGB(0, 153, 165) - Medium teal
- **Text**: RGB(217, 242, 242) - Light cyan-white

### UI Element Colors
- **Window Background**: Very dark blue-black
- **Title Bar (Active)**: RGB(0, 128, 140) - Bright teal
- **Menu Bar**: Dark gray-blue
- **Borders**: Cyan-tinted semi-transparent
- **Buttons**: Dark with cyan hover/active states
- **Input Fields**: Dark blue-tinted backgrounds
- **Sliders/Checks**: Bright cyan

## Typography

### Font Family: Hacked
- **Usage**: Large headings
- **Sizes**:
  - 20px (Section headings)
- **Style**: Technical monospace display font
- **Color**: Bright cyan (#00FFFF)

### Font Family: Orbitron
- **Usage**: Default UI text
- **Sizes**: 15px
- **Weight**: Medium
- **Style**: Futuristic geometric sans-serif
- **Application**: Menu items, buttons, labels, general text

### Font Family: Erbos Draco NBP
- **Usage**: Numerical input fields and displays
- **Sizes**: 14px
- **Style**: Monospace technical font
- **Application**: BPM, time, beat counters, numerical parameters

## Key UI Elements

### Menu Bar
```
┌─────────────────────────────────────────────────────┐
│ [icon]  File  View  Device  Help   VSync  FPS ...   │
│                 <center demo name>                   │
└─────────────────────────────────────────────────────┘
```

### Button States
- **Normal**: Dark blue-gray background
- **Hovered**: Slightly lighter with cyan tint
- **Active**: Bright cyan/teal background
- **Text**: Light cyan-white

### Input Fields
- **Background**: Very dark blue-tinted
- **Border**: Subtle cyan
- **Text**: Orbitron font (general) or Erbos Draco (numbers)
- **Focus**: Brighter cyan border

### Windows
- **Background**: Very dark blue-black
- **Title Bar (Active)**: Bright teal with Orbitron text
- **Title Bar (Inactive)**: Very dark
- **Borders**: Cyan-tinted

## Design Principles

1. **High Contrast**: Light text on dark backgrounds for long coding sessions
2. **Sharp Geometry**: No rounded corners, angular industrial design
3. **Consistent Accents**: Cyan/teal used consistently for all interactive elements
4. **Hierarchy**: Font sizes and colors establish clear visual hierarchy
5. **Demoscene Aesthetic**: Technical, futuristic, cyberpunk-inspired

## Implementation Notes

- All colors defined in `UISystem::SetupImGuiStyle()`
- Fonts loaded in `UISystem::Initialize()`
- Orbitron set as `io.FontDefault` for all standard UI text
- Titlebar icon is loaded from `editor_assets/*.ico` and rendered in the custom titlebar
- Center titlebar text uses project filename stem, fallback `Untitled Demo`
- Graceful fallback to default font if loading fails

## Reference Image
The design is inspired by the futuristic UI mockup provided in the issue, featuring:
- Dark backgrounds with glowing cyan elements
- Technical fonts
- High-tech display aesthetic
- Demoscene/synthwave visual style

## Browser-Safe Color Values (for documentation)
- Cyan: `#00FFFF` or `rgb(0, 255, 255)`
- Teal: `#0099A5` or `rgb(0, 153, 165)`
- Dark Teal: `#008090` or `rgb(0, 128, 144)`
- Background: `#020304` or `rgb(2, 3, 4)`
- Text: `#D9F2F2` or `rgb(217, 242, 242)`
