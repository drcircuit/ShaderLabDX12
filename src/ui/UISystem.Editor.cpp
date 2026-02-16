#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/UI/UISystemAssets.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

namespace ShaderLab {

namespace {
float GetAspectRatioValue(AspectRatio ratio) {
    switch (ratio) {
        case AspectRatio::Ratio_16_10:
            return 16.0f / 10.0f;
        case AspectRatio::Ratio_4_3:
            return 4.0f / 3.0f;
        case AspectRatio::Ratio_16_9:
        default:
            return 16.0f / 9.0f;
    }
}

ImVec2 FitAspect(ImVec2 avail, float aspect) {
    if (aspect <= 0.0f) {
        return avail;
    }

    float width = avail.x;
    float height = width / aspect;
    if (height > avail.y) {
        height = avail.y;
        width = height * aspect;
    }

    width = (std::max)(1.0f, width);
    height = (std::max)(1.0f, height);
    return ImVec2(width, height);
}
}

void UISystem::ShowModeWindows() {
    if (m_previewFullscreen) {
        ShowFullscreenPreview();
        return;
    }

    auto ShowPreviewWindow = [&]() {
        if (!ImGui::Begin("Preview")) {
            ImGui::End();
            return;
        }

        int aspectIndex = 0;
        switch (m_aspectRatio) {
            case AspectRatio::Ratio_16_10: aspectIndex = 1; break;
            case AspectRatio::Ratio_4_3: aspectIndex = 2; break;
            case AspectRatio::Ratio_16_9:
            default: aspectIndex = 0; break;
        }

        const char* aspectLabels[] = { "16:9", "16:10", "4:3" };
        ImGui::Text("Aspect");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##AspectRatio", &aspectIndex, aspectLabels, 3)) {
            m_aspectRatio = (aspectIndex == 1) ? AspectRatio::Ratio_16_10 : (aspectIndex == 2) ? AspectRatio::Ratio_4_3 : AspectRatio::Ratio_16_9;
        }

        ImGui::Separator();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 drawSize = FitAspect(avail, GetAspectRatioValue(m_aspectRatio));
        CreatePreviewTexture(static_cast<uint32_t>(drawSize.x), static_cast<uint32_t>(drawSize.y));

        ImVec2 cursorStart = ImGui::GetCursorPos();
        ImVec2 screenStart = ImGui::GetCursorScreenPos();
        ImVec2 screenEnd = ImVec2(screenStart.x + avail.x, screenStart.y + avail.y);
        ImGui::GetWindowDrawList()->AddRectFilled(screenStart, screenEnd, IM_COL32(10, 10, 12, 255));

        ImGui::SetCursorPos(ImVec2(cursorStart.x + (avail.x - drawSize.x) * 0.5f, cursorStart.y + (avail.y - drawSize.y) * 0.5f));

        if (m_previewTexture && m_previewSrvGpuHandle.ptr != 0) {
            ImGui::Image((ImTextureID)m_previewSrvGpuHandle.ptr, drawSize);
        } else {
            ImGui::Dummy(drawSize);
        }

        ImGui::SetCursorPos(cursorStart);
        ImGui::Dummy(avail);

        ImGui::End();
    };

    // Always create all windows (for docking), but only show content for active mode

