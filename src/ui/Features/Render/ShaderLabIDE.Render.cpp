#include "ShaderLab/UI/ShaderLabIDE.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/UI/UISystemAssets.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"
#include "ShaderLab/Graphics/Dx12ResourceService.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Core/CompilationService.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace ShaderLab {

namespace {
constexpr uint32_t kComputeHistorySlots = 8;
constexpr uint32_t kComputeDescriptorCount = 11; // t0 + t1..t8 + u0 + b0

struct ComputeDispatchParams {
    float param0;
    float param1;
    float param2;
    float param3;
    float time;
    float invWidth;
    float invHeight;
    uint32_t frame;
};

struct UiComputeSceneResources {
    ComPtr<ID3D12Resource> textureA;
    ComPtr<ID3D12Resource> textureB;
    uint32_t width = 0;
    uint32_t height = 0;
    ID3D12Device* ownerDevice = nullptr;
};

ComPtr<ID3D12RootSignature> g_uiComputeRootSignature;
ComPtr<ID3D12DescriptorHeap> g_uiComputeDescriptorHeap;
ComPtr<ID3D12Resource> g_uiComputeParamsBuffer;
uint8_t* g_uiComputeParamsMapped = nullptr;
ID3D12Device* g_uiComputeDevice = nullptr;
std::unordered_map<int, UiComputeSceneResources> g_uiComputeSceneResources;
std::unordered_map<Scene::ComputeEffect*, ID3D12Device*> g_uiComputePipelineDeviceMap;

void ResetUiComputeDeviceState() {
    if (g_uiComputeParamsBuffer && g_uiComputeParamsMapped) {
        g_uiComputeParamsBuffer->Unmap(0, nullptr);
    }

    g_uiComputeParamsMapped = nullptr;
    g_uiComputeParamsBuffer.Reset();
    g_uiComputeDescriptorHeap.Reset();
    g_uiComputeRootSignature.Reset();
    g_uiComputeSceneResources.clear();
    g_uiComputePipelineDeviceMap.clear();
    g_uiComputeDevice = nullptr;
}

UINT DescriptorStep(ID3D12Device* device) {
    return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

uint32_t Align256(uint32_t value) {
    return (value + 255u) & ~255u;
}

bool EnsureUiComputeRootSignature(Device* deviceRef) {
    if (!deviceRef) return false;
    ID3D12Device* device = deviceRef->GetDevice();
    if (g_uiComputeDevice && g_uiComputeDevice != device) {
        ResetUiComputeDeviceState();
    }
    if (!g_uiComputeDevice) {
        g_uiComputeDevice = device;
    }
    if (g_uiComputeRootSignature) return true;

    D3D12_DESCRIPTOR_RANGE inputRange = {};
    inputRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    inputRange.NumDescriptors = 1;
    inputRange.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE historyRange = {};
    historyRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    historyRange.NumDescriptors = kComputeHistorySlots;
    historyRange.BaseShaderRegister = 1;

    D3D12_DESCRIPTOR_RANGE outputRange = {};
    outputRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputRange.NumDescriptors = 1;
    outputRange.BaseShaderRegister = 0;

    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange.NumDescriptors = 1;
    cbvRange.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER rootParams[4] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = &inputRange;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &historyRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].DescriptorTable.pDescriptorRanges = &outputRange;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[3].DescriptorTable.pDescriptorRanges = &cbvRange;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = _countof(rootParams);
    rootDesc.pParameters = rootParams;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           serialized.GetAddressOf(), errors.GetAddressOf()))) {
        return false;
    }

    return SUCCEEDED(deviceRef->GetDevice()->CreateRootSignature(
        0,
        serialized->GetBufferPointer(),
        serialized->GetBufferSize(),
        IID_PPV_ARGS(g_uiComputeRootSignature.ReleaseAndGetAddressOf())));
}

bool EnsureUiComputeDispatchResources(Device* deviceRef) {
    if (!deviceRef) return false;
    ID3D12Device* device = deviceRef->GetDevice();
    if (g_uiComputeDevice && g_uiComputeDevice != device) {
        ResetUiComputeDeviceState();
    }
    if (!g_uiComputeDevice) {
        g_uiComputeDevice = device;
    }

    if (!g_uiComputeDescriptorHeap) {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = kComputeDescriptorCount;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(g_uiComputeDescriptorHeap.ReleaseAndGetAddressOf())))) {
            return false;
        }
    }

    if (!g_uiComputeParamsBuffer) {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Width = Align256(static_cast<uint32_t>(sizeof(ComputeDispatchParams)));
        bufferDesc.Height = 1;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(g_uiComputeParamsBuffer.ReleaseAndGetAddressOf())))) {
            return false;
        }

        if (FAILED(g_uiComputeParamsBuffer->Map(0, nullptr, reinterpret_cast<void**>(&g_uiComputeParamsMapped)))) {
            g_uiComputeParamsBuffer.Reset();
            g_uiComputeParamsMapped = nullptr;
            return false;
        }
    }

    return true;
}

void ComputeShaderMusicalTiming(const PreviewTransport& transport,
                                float& outIBeat,
                                float& outIBar,
                                float& outFBeat,
                                float& outFBarBeat,
                                float& outFBarBeat16) {
    constexpr float kBeatsPerBar = 4.0f;
    constexpr float kSixteenthPerBeat = 4.0f;
    const float beatsPerSecond = transport.bpm / 60.0f;
    float exactBeat = 0.0f;
    if (beatsPerSecond > 0.0f) {
        exactBeat = static_cast<float>(transport.timeSeconds * static_cast<double>(beatsPerSecond));
        if (exactBeat < 0.0f) {
            exactBeat = 0.0f;
        }
    }
    const float beat = std::floor(exactBeat);
    const float bar = std::floor(beat / kBeatsPerBar);
    const float beatInBar = exactBeat - std::floor(exactBeat / kBeatsPerBar) * kBeatsPerBar;
    float barBeat16 = std::floor(beatInBar * kSixteenthPerBeat);
    if (barBeat16 < 0.0f) {
        barBeat16 = 0.0f;
    }
    if (barBeat16 > 15.0f) {
        barBeat16 = 15.0f;
    }

    outIBeat = beat;
    outIBar = bar;
    outFBeat = exactBeat;
    outFBarBeat = beatInBar;
    outFBarBeat16 = barBeat16;
}
}

