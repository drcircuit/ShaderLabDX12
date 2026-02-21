#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UIConfig.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/UI/UISystemAssets.h"
#include "ShaderLab/UI/OpenFontIcons.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <filesystem>
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

bool IconButton(const char* id, uint32_t iconCodepoint, const char* tooltip, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
    const std::string icon = OpenFontIcons::ToUtf8(iconCodepoint);
    const std::string buttonId = std::string("##") + id;

    ImVec2 buttonSize = size;
    if (buttonSize.x <= 0.0f || buttonSize.y <= 0.0f) {
        const ImVec2 textSize = ImGui::CalcTextSize(icon.c_str());
        const ImVec2 pad = ImGui::GetStyle().FramePadding;
        if (buttonSize.x <= 0.0f) buttonSize.x = textSize.x + pad.x * 2.0f;
        if (buttonSize.y <= 0.0f) buttonSize.y = textSize.y + pad.y * 2.0f;
    }

    const bool pressed = ImGui::InvisibleButton(buttonId.c_str(), buttonSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();

    const ImU32 bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    drawList->AddRectFilled(min, max, bg, style.FrameRounding);
    if (style.FrameBorderSize > 0.0f) {
        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();

    float textX = min.x;
    float textY = min.y;
    bool usedGlyphBounds = false;
    if (font) {
        if (ImFontBaked* baked = font->GetFontBaked(fontSize)) {
            if (ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(iconCodepoint))) {
                const float glyphW = glyph->X1 - glyph->X0;
                const float glyphH = glyph->Y1 - glyph->Y0;
                textX = min.x + (buttonSize.x - glyphW) * 0.5f - glyph->X0;
                textY = min.y + (buttonSize.y - glyphH) * 0.5f - glyph->Y0;
                usedGlyphBounds = true;
            }
        }
    }
    if (!usedGlyphBounds) {
        const ImVec2 textSize = ImGui::CalcTextSize(icon.c_str());
        textX = min.x + (buttonSize.x - textSize.x) * 0.5f;
        textY = min.y + (buttonSize.y - textSize.y) * 0.5f;
    }

    drawList->AddText(font, fontSize, ImVec2(std::floor(textX), std::floor(textY)), ImGui::GetColorU32(ImGuiCol_CheckMark), icon.c_str());

    if (ImGui::IsItemHovered() && tooltip && *tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return pressed;
}

bool LabeledActionButton(const char* id, uint32_t iconCodepoint, const char* label, const char* tooltip, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
    const std::string icon = OpenFontIcons::ToUtf8(iconCodepoint);
    const std::string buttonId = std::string("##") + id;
    const char* safeLabel = (label && *label) ? label : "";

    ImVec2 buttonSize = size;
    if (buttonSize.x <= 0.0f) {
        buttonSize.x = ImGui::CalcItemWidth();
    }
    if (buttonSize.y <= 0.0f) {
        buttonSize.y = ImGui::GetFrameHeight();
    }

    const bool pressed = ImGui::InvisibleButton(buttonId.c_str(), buttonSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();

    const ImU32 bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    drawList->AddRectFilled(min, max, bg, style.FrameRounding);
    if (style.FrameBorderSize > 0.0f) {
        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    const ImVec2 iconSize = ImGui::CalcTextSize(icon.c_str());
    const ImVec2 labelSize = ImGui::CalcTextSize(safeLabel);
    const float gap = safeLabel[0] ? style.ItemInnerSpacing.x : 0.0f;
    const float contentWidth = iconSize.x + gap + labelSize.x;
    const float baseX = min.x + (buttonSize.x - contentWidth) * 0.5f;
    float iconX = baseX;
    float iconY = min.y + (buttonSize.y - iconSize.y) * 0.5f;
    if (font) {
        if (ImFontBaked* baked = font->GetFontBaked(fontSize)) {
            if (ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(iconCodepoint))) {
                const float glyphW = glyph->X1 - glyph->X0;
                const float glyphH = glyph->Y1 - glyph->Y0;
                iconX = baseX + (iconSize.x - glyphW) * 0.5f - glyph->X0;
                iconY = min.y + (buttonSize.y - glyphH) * 0.5f - glyph->Y0;
            }
        }
    }
    const float labelY = min.y + (buttonSize.y - labelSize.y) * 0.5f;

    drawList->AddText(font, fontSize, ImVec2(std::floor(iconX), std::floor(iconY)), ImGui::GetColorU32(ImGuiCol_CheckMark), icon.c_str());
    if (safeLabel[0]) {
        drawList->AddText(ImVec2(std::floor(baseX + iconSize.x + gap), std::floor(labelY)), ImGui::GetColorU32(ImGuiCol_TextLink), safeLabel);
    }

    if (ImGui::IsItemHovered() && tooltip && *tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return pressed;
}

float CompactActionHeight() {
    return ImGui::GetFrameHeight();
}

ImVec2 CompactActionSize(float width = -1.0f) {
    return ImVec2(width, CompactActionHeight());
}

std::string FormatByteSize(size_t bytes) {
    if (bytes == 0) return "0 B";
    static const char* units[] = { "B", "KB", "MB", "GB" };
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 3) {
        value /= 1024.0;
        ++unitIndex;
    }
    char buffer[32] = {};
    if (unitIndex == 0) {
        std::snprintf(buffer, sizeof(buffer), "%zu %s", bytes, units[unitIndex]);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[unitIndex]);
    }
    return std::string(buffer);
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
        ImVec4 previewBackdrop = m_uiThemeColors.WindowBackground;
        previewBackdrop.w = (std::clamp)(m_uiThemeColors.PanelOpacity, 0.0f, 1.0f);
        ImGui::GetWindowDrawList()->AddRectFilled(screenStart, screenEnd, ImGui::GetColorU32(previewBackdrop));

        ImGui::SetCursorPos(ImVec2(cursorStart.x + (avail.x - drawSize.x) * 0.5f, cursorStart.y + (avail.y - drawSize.y) * 0.5f));

        if (m_previewTexture && m_previewSrvGpuHandle.ptr != 0) {
            ImGui::Image((ImTextureID)m_previewSrvGpuHandle.ptr, drawSize);
        } else {
            ImGui::Dummy(drawSize);
        }

        if (m_currentMode == UIMode::Scene && m_screenKeysOverlayEnabled) {
            ImGui::SetNextWindowPos(ImVec2(screenStart.x + 8.0f, screenStart.y + 8.0f), ImGuiCond_Always);
            ImGui::SetNextWindowBgAlpha(0.68f);
            ImGuiWindowFlags overlayFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoFocusOnAppearing;

            ImGui::SetNextWindowSize(ImVec2(330.0f, 200.0f), ImGuiCond_Always);

            if (ImGui::Begin("##ScreenKeysOverlay", nullptr, overlayFlags)) {
                if (m_fontMenuSmall) {
                    ImGui::PushFont(m_fontMenuSmall);
                }

                ImGui::TextUnformatted("Screen Keys");
                ImGui::SameLine();
                if (LabeledActionButton("ScreenKeysCopy", OpenFontIcons::kCopy, "Copy", "Copy key log", ImVec2(100.0f, 0.0f))) {
                    std::string clipboard;
                    clipboard.reserve(m_screenKeyLog.size() * 8);
                    for (size_t i = 0; i < m_screenKeyLog.size(); ++i) {
                        if (i > 0) {
                            clipboard.push_back('\n');
                        }
                        clipboard += m_screenKeyLog[i];
                    }
                    if (clipboard.empty()) {
                        clipboard = "(empty)";
                    }
                    ImGui::SetClipboardText(clipboard.c_str());
                }
                ImGui::SameLine();
                if (LabeledActionButton("ScreenKeysClear", OpenFontIcons::kTrash2, "Clear", "Clear key log", ImVec2(100.0f, 0.0f))) {
                    m_screenKeyLog.clear();
                }

                ImGui::Separator();
                if (m_screenKeyLog.empty()) {
                    ImGui::TextDisabled("No keys yet");
                } else {
                    ImGui::BeginChild("ScreenKeyLogScroll", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
                    for (int i = 0; i < (int)m_screenKeyLog.size(); ++i) {
                        ImGui::TextUnformatted(m_screenKeyLog[i].c_str());
                    }
                    ImGui::SetScrollHereY(1.0f);
                    ImGui::EndChild();
                }

                if (m_fontMenuSmall) {
                    ImGui::PopFont();
                }
            }
            ImGui::End();
        }

        ImGui::SetCursorPos(cursorStart);
        ImGui::Dummy(avail);

        ImGui::End();
    };

    // Always create all windows (for docking), but only show content for active mode

    // Demo mode windows
    if (m_currentMode == UIMode::Demo) {
        ShowDemoMetadata();
        ShowDemoPlaylist();
        ShowAudioLibrary();

        ShowSceneList();

        if (ImGui::Begin("Demo: Runtime Log")) {
            if (LabeledActionButton("ClearRuntimeLog", OpenFontIcons::kTrash2, "Clear Log", "Clear log")) {
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

            ImGui::PushStyleColor(ImGuiCol_ChildBg, m_uiThemeColors.ConsoleBackground);
            ImGui::PushStyleColor(ImGuiCol_Text, m_uiThemeColors.ConsoleFontColor);
            if (ImGui::BeginChild("RuntimeLog", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
                for (const auto& line : m_demoLog) {
                    ImGui::TextUnformatted(line.c_str());
                }
                if (m_demoLogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 4.0f) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleColor(2);
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

                if (LabeledActionButton("AddFileTexture", OpenFontIcons::kFilePlus, "Add File", "Add file texture", ImVec2(120.0f, 0.0f))) {
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
                if (LabeledActionButton("AddSceneSampler", OpenFontIcons::kPlus, "Add Scene", "Add scene sampler", ImVec2(130.0f, 0.0f))) {
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
                                if (LabeledActionButton("BrowseTexture", OpenFontIcons::kFolder, "Browse", "Browse texture file", ImVec2(120.0f, 0.0f))) {
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

            if (LabeledActionButton("AddPreset", OpenFontIcons::kPlus, "Add Preset", "Add preset effect", ImVec2(130.0f, 0.0f))) {
                if (selectedPresetIndex >= 0 && selectedPresetIndex < (int)kPostFxPresetCount) {
                    const auto& preset = kPostFxPresets[selectedPresetIndex];
                    m_postFxDraftChain.emplace_back(preset.name, preset.code);
                    m_postFxSelectedIndex = (int)m_postFxDraftChain.size() - 1;
                    SyncPostFxEditorToSelection();
                }
            }
            ImGui::SameLine();
            if (LabeledActionButton("AddEmptyFx", OpenFontIcons::kFilePlus, "Add Empty", "Add empty effect", ImVec2(130.0f, 0.0f))) {
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
                if (LabeledActionButton("ApplyFxDraft", OpenFontIcons::kCheck, "Apply", "Apply draft to scene", ImVec2(110.0f, 0.0f))) {
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
                if (LabeledActionButton("MoveFxUp", OpenFontIcons::kChevronUp, "Up", "Move effect up", ImVec2(90.0f, 0.0f)) && m_postFxSelectedIndex > 0) {
                    std::swap(m_postFxDraftChain[m_postFxSelectedIndex], m_postFxDraftChain[m_postFxSelectedIndex - 1]);
                    m_postFxSelectedIndex -= 1;
                }
                ImGui::SameLine();
                if (LabeledActionButton("MoveFxDown", OpenFontIcons::kChevronDown, "Down", "Move effect down", ImVec2(90.0f, 0.0f)) && m_postFxSelectedIndex + 1 < (int)m_postFxDraftChain.size()) {
                    std::swap(m_postFxDraftChain[m_postFxSelectedIndex], m_postFxDraftChain[m_postFxSelectedIndex + 1]);
                    m_postFxSelectedIndex += 1;
                }
                ImGui::SameLine();
                if (LabeledActionButton("RemoveFx", OpenFontIcons::kTrash2, "Remove", "Remove effect", ImVec2(110.0f, 0.0f))) {
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
    ImVec4 previewBackdrop = m_uiThemeColors.WindowBackground;
    previewBackdrop.w = (std::clamp)(m_uiThemeColors.PanelOpacity, 0.0f, 1.0f);
    ImGui::GetWindowDrawList()->AddRectFilled(screenStart, screenEnd, ImGui::GetColorU32(previewBackdrop));
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
        if (LabeledActionButton("AddScene", OpenFontIcons::kPlus, "Add Scene", "Add scene", ImVec2(130.0f, 0.0f))) {
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

        if (m_currentMode == UIMode::Scene) {
            ImGui::Separator();
            if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                auto& scene = m_scenes[m_activeSceneIndex];
                ImGui::Text("Scene Config");

                char sceneNameBuffer[128];
                std::snprintf(sceneNameBuffer, sizeof(sceneNameBuffer), "%s", scene.name.c_str());
                if (ImGui::InputText("Scene Name", sceneNameBuffer, sizeof(sceneNameBuffer))) {
                    if (sceneNameBuffer[0] == '\0') {
                        std::snprintf(sceneNameBuffer, sizeof(sceneNameBuffer), "Scene %d", m_activeSceneIndex + 1);
                    }
                    scene.name = sceneNameBuffer;
                }

                char sceneDescriptionBuffer[1024];
                std::snprintf(sceneDescriptionBuffer, sizeof(sceneDescriptionBuffer), "%s", scene.description.c_str());
                if (ImGui::InputTextMultiline("Description", sceneDescriptionBuffer, sizeof(sceneDescriptionBuffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5.5f))) {
                    scene.description = sceneDescriptionBuffer;
                }
            }
        }
    }
    ImGui::End();
}

void UISystem::ShowSnippetBin() {
    if (!ImGui::Begin("Scene: Snippets")) {
        ImGui::End();
        return;
    }

    namespace fs = std::filesystem;

    ImGui::Text("Reusable Snippets");
    ImGui::Separator();

    if (m_snippetFolders.empty()) {
        ShaderSnippetFolder folder;
        folder.name = "General";
        if (!m_snippetsDirectoryPath.empty()) {
            folder.filePath = (fs::path(m_snippetsDirectoryPath) / "General.json").string();
        }
        m_snippetFolders.push_back(std::move(folder));
        m_selectedSnippetFolderIndex = 0;
    }

    if (m_selectedSnippetFolderIndex < 0 || m_selectedSnippetFolderIndex >= (int)m_snippetFolders.size()) {
        m_selectedSnippetFolderIndex = 0;
    }

    std::vector<const char*> folderNames;
    folderNames.reserve(m_snippetFolders.size());
    for (auto& folder : m_snippetFolders) {
        folderNames.push_back(folder.name.c_str());
    }

    ImGui::TextUnformatted("Folder");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##SnippetFolder", &m_selectedSnippetFolderIndex, folderNames.data(), (int)folderNames.size());

    static char newFolderName[64] = "";
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##NewSnippetFolder", newFolderName, sizeof(newFolderName));

    if (LabeledActionButton("SnippetAddFolder", OpenFontIcons::kPlus, "Add Folder", "Add folder", ImVec2(140.0f, 0.0f))) {
        std::string folderName = newFolderName;
        if (folderName.empty()) {
            folderName = "Folder " + std::to_string((int)m_snippetFolders.size() + 1);
        }

        auto sanitize = [](const std::string& value) {
            std::string out;
            out.reserve(value.size());
            for (char c : value) {
                const bool ok =
                    (c >= 'a' && c <= 'z') ||
                    (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') ||
                    c == '_' || c == '-';
                out.push_back(ok ? c : '_');
            }
            if (out.empty()) {
                out = "Folder";
            }
            return out;
        };

        fs::path dir = m_snippetsDirectoryPath.empty() ? fs::current_path() : fs::path(m_snippetsDirectoryPath);
        std::string stem = sanitize(folderName);
        fs::path filePath = dir / (stem + ".json");
        int suffix = 2;
        while (fs::exists(filePath)) {
            filePath = dir / (stem + "_" + std::to_string(suffix) + ".json");
            ++suffix;
        }

        ShaderSnippetFolder folder;
        folder.name = folderName;
        folder.filePath = filePath.string();
        m_snippetFolders.push_back(std::move(folder));
        m_selectedSnippetFolderIndex = (int)m_snippetFolders.size() - 1;
        m_selectedSnippetIndex = -1;
        SaveGlobalSnippets();
        newFolderName[0] = '\0';
    }
    ImGui::SameLine();
    if (LabeledActionButton("SnippetDeleteFolder", OpenFontIcons::kTrash2, "Delete Folder", "Delete folder", ImVec2(160.0f, 0.0f)) && m_snippetFolders.size() > 1) {
        fs::path fileToDelete = m_snippetFolders[m_selectedSnippetFolderIndex].filePath;
        m_snippetFolders.erase(m_snippetFolders.begin() + m_selectedSnippetFolderIndex);
        if (m_selectedSnippetFolderIndex >= (int)m_snippetFolders.size()) {
            m_selectedSnippetFolderIndex = (int)m_snippetFolders.size() - 1;
        }
        m_selectedSnippetIndex = -1;
        if (!fileToDelete.empty()) {
            std::error_code ec;
            fs::remove(fileToDelete, ec);
        }
        SaveGlobalSnippets();
    }

    auto& activeFolder = m_snippetFolders[m_selectedSnippetFolderIndex];
    auto& snippets = activeFolder.snippets;

    ImGui::Separator();

    if (ImGui::BeginTable("SnippetCreateActions", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("CreateA", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("CreateB", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextColumn();
        if (LabeledActionButton("SnippetSaveCurrent", OpenFontIcons::kSave, "Save Current", "Save current shader as snippet", ImVec2(-FLT_MIN, 0.0f))) {
            ShaderSnippet snippet;
            snippet.name = "Snippet " + std::to_string(m_nextSnippetId++);
            snippet.code = m_shaderState.text;
            if (!snippet.code.empty()) {
                snippets.push_back(std::move(snippet));
                m_selectedSnippetIndex = (int)snippets.size() - 1;
                SaveGlobalSnippets();
            }
        }

        ImGui::TableNextColumn();
        if (LabeledActionButton("SnippetAddEmpty", OpenFontIcons::kFilePlus, "Add Empty", "Add empty snippet", ImVec2(-FLT_MIN, 0.0f))) {
            ShaderSnippet snippet;
            snippet.name = "Snippet " + std::to_string(m_nextSnippetId++);
            snippet.code = "float4 SnippetFunc(float2 uv, float t) {\n    return float4(uv, sin(t), 1.0);\n}\n";
            snippets.push_back(std::move(snippet));
            m_selectedSnippetIndex = (int)snippets.size() - 1;
            SaveGlobalSnippets();
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    if (snippets.empty()) {
        ImGui::TextDisabled("No snippets yet.");
        ImGui::TextDisabled("Create one from current shader text.");
        ImGui::End();
        return;
    }

    if (m_selectedSnippetIndex < 0 || m_selectedSnippetIndex >= (int)snippets.size()) {
        m_selectedSnippetIndex = 0;
    }

    std::vector<const char*> snippetNames;
    snippetNames.reserve(snippets.size());
    for (const auto& snippet : snippets) {
        snippetNames.push_back(snippet.name.c_str());
    }

    ImGui::TextUnformatted("Snippet");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("##SnippetSelector", &m_selectedSnippetIndex, snippetNames.data(), (int)snippetNames.size());

    auto& selected = snippets[m_selectedSnippetIndex];

    if (m_snippetDraftFolderIndex != m_selectedSnippetFolderIndex ||
        m_snippetDraftIndex != m_selectedSnippetIndex) {
        m_snippetDraftFolderIndex = m_selectedSnippetFolderIndex;
        m_snippetDraftIndex = m_selectedSnippetIndex;
        m_snippetDraftCode = selected.code;
        m_snippetDraftDirty = false;
        m_snippetTextEditor.SetText(m_snippetDraftCode);
    }

    if (ImGui::BeginTable("SnippetActions", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("SnipActA", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("SnipActB", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("SnipActC", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableNextColumn();
        if (LabeledActionButton("SnippetInsert", OpenFontIcons::kInsert, "Insert", "Insert at cursor", ImVec2(-FLT_MIN, 0.0f))) {
            InsertSnippetIntoEditor(selected.code);
        }

        ImGui::TableNextColumn();
        if (LabeledActionButton("SnippetOverwrite", OpenFontIcons::kCopy, "Overwrite", "Overwrite snippet with current shader", ImVec2(-FLT_MIN, 0.0f))) {
            selected.code = m_shaderState.text;
            m_snippetDraftCode = selected.code;
            m_snippetTextEditor.SetText(m_snippetDraftCode);
            m_snippetDraftDirty = false;
            SaveGlobalSnippets();
        }

        ImGui::TableNextColumn();
        if (LabeledActionButton("SnippetDelete", OpenFontIcons::kTrash2, "Delete", "Delete snippet", ImVec2(-FLT_MIN, 0.0f))) {
            snippets.erase(snippets.begin() + m_selectedSnippetIndex);
            if (m_selectedSnippetIndex >= (int)snippets.size()) {
                m_selectedSnippetIndex = (int)snippets.size() - 1;
            }
            SaveGlobalSnippets();
            ImGui::EndTable();
            ImGui::End();
            return;
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Snippet Code");
    ImGui::SameLine();
    if (LabeledActionButton("SnippetEditMode", m_snippetCodeLocked ? OpenFontIcons::kLock : OpenFontIcons::kUnlock,
                            m_snippetCodeLocked ? "Locked" : "Edit",
                            m_snippetCodeLocked ? "Snippet is locked" : "Edit snippet draft",
                            ImVec2(120.0f, 0.0f))) {
        m_snippetCodeLocked = !m_snippetCodeLocked;
        if (!m_snippetCodeLocked) {
            m_snippetDraftCode = selected.code;
            m_snippetTextEditor.SetText(m_snippetDraftCode);
            m_snippetDraftDirty = false;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled(m_snippetCodeLocked ? "Locked" : "Edit Draft");

    if (m_snippetTextEditor.GetText() != m_snippetDraftCode) {
        m_snippetTextEditor.SetText(m_snippetDraftCode);
    }
    m_snippetTextEditor.SetReadOnly(m_snippetCodeLocked);
    float codeEditorHeight = ImGui::GetContentRegionAvail().y;
    if (!m_snippetCodeLocked) {
        codeEditorHeight -= (ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() + 6.0f);
    }
    codeEditorHeight = (std::max)(180.0f, codeEditorHeight);
    int snippetFontIndex = (std::max)(0, (std::min)(4, (int)m_shaderState.codeFontSize));
    ImFont* activeSnippetCodeFont = m_fontCodeSizes[snippetFontIndex] ? m_fontCodeSizes[snippetFontIndex] : m_fontCode;
    if (activeSnippetCodeFont) {
        ImGui::PushFont(activeSnippetCodeFont);
    }
    m_snippetTextEditor.Render("##SnippetCodeEditor", ImVec2(-1.0f, codeEditorHeight), true);
    if (activeSnippetCodeFont) {
        ImGui::PopFont();
    }
    if (!m_snippetCodeLocked && m_snippetTextEditor.IsTextChanged()) {
        m_snippetDraftCode = m_snippetTextEditor.GetText();
        m_snippetDraftDirty = (m_snippetDraftCode != selected.code);
    }

    if (!m_snippetCodeLocked) {
        if (ImGui::BeginTable("SnippetDraftActions", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("DraftA", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("DraftB", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableNextColumn();
            if (LabeledActionButton("SnippetSaveDraft", OpenFontIcons::kSave, "Save Edits", "Save snippet edits", ImVec2(-FLT_MIN, 0.0f))) {
                selected.code = m_snippetDraftCode;
                m_snippetDraftDirty = false;
                SaveGlobalSnippets();
            }

            ImGui::TableNextColumn();
            if (LabeledActionButton("SnippetRevertDraft", OpenFontIcons::kRefresh, "Revert", "Revert unsaved edits", ImVec2(-FLT_MIN, 0.0f))) {
                m_snippetDraftCode = selected.code;
                m_snippetTextEditor.SetText(m_snippetDraftCode);
                m_snippetDraftDirty = false;
            }
            ImGui::EndTable();
        }

        if (m_snippetDraftDirty) {
            ImGui::TextDisabled("Unsaved snippet edits");
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
        ImVec4 statusColor = m_uiThemeColors.StatusFontColor;
        switch (m_shaderState.status) {
            case CompileStatus::Dirty:
                statusText = "Dirty";
                statusColor = m_uiThemeColors.TrackerAccentBeatFontColor;
                break;
            case CompileStatus::Compiling:
                statusText = "Compiling...";
                statusColor = m_uiThemeColors.IconColor;
                break;
            case CompileStatus::Success:
                statusText = "OK";
                statusColor = m_uiThemeColors.ButtonIconColor;
                break;
            case CompileStatus::Error:
                statusText = "Error";
                statusColor = m_uiThemeColors.ActivePanelTitleColor;
                break;
        }

        const bool editorFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        const bool ctrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl);
        const bool ctrlEnterPressed = editorFocused && ctrlDown && ImGui::IsKeyPressed(ImGuiKey_Enter);
        if (LabeledActionButton("CompileShader", OpenFontIcons::kPlay, "Compile", "Compile (Ctrl+Enter / F7)", ImVec2(120.0f, 0.0f)) ||
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
                            m_playbackBlockedByCompileError = true;
                        } else {
                            m_shaderState.status = CompileStatus::Success;
                            m_shaderState.lastCompiledText = m_shaderState.text;
                            m_playbackBlockedByCompileError = false;
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
                            m_scenes[m_activeSceneIndex].compiledShaderBytes = m_previewRenderer->GetLastCompiledPixelShaderSize();
                            m_scenes[m_activeSceneIndex].isDirty = false;
                        }

                        m_shaderState.status = CompileStatus::Success;
                        m_shaderState.lastCompiledText = m_shaderState.text;
                        m_playbackBlockedByCompileError = false;
                    } else {
                        if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                            m_scenes[m_activeSceneIndex].compiledShaderBytes = 0;
                        }
                        m_shaderState.status = CompileStatus::Error;
                        m_playbackBlockedByCompileError = true;
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
        if (m_shaderState.status == CompileStatus::Success) {
            size_t compiledBytes = 0;
            if (m_currentMode == UIMode::PostFX) {
                if (m_postFxSelectedIndex >= 0 && m_postFxSelectedIndex < (int)m_postFxDraftChain.size()) {
                    compiledBytes = m_postFxDraftChain[m_postFxSelectedIndex].compiledShaderBytes;
                }
            } else if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
                compiledBytes = m_scenes[m_activeSceneIndex].compiledShaderBytes;
            }

            if (compiledBytes > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", FormatByteSize(compiledBytes).c_str());
            }
        }

        // Theme selector + code font size selector
        ImGui::SameLine();
        const float themeWidth = 150.0f;
        const float fontSizeWidth = 96.0f;
        const float rightPad = 10.0f;
        const float totalControlsWidth = themeWidth + fontSizeWidth + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - totalControlsWidth - rightPad);
        ImGui::SetNextItemWidth(themeWidth);
        const char* themeNames[] = { "Dark (Enhanced)", "Dark", "Light", "Retro Blue" };
        int currentTheme = (int)m_shaderState.theme;
        if (ImGui::Combo("##Theme", &currentTheme, themeNames, 4)) {
            m_shaderState.theme = (EditorTheme)currentTheme;
            auto ApplyPalette = [&](const TextEditor::Palette& palette) {
                m_textEditor.SetPalette(palette);
                m_snippetTextEditor.SetPalette(palette);
            };
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
                    ApplyPalette(palette);
                    break;
                case EditorTheme::DarkOriginal:
                    ApplyPalette(TextEditor::GetDarkPalette());
                    break;
                case EditorTheme::Light:
                    ApplyPalette(TextEditor::GetLightPalette());
                    break;
                case EditorTheme::RetroBlue:
                    ApplyPalette(TextEditor::GetRetroBluePalette());
                    break;
            }
            ApplyCodeEditorControlOpacity();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(fontSizeWidth);
        const char* codeSizeNames[] = { "XS", "S", "M", "L", "XL" };
        int currentCodeFontSize = (int)m_shaderState.codeFontSize;
        if (ImGui::Combo("##CodeFontSize", &currentCodeFontSize, codeSizeNames, 5)) {
            currentCodeFontSize = (std::max)(0, (std::min)(4, currentCodeFontSize));
            m_shaderState.codeFontSize = (CodeFontSize)currentCodeFontSize;
        }

        ImGui::Separator();

        // Search/Replace bar (toggle with Ctrl+F)
        if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_F)) {
            m_shaderState.showSearchReplace = !m_shaderState.showSearchReplace;
        }

        if (m_shaderState.showSearchReplace) {
            ImVec4 searchBarBg = m_uiThemeColors.ControlBackground;
            searchBarBg.w = (std::clamp)(m_uiThemeColors.ControlOpacity, 0.0f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, searchBarBg);
            ImGui::BeginChild("SearchBar", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3.5f), true);

            // Search field
            ImGui::Text("Find:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-130);
            // Set focus on search field when first opened
            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputText("##Search", m_shaderState.searchBuffer, sizeof(m_shaderState.searchBuffer));
            ImGui::SameLine();
            if (LabeledActionButton("FindNext", OpenFontIcons::kSearch, "Next", "Find next", ImVec2(120.0f, 0.0f))) {
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
            ImGui::SetNextItemWidth(-130);
            ImGui::InputText("##Replace", m_shaderState.replaceBuffer, sizeof(m_shaderState.replaceBuffer));
            ImGui::SameLine();
            if (LabeledActionButton("ReplaceAll", OpenFontIcons::kRefresh, "Replace All", "Replace all", ImVec2(120.0f, 0.0f))) {
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
            if (LabeledActionButton("CloseSearch", OpenFontIcons::kX, "Close", "Close search", ImVec2(120.0f, 0.0f))) {
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
        m_textEditor.SetHandleMouseInputs(true);
        int codeFontIndex = (std::max)(0, (std::min)(4, (int)m_shaderState.codeFontSize));
        ImFont* activeCodeFont = m_fontCodeSizes[codeFontIndex] ? m_fontCodeSizes[codeFontIndex] : m_fontCode;
        if (activeCodeFont) {
            ImGui::PushFont(activeCodeFont);
        }
        m_textEditor.Render("##ShaderCode", ImVec2(-1, -statusBarHeight), true);
        if (activeCodeFont) {
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
                 ImGui::TextColored(m_uiThemeColors.ButtonIconColor, "Compilation Successful.");
            }
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, m_uiThemeColors.ActivePanelTitleColor);
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

        // 4. Split Left Stack into Metadata / Scene / Audio / Log
        ImGuiID dock_left_mid = 0;
        ImGuiID dock_left_upper = 0;
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.33f, &dock_left_bottom, &dock_left);
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.50f, &dock_left_mid, &dock_left_top);
        ImGui::DockBuilderSplitNode(dock_left_top, ImGuiDir_Down, 0.60f, &dock_left_top, &dock_left_upper);

        ImGui::DockBuilderDockWindow("Transport", dock_up);
        ImGui::DockBuilderDockWindow("Demo: Playlist", dock_right_top);
        ImGui::DockBuilderDockWindow("Preview", dock_right_bottom);

        ImGui::DockBuilderDockWindow("Demo: Metadata", dock_left_top);
        ImGui::DockBuilderDockWindow("Demo: Scene Library", dock_left_upper);
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
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Left, 0.24f, &dock_left, &dock_down);

        // 3. Split Right (Preview/Diag)
        ImGui::DockBuilderSplitNode(dock_down, ImGuiDir_Right, 0.27f, &dock_right, &dock_center);

        // Split Right Stack
        ImGuiID dock_right_top = 0, dock_right_bot = 0;
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.62f, &dock_right_top, &dock_right_bot);

        // Split Left Stack
        ImGuiID dock_left_top = 0, dock_left_bot = 0;
        ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Up, 0.40f, &dock_left_top, &dock_left_bot);

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
