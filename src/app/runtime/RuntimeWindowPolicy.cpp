#include "ShaderLab/Runtime/RuntimeWindowPolicy.h"

#include "ShaderLab/Graphics/CommandQueue.h"
#include "ShaderLab/Graphics/Swapchain.h"

#include <algorithm>
#include <cstdio>
#include <windows.h>

namespace ShaderLab::RuntimeWindowPolicy {

void UpdateWindowTitleWithStats(
    NativeWindowHandle hwnd,
    const std::string& baseWindowTitle,
    float lastMeasuredFps,
    bool vsyncEnabled,
    bool windowed,
    bool screenSaverMode) {
    HWND nativeHwnd = reinterpret_cast<HWND>(hwnd);
    if (!nativeHwnd || screenSaverMode) {
        return;
    }

    char title[512] = {};
    std::snprintf(
        title,
        sizeof(title),
        "%s | FPS: %.1f | VSync: %s | Fullscreen: %s",
        baseWindowTitle.c_str(),
        lastMeasuredFps,
        vsyncEnabled ? "On" : "Off",
        windowed ? "Windowed" : "Borderless");
    SetWindowTextA(nativeHwnd, title);
}

bool PresentInitialBlackFrame(CommandQueue* commandQueue, Swapchain* swapchain) {
    if (!commandQueue || !swapchain) {
        return false;
    }

    commandQueue->ResetCommandList();
    auto* cmdList = commandQueue->GetCommandList();
    if (!cmdList) {
        return false;
    }

    auto* backBuffer = swapchain->GetCurrentBackBuffer();
    if (!backBuffer) {
        return false;
    }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = backBuffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmdList->ResourceBarrier(1, &barrier);

    auto rtv = swapchain->GetCurrentRTV();
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    cmdList->ResourceBarrier(1, &barrier);

    commandQueue->ExecuteCommandList();
    swapchain->Present(true);
    return true;
}

void SetFullscreen(
    NativeWindowHandle hwnd,
    bool& windowed,
    WindowRect& windowedRect,
    const std::string& baseWindowTitle,
    float lastMeasuredFps,
    bool vsyncEnabled,
    bool screenSaverMode) {
    HWND nativeHwnd = reinterpret_cast<HWND>(hwnd);
    if (!nativeHwnd) {
        return;
    }

    if (windowed) {
        RECT rc{};
        GetWindowRect(nativeHwnd, &rc);
        windowedRect = { rc.left, rc.top, rc.right, rc.bottom };
    }

    windowed = false;
    DWORD style = WS_POPUP | WS_VISIBLE;
    SetWindowLongPtr(nativeHwnd, GWL_STYLE, style);
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(nativeHwnd, HWND_TOPMOST, 0, 0, screenW, screenH, SWP_FRAMECHANGED);

    UpdateWindowTitleWithStats(hwnd, baseWindowTitle, lastMeasuredFps, vsyncEnabled, windowed, screenSaverMode);
}

void SetWindowed(
    NativeWindowHandle hwnd,
    bool& windowed,
    WindowRect& windowedRect,
    const std::string& baseWindowTitle,
    float lastMeasuredFps,
    bool vsyncEnabled,
    bool screenSaverMode) {
    HWND nativeHwnd = reinterpret_cast<HWND>(hwnd);
    if (!nativeHwnd) {
        return;
    }

    windowed = true;
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    SetWindowLongPtr(nativeHwnd, GWL_STYLE, style);

    int width  = windowedRect.Width();
    int height = windowedRect.Height();
    if (width <= 0 || height <= 0) {
        width  = 1280;
        height = 720;
    }
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width)  / 2;
    int y = (screenH - height) / 2;
    SetWindowPos(nativeHwnd, HWND_NOTOPMOST, x, y, width, height, SWP_FRAMECHANGED | SWP_SHOWWINDOW);

    UpdateWindowTitleWithStats(hwnd, baseWindowTitle, lastMeasuredFps, vsyncEnabled, windowed, screenSaverMode);
}

} // namespace ShaderLab::RuntimeWindowPolicy
