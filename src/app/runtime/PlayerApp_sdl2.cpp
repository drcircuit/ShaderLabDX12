// ---------------------------------------------------------------------------
// PlayerApp_sdl2.cpp
//
// SDL2 + Vulkan implementation of RunPlayerApp().
// This file is compiled instead of PlayerApp.cpp on Linux and macOS.
//
// The overall structure mirrors PlayerApp.cpp (Win32 + D3D12):
//   1. Create an SDL2 window.
//   2. Initialise VulkanDevice, VulkanCommandQueue, VulkanSwapchain.
//   3. Initialise DemoPlayer.
//   4. Run the SDL2 event / render loop.
//   5. Shut everything down cleanly.
// ---------------------------------------------------------------------------

#include "ShaderLab/App/PlayerApp.h"
#include "ShaderLab/Runtime/RuntimeStartupPolicy.h"
#include "ShaderLab/Runtime/RuntimeWindowPolicy.h"

#include "ShaderLab/App/DemoPlayer.h"
#include "ShaderLab/Graphics/Vulkan/VulkanDevice.h"
#include "ShaderLab/Graphics/Vulkan/VulkanCommandQueue.h"
#include "ShaderLab/Graphics/Vulkan/VulkanSwapchain.h"
#include "ShaderLab/Graphics/RenderContext.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <memory>
#include <string>
#include <chrono>
#include <limits.h>   // PATH_MAX (POSIX)

#ifndef SHADERLAB_TINY_PLAYER
#define SHADERLAB_TINY_PLAYER 0
#endif

#ifndef SHADERLAB_RUNTIME_IMGUI
#define SHADERLAB_RUNTIME_IMGUI 1
#endif

#if !SHADERLAB_TINY_PLAYER
#include <filesystem>
#include <iostream>
#endif

#if SHADERLAB_RUNTIME_IMGUI
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#endif

