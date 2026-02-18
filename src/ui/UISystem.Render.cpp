#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/UI/UISystemAssets.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace ShaderLab {

void UISystem::EnsureSceneTexture(int sceneIndex, uint32_t width, uint32_t height) {
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
        scene.texture.Reset();
        scene.srvHeap.Reset();
        scene.textureValid = false;

        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.DepthOrArraySize = (scene.outputType == TextureType::TextureCube) ? 6 : 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        memcpy(clearValue.Color, clearColor, sizeof(clearColor));

        m_deviceRef->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            &clearValue, IID_PPV_ARGS(&scene.texture));

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

void UISystem::EnsurePostFxResources(Scene& scene, uint32_t width, uint32_t height) {
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

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    memcpy(clearValue.Color, clearColor, sizeof(clearColor));

    m_deviceRef->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&scene.postFxTextureA));

    m_deviceRef->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&scene.postFxTextureB));

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

void UISystem::EnsurePostFxPreviewResources(uint32_t width, uint32_t height) {
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

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    memcpy(clearValue.Color, clearColor, sizeof(clearColor));

    m_deviceRef->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&m_postFxPreviewTextureA));

    m_deviceRef->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&m_postFxPreviewTextureB));

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

void UISystem::EnsurePostFxHistory(Scene::PostFXEffect& effect, uint32_t width, uint32_t height) {
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

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (int i = 0; i < kPostFxHistoryCount; ++i) {
        m_deviceRef->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr, IID_PPV_ARGS(&effect.historyTextures[i]));
    }
}

bool UISystem::CompilePostFxEffect(Scene::PostFXEffect& effect, std::vector<std::string>& outErrors) {
    outErrors.clear();
    if (!m_previewRenderer) return false;
    std::vector<PreviewRenderer::TextureDecl> decls = { {0, "Texture2D"} };
    auto pso = m_previewRenderer->CompileShader(effect.shaderCode, decls, outErrors, true);
    if (pso) {
        effect.pipelineState = pso;
        effect.compiledShaderBytes = m_previewRenderer->GetLastCompiledPixelShaderSize();
        effect.isDirty = false;
        effect.lastCompiledCode = effect.shaderCode;
        return true;
    }
    effect.pipelineState = nullptr;
    effect.compiledShaderBytes = 0;
    return false;
}

ID3D12Resource* UISystem::ApplyPostFxChain(ID3D12GraphicsCommandList* commandList,
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
        m_previewRenderer->Render(
            commandList,
            fx.pipelineState.Get(),
            currentOutput,
            rtvHandle,
            srvGpu,
            width, height,
            (float)timeSeconds
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

ID3D12Resource* UISystem::GetSceneFinalTexture(ID3D12GraphicsCommandList* commandList,
                                              int sceneIndex,
                                              uint32_t width, uint32_t height,
                                              double timeSeconds) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_scenes.size()) return nullptr;
    RenderScene(commandList, sceneIndex, width, height, timeSeconds);
    auto& scene = m_scenes[sceneIndex];
    if (!scene.texture) return nullptr;

    if (scene.postFxChain.empty()) return scene.texture.Get();
    return ApplyPostFxChain(commandList, scene, scene.postFxChain, scene.texture.Get(), width, height, timeSeconds, false);
}

void UISystem::RenderScene(ID3D12GraphicsCommandList* commandList, int sceneIndex, uint32_t width, uint32_t height, double time) {
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

         m_previewRenderer->Render(
            commandList,
            scene.pipelineState.Get(),
            scene.texture.Get(),
            rtvHandle,
            scene.srvHeap ? scene.srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{},
            width, height,
            static_cast<float>(time)
         );
    }

    // Barrier: RENDER_TARGET -> PS_RESOURCE
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commandList->ResourceBarrier(1, &barrier);

    scene.textureValid = true;
    m_renderStack.pop_back(); // Always pop at the end
}

namespace {
    std::string GetTransitionShader(TransitionType type) {
        std::string common = R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    uv.y = 1.0 - uv.y;
    float t = saturate(iTime);
    float4 colA = iChannel0.Sample(iSampler0, uv);
    float4 colB = iChannel1.Sample(iSampler1, uv);
)";
        switch(type) {
            case TransitionType::Crossfade: return common + R"(
    return lerp(colA, colB, t);
}
)";
            case TransitionType::DipToBlack: return common + R"(
    return (t < 0.5) ? lerp(colA, float4(0,0,0,1), t*2.0) : lerp(float4(0,0,0,1), colB, (t-0.5)*2.0);
}
)";
            case TransitionType::FadeOut: return common + R"(
    return lerp(colA, float4(0,0,0,1), t);
}
)";
            case TransitionType::FadeIn: return common + R"(
    return lerp(float4(0,0,0,1), colB, t);
}
)";
            case TransitionType::Glitch: return common + R"(
    float offset = iTime * 10.0;
    float noise = frac(sin(dot(float2(floor(uv.y * 20.0) + offset, offset), float2(12.9898, 78.233))) * 43758.5453);
    float disp = (noise - 0.5) * 0.1 * sin(t * 3.14159);
    float2 uv2 = uv + float2(disp, 0);
    colA = iChannel0.Sample(iSampler0, uv2);
    colB = iChannel1.Sample(iSampler1, uv2);
    return lerp(colA, colB, t);
}
)";
             case TransitionType::Pixelate: return common + R"(
    float p = sin(t * 3.14159);
    float n = 50.0 * (1.0 - p) + 1.0;
    float2 uvP = floor(uv * n) / n;
    colA = iChannel0.Sample(iSampler0, uvP);
    colB = iChannel1.Sample(iSampler1, uvP);
    return lerp(colA, colB, t);
}
)";
           default:
              return common + R"(
    return lerp(colA, colB, t);
}
)";
        }
    }
}

bool UISystem::RenderPreviewTexture(ID3D12GraphicsCommandList* commandList) {
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

        if (progress >= 1.0) {
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
             // We now have explicit types (FadeOut/FadeIn/DipToBlack), so no need for deduction logic.
             TransitionType effectiveType = m_currentTransitionType;

             if (!m_transitionPSO || m_compiledTransitionType != effectiveType) {
                std::vector<PreviewRenderer::TextureDecl> decls = {
                    {0, "Texture2D"}, {1, "Texture2D"}
                };
                std::string code = GetTransitionShader(effectiveType);
                std::vector<std::string> errs;
                m_transitionPSO = m_previewRenderer->CompileShader(code, decls, errs);
                m_compiledTransitionType = effectiveType;
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
                 m_previewRenderer->Render(
                    commandList,
                    m_transitionPSO.Get(),
                    m_previewTexture.Get(),
                    m_previewRtvHandle,
                    m_transitionSrvHeap->GetGPUDescriptorHandleForHeapStart(),
                    m_previewTextureWidth, m_previewTextureHeight,
                    (float)progress
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
