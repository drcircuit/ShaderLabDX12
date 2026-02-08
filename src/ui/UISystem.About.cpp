#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UISystemAssets.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace ShaderLab {

void UISystem::ShowAboutWindow() {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (!ImGui::Begin("About ShaderLab", &m_showAbout, flags)) {
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float size = (std::max)(0.0f, (std::min)(avail.x, avail.y * 0.65f));

    const float baseLine = ImGui::GetTextLineHeight();
    const float headingScale = 1.35f;
    const float headingHeight = baseLine * headingScale;
    const float textBlockHeight = headingHeight + baseLine * 2.0f + baseLine * 0.8f;
    const float totalHeight = size + textBlockHeight;

    if (avail.y > totalHeight) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (avail.y - totalHeight) * 0.5f);
    }

    if (size > 32.0f) {
        m_aboutTargetWidth = static_cast<uint32_t>(size);
        m_aboutTargetHeight = static_cast<uint32_t>(size);
        ImGui::SetCursorPosX((std::max)(0.0f, (avail.x - size) * 0.5f));
        if (m_aboutSrvGpuHandle.ptr != 0) {
            ImGui::Image((ImTextureID)m_aboutSrvGpuHandle.ptr, ImVec2(size, size));
        } else {
            ImGui::Dummy(ImVec2(size, size));
        }
    }

    ImGui::Spacing();
    ImGui::SetWindowFontScale(headingScale);
    const char* heading = "SHADERLAB v. 1.0";
    ImVec2 headingSize = ImGui::CalcTextSize(heading);
    ImGui::SetCursorPosX((std::max)(0.0f, (avail.x - headingSize.x) * 0.5f));
    ImGui::TextColored(ImVec4(0.45f, 0.95f, 1.0f, 1.0f), "%s", heading);
    ImGui::SetWindowFontScale(1.0f);

    const char* line1 = "by Espen Sande-Larsen";
    ImVec2 line1Size = ImGui::CalcTextSize(line1);
    ImGui::SetCursorPosX((std::max)(0.0f, (avail.x - line1Size.x) * 0.5f));
    ImGui::Text("%s", line1);

    const char* line2 = "aka DrCiRCUiT";
    ImVec2 line2Size = ImGui::CalcTextSize(line2);
    ImGui::SetCursorPosX((std::max)(0.0f, (avail.x - line2Size.x) * 0.5f));
    ImGui::Text("%s", line2);

    ImGui::End();
}

void UISystem::EnsureAboutScene(uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) return;

    if (!m_aboutInitialized) {
        m_aboutScene.name = "AboutLogo";
        m_aboutScene.shaderCode = kAboutLogoShader;
        m_aboutScene.outputType = TextureType::Texture2D;
        m_aboutScene.bindings.clear();
        m_aboutScene.postFxChain.clear();
        m_aboutScene.postFxChain.emplace_back("Glitch A", kAboutGlitchPostFx);
        m_aboutScene.postFxChain.emplace_back("Glitch B", kAboutGlitchPostFx);
        m_aboutInitialized = true;
    }

    bool needsCreate = !m_aboutScene.texture;
    if (m_aboutScene.texture) {
        auto desc = m_aboutScene.texture->GetDesc();
        if (desc.Width != width || desc.Height != height) {
            needsCreate = true;
        }
    }

    if (needsCreate) {
        m_aboutScene.texture.Reset();
        m_aboutScene.srvHeap.Reset();
        m_aboutScene.textureValid = false;

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
            &clearValue, IID_PPV_ARGS(&m_aboutScene.texture));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 8;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_deviceRef->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_aboutScene.srvHeap));
    }

    EnsurePostFxResources(m_aboutScene, width, height);
}

void UISystem::RenderAboutLogo(ID3D12GraphicsCommandList* commandList) {
    if (!commandList || !m_previewRenderer || !m_deviceRef) return;
    if (m_aboutTargetWidth == 0 || m_aboutTargetHeight == 0) return;

    EnsureAboutScene(m_aboutTargetWidth, m_aboutTargetHeight);

    if (m_aboutScene.isDirty || !m_aboutScene.pipelineState) {
        std::vector<PreviewRenderer::TextureDecl> decls;
        std::vector<std::string> errors;
        auto pso = m_previewRenderer->CompileShader(m_aboutScene.shaderCode, decls, errors);
        if (!pso) return;
        m_aboutScene.pipelineState = pso;
        m_aboutScene.isDirty = false;
    }

    // Render base logo
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_deviceRef->GetDevice()->CreateRenderTargetView(m_aboutScene.texture.Get(), nullptr, rtvHandle);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_aboutScene.texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    if (m_aboutScene.srvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_aboutScene.srvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
    }

    m_previewRenderer->Render(
        commandList,
        m_aboutScene.pipelineState.Get(),
        m_aboutScene.texture.Get(),
        rtvHandle,
        m_aboutScene.srvHeap ? m_aboutScene.srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{},
        m_aboutTargetWidth,
        m_aboutTargetHeight,
        static_cast<float>(m_aboutTimeSeconds)
    );

    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commandList->ResourceBarrier(1, &barrier);

    // Apply glitch post FX twice
    ID3D12Resource* output = m_aboutScene.texture.Get();
    if (!m_aboutScene.postFxChain.empty()) {
        output = ApplyPostFxChain(commandList, m_aboutScene, m_aboutScene.postFxChain, output,
                                  m_aboutTargetWidth, m_aboutTargetHeight, m_aboutTimeSeconds, false);
    }

    if (m_srvHeap && output) {
        UINT descriptorSize = m_deviceRef->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += descriptorSize * kAboutSrvIndex;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_deviceRef->GetDevice()->CreateShaderResourceView(output, &srvDesc, srvHandle);

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += descriptorSize * kAboutSrvIndex;
        m_aboutSrvGpuHandle = gpuHandle;
    }
}

} // namespace ShaderLab
