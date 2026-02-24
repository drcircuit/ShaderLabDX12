#pragma once

#include "ShaderLab/Platform/Platform.h"
#include <cstdint>

#ifdef SHADERLAB_GFX_D3D12
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace ShaderLab {

class Device;
class CommandQueue;

class Swapchain {
public:
    static constexpr uint32_t BUFFER_COUNT = 2;

    Swapchain();
    ~Swapchain();

    // hwnd is the native window handle (HWND on Win32, SDL_Window* on SDL2).
    bool Initialize(Device* device, CommandQueue* commandQueue,
                   NativeWindowHandle hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    void Present(bool vsync = true);
    void Resize(uint32_t width, uint32_t height);
    bool SupportsTearing() const { return m_allowTearing; }

#ifdef SHADERLAB_GFX_D3D12
    ID3D12Resource* GetCurrentBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;
#endif

    uint32_t GetCurrentBackBufferIndex() const { return m_currentBackBuffer; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
#ifdef SHADERLAB_GFX_D3D12
    void CreateRenderTargetViews();
    void ReleaseBackBuffers();

    ComPtr<IDXGISwapChain3> m_swapchain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];
    uint32_t m_rtvDescriptorSize = 0;
#endif

    Device* m_device = nullptr;
    CommandQueue* m_commandQueue = nullptr;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_currentBackBuffer = 0;
    NativeWindowHandle m_hwnd = nullptr;
    bool m_allowTearing = false;
};

} // namespace ShaderLab
