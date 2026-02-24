#pragma once

#include "ShaderLab/Platform/Platform.h"

namespace ShaderLab {

struct PlayerLaunchOptions {
    bool debugConsole = false;
    bool loopPlayback = true;
    bool screenSaverMode = false;
    bool vsyncEnabled = true;
    bool startFullscreen = true;
};

// appHandle maps to HINSTANCE on Windows, or nullptr on POSIX platforms.
int RunPlayerApp(NativeAppHandle appHandle, const PlayerLaunchOptions& options);

} // namespace ShaderLab
