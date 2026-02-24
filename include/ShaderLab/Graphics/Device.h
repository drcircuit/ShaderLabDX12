#pragma once

#include "ShaderLab/Platform/Platform.h"
#include <memory>
#include <vector>
#include <string>

#ifdef SHADERLAB_GFX_D3D12
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#endif

namespace ShaderLab {

class Device {
public:
    enum class InitFailureCode {
        None = 0,
        FactoryCreateFailed,
        AdapterSelectionFailed,
        DeviceCreateFailed
    };

    Device();
    ~Device();

    struct AdapterInfo {
        std::wstring name;
        size_t videoMemory;
        uint32_t index;
    };
    
    // Static helper to list adapters without creating a device instance fully
    static std::vector<AdapterInfo> GetAvailableAdapters();

    // Initialize with optional specific adapter index. -1 = Auto/Best
    bool Initialize(bool enableValidation = false, int adapterIndex = -1);
    void Shutdown();

#ifdef SHADERLAB_GFX_D3D12
    ID3D12Device*   GetDevice()  const { return m_device.Get(); }
    IDXGIFactory4*  GetFactory() const { return m_factory.Get(); }
    IDXGIAdapter3*  GetAdapter() const { return m_adapter.Get(); }
#endif

    struct MemoryInfo {
        uint64_t usage = 0;
        uint64_t budget = 0;
    };
    MemoryInfo GetVideoMemoryInfo() const;

    bool IsValid() const;

    std::wstring GetAdapterName() const { return m_adapterName; }
    int GetAdapterIndex() const { return m_adapterIndex; }
    InitFailureCode GetLastInitFailureCode() const { return m_lastInitFailureCode; }

private:
#ifdef SHADERLAB_GFX_D3D12
    ComPtr<IDXGIFactory4>  m_factory;
    ComPtr<IDXGIAdapter3>  m_adapter;
    ComPtr<ID3D12Device>   m_device;
    ComPtr<ID3D12Debug>    m_debugController;
#endif

    std::wstring    m_adapterName;
    int             m_adapterIndex = -1;
    InitFailureCode m_lastInitFailureCode = InitFailureCode::None;

    bool m_validationEnabled = false;
};

} // namespace ShaderLab
