#pragma once

// ---------------------------------------------------------------------------
// VulkanRenderer
//
// Vulkan equivalent of PreviewRenderer.
//
// Renders a single full-screen quad using a fragment shader compiled from
// HLSL to SPIR-V (via DXC -spirv).  The root / push-constant layout mirrors
// the D3D12 PreviewRenderer so that the same HLSL fragment shaders can be
// reused with minimal changes.
//
// Build requirements:
//   - VMA (Vulkan Memory Allocator) for buffer/image allocation
//     https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
//   - DirectXShaderCompiler (DXC) with -spirv support
//     https://github.com/microsoft/DirectXShaderCompiler
// ---------------------------------------------------------------------------

#include "ShaderLab/Platform/Platform.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <cstddef>
#include <cstdint>

namespace ShaderLab {

class VulkanDevice;
class ShaderCompiler;

class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    bool Initialize(VulkanDevice*   device,
                    ShaderCompiler* compiler,
                    VkFormat        renderTargetFormat,
                    VkRenderPass    renderPass,
                    const std::vector<uint8_t>* precompiledVertexShader = nullptr);
    void Shutdown();

    struct TextureDecl {
        int         slot;
        std::string type; // "Texture2D", "TextureCube", "Texture3D"
    };

    // Compile an HLSL fragment shader to a Vulkan pipeline.
    // Returns VK_NULL_HANDLE on failure and fills outErrors.
    VkPipeline CompileShader(const std::string&           shaderSource,
                             const std::vector<TextureDecl>& textureDecls,
                             std::vector<std::string>&    outErrors,
                             bool                         flipFragCoord  = false,
                             const std::string&           entryPoint     = "main");

    // Create a pipeline from pre-compiled SPIR-V bytecode.
    VkPipeline CreatePipelineFromSpirV(const std::vector<uint8_t>& spirvBytecode);

    // Render one full-screen quad.
    void Render(VkCommandBuffer commandBuffer,
                VkPipeline      pipeline,
                VkRenderPass    renderPass,
                VkFramebuffer   framebuffer,
                uint32_t        width,
                uint32_t        height,
                float           timeSeconds,
                float           iBeat    = 0.0f,
                float           iBar     = 0.0f,
                float           fBarBeat16 = 0.0f,
                float           fBeat    = 0.0f,
                float           fBarBeat = 0.0f);

    bool IsValid(VkPipeline pipeline) const { return pipeline != VK_NULL_HANDLE; }

    float  GetLastGPUTimeMs()               const { return m_lastGPUTimeMs; }
    size_t GetLastCompiledShaderSize()       const { return m_lastCompiledShaderSize; }

private:
    bool CreatePipelineLayout();
    bool CreateVertexBuffer();
    VkShaderModule CreateShaderModule(const std::vector<uint8_t>& spirv);

    VulkanDevice*   m_device   = nullptr;
    ShaderCompiler* m_compiler = nullptr;
    VkFormat        m_format   = VK_FORMAT_B8G8R8A8_UNORM;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkBuffer         m_vertexBuffer   = VK_NULL_HANDLE;
    VkDeviceMemory   m_vertexMemory   = VK_NULL_HANDLE;

    std::vector<uint8_t> m_vertexSpirV;

    float  m_lastGPUTimeMs        = 0.0f;
    size_t m_lastCompiledShaderSize = 0;
};

} // namespace ShaderLab
