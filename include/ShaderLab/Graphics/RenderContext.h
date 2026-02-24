#pragma once

#include "ShaderLab/Platform/Platform.h"

// ---------------------------------------------------------------------------
// RenderContext
//
// Platform-neutral frame render context passed to DemoPlayer::Render() and
// related rendering calls.  The exact fields are selected at compile time
// based on the active graphics backend macro.
//
// Usage:
//   D3D12 (Windows):
//     RenderContext ctx;
//     ctx.commandList  = d3d12CmdList;
//     ctx.renderTarget = backBuffer;
//     ctx.rtvHandle    = rtvDescHandle;
//     ctx.width        = swapchain->GetWidth();
//     ctx.height       = swapchain->GetHeight();
//     player.Render(ctx);
//
//   Vulkan (Linux / macOS):
//     RenderContext ctx;
//     ctx.commandBuffer  = vkCmdBuf;
//     ctx.renderTarget   = swapchainImage;
//     ctx.rtvView        = swapchainImageView;
//     ctx.renderPass     = vkRenderPass;
//     ctx.framebuffer    = vkFramebuffer;
//     ctx.width          = swapchainExtent.width;
//     ctx.height         = swapchainExtent.height;
//     player.Render(ctx);
// ---------------------------------------------------------------------------

#if defined(SHADERLAB_GFX_D3D12)

#include <d3d12.h>

namespace ShaderLab {

struct RenderContext {
    ID3D12GraphicsCommandList* commandList  = nullptr;
    ID3D12Resource*            renderTarget = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
    uint32_t width  = 0;
    uint32_t height = 0;
};

} // namespace ShaderLab

#elif defined(SHADERLAB_GFX_VULKAN)

#include <vulkan/vulkan.h>

namespace ShaderLab {

struct RenderContext {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkImage         renderTarget  = VK_NULL_HANDLE;
    VkImageView     rtvView       = VK_NULL_HANDLE;
    VkRenderPass    renderPass    = VK_NULL_HANDLE;
    VkFramebuffer   framebuffer   = VK_NULL_HANDLE;
    uint32_t        width  = 0;
    uint32_t        height = 0;
};

} // namespace ShaderLab

#else

// Null / stub backend
namespace ShaderLab {

struct RenderContext {
    uint32_t width  = 0;
    uint32_t height = 0;
};

} // namespace ShaderLab

#endif