    // Demo mode windows
    if (m_currentMode == UIMode::Demo) {
        ShowDemoPlaylist();
        ShowAudioLibrary();

        ShowSceneList();

        if (ImGui::Begin("Demo: Runtime Log")) {
            if (ImGui::Button("Clear")) {
                m_demoLog.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &m_demoLogAutoScroll);
            ImGui::Separator();

            if (m_transitionActive) {
                const double beatsPerSec = m_transport.bpm / 60.0f;
                const double exactBeat = m_transport.timeSeconds * beatsPerSec;
                const double fromTime = SceneTimeSeconds(exactBeat, m_transitionFromStartBeat, m_transitionFromOffset, m_transport.bpm);
                const double toTime = SceneTimeSeconds(exactBeat, m_transitionToStartBeat, m_transitionToOffset, m_transport.bpm);
                ImGui::Text("Transition: %s", TransitionName(m_currentTransitionType));
                ImGui::TextUnformatted("A:");
                ImGui::SameLine();
                PushNumericFont();
                ImGui::Text("%d", m_transitionFromIndex);
                PopNumericFont();
                ImGui::SameLine();
                ImGui::TextUnformatted("time");
                ImGui::SameLine();
                PushNumericFont();
                ImGui::Text("%.2f", fromTime);
                PopNumericFont();

                ImGui::TextUnformatted("B:");
                ImGui::SameLine();
                PushNumericFont();
                ImGui::Text("%d", m_transitionToIndex);
                PopNumericFont();
                ImGui::SameLine();
                ImGui::TextUnformatted("time");
                ImGui::SameLine();
                PushNumericFont();
                ImGui::Text("%.2f", toTime);
                PopNumericFont();
                ImGui::Separator();
            }

            if (ImGui::BeginChild("RuntimeLog", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
                for (const auto& line : m_demoLog) {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (m_demoLogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();

        ShowPreviewWindow();
    }

    // Scene mode windows
    if (m_currentMode == UIMode::Scene) {
        ShowSceneList();
        ShowSnippetBin();
        if (ImGui::Begin("Scene: Post Stack")) {
            ImGui::Text("Post Processing Stack");
            ImGui::Separator();

            if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                auto& scene = m_scenes[m_activeSceneIndex];
                ImGui::Text("Active Scene: %s", scene.name.c_str());
                ImGui::Separator();

                if (scene.postFxChain.empty()) {
                    ImGui::TextDisabled("No post effects assigned.");
                } else {
                    for (size_t i = 0; i < scene.postFxChain.size(); ++i) {
                        const auto& fx = scene.postFxChain[i];
                        ImGui::BulletText("%s%s", fx.enabled ? "" : "(Disabled) ", fx.name.c_str());
                    }
                }

            } else {
                ImGui::TextDisabled("No active scene selected.");
            }
        }
        ImGui::End();

        if (ImGui::Begin("Scene: Textures & Channels")) {
            ImGui::Text("Textures & Channel Routing");
            ImGui::Separator();

            if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                auto& scene = m_scenes[m_activeSceneIndex];

                auto findNextChannel = [&]() -> int {
                    bool used[8] = {};
                    for (const auto& b : scene.bindings) {
                        if (b.channelIndex >= 0 && b.channelIndex < 8) {
                            used[b.channelIndex] = true;
                        }
                    }
                    for (int c = 0; c < 8; ++c) {
                        if (!used[c]) return c;
                    }
                    return (int)scene.bindings.size() % 8;
                };

                if (ImGui::Button("+ Add File Texture")) {
                    TextureBinding binding;
                    binding.enabled = true;
                    binding.bindingType = BindingType::File;
                    binding.channelIndex = findNextChannel();
                    binding.type = TextureType::Texture2D;

                    OPENFILENAMEA ofn = {};
                    char szFile[260] = {};
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0All Files\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                    if (GetOpenFileNameA(&ofn)) {
                        binding.filePath = szFile;
                        LoadTextureFromFile(binding.filePath, binding.textureResource);
                        binding.fileTextureValid = binding.textureResource != nullptr;
                    }

                    scene.bindings.push_back(binding);
                }
                ImGui::SameLine();
                if (ImGui::Button("+ Add Scene Sampler")) {
                    TextureBinding binding;
                    binding.enabled = true;
                    binding.bindingType = BindingType::Scene;
                    binding.channelIndex = findNextChannel();
                    binding.type = TextureType::Texture2D;

                    int defaultScene = -1;
                    for (int i = 0; i < (int)m_scenes.size(); ++i) {
                        if (i != m_activeSceneIndex) {
                            defaultScene = i;
                            break;
                        }
                    }
                    binding.sourceSceneIndex = defaultScene;
                    scene.bindings.push_back(binding);
                }

                ImGui::Separator();

                ImGui::Text("Active Scene: %s", scene.name.c_str());
                ImGui::Separator();

                // Prepare for thumbnails (use main SRV heap)
                UINT handleStep = 0;
                D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {0};
                D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = {0};
                int thumbnailIdx = 2; // Start after system descriptors

                if (m_deviceRef && m_srvHeap) {
                     handleStep = m_deviceRef->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                     cpuStart = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
                     gpuStart = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
                }

                // Grid Layout
                float panelWidth = ImGui::GetContentRegionAvail().x;
                float tileWidth = 340.0f;
                int columnCount = (std::max)(1, (int)(panelWidth / tileWidth));

                if (ImGui::BeginTable("TextureBindings", columnCount, ImGuiTableFlags_SizingFixedFit)) {
                    int bindingToRemove = -1;
                    for(int i=0; i<(int)scene.bindings.size(); ++i) {
                        ImGui::TableNextColumn();
                        ImGui::PushID(i);
                        auto& binding = scene.bindings[i];

                        // Tile Frame
                        ImGui::BeginGroup();

                        // Thumbnail Logic
                        ImTextureID texID = (ImTextureID)0;
                        if (binding.enabled && m_deviceRef && m_srvHeap) {
                            ID3D12Resource* res = nullptr;
                            if (binding.bindingType == BindingType::File) {
                                res = binding.textureResource.Get();
                            } else if (binding.bindingType == BindingType::Scene && binding.sourceSceneIndex != -1) {
                                if (binding.sourceSceneIndex >= 0 && binding.sourceSceneIndex < (int)m_scenes.size()) {
                                    res = m_scenes[binding.sourceSceneIndex].texture.Get();
                                }
                            }

                            if (res) {
                                // Create transient SRV in main heap
                                D3D12_CPU_DESCRIPTOR_HANDLE dest = cpuStart;
                                dest.ptr += thumbnailIdx * handleStep;

                                D3D12_RESOURCE_DESC desc = res->GetDesc();
                                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                                srvDesc.Format = desc.Format;
                                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                                if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
                                    if (desc.DepthOrArraySize == 6) {
                                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                                        srvDesc.TextureCube.MipLevels = 1;
                                    } else {
                                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                                        srvDesc.Texture2D.MipLevels = 1;
                                    }
                                } else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
                                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                                    srvDesc.Texture3D.MipLevels = 1;
                                }

                                m_deviceRef->GetDevice()->CreateShaderResourceView(res, &srvDesc, dest);
                                texID = (ImTextureID)(gpuStart.ptr + thumbnailIdx * handleStep);
                                thumbnailIdx++;
                            }
                        }

                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
                        ImGui::BeginChild("Tile", ImVec2(tileWidth - 8, 220), true, ImGuiWindowFlags_NoScrollbar);

                        if (texID) {
                            ImGui::Image(texID, ImVec2(128, 128));
                        } else {
                            ImGui::Dummy(ImVec2(128, 128));
                        }
                        ImGui::SameLine();

                        ImGui::BeginGroup();
                        ImGui::TextUnformatted("Channel");
                        ImGui::SameLine();
                        PushNumericFont();
                        ImGui::Text("%d", binding.channelIndex);
                        PopNumericFont();
                        ImGui::TextDisabled("%s", binding.enabled ? "Enabled" : "Disabled");

                        ImGui::Separator();
                        ImGui::Text("Type: %s",
                                    binding.type == TextureType::TextureCube ? "Cube" :
                                    binding.type == TextureType::Texture3D ? "3D" : "2D");

                        if (binding.bindingType == BindingType::Scene) {
                            ImGui::TextUnformatted("Source: Scene");
                            ImGui::SameLine();
                            PushNumericFont();
                            ImGui::Text("%d", binding.sourceSceneIndex);
                            PopNumericFont();
                        } else if (binding.bindingType == BindingType::File) {
                            ImGui::Text("Source: File");
                        }
                        ImGui::EndGroup();

                        ImGui::EndChild();
                        ImGui::PopStyleVar();

                        if (ImGui::BeginPopupContextItem("BindingMenu")) {
                            ImGui::Checkbox("Enabled", &binding.enabled);

                            int channel = binding.channelIndex;
                            if (ImGui::SliderInt("Channel", &channel, 0, 7)) {
                                binding.channelIndex = channel;
                            }

                            const char* typeNames[] = { "2D", "Cube", "3D" };
                            int typeIndex = (binding.type == TextureType::TextureCube) ? 1 : (binding.type == TextureType::Texture3D ? 2 : 0);
                            if (ImGui::Combo("Texture Type", &typeIndex, typeNames, 3)) {
                                binding.type = (typeIndex == 1) ? TextureType::TextureCube : (typeIndex == 2 ? TextureType::Texture3D : TextureType::Texture2D);
                            }

                            const char* bindTypes[] = { "Scene", "File" };
                            int bindTypeIndex = (binding.bindingType == BindingType::Scene) ? 0 : 1;
                            if (ImGui::Combo("Binding Type", &bindTypeIndex, bindTypes, 2)) {
                                binding.bindingType = (bindTypeIndex == 0) ? BindingType::Scene : BindingType::File;
                            }

                            if (binding.bindingType == BindingType::Scene) {
                                std::vector<const char*> sceneNames;
                                sceneNames.reserve(m_scenes.size() + 1);
                                sceneNames.push_back("(None)");
                                for (const auto& s : m_scenes) {
                                    sceneNames.push_back(s.name.c_str());
                                }

                                int sceneIndex = binding.sourceSceneIndex >= 0 ? binding.sourceSceneIndex + 1 : 0;
                                if (ImGui::Combo("Source Scene", &sceneIndex, sceneNames.data(), (int)sceneNames.size())) {
                                    binding.sourceSceneIndex = sceneIndex - 1;
                                }
                            } else {
                                char pathBuf[260] = {};
                                strncpy_s(pathBuf, binding.filePath.c_str(), _TRUNCATE);
                                if (ImGui::InputText("File Path", pathBuf, sizeof(pathBuf))) {
                                    binding.filePath = pathBuf;
                                    if (!binding.filePath.empty()) {
                                        LoadTextureFromFile(binding.filePath, binding.textureResource);
                                        binding.fileTextureValid = binding.textureResource != nullptr;
                                    }
                                }
                                if (ImGui::Button("Browse")) {
                                    OPENFILENAMEA ofn = {};
                                    char szFile[260] = {};
                                    ofn.lStructSize = sizeof(ofn);
                                    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
                                    ofn.lpstrFile = szFile;
                                    ofn.nMaxFile = sizeof(szFile);
                                    ofn.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.tga;*.bmp\0All Files\0*.*\0";
                                    ofn.nFilterIndex = 1;
                                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                                    if (GetOpenFileNameA(&ofn)) {
                                        binding.filePath = szFile;
                                        LoadTextureFromFile(binding.filePath, binding.textureResource);
                                        binding.fileTextureValid = binding.textureResource != nullptr;
                                    }
                                }
                            }

                            if (ImGui::MenuItem("Remove")) {
                                bindingToRemove = i;
                            }
                            ImGui::EndPopup();
                        }

                        ImGui::EndGroup();
                        ImGui::PopID();
                    }
                    ImGui::EndTable();

                    if (bindingToRemove >= 0 && bindingToRemove < (int)scene.bindings.size()) {
                        scene.bindings.erase(scene.bindings.begin() + bindingToRemove);
                    }
                }
            } else {
                ImGui::TextDisabled("No active scene selected.");
            }
        }
        ImGui::End();

        ShowShaderEditor();
        ShowDiagnostics();
        ShowPreviewWindow();
    }

    // Post FX mode windows
    if (m_currentMode == UIMode::PostFX) {
        static int selectedPresetIndex = 0;

        if (ImGui::Begin("FX: Library")) {
            ImGui::Text("Presets");
            ImGui::Separator();

            if (ImGui::BeginListBox("##PostFxPresets")) {
                for (int i = 0; i < (int)kPostFxPresetCount; ++i) {
                    bool isSelected = (i == selectedPresetIndex);
                    if (ImGui::Selectable(kPostFxPresets[i].name, isSelected)) {
                        selectedPresetIndex = i;
                    }
                }
                ImGui::EndListBox();
            }

            if (ImGui::Button("Add Preset")) {
                if (selectedPresetIndex >= 0 && selectedPresetIndex < (int)kPostFxPresetCount) {
                    const auto& preset = kPostFxPresets[selectedPresetIndex];
                    m_postFxDraftChain.emplace_back(preset.name, preset.code);
                    m_postFxSelectedIndex = (int)m_postFxDraftChain.size() - 1;
                    SyncPostFxEditorToSelection();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Empty")) {
                m_postFxDraftChain.emplace_back("New FX", "float4 main(float2 fragCoord, float2 iResolution, float iTime) {\n    float2 uv = fragCoord / iResolution;\n    return iChannel0.Sample(iSampler0, uv);\n}\n");
                m_postFxSelectedIndex = (int)m_postFxDraftChain.size() - 1;
                SyncPostFxEditorToSelection();
            }
        }
        ImGui::End();

        if (ImGui::Begin("FX: Source")) {
            std::vector<const char*> sceneNames;
            sceneNames.reserve(m_scenes.size() + 1);
            sceneNames.push_back("(None)");
            for (const auto& s : m_scenes) {
                sceneNames.push_back(s.name.c_str());
            }

            int sourceIndex = m_postFxSourceSceneIndex >= 0 ? m_postFxSourceSceneIndex + 1 : 0;
            if (ImGui::Combo("Source Scene", &sourceIndex, sceneNames.data(), (int)sceneNames.size())) {
                int newIndex = sourceIndex - 1;
                if (newIndex != m_postFxSourceSceneIndex) {
                    m_postFxSourceSceneIndex = newIndex;
                    if (m_postFxSourceSceneIndex >= 0 && m_postFxSourceSceneIndex < (int)m_scenes.size()) {
                        m_postFxDraftChain = m_scenes[m_postFxSourceSceneIndex].postFxChain;
                    } else {
                        m_postFxDraftChain.clear();
                    }
                    m_postFxSelectedIndex = m_postFxDraftChain.empty() ? -1 : 0;
                    SyncPostFxEditorToSelection();
                }
            }

            if (m_postFxSourceSceneIndex >= 0 && m_postFxSourceSceneIndex < (int)m_scenes.size()) {
                if (ImGui::Button("Apply Draft To Scene")) {
                    m_scenes[m_postFxSourceSceneIndex].postFxChain = m_postFxDraftChain;
                }
            }
        }
        ImGui::End();

        if (ImGui::Begin("FX: Chain")) {
            if (m_postFxDraftChain.empty()) {
                ImGui::TextDisabled("No post effects in chain.");
            } else {
                for (int i = 0; i < (int)m_postFxDraftChain.size(); ++i) {
                    auto& fx = m_postFxDraftChain[i];
                    ImGui::PushID(i);
                    ImGui::Checkbox("##Enabled", &fx.enabled);
                    ImGui::SameLine();
                    bool isSelected = (i == m_postFxSelectedIndex);
                    if (ImGui::Selectable(fx.name.c_str(), isSelected)) {
                        m_postFxSelectedIndex = i;
                        SyncPostFxEditorToSelection();
                    }
                    ImGui::PopID();
                }
            }

            if (m_postFxSelectedIndex >= 0 && m_postFxSelectedIndex < (int)m_postFxDraftChain.size()) {
                ImGui::Separator();
                if (ImGui::Button("Move Up") && m_postFxSelectedIndex > 0) {
                    std::swap(m_postFxDraftChain[m_postFxSelectedIndex], m_postFxDraftChain[m_postFxSelectedIndex - 1]);
                    m_postFxSelectedIndex -= 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Move Down") && m_postFxSelectedIndex + 1 < (int)m_postFxDraftChain.size()) {
                    std::swap(m_postFxDraftChain[m_postFxSelectedIndex], m_postFxDraftChain[m_postFxSelectedIndex + 1]);
                    m_postFxSelectedIndex += 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    m_postFxDraftChain.erase(m_postFxDraftChain.begin() + m_postFxSelectedIndex);
                    if (m_postFxSelectedIndex >= (int)m_postFxDraftChain.size()) {
                        m_postFxSelectedIndex = (int)m_postFxDraftChain.size() - 1;
                    }
                    SyncPostFxEditorToSelection();
                }
            }
        }
        ImGui::End();

        ShowShaderEditor();
        ShowDiagnostics();
        ShowPreviewWindow();
    }
}

void UISystem::ShowFullscreenPreview() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (!ImGui::Begin("Preview Fullscreen", nullptr, flags)) {
        ImGui::End();
        return;
    }

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 drawSize = FitAspect(avail, GetAspectRatioValue(m_aspectRatio));
    CreatePreviewTexture(static_cast<uint32_t>(drawSize.x), static_cast<uint32_t>(drawSize.y));

    bool hasShader = false;
    if (m_currentMode == UIMode::Demo) {
        hasShader = (m_previewTexture != nullptr);
    } else if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
        hasShader = (m_scenes[m_activeSceneIndex].pipelineState != nullptr);
    }

