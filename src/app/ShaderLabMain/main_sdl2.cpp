// ---------------------------------------------------------------------------
// main_sdl2.cpp
//
// Linux / macOS editor entry point.
//
// This file replaces main.cpp (WinMain) on non-Windows platforms.
// It creates an SDL2 window and hands it off to ShaderLabApp (which will
// need a matching SDL2/Vulkan initialisation path — see ShaderLabApp.cpp
// and the SHADERLAB_WINDOW_SDL2 / SHADERLAB_GFX_VULKAN guards).
//
// Build requirements:
//   apt install libsdl2-dev libvulkan-dev          (Ubuntu/Debian)
//   brew install sdl2 molten-vk vulkan-headers     (macOS)
// ---------------------------------------------------------------------------

#include "ShaderLab/App/ShaderLabApp.h"
#include "ShaderLab/Platform/Platform.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>

#include <chrono>
#include <cstdio>

// Forward declare: the global app pointer used by event routing.
static ShaderLab::ShaderLabApp* g_app = nullptr;

int main(int /*argc*/, char** /*argv*/) {
    std::printf("ShaderLab Editor starting (SDL2/Vulkan)...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // Obtain the primary display resolution.
    SDL_DisplayMode dm{};
    int screenW = 1920, screenH = 1080;
    if (SDL_GetCurrentDisplayMode(0, &dm) == 0) { screenW = dm.w; screenH = dm.h; }

    SDL_Window* window = SDL_CreateWindow(
        "DrCiRCUiT's ShaderLab - For democoders, by a democoder - untitled",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        screenW, screenH,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    std::printf("Window created (%dx%d)\n", screenW, screenH);

    ShaderLab::NativeWindowHandle nativeWindow =
        reinterpret_cast<ShaderLab::NativeWindowHandle>(window);

    ShaderLab::ShaderLabApp app;
    g_app = &app;

    std::printf("Initialising application...\n");
    if (!app.Initialize(nativeWindow, static_cast<uint32_t>(screenW), static_cast<uint32_t>(screenH))) {
        std::fprintf(stderr, "Failed to initialise ShaderLabApp\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    std::printf("Application initialised successfully\n");

    // Main loop
    using Clock = std::chrono::steady_clock;
    auto lastTime = Clock::now();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                app.OnResize(static_cast<uint32_t>(event.window.data1),
                             static_cast<uint32_t>(event.window.data2));
            }
        }
        if (!running) break;

        auto  now = Clock::now();
        float dt  = std::chrono::duration<float>(now - lastTime).count();
        lastTime  = now;

        app.Update(dt);
        app.Render();
    }

    app.Shutdown();
    g_app = nullptr;

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
