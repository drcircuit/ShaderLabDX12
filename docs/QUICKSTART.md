# ShaderLab Quick Start Guide

Get up and running with ShaderLab in minutes.

## Prerequisites

Before you begin:
- Windows 10/11 (64-bit)
- Visual Studio 2022 with C++ tools
- CMake 3.20+
- Basic knowledge of HLSL shaders

## Step 1: Clone the Repository

```cmd
git clone https://github.com/drcircuit/ShaderLabDX12.git
cd ShaderLab
```

## Step 2: Setup Third-Party Dependencies

Download and extract these libraries into `third_party/`:

1. **Dear ImGui**: https://github.com/ocornut/imgui (entire imgui folder)
2. **miniaudio**: https://github.com/mackron/miniaudio (just miniaudio.h)
3. **nlohmann/json**: https://github.com/nlohmann/json (include/nlohmann folder)
4. **stb_image**: https://github.com/nothings/stb (just stb_image.h)

Your structure should look like:
```
third_party/
├── imgui/
│   ├── imgui.h
│   └── ...
├── miniaudio/
│   └── miniaudio.h
├── json/
│   └── include/nlohmann/json.hpp
└── stb/
    └── stb_image.h
```

See [BUILD.md](BUILD.md) and [third_party/README.md](../third_party/README.md) for detailed dependency setup.

## Step 3: Build

Open "x64 Native Tools Command Prompt for VS 2022" or use PowerShell and load the toolchain:

```powershell
# If you are not in the VS Developer Prompt
.\tools\dev_env.ps1

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Step 4: Run

```cmd
build\bin\ShaderLabEditor.exe
```

Run the editor from the repository root so it can find creative assets.

The editor window will open with three main panels:
- **Demo View**: Timeline and playback controls
- **Scene View**: Live shader preview
- **Effect View**: Shader code editor

## Your First Shader

1. In the **Effect View**, you'll see example shader code
2. Modify the shader (try changing the color values)
3. Click **Compile** to see changes instantly
4. The **Scene View** updates in real-time

## Adding Music

1. In **Demo View**, click "Load Audio" (when implemented)
2. Select a music file (WAV, MP3, OGG)
3. Set the BPM to match your track
4. Click **Play**

Your shader can now react to the beat using these variables:

```hlsl
cbuffer TimeConstants : register(b0)
{
    float time;          // Audio time in seconds
    float beatPhase;     // 0.0-1.0 within quarter note
    float barProgress;   // 0.0-1.0 within bar
    float bpm;           // Beats per minute
};
```

## Example: Beat-Synced Pulse

```hlsl
float4 main(float4 position : SV_Position) : SV_Target
{
    float2 uv = position.xy / float2(1920, 1080);
    
    // Pulse on every beat
    float pulse = 1.0 - beatPhase;
    float3 color = float3(0.5, 0.2, 0.8) * (1.0 + pulse);
    
    return float4(color, 1.0);
}
```

## Example Shaders

Check out `creative/shaders/` for more examples:
- **example_gradient.hlsl**: Simple gradient with beat pulse
- **example_tunnel.hlsl**: Raymarched tunnel effect

## Key Concepts

### Beat Synchronization
- `beatPhase` loops every quarter note (0.0 → 1.0)
- `barProgress` loops every bar based on time signature
- Use these for smooth, music-synced animations

### Screen Coordinates
```hlsl
// Normalize to 0..1
float2 uv = position.xy / resolution;

// Normalize to -1..1 (centered)
float2 uv = (position.xy / resolution) * 2.0 - 1.0;

// Aspect ratio correction
uv.x *= width / height;
```

### Live Compilation
- Shaders compile in ~100ms with Live mode
- Errors show in the Effect View with line numbers
- No need to restart the editor

## Next Steps

- Read [ARCHITECTURE.md](ARCHITECTURE.md) to understand the codebase
- Explore shader examples in `creative/shaders/`
- Join the community and share your creations
- Check [CONTRIBUTING.md](CONTRIBUTING.md) to contribute

## Troubleshooting

**Editor won't start:**
- Check if third-party dependencies are properly installed
- Verify your GPU supports Direct3D 12

**Shader won't compile:**
- Check for HLSL syntax errors
- Ensure you're using HLSL 6.x features
- Look at diagnostic output in Effect View

**Audio doesn't play:**
- miniaudio not found: check third_party/miniaudio/
- Unsupported format: try WAV files first

## Tips

- Use **Ctrl+S** to save your project (when implemented)
- Press **Space** to pause/resume playback
- **F5** to compile shader (hotkey to be implemented)
- Keep backup copies of your best shaders!

## Community

- **Issues**: Report bugs and request features
- **Discussions**: Ask questions and share ideas
- **Pull Requests**: Contribute code and shaders

## License

- Engine code: Community License (non-commercial)
- Shaders you create: Your choice (we suggest CC BY-NC-SA 4.0)
- Example shaders: CC BY-NC-SA 4.0

---

**Welcome to ShaderLab!** Let's make some awesome demos.

For detailed build instructions, see [BUILD.md](BUILD.md)