    ImVec2 cursorStart = ImGui::GetCursorPos();
    ImVec2 screenStart = ImGui::GetCursorScreenPos();
    ImVec2 screenEnd = ImVec2(screenStart.x + avail.x, screenStart.y + avail.y);
    ImGui::GetWindowDrawList()->AddRectFilled(screenStart, screenEnd, IM_COL32(10, 10, 12, 255));
    ImGui::SetCursorPos(ImVec2(cursorStart.x + (avail.x - drawSize.x) * 0.5f, cursorStart.y + (avail.y - drawSize.y) * 0.5f));

    if (m_previewTexture && m_previewRenderer && hasShader) {
        ImGui::Image((ImTextureID)m_previewSrvGpuHandle.ptr, drawSize);
    } else {
        ImGui::Dummy(drawSize);
    }

    ImGui::SetCursorPos(cursorStart);
    ImGui::Dummy(avail);

    ImGui::End();
}

void UISystem::ShowSceneList() {
    const char* windowName = (m_currentMode == UIMode::Demo) ? "Demo: Scene Library" : "Scene: Library";
    if (ImGui::Begin(windowName)) {
        ImGui::Text("Scenes");
        ImGui::Separator();

        // Add scene button
        if (ImGui::Button("+ Add Scene")) {
            char nameBuf[64];
            snprintf(nameBuf, sizeof(nameBuf), "Scene %d", (int)m_scenes.size() + 1);

            // Use Hello World shader as template
            const char* templateShader = R"(// ShaderToy-style Hello World
// fragCoord: pixel coordinates (e.g., 0 to 1920x1080)
// iResolution: viewport size (e.g., float2(1920, 1080))
// iTime: time in seconds

float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    // Normalize pixel coordinates (0 to 1)
    float2 uv = fragCoord / iResolution;

    // Create colorful sine wave pattern
    float3 color;
    color.r = 0.5 + 0.5 * sin(uv.x * 10.0 + iTime);
    color.g = 0.5 + 0.5 * sin(uv.y * 10.0 + iTime * 1.3);
    color.b = 0.5 + 0.5 * sin((uv.x + uv.y) * 5.0 + iTime * 0.7);

    return float4(color, 1.0);
}

void UISystem::ShowSnippetBin() {
    if (!ImGui::Begin("Scene: Snippets")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Reusable Snippets");
    ImGui::Separator();

    if (ImGui::Button("+ Save Current As Snippet")) {
        ShaderSnippet snippet;
        snippet.name = "Snippet " + std::to_string(m_nextSnippetId++);
        snippet.code = m_shaderState.text;
        if (!snippet.code.empty()) {
            m_snippets.push_back(std::move(snippet));
            m_selectedSnippetIndex = (int)m_snippets.size() - 1;
            SaveGlobalSnippets();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("+ Add Empty Snippet")) {
        ShaderSnippet snippet;
        snippet.name = "Snippet " + std::to_string(m_nextSnippetId++);
        snippet.code = "float4 SnippetFunc(float2 uv, float t) {\n    return float4(uv, sin(t), 1.0);\n}\n";
        m_snippets.push_back(std::move(snippet));
        m_selectedSnippetIndex = (int)m_snippets.size() - 1;
        SaveGlobalSnippets();
    }

    ImGui::Separator();

    if (m_snippets.empty()) {
        ImGui::TextDisabled("No snippets yet.");
        ImGui::TextDisabled("Create one from current shader text.");
        ImGui::End();
        return;
    }

    if (m_selectedSnippetIndex < 0 || m_selectedSnippetIndex >= (int)m_snippets.size()) {
        m_selectedSnippetIndex = 0;
    }

    if (ImGui::BeginListBox("##SnippetList", ImVec2(-1, 170.0f))) {
        for (int i = 0; i < (int)m_snippets.size(); ++i) {
            bool isSelected = (i == m_selectedSnippetIndex);
            if (ImGui::Selectable(m_snippets[i].name.c_str(), isSelected)) {
                m_selectedSnippetIndex = i;
            }
        }
        ImGui::EndListBox();
    }

    auto& selected = m_snippets[m_selectedSnippetIndex];

    char nameBuffer[128] = {};
    strncpy_s(nameBuffer, selected.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        selected.name = nameBuffer;
        if (selected.name.empty()) {
            selected.name = "Snippet " + std::to_string(m_selectedSnippetIndex + 1);
        }
        SaveGlobalSnippets();
    }

    if (ImGui::Button("Insert At Cursor")) {
        InsertSnippetIntoEditor(selected.code);
    }

    ImGui::SameLine();
    if (ImGui::Button("Overwrite With Current Shader")) {
        selected.code = m_shaderState.text;
        SaveGlobalSnippets();
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete Snippet")) {
        m_snippets.erase(m_snippets.begin() + m_selectedSnippetIndex);
        if (m_selectedSnippetIndex >= (int)m_snippets.size()) {
            m_selectedSnippetIndex = (int)m_snippets.size() - 1;
        }
        SaveGlobalSnippets();
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text("Snippet Code:");
    ImGui::BeginChild("SnippetPreview", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(selected.code.c_str());
    ImGui::EndChild();

    ImGui::End();
}
)";

            m_scenes.emplace_back(nameBuf, templateShader);
        }

        ImGui::Separator();

        // Scene list
        for (int i = 0; i < (int)m_scenes.size(); i++) {
            ImGui::PushID(i);
            bool isSelected = (i == m_activeSceneIndex);
            if (ImGui::Selectable(m_scenes[i].name.c_str(), isSelected)) {
                SetActiveScene(i);
            }

            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::BeginMenu("Output Type")) {
                     if (ImGui::MenuItem("2D Texture", nullptr, m_scenes[i].outputType == TextureType::Texture2D)) {
                         m_scenes[i].outputType = TextureType::Texture2D;
                         // Force recreate texture next frame
                         m_scenes[i].texture.Reset();
                     }
                     if (ImGui::MenuItem("Cube Map", nullptr, m_scenes[i].outputType == TextureType::TextureCube)) {
                         m_scenes[i].outputType = TextureType::TextureCube;
                         m_scenes[i].texture.Reset();
                     }
                     ImGui::EndMenu();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Rename")) {
                    // TODO: Implement rename dialog
                }
                if (ImGui::MenuItem("Duplicate")) {
                    m_scenes.push_back(m_scenes[i]);
                    m_scenes.back().name += " (Copy)";
                }
                if (ImGui::MenuItem("Delete", nullptr, false, m_scenes.size() > 1)) {
                    m_scenes.erase(m_scenes.begin() + i);
                    if (m_activeSceneIndex >= (int)m_scenes.size()) {
                        m_activeSceneIndex = (int)m_scenes.size() - 1;
                    }
                    ImGui::EndPopup();
                    ImGui::PopID();
                    i--; // Adjust index since we removed an item
                    continue;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        ImGui::Separator();
        // Properties for active scene
        if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
            auto& scene = m_scenes[m_activeSceneIndex];
            ImGui::Text("Scene Config: %s", scene.name.c_str());

            // Audio/Tracker settings moved to Demo Playlist view
            ImGui::TextDisabled("Audio managed in 'Demo: Playlist'");
        }
    }
    ImGui::End();
}

void UISystem::CreateDefaultScene() {
    // Classic ShaderToy hello world - colorful sine wave pattern
    const char* defaultShader = R"(// ShaderToy-style Hello World
// fragCoord: pixel coordinates (e.g., 0 to 1920x1080)
// iResolution: viewport size (e.g., float2(1920, 1080))
// iTime: time in seconds

float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    // Normalize pixel coordinates (0 to 1)
    float2 uv = fragCoord / iResolution;

    // Create colorful sine wave pattern
    float3 color;
    color.r = 0.5 + 0.5 * sin(uv.x * 10.0 + iTime);
    color.g = 0.5 + 0.5 * sin(uv.y * 10.0 + iTime * 1.3);
    color.b = 0.5 + 0.5 * sin((uv.x + uv.y) * 5.0 + iTime * 0.7);

    return float4(color, 1.0);
}
)";

    m_scenes.emplace_back("Hello World", defaultShader);
    m_shaderState.text = defaultShader;
}

