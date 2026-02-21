#include "ShaderLab/App/PlayerApp.h"
#include "ShaderLab/App/DemoPlayer.h"
#include "ShaderLab/Graphics/CommandQueue.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"

#include <windowsx.h>

namespace ShaderLab {

namespace {

struct TinyRuntimeResources {
    Device* device = nullptr;
    Swapchain* swapchain = nullptr;
    CommandQueue* commandQueue = nullptr;
    DemoPlayer* player = nullptr;
};

TinyRuntimeResources g_resources;

struct TinyRuntimeState {
    bool windowed = false;
    bool screenSaverMode = false;
    bool vsyncEnabled = true;
    bool startFullscreen = true;
    bool runtimeCursorHidden = false;
    POINT lastMousePos = { 0, 0 };
};

TinyRuntimeState g_runtime;

void ShutdownTinyRuntimeResources() {
    if (g_resources.commandQueue) {
        g_resources.commandQueue->WaitForGPU();
    }
    if (g_resources.player) {
        g_resources.player->Shutdown();
        delete g_resources.player;
        g_resources.player = nullptr;
    }
    if (g_resources.swapchain) {
        delete g_resources.swapchain;
        g_resources.swapchain = nullptr;
    }
    if (g_resources.commandQueue) {
        delete g_resources.commandQueue;
        g_resources.commandQueue = nullptr;
    }
    if (g_resources.device) {
        delete g_resources.device;
        g_resources.device = nullptr;
    }
}

LRESULT CALLBACK TinyWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_ERASEBKGND) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        HDC hdc = reinterpret_cast<HDC>(wParam);
        if (hdc) {
            FillRect(hdc, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }
        return 1;
    }

    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc) {
            FillRect(hdc, &ps.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    if (uMsg == WM_SETCURSOR) {
        SetCursor(nullptr);
        return TRUE;
    }

    if (g_runtime.screenSaverMode) {
        switch (uMsg) {
            case WM_MOUSEMOVE: {
                POINT p;
                p.x = GET_X_LPARAM(lParam);
                p.y = GET_Y_LPARAM(lParam);
                const int dx = p.x - g_runtime.lastMousePos.x;
                const int dy = p.y - g_runtime.lastMousePos.y;
                if (dx > 2 || dx < -2 || dy > 2 || dy < -2) {
                    PostQuitMessage(0);
                }
                g_runtime.lastMousePos = p;
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
        case WM_SIZE:
            if (g_resources.swapchain) {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                if (w > 0 && h > 0) {
                    g_resources.swapchain->Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
                    if (g_resources.player) {
                        g_resources.player->OnResize(w, h);
                    }
                }
            }
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

} // namespace

int RunPlayerApp(HINSTANCE hInstance, const PlayerLaunchOptions& options) {
    g_runtime = TinyRuntimeState{};
    g_runtime.screenSaverMode = options.screenSaverMode;
    g_runtime.vsyncEnabled = options.vsyncEnabled;
    g_runtime.startFullscreen = options.startFullscreen;

    const char kClassName[] = "ShaderLabTinyPlayerWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = TinyWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = kClassName;
    wc.hCursor = nullptr;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    RegisterClass(&wc);

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const bool launchFullscreen = (!g_runtime.screenSaverMode && g_runtime.startFullscreen);

    const DWORD windowStyle = (g_runtime.screenSaverMode || launchFullscreen) ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    const int windowW = (g_runtime.screenSaverMode || launchFullscreen) ? screenW : 1280;
    const int windowH = (g_runtime.screenSaverMode || launchFullscreen) ? screenH : 720;
    const int windowX = (g_runtime.screenSaverMode || launchFullscreen) ? 0 : (screenW - windowW) / 2;
    const int windowY = (g_runtime.screenSaverMode || launchFullscreen) ? 0 : (screenH - windowH) / 2;
    const DWORD exStyle = (g_runtime.screenSaverMode || launchFullscreen) ? WS_EX_TOPMOST : 0;

    HWND hwnd = CreateWindowEx(
        exStyle,
        kClassName,
        "ShaderLab Tiny Player",
        windowStyle,
        windowX,
        windowY,
        windowW,
        windowH,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd) {
        return -1;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    if (g_runtime.screenSaverMode) {
        GetCursorPos(&g_runtime.lastMousePos);
        ScreenToClient(hwnd, &g_runtime.lastMousePos);
    }

    g_resources.device = new Device();
    if (!g_resources.device->Initialize()) {
        ShutdownTinyRuntimeResources();
        return -1;
    }

    g_resources.commandQueue = new CommandQueue();
    if (!g_resources.commandQueue->Initialize(g_resources.device, D3D12_COMMAND_LIST_TYPE_DIRECT)) {
        ShutdownTinyRuntimeResources();
        return -1;
    }

    g_resources.swapchain = new Swapchain();
    if (!g_resources.swapchain->Initialize(g_resources.device, g_resources.commandQueue, hwnd, static_cast<uint32_t>(windowW), static_cast<uint32_t>(windowH))) {
        ShutdownTinyRuntimeResources();
        return -1;
    }

    const int runtimeWidth = static_cast<int>(g_resources.swapchain->GetWidth());
    const int runtimeHeight = static_cast<int>(g_resources.swapchain->GetHeight());
    g_resources.player = new DemoPlayer();
    if (!g_resources.player->Initialize(hwnd, g_resources.device, g_resources.swapchain, runtimeWidth, runtimeHeight)) {
        ShutdownTinyRuntimeResources();
        return -1;
    }

    g_resources.player->SetLooping(options.loopPlayback);
    g_resources.player->SetVsyncEnabled(g_runtime.vsyncEnabled);
    g_resources.player->LoadProject("assets/track.bin");

    MSG msg = {};
    LARGE_INTEGER freq, lastTime, currTime;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&lastTime);

    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                PostQuitMessage(0);
            }
        } else {
            QueryPerformanceCounter(&currTime);
            const double dt = static_cast<double>(currTime.QuadPart - lastTime.QuadPart) / static_cast<double>(freq.QuadPart);
            lastTime = currTime;

            g_resources.player->Update(0.0, static_cast<float>(dt));

            g_resources.commandQueue->ResetCommandList();
            auto* cmdList = g_resources.commandQueue->GetCommandList();

            auto* backBuffer = g_resources.swapchain->GetCurrentBackBuffer();
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = backBuffer;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            cmdList->ResourceBarrier(1, &barrier);

            auto rtv = g_resources.swapchain->GetCurrentRTV();
            float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
            cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
            cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            g_resources.player->Render(cmdList, backBuffer, rtv);
            g_runtime.vsyncEnabled = g_resources.player->IsVsyncEnabled();

            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            cmdList->ResourceBarrier(1, &barrier);

            g_resources.commandQueue->ExecuteCommandList();
            g_resources.swapchain->Present(g_runtime.vsyncEnabled);
            g_resources.commandQueue->WaitForGPU();
        }
    }

    ShutdownTinyRuntimeResources();
    return 0;
}

} // namespace ShaderLab
