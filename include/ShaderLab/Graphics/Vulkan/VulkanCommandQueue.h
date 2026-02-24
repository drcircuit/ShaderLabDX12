#pragma once

// ---------------------------------------------------------------------------
// VulkanCommandQueue
//
// Thin wrapper around a Vulkan graphics queue + per-frame command buffer.
// Mirrors the interface of the D3D12 CommandQueue for easy drop-in use.
// ---------------------------------------------------------------------------

#include "ShaderLab/Platform/Platform.h"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace ShaderLab {

class VulkanDevice;

class VulkanCommandQueue {
public:
    VulkanCommandQueue();
    ~VulkanCommandQueue();

    bool Initialize(VulkanDevice* device);
    void Shutdown();

    VkCommandBuffer GetCommandBuffer() const { return m_commandBuffer; }
    VkQueue         GetQueue()         const { return m_queue; }

    // Reset and open a new command buffer for recording.
    void ResetCommandList();

    // Submit the recorded command buffer.
    void ExecuteCommandList();

    // Block until the submitted work has completed on the GPU.
    void WaitForGPU();

    uint64_t SignalFence();

private:
    VulkanDevice*   m_device        = nullptr;
    VkQueue         m_queue         = VK_NULL_HANDLE;
    VkCommandPool   m_commandPool   = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkFence         m_fence         = VK_NULL_HANDLE;
    uint64_t        m_fenceValue    = 0;
};

} // namespace ShaderLab