void UISystem::ShowShaderEditor() {
    if (ImGui::Begin("Shader Editor")) {
        // Compile button and status
        const char* statusText = "Clean";
        ImVec4 statusColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        switch (m_shaderState.status) {
            case CompileStatus::Dirty:
                statusText = "Dirty";
                statusColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                break;
            case CompileStatus::Compiling:
                statusText = "Compiling...";
                statusColor = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
                break;
            case CompileStatus::Success:
                statusText = "OK";
                statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                break;
            case CompileStatus::Error:
                statusText = "Error";
                statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
                break;
        }

        const bool editorFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool ctrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
        const bool ctrlEnterPressed = editorFocused && ctrlDown && ImGui::IsKeyPressed(ImGuiKey_Enter);
        if (ImGui::Button("Compile (Ctrl+Enter)") ||
            ctrlEnterPressed ||
            (editorFocused && ImGui::IsKeyPressed(ImGuiKey_F7))) {

            if (m_previewRenderer) {
                m_shaderState.status = CompileStatus::Compiling;
                m_shaderState.diagnostics.clear();

                std::vector<std::string> errors;

                if (m_currentMode == UIMode::PostFX) {
                    if (m_postFxSelectedIndex >= 0 && m_postFxSelectedIndex < (int)m_postFxDraftChain.size()) {
                        auto& selected = m_postFxDraftChain[m_postFxSelectedIndex];
                        selected.shaderCode = m_shaderState.text;

                        bool anyErrors = false;
                        // Ensure the source scene is compiled first for valid input textures.
                        if (m_postFxSourceSceneIndex >= 0 && m_postFxSourceSceneIndex < (int)m_scenes.size()) {
                            if (m_scenes[m_postFxSourceSceneIndex].isDirty ||
                                !m_scenes[m_postFxSourceSceneIndex].pipelineState) {
                                if (!CompileScene(m_postFxSourceSceneIndex)) {
                                    anyErrors = true;
                                    Diagnostic diag;
                                    diag.message = "Source Scene: compilation failed.";
                                    m_shaderState.diagnostics.push_back(diag);
                                }
                            }
                        }

                        for (auto& effect : m_postFxDraftChain) {
                            if (!effect.pipelineState || effect.isDirty) {
                                std::vector<std::string> fxErrors;
                                if (!CompilePostFxEffect(effect, fxErrors)) {
                                    anyErrors = true;
                                    for (const auto& error : fxErrors) {
                                        Diagnostic diag;
                                        diag.message = effect.name + ": " + error;
                                        m_shaderState.diagnostics.push_back(diag);
                                    }
                                }
                            }
                        }

                        if (anyErrors) {
                            m_shaderState.status = CompileStatus::Error;
                        } else {
                            m_shaderState.status = CompileStatus::Success;
                            m_shaderState.lastCompiledText = m_shaderState.text;
                        }
                    } else {
                        m_shaderState.status = CompileStatus::Error;
                        Diagnostic diag;
                        diag.message = "No post fx selected.";
                        m_shaderState.diagnostics.push_back(diag);
                    }
                } else {
                    // Collect texture declarations from current active scene
                    std::vector<PreviewRenderer::TextureDecl> decls;
                    if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                        auto& scene = m_scenes[m_activeSceneIndex];
                        for(const auto& b : scene.bindings) {
                            PreviewRenderer::TextureDecl decl;
                            decl.slot = b.channelIndex;
                            switch(b.type) {
                                case TextureType::TextureCube: decl.type = "TextureCube"; break;
                                case TextureType::Texture3D: decl.type = "Texture3D"; break;
                                default: decl.type = "Texture2D"; break;
                            }
                            decls.push_back(decl);
                        }
                    }

                    auto pso = m_previewRenderer->CompileShader(m_shaderState.text, decls, errors);

                    if (pso) {
                        if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                            m_scenes[m_activeSceneIndex].pipelineState = pso;
                            m_scenes[m_activeSceneIndex].shaderCode = m_shaderState.text;
                        }

                        m_shaderState.status = CompileStatus::Success;
                        m_shaderState.lastCompiledText = m_shaderState.text;
                    } else {
                        m_shaderState.status = CompileStatus::Error;
                        for (const auto& error : errors) {
                            Diagnostic diag;
                            diag.message = error;
                            m_shaderState.diagnostics.push_back(diag);
                        }
                    }
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextColored(statusColor, "%s", statusText);

        // Theme selector
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 160);
        ImGui::SetNextItemWidth(150);
        const char* themeNames[] = { "Dark (Enhanced)", "Dark", "Light", "Retro Blue" };
        int currentTheme = (int)m_shaderState.theme;
        if (ImGui::Combo("##Theme", &currentTheme, themeNames, 4)) {
            m_shaderState.theme = (EditorTheme)currentTheme;
            TextEditor::Palette palette;
            switch (m_shaderState.theme) {
                case EditorTheme::Dark:
                    // Enhanced dark palette with vibrant colors (IM_COL32 AABBGGRR format)
                    palette = TextEditor::GetDarkPalette();
                    palette[(int)TextEditor::PaletteIndex::Keyword] = 0xffd69c56;         // #569cd6 (Blue)
                    palette[(int)TextEditor::PaletteIndex::KnownIdentifier] = 0xffb0c94e; // #4ec9b0 (Teal)
                    palette[(int)TextEditor::PaletteIndex::Number] = 0xffa8ceb5;          // #b5cea8 (Light Green)
                    palette[(int)TextEditor::PaletteIndex::String] = 0xff7891ce;          // #ce9178 (Orange)
                    palette[(int)TextEditor::PaletteIndex::Comment] = 0xff55996a;         // #6a9955 (Green)
                    palette[(int)TextEditor::PaletteIndex::MultiLineComment] = 0xff55996a;
                    palette[(int)TextEditor::PaletteIndex::Identifier] = 0xffdcdcdc;      // #dcdcdc (White/Grey)
                    palette[(int)TextEditor::PaletteIndex::Punctuation] = 0xffdcdcdc;
                    palette[(int)TextEditor::PaletteIndex::Preprocessor] = 0xff9b9b9b;
                    m_textEditor.SetPalette(palette);
                    break;
                case EditorTheme::DarkOriginal:
                    m_textEditor.SetPalette(TextEditor::GetDarkPalette());
                    break;
                case EditorTheme::Light:
                    m_textEditor.SetPalette(TextEditor::GetLightPalette());
                    break;
                case EditorTheme::RetroBlue:
                    m_textEditor.SetPalette(TextEditor::GetRetroBluePalette());
                    break;
            }
        }

        ImGui::Separator();

        // Search/Replace bar (toggle with Ctrl+F)
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_F)) {
            m_shaderState.showSearchReplace = !m_shaderState.showSearchReplace;
        }

        if (m_shaderState.showSearchReplace) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
            ImGui::BeginChild("SearchBar", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3.5f), true);

            // Search field
            ImGui::Text("Search:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-80);
            // Set focus on search field when first opened
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputText("##Search", m_shaderState.searchBuffer, sizeof(m_shaderState.searchBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Find Next")) {
                std::string searchStr = m_shaderState.searchBuffer;
                if (!searchStr.empty()) {
                    std::string text = m_textEditor.GetText();
                    auto cursor = m_textEditor.GetCursorPosition();

                    // Convert cursor position to text index
                    auto lines = m_textEditor.GetTextLines();
                    size_t textPos = 0;
                    for (int i = 0; i < cursor.mLine && i < (int)lines.size(); i++) {
                        textPos += lines[i].length() + 1; // +1 for newline
                    }
                    textPos += cursor.mColumn;

                    // Start searching from AFTER current position to find next match
                    textPos += 1;

                    // Find next occurrence
                    size_t found = text.find(searchStr, textPos);
                    if (found == std::string::npos) {
                        // Wrap around to beginning
                        found = text.find(searchStr, 0);
                    }

                    if (found != std::string::npos) {
                        // Convert text position back to coordinates
                        TextEditor::Coordinates newPos;
                        size_t pos = 0;
                        for (int line = 0; line < (int)lines.size(); line++) {
                            if (pos + lines[line].length() >= found) {
                                newPos.mLine = line;
                                newPos.mColumn = (int)(found - pos);
                                break;
                            }
                            pos += lines[line].length() + 1;
                        }
                        m_textEditor.SetCursorPosition(newPos);
                        // Select the found text
                        TextEditor::Coordinates endPos = newPos;
                        endPos.mColumn += (int)searchStr.length();
                        m_textEditor.SetSelection(newPos, endPos);
                    }
                }
            }

            // Replace field
            ImGui::Text("Replace:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-80);
            ImGui::InputText("##Replace", m_shaderState.replaceBuffer, sizeof(m_shaderState.replaceBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Replace All")) {
                std::string search = m_shaderState.searchBuffer;
                std::string replace = m_shaderState.replaceBuffer;
                if (!search.empty()) {
                    std::string text = m_textEditor.GetText();
                    size_t pos = 0;
                    int count = 0;
                    while ((pos = text.find(search, pos)) != std::string::npos) {
                        text.replace(pos, search.length(), replace);
                        pos += replace.length();
                        count++;
                    }
                    if (count > 0) {
                        m_textEditor.SetText(text);
                        m_shaderState.text = text;
                        if (m_currentMode == UIMode::PostFX) {
                            if (m_postFxSelectedIndex >= 0 && m_postFxSelectedIndex < (int)m_postFxDraftChain.size()) {
                                auto& effect = m_postFxDraftChain[m_postFxSelectedIndex];
                                effect.shaderCode = text;
                                effect.isDirty = true;
                            }
                        } else if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                            m_scenes[m_activeSceneIndex].shaderCode = text;
                        }
                        m_shaderState.status = CompileStatus::Dirty;
                    }
                }
            }

            // Close button on new line
            if (ImGui::Button("Close Search")) {
                m_shaderState.showSearchReplace = false;
            }

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // Calculate height for text editor (leave room for status bar)
        // Now that diagnostics are separate, we can give more space to the editor
        float statusBarHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

        // Sync text editor with shader state when not actively editing
        if (!editorFocused && m_textEditor.GetText() != m_shaderState.text) {
            m_textEditor.SetText(m_shaderState.text);
        }

        // Render the text editor. Use available height minus status bar.
        // We use a small negative Y to leave room for the status bar at bottom.
        const bool prevKeyboardInput = m_textEditor.IsHandleKeyboardInputsEnabled();
        if (ctrlEnterPressed) {
            m_textEditor.SetHandleKeyboardInputs(false);
        }
        if (m_fontCode) {
            ImGui::PushFont(m_fontCode);
        }
        m_textEditor.Render("##ShaderCode", ImVec2(-1, -statusBarHeight), true);
        if (m_fontCode) {
            ImGui::PopFont();
        }
        if (ctrlEnterPressed) {
            m_textEditor.SetHandleKeyboardInputs(prevKeyboardInput);
        }

        // Check if text was modified
        if (m_textEditor.IsTextChanged()) {
            m_shaderState.text = m_textEditor.GetText();
            if (m_currentMode == UIMode::PostFX) {
                if (m_postFxSelectedIndex >= 0 && m_postFxSelectedIndex < (int)m_postFxDraftChain.size()) {
                    auto& effect = m_postFxDraftChain[m_postFxSelectedIndex];
                    effect.shaderCode = m_shaderState.text;
                    effect.isDirty = true;
                }
            } else {
                if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                    m_scenes[m_activeSceneIndex].shaderCode = m_shaderState.text;
                    m_scenes[m_activeSceneIndex].isDirty = true;
                }
            }
            if (m_shaderState.text != m_shaderState.lastCompiledText) {
                m_shaderState.status = CompileStatus::Dirty;
            }
        }

        // Status bar at bottom
        ImGui::Separator();
        auto cursorPos = m_textEditor.GetCursorPosition();
        ImGui::TextUnformatted("Lines:");
        ImGui::SameLine();
        PushNumericFont();
        ImGui::Text("%d", m_textEditor.GetTotalLines());
        PopNumericFont();
        ImGui::SameLine();
        ImGui::TextUnformatted("| Ln");
        ImGui::SameLine();
        PushNumericFont();
        ImGui::Text("%d", cursorPos.mLine + 1);
        PopNumericFont();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(", Col");
        ImGui::SameLine();
        PushNumericFont();
        ImGui::Text("%d", cursorPos.mColumn + 1);
        PopNumericFont();
        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
        ImGui::TextUnformatted(m_textEditor.IsOverwrite() ? "Ovr" : "Ins");

    }
    ImGui::End();
}