void ShaderLabIDE::Render(ID3D12GraphicsCommandList* commandList) {
    // Only attempt preview rendering if we have all required components initialized
    bool previewRendered = false;
    if (m_previewRenderer && m_swapchainRef && m_deviceRef) {
        if (m_showAbout) {
            RenderAboutLogo(commandList);
        }
        previewRendered = RenderPreviewTexture(commandList);

        // If preview was rendered, restore render target and viewport for ImGui
        if (previewRendered) {
            // Reset render target to backbuffer after preview rendering
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapchainRef->GetCurrentRTV();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            // Restore viewport and scissor rect to full window size
            D3D12_VIEWPORT viewport{};
            viewport.Width = static_cast<float>(m_swapchainRef->GetWidth());
            viewport.Height = static_cast<float>(m_swapchainRef->GetHeight());
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            commandList->RSSetViewports(1, &viewport);

            D3D12_RECT scissor{};
            scissor.right = static_cast<LONG>(m_swapchainRef->GetWidth());
            scissor.bottom = static_cast<LONG>(m_swapchainRef->GetHeight());
            commandList->RSSetScissorRects(1, &scissor);
        }
    }

    // Set descriptor heap for ImGui (must be set before rendering)
    if (m_srvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void ShaderLabIDE::EnsureSceneTexture(int sceneIndex, uint32_t width, uint32_t height) {
    if (sceneIndex < 0 || sceneIndex >= m_scenes.size()) return;
    auto& scene = m_scenes[sceneIndex];
    if (width == 0 || height == 0) return;

    bool needsCreate = !scene.texture;
    if (scene.texture) {
        auto desc = scene.texture->GetDesc();
        if (desc.Width != width || desc.Height != height) {
            needsCreate = true;
        }
    }

    // Safety check: ensure heap is size 8
    if (scene.srvHeap && scene.srvHeap->GetDesc().NumDescriptors != 8) {
         scene.srvHeap.Reset();
         // If heap is invalid, we MUST continue to create it
         needsCreate = true;
         scene.texture.Reset();
         scene.textureValid = false;
    }

    if (needsCreate) {
        Dx12ResourceService resourceService(m_deviceRef->GetDevice());
        scene.texture.Reset();
        scene.srvHeap.Reset();
        scene.textureValid = false;

        TextureAllocationRequest textureRequest{};
        textureRequest.width = width;
        textureRequest.height = height;
        textureRequest.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureRequest.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        textureRequest.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (!resourceService.AllocateTexture2D(textureRequest, scene.texture)) {
            return;
        }

        // Create descriptor heap for this scene's inputs (4 textures table for the shader to use)
        // NOT the SRV for the texture itself (we'll just use transient handles or copy descriptors when needed)
        // Wait, different concept: When rendering THIS scene, what does it bind?
        // It binds a table of 4 textures. We need a GPU descriptor heap for that table.

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 8;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_deviceRef->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&scene.srvHeap));
    }
}

// Helper to check for cycles before rendering
bool HasCycle(const std::vector<Scene>& scenes, int current, int target, const std::vector<int>& stack) {
    if (current == target) return true;
    for (int s : stack) { if (s == current) return true; } // Recursion check in stack

    const auto& scene = scenes[current];
    for (const auto& binding : scene.bindings) {
        if (binding.enabled && binding.sourceSceneIndex != -1) {
            // If checking if 'target' is reachable from 'current'
             if (binding.sourceSceneIndex == target) return true;
             // Deep check? No, this function is "is target upstream of current"?
        }
    }
    return false;
}

void ShaderLabIDE::EnsurePostFxResources(Scene& scene, uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) return;

    bool needsCreate = !scene.postFxTextureA || !scene.postFxTextureB;
    if (scene.postFxTextureA) {
        auto desc = scene.postFxTextureA->GetDesc();
        if (desc.Width != width || desc.Height != height) needsCreate = true;
    }

    if (!needsCreate) return;

    scene.postFxTextureA.Reset();
    scene.postFxTextureB.Reset();
    scene.postFxSrvHeap.Reset();
    scene.postFxRtvHeap.Reset();
    scene.postFxValid = false;

    Dx12ResourceService resourceService(m_deviceRef->GetDevice());
    TextureAllocationRequest textureRequest{};
    textureRequest.width = width;
    textureRequest.height = height;
    textureRequest.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureRequest.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    textureRequest.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (!resourceService.AllocateTexture2D(textureRequest, scene.postFxTextureA)) {
        return;
    }
    if (!resourceService.AllocateTexture2D(textureRequest, scene.postFxTextureB)) {
        scene.postFxTextureA.Reset();
        return;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 8 * kMaxPostFxChain;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&scene.postFxSrvHeap));

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&scene.postFxRtvHeap));
}

void ShaderLabIDE::EnsurePostFxPreviewResources(uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) return;

    bool needsCreate = !m_postFxPreviewTextureA || !m_postFxPreviewTextureB;
    if (m_postFxPreviewTextureA) {
        auto desc = m_postFxPreviewTextureA->GetDesc();
        if (desc.Width != width || desc.Height != height) needsCreate = true;
    }
    if (!needsCreate) return;

    m_postFxPreviewTextureA.Reset();
    m_postFxPreviewTextureB.Reset();
    m_postFxPreviewSrvHeap.Reset();
    m_postFxPreviewRtvHeap.Reset();

    Dx12ResourceService resourceService(m_deviceRef->GetDevice());
    TextureAllocationRequest textureRequest{};
    textureRequest.width = width;
    textureRequest.height = height;
    textureRequest.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureRequest.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    textureRequest.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    if (!resourceService.AllocateTexture2D(textureRequest, m_postFxPreviewTextureA)) {
        return;
    }
    if (!resourceService.AllocateTexture2D(textureRequest, m_postFxPreviewTextureB)) {
        m_postFxPreviewTextureA.Reset();
        return;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 8 * kMaxPostFxChain;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_postFxPreviewSrvHeap));

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_postFxPreviewRtvHeap));

    m_postFxPreviewWidth = width;
    m_postFxPreviewHeight = height;
}

