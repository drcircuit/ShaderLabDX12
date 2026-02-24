// ---------------------------------------------------------------------------
// VulkanSwapchain.cpp  –  Vulkan WSI swapchain
// ---------------------------------------------------------------------------

#include "ShaderLab/Graphics/Vulkan/VulkanSwapchain.h"
#include "ShaderLab/Graphics/Vulkan/VulkanDevice.h"
#include "ShaderLab/Graphics/Vulkan/VulkanCommandQueue.h"

#include <algorithm>
#include <cstdio>

namespace ShaderLab {

VulkanSwapchain::VulkanSwapchain()  = default;
VulkanSwapchain::~VulkanSwapchain() { Shutdown(); }

bool VulkanSwapchain::Initialize(VulkanDevice*       device,
                                  VulkanCommandQueue* commandQueue,
                                  NativeWindowHandle  /*hwnd*/,
                                  uint32_t            width,
                                  uint32_t            height) {
    if (!device || !device->IsValid()) return false;
    m_device       = device;
    m_commandQueue = commandQueue;

    CreateSwapchain(width, height);
    CreateImageViews();
    CreateRenderPass();
    CreateFramebuffers();

    // Synchronisation primitives
    VkSemaphoreCreateInfo semCi{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    vkCreateSemaphore(device->GetDevice(), &semCi, nullptr, &m_imageAvailableSemaphore);
    vkCreateSemaphore(device->GetDevice(), &semCi, nullptr, &m_renderFinishedSemaphore);

    return m_swapchain != VK_NULL_HANDLE;
}

void VulkanSwapchain::Shutdown() {
    if (!m_device) return;
    VkDevice vkDevice = m_device->GetDevice();
    vkDeviceWaitIdle(vkDevice);

    if (m_renderFinishedSemaphore) {
        vkDestroySemaphore(vkDevice, m_renderFinishedSemaphore, nullptr);
        m_renderFinishedSemaphore = VK_NULL_HANDLE;
    }
    if (m_imageAvailableSemaphore) {
        vkDestroySemaphore(vkDevice, m_imageAvailableSemaphore, nullptr);
        m_imageAvailableSemaphore = VK_NULL_HANDLE;
    }

    DestroySwapchainObjects();
    m_device = nullptr;
}

bool VulkanSwapchain::AcquireNextImage() {
    VkResult result = vkAcquireNextImageKHR(
        m_device->GetDevice(),
        m_swapchain,
        UINT64_MAX,
        m_imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &m_currentImageIndex);
    return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
}

void VulkanSwapchain::Present(bool /*vsync*/) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_renderFinishedSemaphore;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &m_currentImageIndex;

    VkResult result = vkQueuePresentKHR(m_device->GetGraphicsQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        Resize(m_width, m_height);
    }
}

void VulkanSwapchain::Resize(uint32_t width, uint32_t height) {
    if (!m_device) return;
    vkDeviceWaitIdle(m_device->GetDevice());
    DestroySwapchainObjects();
    CreateSwapchain(width, height);
    CreateImageViews();
    CreateFramebuffers();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void VulkanSwapchain::CreateSwapchain(uint32_t width, uint32_t height) {
    VkDevice         vkDevice = m_device->GetDevice();
    VkPhysicalDevice physDev  = m_device->GetPhysicalDevice();
    VkSurfaceKHR     surface  = m_device->GetSurface();

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, surface, &caps);

    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX) {
        extent = caps.currentExtent;
    } else {
        extent.width  = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    m_width  = extent.width;
    m_height = extent.height;

    // Pick a surface format – prefer BGRA8 SRGB.
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physDev, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = f;
            break;
        }
    }
    m_format = chosen.format;

    // Pick present mode: prefer MAILBOX for low-latency, fall back to FIFO.
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physDev, surface, &modeCount, modes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
    }

    uint32_t imageCount = std::max(BUFFER_COUNT, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = surface;
    ci.minImageCount    = imageCount;
    ci.imageFormat      = chosen.format;
    ci.imageColorSpace  = chosen.colorSpace;
    ci.imageExtent      = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = caps.currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = presentMode;
    ci.clipped          = VK_TRUE;

    if (vkCreateSwapchainKHR(vkDevice, &ci, nullptr, &m_swapchain) != VK_SUCCESS) {
        std::fprintf(stderr, "[VulkanSwapchain] Failed to create swapchain\n");
        return;
    }

    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(vkDevice, m_swapchain, &actualCount, nullptr);
    m_images.resize(actualCount);
    vkGetSwapchainImagesKHR(vkDevice, m_swapchain, &actualCount, m_images.data());
}

void VulkanSwapchain::CreateImageViews() {
    VkDevice vkDevice = m_device->GetDevice();
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo ci{};
        ci.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image                           = m_images[i];
        ci.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        ci.format                          = m_format;
        ci.components                      = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ci.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ci.subresourceRange.baseMipLevel   = 0;
        ci.subresourceRange.levelCount     = 1;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount     = 1;
        vkCreateImageView(vkDevice, &ci, nullptr, &m_imageViews[i]);
    }
}

void VulkanSwapchain::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_format;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpCi{};
    rpCi.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCi.attachmentCount = 1;
    rpCi.pAttachments    = &colorAttachment;
    rpCi.subpassCount    = 1;
    rpCi.pSubpasses      = &subpass;
    rpCi.dependencyCount = 1;
    rpCi.pDependencies   = &dep;

    vkCreateRenderPass(m_device->GetDevice(), &rpCi, nullptr, &m_renderPass);
}

void VulkanSwapchain::CreateFramebuffers() {
    VkDevice vkDevice = m_device->GetDevice();
    m_framebuffers.resize(m_imageViews.size());
    for (size_t i = 0; i < m_imageViews.size(); ++i) {
        VkFramebufferCreateInfo fbCi{};
        fbCi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCi.renderPass      = m_renderPass;
        fbCi.attachmentCount = 1;
        fbCi.pAttachments    = &m_imageViews[i];
        fbCi.width           = m_width;
        fbCi.height          = m_height;
        fbCi.layers          = 1;
        vkCreateFramebuffer(vkDevice, &fbCi, nullptr, &m_framebuffers[i]);
    }
}

void VulkanSwapchain::DestroySwapchainObjects() {
    VkDevice vkDevice = m_device->GetDevice();
    for (auto fb : m_framebuffers) { vkDestroyFramebuffer(vkDevice, fb, nullptr); }
    m_framebuffers.clear();
    if (m_renderPass) {
        vkDestroyRenderPass(vkDevice, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    for (auto iv : m_imageViews) { vkDestroyImageView(vkDevice, iv, nullptr); }
    m_imageViews.clear();
    m_images.clear();
    if (m_swapchain) {
        vkDestroySwapchainKHR(vkDevice, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

} // namespace ShaderLab
