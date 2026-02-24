#pragma once

#include "ShaderLab/Platform/Platform.h"
#include <cstdint>

#ifdef SHADERLAB_GFX_D3D12
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

namespace ShaderLab {

struct TextureAllocationRequest {
    uint32_t width = 1;
    uint32_t height = 1;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
};

struct TextureResourceAllocationRequest {
    D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    uint64_t width = 1;
    uint32_t height = 1;
    uint16_t depthOrArraySize = 1;
    uint16_t mipLevels = 1;
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_SAMPLE_DESC sampleDesc = {1, 0};
    D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
};

struct ResourceBufferAllocationRequest {
    uint64_t sizeBytes = 0;
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
};

class IResourceService {
public:
    virtual ~IResourceService() = default;

    virtual bool AllocateTexture(const TextureResourceAllocationRequest& request, ComPtr<ID3D12Resource>& outResource) = 0;
    virtual bool AllocateTexture2D(const TextureAllocationRequest& request, ComPtr<ID3D12Resource>& outResource) = 0;
    virtual bool AllocateBuffer(const ResourceBufferAllocationRequest& request, ComPtr<ID3D12Resource>& outResource) = 0;
};

} // namespace ShaderLab

#elif defined(SHADERLAB_GFX_VULKAN)
#include <vulkan/vulkan.h>

namespace ShaderLab {

struct TextureAllocationRequest {
    uint32_t width  = 1;
    uint32_t height = 1;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
};

struct TextureResourceAllocationRequest : TextureAllocationRequest {
    uint32_t depth       = 1;
    uint32_t mipLevels   = 1;
    uint32_t arrayLayers = 1;
};

struct ResourceBufferAllocationRequest {
    uint64_t sizeBytes = 0;
    VkBufferUsageFlags    usage   = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    VkMemoryPropertyFlags memFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
};

class IResourceService {
public:
    virtual ~IResourceService() = default;
    // Vulkan-specific allocation methods TBD with chosen allocator (e.g. VMA)
};

} // namespace ShaderLab

#else

namespace ShaderLab {
struct TextureAllocationRequest { uint32_t width = 1; uint32_t height = 1; };
struct TextureResourceAllocationRequest : TextureAllocationRequest {};
struct ResourceBufferAllocationRequest { uint64_t sizeBytes = 0; };
class IResourceService { public: virtual ~IResourceService() = default; };
} // namespace ShaderLab

#endif
