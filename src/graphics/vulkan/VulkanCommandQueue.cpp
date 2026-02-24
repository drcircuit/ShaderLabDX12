// ---------------------------------------------------------------------------
// VulkanCommandQueue.cpp  –  Vulkan command queue / buffer management
// ---------------------------------------------------------------------------

#include "ShaderLab/Graphics/Vulkan/VulkanCommandQueue.h"
#include "ShaderLab/Graphics/Vulkan/VulkanDevice.h"

#include <cstdio>

namespace ShaderLab {

VulkanCommandQueue::VulkanCommandQueue()  = default;
VulkanCommandQueue::~VulkanCommandQueue() { Shutdown(); }

bool VulkanCommandQueue::Initialize(VulkanDevice* device) {
    if (!device || !device->IsValid()) return false;
    m_device = device;
    m_queue  = device->GetGraphicsQueue();

    VkCommandPoolCreateInfo poolCi{};
    poolCi.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCi.queueFamilyIndex = device->GetGraphicsFamily();
    poolCi.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device->GetDevice(), &poolCi, nullptr, &m_commandPool) != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanCommandQueue] Failed to create command pool\n");
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device->GetDevice(), &allocInfo, &m_commandBuffer) != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanCommandQueue] Failed to allocate command buffer\n");
        return false;
    }

    VkFenceCreateInfo fenceCi{};
    fenceCi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCi.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so WaitForGPU() on first call is a no-op.

    if (vkCreateFence(device->GetDevice(), &fenceCi, nullptr, &m_fence) != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanCommandQueue] Failed to create fence\n");
        return false;
    }

    return true;
}

void VulkanCommandQueue::Shutdown() {
    if (!m_device) return;
    VkDevice vkDevice = m_device->GetDevice();

    if (m_fence) {
        vkDestroyFence(vkDevice, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
    if (m_commandPool) {
        vkDestroyCommandPool(vkDevice, m_commandPool, nullptr);
        m_commandPool   = VK_NULL_HANDLE;
        m_commandBuffer = VK_NULL_HANDLE;
    }
    m_device = nullptr;
}

void VulkanCommandQueue::ResetCommandList() {
    WaitForGPU(); // Ensure previous submission is done before recording new commands.
    vkResetCommandBuffer(m_commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
}

void VulkanCommandQueue::ExecuteCommandList() {
    vkEndCommandBuffer(m_commandBuffer);

    // Reset the fence before submitting so we can wait on it.
    vkResetFences(m_device->GetDevice(), 1, &m_fence);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &m_commandBuffer;

    vkQueueSubmit(m_queue, 1, &submitInfo, m_fence);
    ++m_fenceValue;
}

void VulkanCommandQueue::WaitForGPU() {
    if (!m_fence) return;
    vkWaitForFences(m_device->GetDevice(), 1, &m_fence, VK_TRUE, UINT64_MAX);
}

uint64_t VulkanCommandQueue::SignalFence() {
    // Fence signaling is implicit via vkQueueSubmit; return the last value.
    return m_fenceValue;
}

} // namespace ShaderLab
