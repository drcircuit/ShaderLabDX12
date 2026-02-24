#pragma once

#include "ShaderLab/Platform/Platform.h"
#include <cstdint>

#ifdef SHADERLAB_GFX_D3D12
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace ShaderLab {

struct BufferAllocationRequest {
    uint64_t sizeBytes = 0;
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
};

class IGraphicsDeviceService {
public:
    virtual ~IGraphicsDeviceService() = default;

    virtual ID3D12Device* DeviceHandle() const = 0;
    virtual ID3D12CommandQueue* GraphicsQueueHandle() const = 0;
    virtual bool AllocateBuffer(const BufferAllocationRequest& request, ComPtr<ID3D12Resource>& outResource) = 0;
};

} // namespace ShaderLab

#elif defined(SHADERLAB_GFX_VULKAN)
#include <vulkan/vulkan.h>

namespace ShaderLab {

struct BufferAllocationRequest {
    uint64_t         sizeBytes    = 0;
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkBufferUsageFlags    usage    = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
};

class IGraphicsDeviceService {
public:
    virtual ~IGraphicsDeviceService() = default;

    virtual VkDevice DeviceHandle()  const = 0;
    virtual VkQueue  GraphicsQueue() const = 0;
    // AllocateBuffer signature to be finalised with a chosen allocator (e.g. VMA)
};

} // namespace ShaderLab

#else

namespace ShaderLab {
struct BufferAllocationRequest { uint64_t sizeBytes = 0; };
class IGraphicsDeviceService {
public:
    virtual ~IGraphicsDeviceService() = default;
};
} // namespace ShaderLab

#endif
