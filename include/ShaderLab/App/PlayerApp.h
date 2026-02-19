#pragma once

#include <windows.h>

namespace ShaderLab {

struct PlayerLaunchOptions {
    bool debugConsole = false;
    bool loopPlayback = true;
    bool screenSaverMode = false;
    bool vsyncEnabled = true;
};

int RunPlayerApp(HINSTANCE hInstance, const PlayerLaunchOptions& options);

} // namespace ShaderLab