namespace ShaderLab {

namespace {

struct VulkanRuntimeResources {
    VulkanDevice*       device       = nullptr;
    VulkanCommandQueue* commandQueue = nullptr;
    VulkanSwapchain*    swapchain    = nullptr;
    DemoPlayer*         player       = nullptr;
};

VulkanRuntimeResources g_Resources;

struct RuntimeAppState {
    bool         debugConsole   = false;
    bool         windowed       = false;
    bool         screenSaverMode= false;
    bool         vsyncEnabled   = true;
    bool         startFullscreen= true;
    float        lastMeasuredFps= 0.0f;
    double       fpsAccumSeconds= 0.0;
    int          fpsAccumFrames = 0;
    std::string  baseWindowTitle= "DrCiRCUiT's ShaderLab - For democoders, by a democoder - demo";
    WindowRect   windowedRect;
    bool         runtimeCursorHidden = false;
};

RuntimeAppState g_Runtime;

void ShutdownVulkanResources() {
    if (g_Resources.commandQueue) g_Resources.commandQueue->WaitForGPU();
    if (g_Resources.player) {
        g_Resources.player->Shutdown();
        delete g_Resources.player;
        g_Resources.player = nullptr;
    }
    delete g_Resources.swapchain;     g_Resources.swapchain     = nullptr;
    delete g_Resources.commandQueue;  g_Resources.commandQueue  = nullptr;
    delete g_Resources.device;        g_Resources.device        = nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
int RunPlayerApp(NativeAppHandle /*appHandle*/, const PlayerLaunchOptions& options) {
    g_Runtime = RuntimeAppState{};
    g_Runtime.debugConsole    = options.debugConsole;
    g_Runtime.screenSaverMode = options.screenSaverMode;
    g_Runtime.vsyncEnabled    = options.vsyncEnabled;
    g_Runtime.startFullscreen = options.startFullscreen;

    // SDL2 initialisation
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
#if !SHADERLAB_TINY_PLAYER
        RuntimeStartupPolicy::EmitRuntimeError("E300", "SDL_Init failed");
#endif
        return -1;
    }

    int screenW = 1920, screenH = 1080;
    {
        SDL_DisplayMode dm{};
        if (SDL_GetCurrentDisplayMode(0, &dm) == 0) { screenW = dm.w; screenH = dm.h; }
    }

    const bool launchFullscreen = (!g_Runtime.screenSaverMode && g_Runtime.startFullscreen);
    const int  winW = launchFullscreen ? screenW : 1280;
    const int  winH = launchFullscreen ? screenH : 720;

    Uint32 windowFlags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN;
    if (launchFullscreen || g_Runtime.screenSaverMode) windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    SDL_Window* sdlWindow = SDL_CreateWindow(
        "DrCiRCUiT's ShaderLab - For democoders, by a democoder - demo",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        windowFlags);

    if (!sdlWindow) {
#if !SHADERLAB_TINY_PLAYER
        RuntimeStartupPolicy::EmitRuntimeError("E301", "SDL_CreateWindow failed");
#endif
        SDL_Quit();
        return -1;
    }

    if (g_Runtime.screenSaverMode) SDL_ShowCursor(SDL_DISABLE);
    RuntimeStartupPolicy::HideRuntimeCursor(g_Runtime.runtimeCursorHidden);

    NativeWindowHandle nativeWindow = reinterpret_cast<NativeWindowHandle>(sdlWindow);

    // Vulkan device
    g_Resources.device = new VulkanDevice();
    if (!g_Resources.device->Initialize(nativeWindow, false)) {
#if !SHADERLAB_TINY_PLAYER
        RuntimeStartupPolicy::EmitRuntimeError("E100", "VulkanDevice init failed");
#endif
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
        return -1;
    }

    g_Resources.commandQueue = new VulkanCommandQueue();
    if (!g_Resources.commandQueue->Initialize(g_Resources.device)) {
#if !SHADERLAB_TINY_PLAYER
        RuntimeStartupPolicy::EmitRuntimeError("E103", "VulkanCommandQueue init failed");
#endif
        ShutdownVulkanResources();
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
        return -1;
    }

    g_Resources.swapchain = new VulkanSwapchain();
    if (!g_Resources.swapchain->Initialize(g_Resources.device, g_Resources.commandQueue,
                                            nativeWindow,
                                            static_cast<uint32_t>(winW),
                                            static_cast<uint32_t>(winH))) {
#if !SHADERLAB_TINY_PLAYER
        RuntimeStartupPolicy::EmitRuntimeError("E104", "VulkanSwapchain init failed");
#endif
        ShutdownVulkanResources();
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
        return -1;
    }

#if SHADERLAB_RUNTIME_IMGUI
    // ImGui SDL2 + Vulkan backends
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForVulkan(sdlWindow);
    ImGui_ImplVulkan_InitInfo vkInfo{};
    vkInfo.Instance       = g_Resources.device->GetInstance();
    vkInfo.PhysicalDevice = g_Resources.device->GetPhysicalDevice();
    vkInfo.Device         = g_Resources.device->GetDevice();
    vkInfo.QueueFamily    = g_Resources.device->GetGraphicsFamily();
    vkInfo.Queue          = g_Resources.device->GetGraphicsQueue();
    vkInfo.RenderPass     = g_Resources.swapchain->GetRenderPass();
    vkInfo.MinImageCount  = VulkanSwapchain::BUFFER_COUNT;
    vkInfo.ImageCount     = VulkanSwapchain::BUFFER_COUNT;
    ImGui_ImplVulkan_Init(&vkInfo);
#endif

    // Resolve project path
#if SHADERLAB_TINY_PLAYER
    const std::string projectPath = "assets/track.bin";
    const std::string projectName = "demo";
#else
    std::string projectPath = "project.json";
    std::string projectName = "demo";
    {
        namespace fs = std::filesystem;
        char exeBuf[PATH_MAX] = {};
#ifdef __linux__
        ssize_t len = readlink("/proc/self/exe", exeBuf, sizeof(exeBuf) - 1);
        if (len > 0) exeBuf[len] = '\0';
#elif defined(__APPLE__)
        uint32_t sz = sizeof(exeBuf); _NSGetExecutablePath(exeBuf, &sz);
#endif
        fs::path exePath(exeBuf);
        fs::path jsonPath = exePath.replace_extension(".json");
        if (!fs::exists(jsonPath)) jsonPath = exePath.parent_path() / "project.json";
        if (fs::exists(jsonPath)) {
            projectPath = jsonPath.string();
            projectName = jsonPath.stem().string();
        }
    }
#endif

    g_Resources.player = new DemoPlayer();
    if (!g_Resources.player->Initialize(nativeWindow, nullptr, nullptr,
                                         static_cast<int>(g_Resources.swapchain->GetWidth()),
                                         static_cast<int>(g_Resources.swapchain->GetHeight()))) {
#if !SHADERLAB_TINY_PLAYER
        RuntimeStartupPolicy::EmitRuntimeError("E106", "DemoPlayer init failed");
#endif
        ShutdownVulkanResources();
        SDL_DestroyWindow(sdlWindow);
        SDL_Quit();
        return -1;
    }
    g_Resources.player->SetLooping(options.loopPlayback);
    g_Resources.player->SetVsyncEnabled(g_Runtime.vsyncEnabled);

    if (!g_Runtime.screenSaverMode) {
        g_Runtime.baseWindowTitle = "DrCiRCUiT's ShaderLab - For democoders, by a democoder - " + projectName;
        RuntimeWindowPolicy::UpdateWindowTitleWithStats(nativeWindow, g_Runtime.baseWindowTitle,
            0.0f, g_Runtime.vsyncEnabled, g_Runtime.windowed, false);
    }

#if !SHADERLAB_TINY_PLAYER
    std::cout << "Loading project: " << projectPath << std::endl;
#endif
    g_Resources.player->LoadProject(projectPath);

    // --------------- Main loop ---------------
    using Clock = std::chrono::steady_clock;
    auto lastTime = Clock::now();

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
#if SHADERLAB_RUNTIME_IMGUI
            ImGui_ImplSDL2_ProcessEvent(&event);
#endif
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    } else if (event.key.keysym.sym == SDLK_RETURN &&
                               (event.key.keysym.mod & KMOD_ALT)) {
                        if (g_Runtime.windowed)
                            RuntimeWindowPolicy::SetFullscreen(nativeWindow, g_Runtime.windowed, g_Runtime.windowedRect, g_Runtime.baseWindowTitle, g_Runtime.lastMeasuredFps, g_Runtime.vsyncEnabled, g_Runtime.screenSaverMode);
                        else
                            RuntimeWindowPolicy::SetWindowed(nativeWindow, g_Runtime.windowed, g_Runtime.windowedRect, g_Runtime.baseWindowTitle, g_Runtime.lastMeasuredFps, g_Runtime.vsyncEnabled, g_Runtime.screenSaverMode);
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        g_Resources.swapchain->Resize(
                            static_cast<uint32_t>(event.window.data1),
                            static_cast<uint32_t>(event.window.data2));
                        if (g_Resources.player)
                            g_Resources.player->OnResize(event.window.data1, event.window.data2);
                    }
                    break;
                default:
                    if (g_Runtime.screenSaverMode) running = false;
                    break;
            }
        }
        if (!running) break;

        auto  now = Clock::now();
        float dt  = std::chrono::duration<float>(now - lastTime).count();
        lastTime  = now;

        g_Resources.player->Update(0.0, dt);

        if (!g_Resources.swapchain->AcquireNextImage()) continue;

        g_Resources.commandQueue->ResetCommandList();
        VkCommandBuffer cmdBuf = g_Resources.commandQueue->GetCommandBuffer();

        RenderContext ctx;
        ctx.commandBuffer = cmdBuf;
        ctx.renderTarget  = g_Resources.swapchain->GetCurrentImage();
        ctx.rtvView       = g_Resources.swapchain->GetCurrentImageView();
        ctx.renderPass    = g_Resources.swapchain->GetRenderPass();
        ctx.framebuffer   = g_Resources.swapchain->GetCurrentFramebuffer();
        ctx.width         = g_Resources.swapchain->GetWidth();
        ctx.height        = g_Resources.swapchain->GetHeight();

        g_Resources.player->Render(ctx);

        g_Resources.commandQueue->ExecuteCommandList();
        g_Resources.swapchain->Present(g_Runtime.vsyncEnabled);
        g_Resources.commandQueue->WaitForGPU();

        g_Runtime.fpsAccumSeconds += static_cast<double>(dt);
        g_Runtime.fpsAccumFrames  += 1;
        if (g_Runtime.fpsAccumSeconds >= 0.25) {
            g_Runtime.lastMeasuredFps  = static_cast<float>(g_Runtime.fpsAccumFrames / g_Runtime.fpsAccumSeconds);
            g_Runtime.fpsAccumSeconds  = 0.0;
            g_Runtime.fpsAccumFrames   = 0;
            RuntimeWindowPolicy::UpdateWindowTitleWithStats(nativeWindow, g_Runtime.baseWindowTitle,
                g_Runtime.lastMeasuredFps, g_Runtime.vsyncEnabled, g_Runtime.windowed, g_Runtime.screenSaverMode);
        }
    }

    // --------------- Cleanup ---------------
#if SHADERLAB_RUNTIME_IMGUI
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
#endif

    ShutdownVulkanResources();
    RuntimeStartupPolicy::RestoreRuntimeCursor(g_Runtime.runtimeCursorHidden);
    SDL_DestroyWindow(sdlWindow);
    SDL_Quit();
    return 0;
}

} // namespace ShaderLab