void UISystem::ShowDiagnostics() {
    // Only show if we have diagnostics or we want a persistent "Output" window
    if (ImGui::Begin("Diagnostics")) {
        if (m_shaderState.diagnostics.empty()) {
            ImGui::TextDisabled("No errors or warnings.");
            if (m_shaderState.status == CompileStatus::Success) {
                 ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Compilation Successful.");
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
            PushNumericFont();
            ImGui::Text("%d", (int)m_shaderState.diagnostics.size());
            PopNumericFont();
            ImGui::SameLine();
            ImGui::TextUnformatted("Errors Found:");
            ImGui::PopStyleColor();
            ImGui::Separator();

            for (int i = 0; i < (int)m_shaderState.diagnostics.size(); ++i) {
                const auto& diag = m_shaderState.diagnostics[i];
                ImGui::PushID(i);

                // Selectable error to jump to line
                char buf[512];
                snprintf(buf, sizeof(buf), "Line %d, Col %d: %s", diag.line, diag.column, diag.message.c_str());
                 const bool selected = ImGui::Selectable("##DiagSelect", false);
                 ImGui::SameLine(0.0f, 0.0f);
                 ImGui::TextUnformatted("Line ");
                 ImGui::SameLine(0.0f, 0.0f);
                 PushNumericFont();
                 ImGui::Text("%d", diag.line);
                 PopNumericFont();
                 ImGui::SameLine(0.0f, 0.0f);
                 ImGui::TextUnformatted(", Col ");
                 ImGui::SameLine(0.0f, 0.0f);
                 PushNumericFont();
                 ImGui::Text("%d", diag.column);
                 PopNumericFont();
                 ImGui::SameLine(0.0f, 0.0f);
                 ImGui::TextUnformatted(": ");
                 ImGui::SameLine(0.0f, 0.0f);
                 ImGui::TextUnformatted(diag.message.c_str());
                 if (selected) {
                     // Jump to Error
                     TextEditor::Coordinates coord(diag.line - 1, diag.column > 0 ? diag.column - 1 : 0);
                     m_textEditor.SetCursorPosition(coord);
                     // Also scroll there
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::End();

}

void UISystem::BuildLayout(UIMode mode) {
    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");

    // Clear existing layout
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    if (mode == UIMode::Demo) {
        // Demoscene Layout
        // Top: Transport (Small)
        // Left: Library (25%)
        // Center: Tracker (Top 50%), Preview (Bottom 50%)

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_up = 0, dock_down = 0;
        ImGuiID dock_left = 0, dock_right = 0;
        ImGuiID dock_right_top = 0, dock_right_bottom = 0;
        ImGuiID dock_left_top = 0, dock_left_bottom = 0;

        // 1. Top Transport
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Up, 0.08f, &dock_up, &dock_down);

        // 2. Left Library (from remaining space)
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Left, 0.25f, &dock_left, &dock_right);

        // 3. Right: Split into Tracker (Top) and Preview (Bottom)
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.50f, &dock_right_top, &dock_right_bottom);

        // 4. Split Left Stack into Scene / Audio / Log
        ImGuiID dock_left_mid = 0;
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.33f, &dock_left_bottom, &dock_left);
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.50f, &dock_left_mid, &dock_left_top);

        ImGui::DockBuilderDockWindow("Transport", dock_up);
        ImGui::DockBuilderDockWindow("Demo: Playlist", dock_right_top);
        ImGui::DockBuilderDockWindow("Preview", dock_right_bottom);

        ImGui::DockBuilderDockWindow("Demo: Scene Library", dock_left_top);
        ImGui::DockBuilderDockWindow("Audio Library", dock_left_mid);
        ImGui::DockBuilderDockWindow("Demo: Runtime Log", dock_left_bottom);

    } else if (mode == UIMode::PostFX) {
        // Post FX Layout
        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_up = 0, dock_down = 0;
        ImGuiID dock_left = 0, dock_center = 0, dock_right = 0;

        // 1. Top Transport
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Up, 0.08f, &dock_up, &dock_down);

        // 2. Left column (Library/Source)
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Left, 0.22f, &dock_left, &dock_down);

        // 3. Right column (Preview/Diagnostics)
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Right, 0.28f, &dock_right, &dock_center);

        // Split left stack
        ImGuiID dock_left_top = 0, dock_left_bot = 0;
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.65f, &dock_left_top, &dock_left_bot);

        // Split center stack
        ImGuiID dock_center_top = 0, dock_center_bot = 0;
        ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Up, 0.28f, &dock_center_top, &dock_center_bot);

        // Split right stack
        ImGuiID dock_right_top = 0, dock_right_bot = 0;
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.60f, &dock_right_top, &dock_right_bot);

        ImGui::DockBuilderDockWindow("Transport", dock_up);
        ImGui::DockBuilderDockWindow("FX: Library", dock_left_top);
        ImGui::DockBuilderDockWindow("FX: Source", dock_left_bot);
        ImGui::DockBuilderDockWindow("FX: Chain", dock_center_top);
        ImGui::DockBuilderDockWindow("Shader Editor", dock_center_bot);
        ImGui::DockBuilderDockWindow("Preview", dock_right_top);
        ImGui::DockBuilderDockWindow("Diagnostics", dock_right_bot);
    } else {
        // Scene Mode Layout
        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_up = 0, dock_down = 0;
        ImGuiID dock_left = 0, dock_center = 0, dock_right = 0;

        // 1. Top Transport
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Up, 0.08f, &dock_up, &dock_down);

        // 2. Split Left (Library)
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Left, 0.20f, &dock_left, &dock_down);

        // 3. Split Right (Preview/Diag)
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Right, 0.30f, &dock_right, &dock_center);

        // Split Right Stack
        ImGuiID dock_right_top = 0, dock_right_bot = 0;
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.60f, &dock_right_top, &dock_right_bot);

        // Split Left Stack
        ImGuiID dock_left_top = 0, dock_left_bot = 0;
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.60f, &dock_left_top, &dock_left_bot);

        ImGui::DockBuilderDockWindow("Transport", dock_up);
        ImGui::DockBuilderDockWindow("Scene: Library", dock_left_top);
        ImGui::DockBuilderDockWindow("Scene: Snippets", dock_left_bot);
        ImGui::DockBuilderDockWindow("Scene: Post Stack", dock_left_bot);
        ImGui::DockBuilderDockWindow("Shader Editor", dock_center);
        ImGui::DockBuilderDockWindow("Preview", dock_right_top);
        ImGui::DockBuilderDockWindow("Diagnostics", dock_right_bot);
           ImGui::DockBuilderDockWindow("Scene: Textures & Channels", dock_right_bot);
    }

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace ShaderLab
