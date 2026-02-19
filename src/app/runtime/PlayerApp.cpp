#include "ShaderLab/App/PlayerApp.h"

#include "ShaderLab/App/DemoPlayer.h"
#include "ShaderLab/Graphics/CommandQueue.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"

#include <algorithm>
#include <conio.h>
#include <cstdio>
#include <memory>
#include <string>
#ifndef SHADERLAB_TINY_PLAYER
#define SHADERLAB_TINY_PLAYER 0
#endif

#if !SHADERLAB_TINY_PLAYER
#include <filesystem>
#include <iostream>
#endif
#include <windowsx.h>

#ifndef SHADERLAB_RUNTIME_IMGUI
#define SHADERLAB_RUNTIME_IMGUI 1
#endif

#if SHADERLAB_RUNTIME_IMGUI
#include "imgui.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace ShaderLab {

namespace {

Device* g_Device = nullptr;
Swapchain* g_Swapchain = nullptr;
CommandQueue* g_CommandQueue = nullptr;
DemoPlayer* g_Player = nullptr;
bool g_DebugConsole = false;
bool g_Windowed = false;
bool g_ScreenSaverMode = false;
bool g_VsyncEnabled = true;
float g_LastMeasuredFps = 0.0f;
double g_FpsAccumSeconds = 0.0;
int g_FpsAccumFrames = 0;
std::string g_BaseWindowTitle = "DrCiRCUiT's ShaderLab - For democoders, by a democoder - demo";
RECT g_WindowedRect = { 0, 0, 1280, 720 };
POINT g_LastMousePos = { 0, 0 };

void EnableConsoleLogging() {
#if !SHADERLAB_TINY_PLAYER
    if (!AllocConsole()) return;
    FILE* ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
    std::ios::sync_with_stdio(true);
    std::cout.clear();
    std::cerr.clear();
#endif
}

void ToggleWindowVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
}

void UpdateWindowTitleWithStats(HWND hwnd) {
    if (!hwnd || g_ScreenSaverMode) {
        return;
    }

    char title[512] = {};
    std::snprintf(
        title,
        sizeof(title),
        "%s | FPS: %.1f | VSync: %s | Fullscreen: %s",
        g_BaseWindowTitle.c_str(),
        g_LastMeasuredFps,
        g_VsyncEnabled ? "On" : "Off",
        g_Windowed ? "Windowed" : "Exclusive");
    SetWindowTextA(hwnd, title);
}

void SetFullscreen(HWND hwnd) {
    if (!hwnd) {
        return;
    }

    if (g_Windowed) {
        GetWindowRect(hwnd, &g_WindowedRect);
    }

    if (g_Swapchain) {
        g_Swapchain->SetExclusiveFullscreen(false);
    }

    g_Windowed = false;
    DWORD style = WS_POPUP | WS_VISIBLE;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenW, screenH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    if (g_Swapchain) {
        g_Swapchain->SetExclusiveFullscreen(true);
    }

    UpdateWindowTitleWithStats(hwnd);
}

