// ---------------------------------------------------------------------------
// VulkanRenderer.cpp  –  Full-screen quad renderer using Vulkan + HLSL/SPIR-V
//
// HLSL shaders are compiled to SPIR-V at runtime using DXC with the -spirv
// flag.  The compiled SPIR-V bytecode is then loaded directly into a Vulkan
// shader module, keeping the shader source fully reusable from Windows.
//
// Push-constant layout (mirrors the D3D12 PreviewRenderer cbuffer):
//   offset  0  : float iTime
//   offset  4  : float iBeat
//   offset  8  : float iBar
//   offset 12  : float fBarBeat16
//   offset 16  : float fBeat
//   offset 20  : float fBarBeat
//   offset 24  : float iResolutionX
//   offset 28  : float iResolutionY
// ---------------------------------------------------------------------------

#include "ShaderLab/Graphics/Vulkan/VulkanRenderer.h"
#include "ShaderLab/Graphics/Vulkan/VulkanDevice.h"
#include "ShaderLab/Shader/ShaderCompiler.h"

#include <cstdio>
#include <cstring>

namespace ShaderLab {

// Full-screen triangle (NDC, no index buffer needed).
static const float kFullscreenTriVerts[] = {
    -1.0f, -1.0f,
     3.0f, -1.0f,
    -1.0f,  3.0f,
};

struct PushConstants {
    float iTime;
    float iBeat;
    float iBar;
    float fBarBeat16;
    float fBeat;
    float fBarBeat;
    float iResolutionX;
    float iResolutionY;
};

VulkanRenderer::VulkanRenderer()  = default;
VulkanRenderer::~VulkanRenderer() { Shutdown(); }

bool VulkanRenderer::Initialize(VulkanDevice*             device,
                                 ShaderCompiler*           compiler,
                                 VkFormat                  renderTargetFormat,
                                 VkRenderPass              renderPass,
                                 const std::vector<uint8_t>* precompiledVertexShader) {
    if (!device || !device->IsValid()) return false;
    m_device     = device;
    m_compiler   = compiler;
    m_format     = renderTargetFormat;
    m_renderPass = renderPass; // Store for pipeline creation

    if (precompiledVertexShader && !precompiledVertexShader->empty()) {
        m_vertexSpirV = *precompiledVertexShader;
    }

    if (!CreatePipelineLayout()) return false;
    if (!CreateVertexBuffer())   return false;

    return true;
}

void VulkanRenderer::Shutdown() {
    if (!m_device) return;
    VkDevice vkDevice = m_device->GetDevice();

    if (m_vertexBuffer) { vkDestroyBuffer(vkDevice, m_vertexBuffer, nullptr);     m_vertexBuffer = VK_NULL_HANDLE; }
    if (m_vertexMemory) { vkFreeMemory(vkDevice, m_vertexMemory, nullptr);         m_vertexMemory = VK_NULL_HANDLE; }
    if (m_pipelineLayout) { vkDestroyPipelineLayout(vkDevice, m_pipelineLayout, nullptr); m_pipelineLayout = VK_NULL_HANDLE; }

    m_device = nullptr;
}

VkPipeline VulkanRenderer::CompileShader(const std::string&              shaderSource,
                                          const std::vector<TextureDecl>& /*textureDecls*/,
                                          std::vector<std::string>&       outErrors,
                                          bool                            /*flipFragCoord*/,
                                          const std::string&              entryPoint) {
    if (!m_compiler) {
        outErrors.push_back("No ShaderCompiler available");
        return VK_NULL_HANDLE;
    }

    // DXC -spirv compiles HLSL to SPIR-V for Vulkan.
    auto result = m_compiler->CompileFromSource(
        shaderSource, entryPoint, "ps_6_0",
        L"shader.hlsl", ShaderCompileMode::Live);

    if (!result.success) {
        for (const auto& diag : result.diagnostics) {
            if (diag.isError) outErrors.push_back(diag.message);
        }
        return VK_NULL_HANDLE;
    }

    return CreatePipelineFromSpirV(result.bytecode);
}

VkPipeline VulkanRenderer::CreatePipelineFromSpirV(const std::vector<uint8_t>& spirvBytecode) {
    if (spirvBytecode.empty() || !m_device) return VK_NULL_HANDLE;

    VkShaderModule fragModule = CreateShaderModule(spirvBytecode);
    if (!fragModule) return VK_NULL_HANDLE;

    VkShaderModule vertModule = VK_NULL_HANDLE;
    if (!m_vertexSpirV.empty()) {
        vertModule = CreateShaderModule(m_vertexSpirV);
    }

    // If no vertex SPIR-V was provided, a passthrough vertex shader must be
    // supplied externally.  For now, return null to signal that.
    if (!vertModule) {
        vkDestroyShaderModule(m_device->GetDevice(), fragModule, nullptr);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName  = "main";

    // Full-screen triangle: 2 floats per vertex, no index buffer.
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = 2 * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr{};
    attr.binding  = 0;
    attr.location = 0;
    attr.format   = VK_FORMAT_R32G32_SFLOAT;
    attr.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions    = &attr;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode    = VK_CULL_MODE_NONE;
    raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth   = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blendAttach{};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments    = &blendAttach;

    const VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates    = dynStates;

    // NOTE: renderPass must match the one the swapchain was created with.
    // We pass VK_NULL_HANDLE here; the caller must provide a compatible render pass
    // or this will be extended to accept it as a parameter.
    VkGraphicsPipelineCreateInfo pipelineCi{};
    pipelineCi.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCi.stageCount          = 2;
    pipelineCi.pStages             = stages;
    pipelineCi.pVertexInputState   = &vertexInput;
    pipelineCi.pInputAssemblyState = &ia;
    pipelineCi.pViewportState      = &vp;
    pipelineCi.pRasterizationState = &raster;
    pipelineCi.pMultisampleState   = &ms;
    pipelineCi.pColorBlendState    = &blend;
    pipelineCi.pDynamicState       = &dyn;
    pipelineCi.layout              = m_pipelineLayout;
    pipelineCi.renderPass          = m_renderPass;  // Use stored render pass from Initialize()
    pipelineCi.subpass             = 0;

    VkPipeline pipeline = VK_NULL_HANDLE;
    vkCreateGraphicsPipelines(m_device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineCi, nullptr, &pipeline);

    vkDestroyShaderModule(m_device->GetDevice(), vertModule, nullptr);
    vkDestroyShaderModule(m_device->GetDevice(), fragModule, nullptr);

    m_lastCompiledShaderSize = spirvBytecode.size();
    return pipeline;
}

void VulkanRenderer::Render(VkCommandBuffer commandBuffer,
                             VkPipeline      pipeline,
                             VkRenderPass    renderPass,
                             VkFramebuffer   framebuffer,
                             uint32_t        width,
                             uint32_t        height,
                             float           timeSeconds,
                             float           iBeat,
                             float           iBar,
                             float           fBarBeat16,
                             float           fBeat,
                             float           fBarBeat) {
    if (!pipeline || !renderPass || !framebuffer) return;

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = renderPass;
    rpBegin.framebuffer       = framebuffer;
    rpBegin.renderArea.offset = { 0, 0 };
    rpBegin.renderArea.extent = { width, height };
    VkClearValue clearVal{};
    clearVal.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues    = &clearVal;

    vkCmdBeginRenderPass(commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    VkRect2D   scissor { { 0, 0 }, { width, height } };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer,  0, 1, &scissor);

    PushConstants pc{ timeSeconds, iBeat, iBar, fBarBeat16, fBeat, fBarBeat,
                      static_cast<float>(width), static_cast<float>(height) };
    vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(PushConstants), &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
}

// ---------------------------------------------------------------------------
bool VulkanRenderer::CreatePipelineLayout() {
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo ci{};
    ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.pushConstantRangeCount = 1;
    ci.pPushConstantRanges    = &pcRange;

    return vkCreatePipelineLayout(m_device->GetDevice(), &ci, nullptr, &m_pipelineLayout) == VK_SUCCESS;
}

bool VulkanRenderer::CreateVertexBuffer() {
    VkDevice vkDevice = m_device->GetDevice();
    VkDeviceSize size = sizeof(kFullscreenTriVerts);

    VkBufferCreateInfo bufCi{};
    bufCi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCi.size        = size;
    bufCi.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufCi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(vkDevice, &bufCi, nullptr, &m_vertexBuffer) != VK_SUCCESS) return false;

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(vkDevice, m_vertexBuffer, &memReq);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_device->GetPhysicalDevice(), &memProps);

    uint32_t memTypeIndex = UINT32_MAX;
    const VkMemoryPropertyFlags required = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReq.memoryTypeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & required) == required) {
            memTypeIndex = i;
            break;
        }
    }
    if (memTypeIndex == UINT32_MAX) return false;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &m_vertexMemory) != VK_SUCCESS) return false;
    vkBindBufferMemory(vkDevice, m_vertexBuffer, m_vertexMemory, 0);

    void* mapped = nullptr;
    vkMapMemory(vkDevice, m_vertexMemory, 0, size, 0, &mapped);
    std::memcpy(mapped, kFullscreenTriVerts, static_cast<size_t>(size));
    vkUnmapMemory(vkDevice, m_vertexMemory);

    return true;
}

VkShaderModule VulkanRenderer::CreateShaderModule(const std::vector<uint8_t>& spirv) {
    if (spirv.size() % 4 != 0) return VK_NULL_HANDLE;

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = spirv.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(spirv.data());

    VkShaderModule mod = VK_NULL_HANDLE;
    vkCreateShaderModule(m_device->GetDevice(), &ci, nullptr, &mod);
    return mod;
}

} // namespace ShaderLab
