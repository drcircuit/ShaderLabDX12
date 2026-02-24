#pragma once

// ---------------------------------------------------------------------------
// VulkanSwapchain
//
// Vulkan WSI swapchain.  Mirrors the interface of the D3D12 Swapchain class.
// On macOS, Vulkan surfaces are provided by MoltenVK via SDL2's
// SDL_Vulkan_CreateSurface() call; no Metal-specific code is needed here.
// ---------------------------------------------------------------------------

#include "ShaderLab/Platform/Platform.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace ShaderLab {

class VulkanDevice;
class VulkanCommandQueue;

class VulkanSwapchain {
public:
    static constexpr uint32_t BUFFER_COUNT = 2;

    VulkanSwapchain();
    ~VulkanSwapchain();

    // hwnd is a NativeWindowHandle (SDL_Window* on SDL2 builds).
    bool Initialize(VulkanDevice*       device,
                    VulkanCommandQueue* commandQueue,
                    NativeWindowHandle  hwnd,
                    uint32_t            width,
                    uint32_t            height);
    void Shutdown();

    void Present(bool vsync = true);
    void Resize(uint32_t width, uint32_t height);

    // Returns the index of the image acquired for the current frame.
    uint32_t GetCurrentBackBufferIndex() const { return m_currentImageIndex; }

    // Vulkan-specific accessors used by the render loop.
    VkImage         GetCurrentImage()      const { return m_images[m_currentImageIndex]; }
    VkImageView     GetCurrentImageView()  const { return m_imageViews[m_currentImageIndex]; }
    VkFramebuffer   GetCurrentFramebuffer() const { return m_framebuffers[m_currentImageIndex]; }
    VkRenderPass    GetRenderPass()         const { return m_renderPass; }
    VkSemaphore     GetImageAvailableSemaphore() const { return m_imageAvailableSemaphore; }
    VkSemaphore     GetRenderFinishedSemaphore() const { return m_renderFinishedSemaphore; }

    uint32_t GetWidth()  const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    // Acquire the next swapchain image; call before building the command buffer.
    bool AcquireNextImage();

private:
    void CreateSwapchain(uint32_t width, uint32_t height);
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void DestroySwapchainObjects();

    VulkanDevice*       m_device       = nullptr;
    VulkanCommandQueue* m_commandQueue = nullptr;

    VkSwapchainKHR          m_swapchain   = VK_NULL_HANDLE;
    VkFormat                m_format      = VK_FORMAT_B8G8R8A8_UNORM;
    VkRenderPass            m_renderPass  = VK_NULL_HANDLE;

    std::vector<VkImage>       m_images;
    std::vector<VkImageView>   m_imageViews;
    std::vector<VkFramebuffer> m_framebuffers;

    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;

    uint32_t m_width             = 0;
    uint32_t m_height            = 0;
    uint32_t m_currentImageIndex = 0;
};

} // namespace ShaderLab
