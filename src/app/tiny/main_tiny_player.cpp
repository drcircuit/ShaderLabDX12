#include <windows.h>

#include <vector>
#include <string>

#include "ShaderLab/App/PlayerApp.h"

namespace {

std::vector<std::wstring> GetCommandLineArgs() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    if (!argv) {
        return args;
    }

    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    LocalFree(argv);
    return args;
}

bool HasAnyFlag(const std::vector<std::wstring>& args, const std::vector<std::wstring>& flags) {
    for (size_t i = 1; i < args.size(); ++i) {
        for (const auto& flag : flags) {
            if (_wcsicmp(args[i].c_str(), flag.c_str()) == 0) {
                return true;
            }
        }
    }
    return false;
}

bool IsArg(const std::wstring& arg, const wchar_t* value) {
    return _wcsicmp(arg.c_str(), value) == 0;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    const auto args = GetCommandLineArgs();

    bool loopPlayback = !HasAnyFlag(args, {L"--no-loop", L"-noloop"});
    bool vsyncEnabled = true;
    bool startFullscreen = true;

    if (HasAnyFlag(args, {L"--loop", L"-loop"})) {
        loopPlayback = true;
    }

    for (size_t i = 1; i < args.size(); ++i) {
        if (IsArg(args[i], L"--no-vsync") || IsArg(args[i], L"--vsync-off") || IsArg(args[i], L"--unlimited-fps") || IsArg(args[i], L"--fps-unlimited")) {
            vsyncEnabled = false;
        } else if (IsArg(args[i], L"--vsync") || IsArg(args[i], L"--vsync-on")) {
            vsyncEnabled = true;
        } else if (IsArg(args[i], L"--windowed") || IsArg(args[i], L"-windowed")) {
            startFullscreen = false;
        } else if (IsArg(args[i], L"--fullscreen") || IsArg(args[i], L"-fullscreen")) {
            startFullscreen = true;
        }
    }

    ShaderLab::PlayerLaunchOptions options{};
    options.debugConsole = false;
    options.loopPlayback = loopPlayback;
    options.screenSaverMode = false;
    options.vsyncEnabled = vsyncEnabled;
    options.startFullscreen = startFullscreen;

    return ShaderLab::RunPlayerApp(hInstance, options);
}
