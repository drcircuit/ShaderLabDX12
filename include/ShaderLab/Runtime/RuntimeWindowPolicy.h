#pragma once

#include <string>
#include "ShaderLab/Platform/Platform.h"

namespace ShaderLab {
class CommandQueue;
class Swapchain;
}

namespace ShaderLab::RuntimeWindowPolicy {

// hwnd is a NativeWindowHandle (HWND on Win32, SDL_Window* on SDL2 builds).
void UpdateWindowTitleWithStats(
    NativeWindowHandle hwnd,
    const std::string& baseWindowTitle,
    float lastMeasuredFps,
    bool vsyncEnabled,
    bool windowed,
    bool screenSaverMode);

bool PresentInitialBlackFrame(CommandQueue* commandQueue, Swapchain* swapchain);

void SetFullscreen(
    NativeWindowHandle hwnd,
    bool& windowed,
    WindowRect& windowedRect,
    const std::string& baseWindowTitle,
    float lastMeasuredFps,
    bool vsyncEnabled,
    bool screenSaverMode);

void SetWindowed(
    NativeWindowHandle hwnd,
    bool& windowed,
    WindowRect& windowedRect,
    const std::string& baseWindowTitle,
    float lastMeasuredFps,
    bool vsyncEnabled,
    bool screenSaverMode);

} // namespace ShaderLab::RuntimeWindowPolicy