void SetWindowed(HWND hwnd) {
    if (!hwnd) {
        return;
    }

    if (g_Swapchain) {
        g_Swapchain->SetExclusiveFullscreen(false);
    }

    g_Windowed = true;
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    int width = g_WindowedRect.right - g_WindowedRect.left;
    int height = g_WindowedRect.bottom - g_WindowedRect.top;
    if (width <= 0 || height <= 0) {
        width = 1280;
        height = 720;
    }
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;
    SetWindowPos(hwnd, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    UpdateWindowTitleWithStats(hwnd);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
#if SHADERLAB_RUNTIME_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam)) {
        return true;
    }
#endif

    if (uMsg == WM_SETCURSOR) {
        SetCursor(nullptr);
        return TRUE;
    }

    if (g_ScreenSaverMode) {
        switch (uMsg) {
            case WM_MOUSEMOVE: {
                POINT p;
                p.x = GET_X_LPARAM(lParam);
                p.y = GET_Y_LPARAM(lParam);
                const int dx = p.x - g_LastMousePos.x;
                const int dy = p.y - g_LastMousePos.y;
                if (dx > 2 || dx < -2 || dy > 2 || dy < -2) {
                    PostQuitMessage(0);
                }
                g_LastMousePos = p;
                break;
            }
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
                PostQuitMessage(0);
                return 0;
            default:
                break;
        }
    }

    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SYSKEYDOWN:
            if (!g_ScreenSaverMode && wParam == VK_RETURN && (lParam & (1 << 29))) {
                if (g_Windowed) {
                    SetFullscreen(hwnd);
                } else {
                    SetWindowed(hwnd);
                }
                return 0;
            }
            break;
        case WM_SIZE:
            if (g_Player && g_Swapchain) {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                g_Swapchain->Resize(w, h);
                g_Player->OnResize(w, h);
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace

int RunPlayerApp(HINSTANCE hInstance, const PlayerLaunchOptions& options) {
    g_DebugConsole = options.debugConsole;
    g_ScreenSaverMode = options.screenSaverMode;
    g_VsyncEnabled = options.vsyncEnabled;

    struct HandleCloser {
        void operator()(void* handle) const {
            if (handle) {
                CloseHandle(static_cast<HANDLE>(handle));
            }
        }
    };

    std::unique_ptr<void, HandleCloser> singleInstanceMutex(
        CreateMutexA(nullptr, FALSE, "Local\\ShaderLabMicroPlayerSingleInstance"));
    if (!singleInstanceMutex) {
        return -1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    if (g_DebugConsole) {
        SetEnvironmentVariableA("SHADERLAB_DEBUG_CONSOLE", "1");
        EnableConsoleLogging();
    }

    const char CLASS_NAME[] = "ShaderLabPlayerWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = nullptr;
    RegisterClass(&wc);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    DWORD windowStyle = g_ScreenSaverMode ? (WS_POPUP | WS_VISIBLE) : (WS_OVERLAPPEDWINDOW | WS_VISIBLE);
    int windowW = g_ScreenSaverMode ? screenW : 1280;
    int windowH = g_ScreenSaverMode ? screenH : 720;
    int windowX = g_ScreenSaverMode ? 0 : (screenW - windowW) / 2;
    int windowY = g_ScreenSaverMode ? 0 : (screenH - windowH) / 2;

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "DrCiRCUiT's ShaderLab - For democoders, by a democoder - demo",
        windowStyle,
        windowX,
        windowY,
        windowW,
        windowH,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (hwnd == nullptr) return 0;
    ShowWindow(hwnd, SW_SHOW);

    if (g_ScreenSaverMode) {
        GetCursorPos(&g_LastMousePos);
        ScreenToClient(hwnd, &g_LastMousePos);
    }

    g_Device = new Device();
    if (!g_Device->Initialize()) return -1;

    g_CommandQueue = new CommandQueue();
    if (!g_CommandQueue->Initialize(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT)) return -1;

    g_Swapchain = new Swapchain();
    if (!g_Swapchain->Initialize(g_Device, g_CommandQueue, hwnd, static_cast<uint32_t>(windowW), static_cast<uint32_t>(windowH))) return -1;

    if (!g_ScreenSaverMode) {
        SetWindowed(hwnd);
        SetFullscreen(hwnd);
    }

    const int runtimeWidth = static_cast<int>(g_Swapchain->GetWidth());
    const int runtimeHeight = static_cast<int>(g_Swapchain->GetHeight());
    g_Player = new DemoPlayer();
    if (!g_Player->Initialize(hwnd, g_Device, g_Swapchain, runtimeWidth, runtimeHeight)) {
        return -1;
    }
    g_Player->SetLooping(options.loopPlayback);
    g_Player->SetVsyncEnabled(g_VsyncEnabled);

#if SHADERLAB_TINY_PLAYER
    const std::string projectPath = "assets/track.bin";
    const std::string projectName = "demo";
#else
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path exeP(exePath);
    std::filesystem::path jsonPath = exeP.replace_extension(".json");

    if (!std::filesystem::exists(jsonPath)) {
        jsonPath = exeP.parent_path() / "project.json";
    }

    std::string projectPath = jsonPath.string();
    std::string projectName = jsonPath.stem().string();
    if (projectName.empty()) {
        projectName = "demo";
    }
#endif

    if (!g_ScreenSaverMode) {
        g_BaseWindowTitle = "DrCiRCUiT's ShaderLab - For democoders, by a democoder - " + projectName;
        UpdateWindowTitleWithStats(hwnd);
    }

    #if !SHADERLAB_TINY_PLAYER
    std::cout << "Loading project: " << projectPath << std::endl;
    #endif
    g_Player->LoadProject(projectPath);

    MSG msg = {};
    LARGE_INTEGER freq, lastTime, currTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                if (g_DebugConsole && !g_ScreenSaverMode) {
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    PostQuitMessage(0);
                }
            }

            if (msg.message == WM_KEYDOWN && g_DebugConsole && !g_ScreenSaverMode) {
                bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                if (alt && msg.wParam == 'H') {
                    ToggleWindowVisibility(hwnd);
                } else if (alt && msg.wParam == 'W') {
                    if (!g_Windowed) {
                        SetWindowed(hwnd);
                    } else {
                        SetFullscreen(hwnd);
                    }
                }
            }
        } else {
            QueryPerformanceCounter(&currTime);
            double dt = static_cast<double>(currTime.QuadPart - lastTime.QuadPart) / static_cast<double>(freq.QuadPart);
            lastTime = currTime;

            g_Player->Update(0.0, static_cast<float>(dt));

            g_CommandQueue->ResetCommandList();
            auto cmdList = g_CommandQueue->GetCommandList();

            auto* backBuffer = g_Swapchain->GetCurrentBackBuffer();
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = backBuffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cmdList->ResourceBarrier(1, &barrier);

            auto rtv = g_Swapchain->GetCurrentRTV();
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            g_Player->Render(cmdList, backBuffer, rtv);

            g_VsyncEnabled = g_Player->IsVsyncEnabled();

            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            cmdList->ResourceBarrier(1, &barrier);

            g_CommandQueue->ExecuteCommandList();
            g_Swapchain->Present(g_VsyncEnabled);
            g_CommandQueue->WaitForGPU();

            g_FpsAccumSeconds += dt;
            g_FpsAccumFrames += 1;
            if (g_FpsAccumSeconds >= 0.25) {
                g_LastMeasuredFps = static_cast<float>(g_FpsAccumFrames / g_FpsAccumSeconds);
                g_FpsAccumSeconds = 0.0;
                g_FpsAccumFrames = 0;
                UpdateWindowTitleWithStats(hwnd);
            }
        }
    }

    g_CommandQueue->WaitForGPU();
    g_Player->Shutdown();
    delete g_Player;
    delete g_Swapchain;
    delete g_CommandQueue;
    delete g_Device;
    g_Player = nullptr;
    g_Swapchain = nullptr;
    g_CommandQueue = nullptr;
    g_Device = nullptr;

    if (g_DebugConsole) {
#if !SHADERLAB_TINY_PLAYER
        std::cout << "Press any key to exit..." << std::endl;
        _getch();
#endif
    }
    return 0;
}

} // namespace ShaderLab