void ShaderLabIDE::EnsurePostFxHistory(Scene::PostFXEffect& effect, uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) return;

    bool needsCreate = (int)effect.historyTextures.size() != kPostFxHistoryCount;
    if (!needsCreate) {
        auto desc = effect.historyTextures[0]->GetDesc();
        if (desc.Width != width || desc.Height != height) needsCreate = true;
    }

    if (!needsCreate) return;

    effect.historyTextures.clear();
    effect.historyTextures.resize(kPostFxHistoryCount);
    effect.historyIndex = 0;
    effect.historyInitialized = false;

    Dx12ResourceService resourceService(m_deviceRef->GetDevice());
    TextureAllocationRequest textureRequest{};
    textureRequest.width = width;
    textureRequest.height = height;
    textureRequest.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureRequest.flags = D3D12_RESOURCE_FLAG_NONE;
    textureRequest.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    for (int i = 0; i < kPostFxHistoryCount; ++i) {
        if (!resourceService.AllocateTexture2D(textureRequest, effect.historyTextures[i])) {
            effect.historyTextures.clear();
            effect.historyInitialized = false;
            return;
        }
    }
}

bool ShaderLabIDE::CompileComputePipeline(Scene::ComputeEffect& effect) {
    if (!m_compilationService || !m_deviceRef) return false;
    if (!EnsureUiComputeRootSignature(m_deviceRef)) return false;
    ID3D12Device* device = m_deviceRef->GetDevice();

    const std::string entryPoint = effect.entryPoint.empty() ? "main" : effect.entryPoint;
    const ShaderCompileResult compileResult = m_compilationService->CompileFromSource(
        effect.shaderCode,
        entryPoint,
        "cs_6_0",
        L"compute.hlsl",
        ShaderCompileMode::Build,
        {});

    if (!compileResult.success) {
        effect.pipelineState.Reset();
        effect.compiledShaderBytes = 0;
        return false;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = g_uiComputeRootSignature.Get();
    desc.CS = { compileResult.bytecode.data(), compileResult.bytecode.size() };

    ComPtr<ID3D12PipelineState> pipeline;
    if (FAILED(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(pipeline.GetAddressOf())))) {
        effect.pipelineState.Reset();
        effect.compiledShaderBytes = 0;
        return false;
    }

    effect.pipelineState = pipeline;
    effect.compiledShaderBytes = compileResult.bytecode.size();
    effect.isDirty = false;
    effect.lastCompiledCode = effect.shaderCode;
    g_uiComputePipelineDeviceMap[&effect] = device;
    return true;
}

void ShaderLabIDE::EnsureComputeHistory(Scene::ComputeEffect& effect, uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) return;

    const int historyCount = (std::max)(0, (std::min)(effect.historyCount, static_cast<int>(kComputeHistorySlots)));
    if (historyCount <= 0) {
        effect.historyTextures.clear();
        effect.historyInitialized = false;
        effect.historyIndex = 0;
        return;
    }

    bool needsCreate = static_cast<int>(effect.historyTextures.size()) != historyCount;
    if (!needsCreate && !effect.historyTextures.empty()) {
        auto desc = effect.historyTextures.front()->GetDesc();
        needsCreate = desc.Width != width || desc.Height != height;
    }

    if (!needsCreate) return;

    effect.historyTextures.clear();
    effect.historyTextures.resize(static_cast<size_t>(historyCount));
    effect.historyInitialized = false;
    effect.historyIndex = 0;

    Dx12ResourceService resourceService(m_deviceRef->GetDevice());
    TextureAllocationRequest req{};
    req.width = width;
    req.height = height;
    req.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    req.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    req.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    for (int i = 0; i < historyCount; ++i) {
        if (!resourceService.AllocateTexture2D(req, effect.historyTextures[static_cast<size_t>(i)])) {
            effect.historyTextures.clear();
            effect.historyInitialized = false;
            effect.historyIndex = 0;
            return;
        }
    }
}

