// ---------------------------------------------------------------------------
// RuntimeWindowPolicy_sdl2.cpp
//
// SDL2 implementation of RuntimeWindowPolicy.
// Replaces the Win32-specific RuntimeWindowPolicy.cpp on Linux and macOS.
// ---------------------------------------------------------------------------

#include "ShaderLab/Runtime/RuntimeWindowPolicy.h"

#include "ShaderLab/Graphics/Vulkan/VulkanCommandQueue.h"
#include "ShaderLab/Graphics/Vulkan/VulkanSwapchain.h"

// We forward-declare the Vulkan types used here rather than including the
// Vulkan headers, because PresentInitialBlackFrame only needs the queue/swapchain.
// The SDL2 player app handles the full Vulkan render loop.

#include <SDL2/SDL.h>
#include <cstdio>

namespace ShaderLab::RuntimeWindowPolicy {

// ---------------------------------------------------------------------------
void UpdateWindowTitleWithStats(
    NativeWindowHandle hwnd,
    const std::string& baseWindowTitle,
    float              lastMeasuredFps,
    bool               vsyncEnabled,
    bool               windowed,
    bool               screenSaverMode) {

    SDL_Window* window = reinterpret_cast<SDL_Window*>(hwnd);
    if (!window || screenSaverMode) return;

    char title[512] = {};
    std::snprintf(title, sizeof(title),
        "%s | FPS: %.1f | VSync: %s | Fullscreen: %s",
        baseWindowTitle.c_str(),
        lastMeasuredFps,
        vsyncEnabled ? "On" : "Off",
        windowed ? "Windowed" : "Borderless");
    SDL_SetWindowTitle(window, title);
}

// ---------------------------------------------------------------------------
bool PresentInitialBlackFrame(CommandQueue* /*commandQueue*/, Swapchain* /*swapchain*/) {
    // On SDL2/Vulkan the initial black frame is presented by the player app
    // directly after swapchain creation.  Nothing needs to happen here.
    return true;
}

// ---------------------------------------------------------------------------
void SetFullscreen(
    NativeWindowHandle hwnd,
    bool&              windowed,
    WindowRect&        windowedRect,
    const std::string& baseWindowTitle,
    float              lastMeasuredFps,
    bool               vsyncEnabled,
    bool               screenSaverMode) {

    SDL_Window* window = reinterpret_cast<SDL_Window*>(hwnd);
    if (!window) return;

    // Save the current windowed bounds before going fullscreen.
    if (windowed) {
        int x, y, w, h;
        SDL_GetWindowPosition(window, &x, &y);
        SDL_GetWindowSize(window, &w, &h);
        windowedRect = { x, y, x + w, y + h };
    }

    windowed = false;
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    UpdateWindowTitleWithStats(hwnd, baseWindowTitle, lastMeasuredFps, vsyncEnabled, windowed, screenSaverMode);
}

// ---------------------------------------------------------------------------
void SetWindowed(
    NativeWindowHandle hwnd,
    bool&              windowed,
    WindowRect&        windowedRect,
    const std::string& baseWindowTitle,
    float              lastMeasuredFps,
    bool               vsyncEnabled,
    bool               screenSaverMode) {

    SDL_Window* window = reinterpret_cast<SDL_Window*>(hwnd);
    if (!window) return;

    windowed = true;
    SDL_SetWindowFullscreen(window, 0);

    int w = windowedRect.Width();
    int h = windowedRect.Height();
    if (w <= 0 || h <= 0) { w = 1280; h = 720; }

    SDL_SetWindowSize(window, w, h);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    UpdateWindowTitleWithStats(hwnd, baseWindowTitle, lastMeasuredFps, vsyncEnabled, windowed, screenSaverMode);
}

} // namespace ShaderLab::RuntimeWindowPolicy
