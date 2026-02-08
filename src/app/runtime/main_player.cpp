#include <windows.h>
#include "ShaderLab/App/DemoPlayer.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"
#include "ShaderLab/Graphics/CommandQueue.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include <iostream>
#include <filesystem>
#include <shellapi.h>
#include <conio.h>
#include "imgui.h" // Add this for IMGUI_IMPL_API

using namespace ShaderLab;

// Global variables
Device* g_Device = nullptr;
Swapchain* g_Swapchain = nullptr;
CommandQueue* g_CommandQueue = nullptr;
DemoPlayer* g_Player = nullptr;
bool g_DebugConsole = false;
bool g_Windowed = false;
RECT g_WindowedRect = { 0, 0, 1280, 720 };

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
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

static bool HasFlag(const wchar_t* flag) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], flag) == 0) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

static void EnableConsoleLogging() {
    if (!AllocConsole()) return;
    FILE* ignored = nullptr;
    freopen_s(&ignored, "CONOUT$", "w", stdout);
    freopen_s(&ignored, "CONOUT$", "w", stderr);
    std::ios::sync_with_stdio(true);
    std::cout.clear();
    std::cerr.clear();
}

static void ToggleWindowVisibility(HWND hwnd) {
    if (IsWindowVisible(hwnd)) {
        ShowWindow(hwnd, SW_HIDE);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
}

static void SetFullscreen(HWND hwnd) {
    g_Windowed = false;
    DWORD style = WS_POPUP | WS_VISIBLE;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, screenW, screenH, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

static void SetWindowed(HWND hwnd) {
    g_Windowed = true;
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    int width = g_WindowedRect.right - g_WindowedRect.left;
    int height = g_WindowedRect.bottom - g_WindowedRect.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;
    SetWindowPos(hwnd, HWND_TOP, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    g_DebugConsole = HasFlag(L"-d");
    if (g_DebugConsole) {
        SetEnvironmentVariableA("SHADERLAB_DEBUG_CONSOLE", "1");
        EnableConsoleLogging();
    }
    // 1. Register Window Class
    const char CLASS_NAME[] = "ShaderLabPlayerWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    // 2. Create Window
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    
    // Switch to borderless fullscreen by default
    int width = screenW;
    int height = screenH;
    DWORD style = WS_POPUP | WS_VISIBLE; // Fullscreen style
    
    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, "DrCiRCUiT's ShaderLab - For democoders, by a democoder - demo",
        style, 
        0, 0, width, height,
        nullptr, nullptr, hInstance, nullptr
    );

    if (hwnd == nullptr) return 0;
    ShowWindow(hwnd, SW_SHOW);

    // 3. Initialize Graphics
    g_Device = new Device();
    if (!g_Device->Initialize()) return -1;
    
    g_CommandQueue = new CommandQueue();
    if (!g_CommandQueue->Initialize(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT)) return -1;

    g_Swapchain = new Swapchain();
    if (!g_Swapchain->Initialize(g_Device, g_CommandQueue, hwnd, width, height)) return -1;

    // 4. Initialize Player
    g_Player = new DemoPlayer();
    if (!g_Player->Initialize(hwnd, g_Device, g_Swapchain, width, height)) {
        return -1;
    }

    // 5. Load Project
    // Determine project file based on executable name
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::filesystem::path exeP(exePath);
    std::filesystem::path jsonPath = exeP.replace_extension(".json");
    
    // Fallback to "project.json" if the matching json doesn't exist
    if (!std::filesystem::exists(jsonPath)) {
        jsonPath = exeP.parent_path() / "project.json";
    }

    std::string projectName = jsonPath.stem().string();
    if (projectName.empty()) {
        projectName = "demo";
    }
    std::string title = "DrCiRCUiT's ShaderLab - For democoders, by a democoder - " + projectName;
    SetWindowTextA(hwnd, title.c_str());

    std::cout << "Expected disk project path: " << jsonPath << std::endl;
    std::cout << "Loading project: " << jsonPath << std::endl;
    g_Player->LoadProject(jsonPath.string());

    // 6. Main Loop
    MSG msg = {};
    LARGE_INTEGER freq, lastTime, currTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            
            // Hide on Escape only in debug console mode; otherwise quit
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                if (g_DebugConsole) {
                    ShowWindow(hwnd, SW_HIDE);
                } else {
                    PostQuitMessage(0);
                }
            }

            if (msg.message == WM_KEYDOWN && g_DebugConsole) {
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
            // Update Time
            QueryPerformanceCounter(&currTime);
            double dt = (double)(currTime.QuadPart - lastTime.QuadPart) / freq.QuadPart;
            lastTime = currTime;

            // Run
            g_Player->Update(0.0, (float)dt);

            // Render
            g_CommandQueue->ResetCommandList();
            auto cmdList = g_CommandQueue->GetCommandList();
            
            // Barrier: Backbuffer -> RenderTarget
            auto* backBuffer = g_Swapchain->GetCurrentBackBuffer();
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = backBuffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cmdList->ResourceBarrier(1, &barrier);
            
            // Clear
            auto rtv = g_Swapchain->GetCurrentRTV();
            float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            
            g_Player->Render(cmdList, backBuffer, rtv);
            
            // Barrier: RenderTarget -> Present
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            cmdList->ResourceBarrier(1, &barrier);

            g_CommandQueue->ExecuteCommandList();
            g_Swapchain->Present();
            g_CommandQueue->WaitForGPU();
        }
    }

    g_CommandQueue->WaitForGPU();
    g_Player->Shutdown();
    delete g_Player;
    delete g_Swapchain;
    delete g_CommandQueue;
    delete g_Device;

    if (g_DebugConsole) {
        std::cout << "Press any key to exit..." << std::endl;
        _getch();
    }
    return 0;
}