ID3D12Resource* ShaderLabIDE::ApplyComputeEffectChain(ID3D12GraphicsCommandList* commandList,
                                                      int sceneIndex,
                                                      std::vector<Scene::ComputeEffect>& chain,
                                                      ID3D12Resource* inputTexture,
                                                      uint32_t width,
                                                      uint32_t height,
                                                      double timeSeconds) {
    if (!commandList || !m_deviceRef || !inputTexture || chain.empty()) return inputTexture;
    if (!EnsureUiComputeRootSignature(m_deviceRef) || !EnsureUiComputeDispatchResources(m_deviceRef)) {
        return inputTexture;
    }

    bool anyEnabled = false;
    for (const auto& fx : chain) {
        if (fx.enabled) {
            anyEnabled = true;
            break;
        }
    }
    if (!anyEnabled) return inputTexture;

    ID3D12Device* device = m_deviceRef->GetDevice();
    auto& resources = g_uiComputeSceneResources[sceneIndex];
    const bool recreate = !resources.textureA || !resources.textureB || resources.width != width || resources.height != height || resources.ownerDevice != device;
    if (recreate) {
        resources = {};
        Dx12ResourceService resourceService(m_deviceRef->GetDevice());
        TextureAllocationRequest req{};
        req.width = width;
        req.height = height;
        req.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        req.flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        req.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (!resourceService.AllocateTexture2D(req, resources.textureA) ||
            !resourceService.AllocateTexture2D(req, resources.textureB)) {
            resources = {};
            return inputTexture;
        }
        resources.width = width;
        resources.height = height;
        resources.ownerDevice = device;
    }

    const UINT step = DescriptorStep(device);
    const auto heapCpu = g_uiComputeDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    const auto heapGpu = g_uiComputeDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

    ID3D12Resource* currentInput = inputTexture;
    ID3D12Resource* outputA = resources.textureA.Get();
    ID3D12Resource* outputB = resources.textureB.Get();
    ID3D12Resource* currentOutput = outputA;

    for (auto& fx : chain) {
        if (!fx.enabled) continue;
        auto fxDeviceIt = g_uiComputePipelineDeviceMap.find(&fx);
        const bool pipelineDeviceMismatch = (fxDeviceIt == g_uiComputePipelineDeviceMap.end()) || (fxDeviceIt->second != device);
        if (pipelineDeviceMismatch) {
            fx.pipelineState.Reset();
            fx.isDirty = true;
            fx.historyIndex = 0;
            fx.historyInitialized = false;
            fx.historyTextures.clear();
        }
        if (fx.isDirty || !fx.pipelineState) {
            if (!CompileComputePipeline(fx)) {
                continue;
            }
        }

        EnsureComputeHistory(fx, width, height);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

        D3D12_CPU_DESCRIPTOR_HANDLE inputCpu = heapCpu;
        device->CreateShaderResourceView(currentInput, &srvDesc, inputCpu);

        D3D12_CPU_DESCRIPTOR_HANDLE fallbackHistoryCpu = heapCpu;
        fallbackHistoryCpu.ptr += static_cast<SIZE_T>(step) * 1;
        device->CreateShaderResourceView(currentInput, &srvDesc, fallbackHistoryCpu);

        for (uint32_t i = 0; i < kComputeHistorySlots; ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE histCpu = heapCpu;
            histCpu.ptr += static_cast<SIZE_T>(step) * (1 + i);

            ID3D12Resource* historyRes = nullptr;
            const int historyCount = static_cast<int>(fx.historyTextures.size());
            if (historyCount > 0) {
                int readIndex = fx.historyIndex - static_cast<int>(i);
                while (readIndex < 0) readIndex += historyCount;
                readIndex %= historyCount;
                historyRes = fx.historyTextures[static_cast<size_t>(readIndex)].Get();
            }
            if (!historyRes) {
                if (i > 0) {
                    device->CopyDescriptorsSimple(1, histCpu, fallbackHistoryCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                    continue;
                }
                historyRes = currentInput;
            }
            device->CreateShaderResourceView(historyRes, &srvDesc, histCpu);
        }

        D3D12_CPU_DESCRIPTOR_HANDLE outputCpu = heapCpu;
        outputCpu.ptr += static_cast<SIZE_T>(step) * 9;
        device->CreateUnorderedAccessView(currentOutput, nullptr, &uavDesc, outputCpu);

        if (!g_uiComputeParamsMapped || !g_uiComputeParamsBuffer) {
            return currentInput;
        }

        ComputeDispatchParams params{};
        params.param0 = fx.param0;
        params.param1 = fx.param1;
        params.param2 = fx.param2;
        params.param3 = fx.param3;
        params.time = static_cast<float>(timeSeconds);
        params.invWidth = width > 0 ? 1.0f / static_cast<float>(width) : 0.0f;
        params.invHeight = height > 0 ? 1.0f / static_cast<float>(height) : 0.0f;
        params.frame = static_cast<uint32_t>(m_transport.timeSeconds * 60.0);
        std::memcpy(g_uiComputeParamsMapped, &params, sizeof(params));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = g_uiComputeParamsBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = Align256(static_cast<uint32_t>(sizeof(ComputeDispatchParams)));
        D3D12_CPU_DESCRIPTOR_HANDLE cbvCpu = heapCpu;
        cbvCpu.ptr += static_cast<SIZE_T>(step) * 10;
        device->CreateConstantBufferView(&cbvDesc, cbvCpu);

        D3D12_RESOURCE_BARRIER beginBarriers[2] = {};
        beginBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        beginBarriers[0].Transition.pResource = currentInput;
        beginBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        beginBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        beginBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        beginBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        beginBarriers[1].Transition.pResource = currentOutput;
        beginBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        beginBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        beginBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(2, beginBarriers);

        ID3D12DescriptorHeap* heaps[] = { g_uiComputeDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
        commandList->SetComputeRootSignature(g_uiComputeRootSignature.Get());
        commandList->SetPipelineState(fx.pipelineState.Get());

        D3D12_GPU_DESCRIPTOR_HANDLE inputGpu = heapGpu;
        D3D12_GPU_DESCRIPTOR_HANDLE historyGpu = heapGpu;
        historyGpu.ptr += static_cast<UINT64>(step) * 1;
        D3D12_GPU_DESCRIPTOR_HANDLE outputGpu = heapGpu;
        outputGpu.ptr += static_cast<UINT64>(step) * 9;
        D3D12_GPU_DESCRIPTOR_HANDLE cbvGpu = heapGpu;
        cbvGpu.ptr += static_cast<UINT64>(step) * 10;

        commandList->SetComputeRootDescriptorTable(0, inputGpu);
        commandList->SetComputeRootDescriptorTable(1, historyGpu);
        commandList->SetComputeRootDescriptorTable(2, outputGpu);
        commandList->SetComputeRootDescriptorTable(3, cbvGpu);

        const uint32_t tgx = (std::max)(1u, fx.threadGroupX);
        const uint32_t tgy = (std::max)(1u, fx.threadGroupY);
        const uint32_t tgz = (std::max)(1u, fx.threadGroupZ);
        const uint32_t groupsX = (width + tgx - 1u) / tgx;
        const uint32_t groupsY = (height + tgy - 1u) / tgy;
        const uint32_t groupsZ = (1u + tgz - 1u) / tgz;
        commandList->Dispatch(groupsX, groupsY, groupsZ);

        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = currentOutput;
        commandList->ResourceBarrier(1, &uavBarrier);

        D3D12_RESOURCE_BARRIER endBarriers[2] = {};
        endBarriers[0] = beginBarriers[0];
        endBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        endBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        endBarriers[1] = beginBarriers[1];
        endBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        endBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(2, endBarriers);

        if (!fx.historyTextures.empty()) {
            const int historyCount = static_cast<int>(fx.historyTextures.size());
            const int writeIndex = (fx.historyIndex + 1) % historyCount;

            D3D12_RESOURCE_BARRIER preCopy[2] = {};
            preCopy[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preCopy[0].Transition.pResource = currentOutput;
            preCopy[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            preCopy[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            preCopy[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            preCopy[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preCopy[1].Transition.pResource = fx.historyTextures[static_cast<size_t>(writeIndex)].Get();
            preCopy[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            preCopy[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            preCopy[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(2, preCopy);

            commandList->CopyResource(fx.historyTextures[static_cast<size_t>(writeIndex)].Get(), currentOutput);

            preCopy[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            preCopy[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            preCopy[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            preCopy[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            commandList->ResourceBarrier(2, preCopy);

            fx.historyIndex = writeIndex;
            fx.historyInitialized = true;
        }

        currentInput = currentOutput;
        currentOutput = (currentOutput == outputA) ? outputB : outputA;
    }

    return currentInput;
}

bool ShaderLabIDE::CompilePostFxEffect(Scene::PostFXEffect& effect, std::vector<std::string>& outErrors) {
    outErrors.clear();
    if (!m_previewRenderer || !m_compilationService) return false;

    std::vector<CompilationTextureBinding> bindings = { {0, "Texture2D"} };
    const ShaderCompileResult compileResult = m_compilationService->CompilePreviewShader(
        effect.shaderCode,
        bindings,
        true,
        "main",
        L"postfx.hlsl",
        ShaderCompileMode::Live);

    for (const auto& diagnostic : compileResult.diagnostics) {
        outErrors.push_back(diagnostic.message);
    }

    ComPtr<ID3D12PipelineState> pso;
    if (compileResult.success) {
        pso = m_previewRenderer->CreatePSOFromBytecode(compileResult.bytecode);
        if (!pso) {
            outErrors.push_back("Failed to create graphics pipeline state from compiled post-fx shader.");
        }
    }

    if (pso) {
        effect.pipelineState = pso;
        effect.compiledShaderBytes = compileResult.bytecode.size();
        effect.isDirty = false;
        effect.lastCompiledCode = effect.shaderCode;
        return true;
    }
    effect.pipelineState = nullptr;
    effect.compiledShaderBytes = 0;
    return false;
}

ID3D12Resource* ShaderLabIDE::ApplyPostFxChain(ID3D12GraphicsCommandList* commandList,
                                          Scene& scene,
                                          std::vector<Scene::PostFXEffect>& chain,
                                          ID3D12Resource* inputTexture,
                                          uint32_t width, uint32_t height,
                                          double timeSeconds,
                                          bool usePreviewResources) {
    if (!commandList || !inputTexture) return inputTexture;

    bool anyEnabled = false;
    for (const auto& fx : chain) {
        if (fx.enabled) { anyEnabled = true; break; }
    }
    if (!anyEnabled) return inputTexture;

    ID3D12Resource* ping = nullptr;
    ID3D12Resource* pong = nullptr;
    ID3D12DescriptorHeap* srvHeap = nullptr;
    ID3D12DescriptorHeap* rtvHeap = nullptr;

    if (usePreviewResources) {
        EnsurePostFxPreviewResources(width, height);
        ping = m_postFxPreviewTextureA.Get();
        pong = m_postFxPreviewTextureB.Get();
        srvHeap = m_postFxPreviewSrvHeap.Get();
        rtvHeap = m_postFxPreviewRtvHeap.Get();
    } else {
        EnsurePostFxResources(scene, width, height);
        ping = scene.postFxTextureA.Get();
        pong = scene.postFxTextureB.Get();
        srvHeap = scene.postFxSrvHeap.Get();
        rtvHeap = scene.postFxRtvHeap.Get();
    }

    if (!ping || !pong || !srvHeap || !rtvHeap) return inputTexture;

    auto device = m_deviceRef->GetDevice();
    auto handleStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto startHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();

    auto bindInput = [&](ID3D12Resource* src, Scene::PostFXEffect& fx, int baseSlot) {
        D3D12_CPU_DESCRIPTOR_HANDLE dest = startHandle;
        dest.ptr += baseSlot * handleStep;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        device->CreateShaderResourceView(src, &srvDesc, dest);

        for (int i = 1; i <= kPostFxHistoryCount; ++i) {
            int historySlot = i;
            int historyIndex = fx.historyIndex - (i - 1);
            while (historyIndex < 0) historyIndex += kPostFxHistoryCount;
            ID3D12Resource* historyRes = nullptr;
            if (!fx.historyTextures.empty()) {
                historyRes = fx.historyTextures[historyIndex].Get();
            }

            D3D12_CPU_DESCRIPTOR_HANDLE histDest = startHandle;
            histDest.ptr += (baseSlot + historySlot) * handleStep;
            if (historyRes) {
                device->CreateShaderResourceView(historyRes, &srvDesc, histDest);
            } else if (m_dummySrvHeap) {
                device->CopyDescriptorsSimple(1, histDest, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
        }

        for (int i = kPostFxHistoryCount + 1; i < 8; ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE dummyDest = startHandle;
            dummyDest.ptr += (baseSlot + i) * handleStep;
            if (m_dummySrvHeap) {
                device->CopyDescriptorsSimple(1, dummyDest, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
        }
    };

    ID3D12Resource* currentInput = inputTexture;
    ID3D12Resource* currentOutput = ping;

    int passIndex = 0;
    for (auto& fx : chain) {
        if (!fx.enabled) continue;
        if (!fx.pipelineState) continue;
        if (passIndex >= kMaxPostFxChain) break;

        EnsurePostFxHistory(fx, width, height);
        if (fx.historyTextures.empty()) continue;

        if (!fx.historyInitialized) {
            for (int i = 0; i < kPostFxHistoryCount; ++i) {
                D3D12_RESOURCE_BARRIER initBarriers[2] = {};
                initBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                initBarriers[0].Transition.pResource = currentInput;
                initBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                initBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                initBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                initBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                initBarriers[1].Transition.pResource = fx.historyTextures[i].Get();
                initBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                initBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                initBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                commandList->ResourceBarrier(2, initBarriers);

                commandList->CopyResource(fx.historyTextures[i].Get(), currentInput);

                std::swap(initBarriers[0].Transition.StateBefore, initBarriers[0].Transition.StateAfter);
                std::swap(initBarriers[1].Transition.StateBefore, initBarriers[1].Transition.StateAfter);
                commandList->ResourceBarrier(2, initBarriers);
            }
            fx.historyInitialized = true;
            fx.historyIndex = 0;
        }

        // Transition output to render target
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = currentOutput;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        // Bind input SRV
        int baseSlot = passIndex * 8;
        bindInput(currentInput, fx, baseSlot);
        ID3D12DescriptorHeap* heaps[] = { srvHeap };
        commandList->SetDescriptorHeaps(1, heaps);

        // RTV
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_deviceRef->GetDevice()->CreateRenderTargetView(currentOutput, nullptr, rtvHandle);

        // Render pass
        D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = srvHeap->GetGPUDescriptorHandleForHeapStart();
        srvGpu.ptr += baseSlot * handleStep;
        float iBeat = 0.0f;
        float iBar = 0.0f;
        float fBeat = 0.0f;
        float fBarBeat = 0.0f;
        float fBarBeat16 = 0.0f;
        ComputeShaderMusicalTiming(m_transport, iBeat, iBar, fBeat, fBarBeat, fBarBeat16);
        m_previewRenderer->Render(
            commandList,
            fx.pipelineState.Get(),
            currentOutput,
            rtvHandle,
            srvGpu,
            width, height,
            (float)timeSeconds,
            iBeat,
            iBar,
            fBarBeat16,
            fBeat,
            fBarBeat
        );

        // Transition output back to SRV
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commandList->ResourceBarrier(1, &barrier);

        int writeIndex = (fx.historyIndex + 1) % kPostFxHistoryCount;
        D3D12_RESOURCE_BARRIER historyBarriers[2] = {};
        historyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        historyBarriers[0].Transition.pResource = currentOutput;
        historyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        historyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        historyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        historyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        historyBarriers[1].Transition.pResource = fx.historyTextures[writeIndex].Get();
        historyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        historyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        historyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(2, historyBarriers);

        commandList->CopyResource(fx.historyTextures[writeIndex].Get(), currentOutput);

        std::swap(historyBarriers[0].Transition.StateBefore, historyBarriers[0].Transition.StateAfter);
        std::swap(historyBarriers[1].Transition.StateBefore, historyBarriers[1].Transition.StateAfter);
        commandList->ResourceBarrier(2, historyBarriers);
        fx.historyIndex = writeIndex;

        // Ping-pong swap
        currentInput = currentOutput;
        currentOutput = (currentOutput == ping) ? pong : ping;
        passIndex++;
    }

    if (!usePreviewResources) {
        scene.postFxValid = true;
    }
    return currentInput;
}

ID3D12Resource* ShaderLabIDE::GetSceneFinalTexture(ID3D12GraphicsCommandList* commandList,
                                              int sceneIndex,
                                              uint32_t width, uint32_t height,
                                              double timeSeconds) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_scenes.size()) return nullptr;
    RenderScene(commandList, sceneIndex, width, height, timeSeconds);
    auto& scene = m_scenes[sceneIndex];
    if (!scene.texture) return nullptr;

    ID3D12Resource* output = scene.texture.Get();

    if (!scene.postFxChain.empty()) {
        output = ApplyPostFxChain(commandList, scene, scene.postFxChain, output, width, height, timeSeconds, false);
    }

    if (!scene.computeEffectChain.empty()) {
        output = ApplyComputeEffectChain(commandList, sceneIndex, scene.computeEffectChain, output, width, height, timeSeconds);
    }

    return output;
}

void ShaderLabIDE::RenderScene(ID3D12GraphicsCommandList* commandList, int sceneIndex, uint32_t width, uint32_t height, double time) {
    // Basic cycle protection using a stack
    for(int s : m_renderStack) {
        if(s == sceneIndex) return; // Cycle detected
    }
    m_renderStack.push_back(sceneIndex);

    EnsureSceneTexture(sceneIndex, width, height);
    if (sceneIndex < 0 || sceneIndex >= (int)m_scenes.size()) {
        m_renderStack.pop_back();
        return;
    }
    auto& scene = m_scenes[sceneIndex];
    if (!scene.texture) {
        // We must pop the stack if we return early!
        m_renderStack.pop_back();
        return;
    }

    // 1. Process dependencies first (render input textures)
    for (const auto& binding : scene.bindings) {
        if (binding.enabled && binding.sourceSceneIndex != -1 && binding.sourceSceneIndex != sceneIndex) {
             RenderScene(commandList, binding.sourceSceneIndex, width, height, time);
        }
    }

    // 2. Setup Descriptor Table for THIS scene's inputs
    // We need to copy descriptors from source scenes into this scene's heap
    // Or create new views pointing to source resources.
    if (scene.srvHeap) {
        auto device = m_deviceRef->GetDevice();
        auto handleStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto startHandle = scene.srvHeap->GetCPUDescriptorHandleForHeapStart();

        for (int i=0; i<8; ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE dest = startHandle;
            dest.ptr += i * handleStep;

            bool bound = false;
            // Find binding for slot i
            for(const auto& b : scene.bindings) {
                if (b.channelIndex == i && b.enabled) {
                    if (b.bindingType == BindingType::Scene) {
                        if (b.sourceSceneIndex != -1 && b.sourceSceneIndex != sceneIndex) {
                             // Validate index BEFORE access to prevent crash
                             if (b.sourceSceneIndex < 0 || b.sourceSceneIndex >= (int)m_scenes.size()) {
                                 continue;
                             }
                             // Get source texture
                             auto& srcScene = m_scenes[b.sourceSceneIndex];
                             bool compatible = false;
                             if(srcScene.texture) {
                                D3D12_RESOURCE_DESC desc = srcScene.texture->GetDesc();

                                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                                srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

                                // Strict type checking
                                if (b.type == TextureType::TextureCube) {
                                    if (desc.DepthOrArraySize == 6 && desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                                        srvDesc.TextureCube.MipLevels = 1;
                                        srvDesc.TextureCube.MostDetailedMip = 0;
                                        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
                                        compatible = true;
                                    }
                                } else if (b.type == TextureType::Texture3D) {
                                     if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
                                         srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                                         srvDesc.Texture3D.MipLevels = 1;
                                         srvDesc.Texture3D.MostDetailedMip = 0;
                                         srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
                                         compatible = true;
                                     }
                                } else {
                                    // 2D
                                    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D && desc.DepthOrArraySize == 1) {
                                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                                        srvDesc.Texture2D.MipLevels = 1;
                                        srvDesc.Texture2D.MostDetailedMip = 0;
                                        srvDesc.Texture2D.PlaneSlice = 0;
                                        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
                                        compatible = true;
                                    }
                                }

                                if (compatible) {
                                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                                    device->CreateShaderResourceView(srcScene.texture.Get(), &srvDesc, dest);
                                    bound = true;
                                }
                             }
                        }
                    } else if (b.bindingType == BindingType::File) {
                        if (b.fileTextureValid && b.textureResource) {
                            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                            srvDesc.Texture2D.MipLevels = 1;
                            srvDesc.Texture2D.MostDetailedMip = 0;
                            srvDesc.Texture2D.PlaneSlice = 0;
                            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

                            device->CreateShaderResourceView(b.textureResource.Get(), &srvDesc, dest);
                            bound = true;
                        }
                    }
                }
            }

            if (!bound) {
                // Bind dummy texture appropriate for the channel type
                // We need to look at what the shader expects. That's determined by iterating scene bindings.
                // But here we are iterating slot i=0..7.

                // Find what type slot i expects
                TextureType type = TextureType::Texture2D;
                for(const auto& b : scene.bindings) {
                    if (b.channelIndex == i) {
                        type = b.type;
                        break;
                    }
                }

                if (type == TextureType::TextureCube && m_dummySrvHeapCube) {
                    device->CopyDescriptorsSimple(1, dest, m_dummySrvHeapCube->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                } else if (type == TextureType::Texture3D && m_dummySrvHeap3D) {
                    device->CopyDescriptorsSimple(1, dest, m_dummySrvHeap3D->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                } else if (m_dummySrvHeap) {
                    device->CopyDescriptorsSimple(1, dest, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
            }
        }
    }

    if (scene.isDirty || !scene.pipelineState) {
        if (!CompileScene(sceneIndex)) {
            m_renderStack.pop_back();
            return;
        }
    }

    // 3. Render THIS scene
    // Create temporary RTV for this scene texture
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_deviceRef->GetDevice()->CreateRenderTargetView(scene.texture.Get(), nullptr, rtvHandle);

    // Barrier: PS_RESOURCE -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = scene.texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // We need to compile the specific shader for this scene if it is not the active one
    // But Wait! PreviewRenderer holds a single pipeline state object currently.
    // Switching shaders per draw calls means we need to recompile or cache PSOs.
    // For now, let's assume we simply can't efficiently render DIFFERENT shaders in one frame easily
    // without refactoring PreviewRenderer to be a Library or having the Scene hold the PSO.

    // MAJOR REFACTOR NEEDED if we want true multi-pass.
    // OPTION: Hack for now -> Set pipeline state if it matches "Active Scene"
    // BUT user wants to use OTHER scenes as textures. That implies they run.

    // Compromise: We only render dependencies if we can.
    // Since we don't store PSOs per scene yet, we can't legitimately render the source scene if it has different code.
    // We will leave the "Render the dependency" part empty for now, effectively binding stale or black textures,
    // UNLESS we quickly move compile logic to Scene struct.
    // Let's assume for now we only bind the texture resource, but we don't update it (it stays black or whatever it was).
    // The prompt asks to "implement a way to add textures into buffers".

    // Updated Plan: We will execute the render for the Active Scene.
    // The inputs (channels) will be bound.
    // If those inputs are other scenes, they should ideally be rendered too.
    // We will skip full recursive rendering for this step and focus on just BINDING the resources.
    // (If we want recursive rendering, we need PSOs per scene).

    if (scene.pipelineState) {
         if (scene.srvHeap) {
             ID3D12DescriptorHeap* heaps[] = { scene.srvHeap.Get() };
             commandList->SetDescriptorHeaps(1, heaps);
         }

            float iBeat = 0.0f;
            float iBar = 0.0f;
            float fBeat = 0.0f;
            float fBarBeat = 0.0f;
            float fBarBeat16 = 0.0f;
            ComputeShaderMusicalTiming(m_transport, iBeat, iBar, fBeat, fBarBeat, fBarBeat16);
         m_previewRenderer->Render(
            commandList,
            scene.pipelineState.Get(),
            scene.texture.Get(),
            rtvHandle,
            scene.srvHeap ? scene.srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{},
            width, height,
            static_cast<float>(time),
            iBeat,
            iBar,
                fBarBeat16,
                fBeat,
                fBarBeat
         );
    }

    // Barrier: RENDER_TARGET -> PS_RESOURCE
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commandList->ResourceBarrier(1, &barrier);

    scene.textureValid = true;
    m_renderStack.pop_back(); // Always pop at the end
}

bool ShaderLabIDE::RenderPreviewTexture(ID3D12GraphicsCommandList* commandList) {
    bool hasActiveScene = (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size());

    // Check Resources
    if (!m_previewRenderer || !m_previewTexture) {
        return false;
    }

    // Clear render stack
    m_renderStack.clear();

    // --- Post FX Mode Preview (Draft Chain) ---
    if (m_currentMode == UIMode::PostFX) {
        int sourceIndex = m_postFxSourceSceneIndex;
        if (sourceIndex < 0 || sourceIndex >= (int)m_scenes.size()) {
            // Clear to black if no source
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_previewTexture.Get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commandList->ResourceBarrier(1, &barrier);

            float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
            commandList->ClearRenderTargetView(m_previewRtvHandle, clearColor, 0, nullptr);

            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            commandList->ResourceBarrier(1, &barrier);
            return true;
        }

        RenderScene(commandList, sourceIndex, m_previewTextureWidth, m_previewTextureHeight, m_transport.timeSeconds);
        ID3D12Resource* input = m_scenes[sourceIndex].texture.Get();
        if (!input) return false;

        ID3D12Resource* finalTex = input;
        if (!m_postFxDraftChain.empty()) {
            finalTex = ApplyPostFxChain(commandList, m_scenes[sourceIndex], m_postFxDraftChain, input,
                                        m_previewTextureWidth, m_previewTextureHeight, m_transport.timeSeconds, true);
        }

        if (!m_computeEffectDraftChain.empty()) {
            finalTex = ApplyComputeEffectChain(commandList,
                                               sourceIndex,
                                               m_computeEffectDraftChain,
                                               finalTex,
                                               m_previewTextureWidth,
                                               m_previewTextureHeight,
                                               m_transport.timeSeconds);
        }

        // Copy final to preview
        D3D12_RESOURCE_BARRIER preCopyBarriers[2] = {};
        preCopyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        preCopyBarriers[0].Transition.pResource = m_previewTexture.Get();
        preCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        preCopyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        preCopyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        preCopyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        preCopyBarriers[1].Transition.pResource = finalTex;
        preCopyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        preCopyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        preCopyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        commandList->ResourceBarrier(2, preCopyBarriers);
        commandList->CopyResource(m_previewTexture.Get(), finalTex);

        D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {};
        postCopyBarriers[0] = preCopyBarriers[0];
        postCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        postCopyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        postCopyBarriers[1] = preCopyBarriers[1];
        postCopyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        postCopyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        commandList->ResourceBarrier(2, postCopyBarriers);
        return true;
    }

    // --- Transition Logic ---
    // Allow transition rendering even if active scene is invalid (e.g. Hold/Black)
    if (m_transitionActive && m_currentMode != UIMode::Scene) {
        float beatsPerSec = m_transport.bpm / 60.0f;
        double exactBeat = m_transport.timeSeconds * beatsPerSec;
        double progress = (exactBeat - m_transitionStartBeat) / m_transitionDurationBeats;

        const bool isPlaying = (m_transport.state == TransportState::Playing);
        if (isPlaying && progress >= 1.0) {
            m_transitionActive = false;
            // Apply pending scene switch
            if (m_pendingActiveScene != -2) {
                SetActiveScene(m_pendingActiveScene);
                m_activeSceneStartBeat = m_transitionToStartBeat;
                m_activeSceneOffset = (m_pendingActiveScene >= 0) ? m_transitionToOffset : 0.0f;
                m_pendingActiveScene = -2;
            }
        } else {
             if (progress < 0.0) progress = 0.0;
             if (progress > 1.0) progress = 1.0;

             // Ensure Transition PSO
             const std::string effectiveStem = m_currentTransitionStem;

             if (!m_transitionPSO || m_compiledTransitionStem != effectiveStem) {
                std::vector<PreviewRenderer::TextureDecl> decls = {
                    {0, "Texture2D"}, {1, "Texture2D"}
                };
                std::string code = GetEditorTransitionShaderSourceByStem(effectiveStem);
                std::vector<std::string> errs;
                m_transitionPSO = m_previewRenderer->CompileShader(code, decls, errs);
                m_compiledTransitionStem = effectiveStem;
            }

            bool validIndices = true; // Indices are always "valid" (handled by Bind returning dummy)

            // Validation: Ensure indices are within bounds or -1
            if (m_transitionFromIndex < 0 || m_transitionFromIndex >= (int)m_scenes.size()) m_transitionFromIndex = -1;
            if (m_transitionToIndex < 0 || m_transitionToIndex >= (int)m_scenes.size()) m_transitionToIndex = -1;

            if (m_transitionPSO && validIndices) {
                ID3D12Resource* fromTex = nullptr;
                ID3D12Resource* toTex = nullptr;
                const double fromTime = SceneTimeSeconds(exactBeat, m_transitionFromStartBeat, m_transitionFromOffset, m_transport.bpm);
                const double toTime = SceneTimeSeconds(exactBeat, m_transitionToStartBeat, m_transitionToOffset, m_transport.bpm);
                if (m_transitionFromIndex != -1)
                    fromTex = GetSceneFinalTexture(commandList, m_transitionFromIndex, m_previewTextureWidth, m_previewTextureHeight,
                        fromTime);
                if (m_transitionToIndex != -1)
                    toTex = GetSceneFinalTexture(commandList, m_transitionToIndex, m_previewTextureWidth, m_previewTextureHeight,
                        toTime);

                if (!fromTex) fromTex = m_dummyTexture.Get();
                if (!toTex) toTex = m_dummyTexture.Get();

                // Determine textures to bind
                 // Create Heap if needed
                 if (!m_transitionSrvHeap) {
                    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
                    heapDesc.NumDescriptors = 8;
                    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                    m_deviceRef->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_transitionSrvHeap));
                 }

                 auto device = m_deviceRef->GetDevice();
                 auto handleStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                 auto startCpu = m_transitionSrvHeap->GetCPUDescriptorHandleForHeapStart();

                 auto Bind = [&](ID3D12Resource* res, int slot) {
                    D3D12_CPU_DESCRIPTOR_HANDLE dest = startCpu;
                    dest.ptr += slot * handleStep;
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    device->CreateShaderResourceView(res, &srvDesc, dest);
                 };

                 Bind(fromTex, 0); // t0
                 Bind(toTex, 1);   // t1

                 // Resource Barrier: Transition m_previewTexture to RENDER_TARGET
                 D3D12_RESOURCE_BARRIER barrier = {};
                 barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                 barrier.Transition.pResource = m_previewTexture.Get();
                 barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                 barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                 barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                 commandList->ResourceBarrier(1, &barrier);

                 // Set the descriptor heap for transition resources
                 ID3D12DescriptorHeap* heaps[] = { m_transitionSrvHeap.Get() };
                 commandList->SetDescriptorHeaps(1, heaps);

                 // Render Transition
                      float iBeat = 0.0f;
                      float iBar = 0.0f;
                      float fBeat = 0.0f;
                      float fBarBeat = 0.0f;
                      float fBarBeat16 = 0.0f;
                      ComputeShaderMusicalTiming(m_transport, iBeat, iBar, fBeat, fBarBeat, fBarBeat16);
                 m_previewRenderer->Render(
                    commandList,
                    m_transitionPSO.Get(),
                    m_previewTexture.Get(),
                    m_previewRtvHandle,
                    m_transitionSrvHeap->GetGPUDescriptorHandleForHeapStart(),
                    m_previewTextureWidth, m_previewTextureHeight,
                          (float)progress,
                          iBeat,
                          iBar,
                          fBarBeat16,
                          fBeat,
                          fBarBeat
                 );

                 // Restore Barrier
                 std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
                 commandList->ResourceBarrier(1, &barrier);

                 return true;
            }
        }
    }

    // --- Normal Render (Active Scene) ---
    // Update status in case transition finished this frame
    hasActiveScene = (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size());

    if (!hasActiveScene) {
        // If no active scene (e.g. Hold/Black) and no transition, clear to black
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_previewTexture.Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
        commandList->ClearRenderTargetView(m_previewRtvHandle, clearColor, 0, nullptr);

        // Restore to Shader Resource
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commandList->ResourceBarrier(1, &barrier);
        return true;
    }

        const double beatsPerSec = m_transport.bpm / 60.0f;
        const double exactBeat = m_transport.timeSeconds * beatsPerSec;
        const double activeTime = SceneTimeSeconds(exactBeat, m_activeSceneStartBeat, m_activeSceneOffset, m_transport.bpm);
        ID3D12Resource* finalTex = GetSceneFinalTexture(commandList, m_activeSceneIndex, m_previewTextureWidth, m_previewTextureHeight,
            activeTime);

        // Copy active scene texture to preview texture for display
        if (finalTex) {
         // Transition m_previewTexture to Copy Dest
         D3D12_RESOURCE_BARRIER preCopyBarriers[2] = {};
         preCopyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
         preCopyBarriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
         preCopyBarriers[0].Transition.pResource = m_previewTexture.Get();
         preCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
         preCopyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
         preCopyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

         // Transition Scene Texture to Copy Source
         preCopyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
         preCopyBarriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            preCopyBarriers[1].Transition.pResource = finalTex;
         preCopyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
         preCopyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
         preCopyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

         commandList->ResourceBarrier(2, preCopyBarriers);

            commandList->CopyResource(m_previewTexture.Get(), finalTex);

         // Restore
         D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {};
         postCopyBarriers[0] = preCopyBarriers[0];
         postCopyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
         postCopyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

         postCopyBarriers[1] = preCopyBarriers[1];
         postCopyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
         postCopyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

         commandList->ResourceBarrier(2, postCopyBarriers);
    }

    return true;
}

} // namespace ShaderLab
