#include "ShaderLab/UI/ShaderLabIDE.h"
#include "ShaderLab/UI/ShaderLabIDECore/ActionWidgets.h"
#include "ShaderLab/UI/OpenFontIcons.h"
#include "ShaderLab/Core/CompilationService.h"

#include <imgui.h>

#include <algorithm>
#include <regex>
#include <sstream>
#include <string>

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

using EditorActionWidgets::LabeledActionButton;

}

void ShaderLabIDE::ShowPreviewWindow() {
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
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Aspect");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(196.0f);
    if (ImGui::Combo("##AspectRatio", &aspectIndex, aspectLabels, 3)) {
        m_aspectRatio = (aspectIndex == 1) ? AspectRatio::Ratio_16_10 : (aspectIndex == 2) ? AspectRatio::Ratio_4_3 : AspectRatio::Ratio_16_9;
    }

    if (m_currentMode == UIMode::Demo) {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float viewUbershaderButtonWidth = (std::clamp)(availableWidth, 240.0f, 340.0f);
        if (LabeledActionButton("ViewUbershader", OpenFontIcons::kCode, "View Ubershader", "Open current ubershader source and compile diagnostics", ImVec2(viewUbershaderButtonWidth, 0.0f))) {
            OpenUbershaderViewer();
        }
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

    ShowUbershaderPopup();

    ImGui::End();
}

void ShaderLabIDE::OpenUbershaderViewer() {
    if (m_currentProjectPath.empty()) {
        m_ubershaderPath.clear();
        m_ubershaderSource = "// Save the project first to generate the ubershader source from project.json.";
        m_ubershaderStatus = "No project path available";
        m_ubershaderTextEditor.SetText(m_ubershaderSource);
        m_showUbershaderPopup = true;
        ImGui::OpenPopup("Ubershader Viewer");
        return;
    }

    std::string source;
    std::string buildError;
    if (!BuildPipeline::GenerateMicroUbershaderSource(m_currentProjectPath, source, buildError)) {
        m_ubershaderPath = m_currentProjectPath;
        m_ubershaderSource = "// Failed to generate micro ubershader source from project.json.";
        m_ubershaderStatus = buildError.empty() ? "Generation failed" : buildError;
        m_ubershaderTextEditor.SetText(m_ubershaderSource);
        m_showUbershaderPopup = true;
        ImGui::OpenPopup("Ubershader Viewer");
        return;
    }

    std::string compileSource = source;
    if (compileSource.rfind("U:", 0) == 0) {
        const size_t firstNewline = compileSource.find('\n');
        if (firstNewline != std::string::npos && firstNewline + 1 < compileSource.size()) {
            compileSource = compileSource.substr(firstNewline + 1);
        }
    }

    std::string entryPoint = "main";
    std::regex entryRegex(R"(float4\s+([A-Za-z_]\w*)\s*\(\s*float2\s+fragCoord\s*,\s*float2\s+iResolution\s*,\s*float\s+iTime\s*\))");
    std::smatch match;
    if (std::regex_search(compileSource, match, entryRegex) && match.size() >= 2) {
        entryPoint = match[1].str();
    }

    std::ostringstream status;
    status << "Generated from: " << m_currentProjectPath;
    status << "\nGeneration status: OK";

    if (m_compilationService) {
        const std::vector<CompilationTextureBinding> bindings = { {0, "Texture2D"}, {1, "Texture2D"} };
        const ShaderCompileResult result = m_compilationService->CompileFromSource(
            compileSource,
            entryPoint,
            "ps_6_0",
            L"ubershader.hlsl",
            ShaderCompileMode::Live,
            bindings);

        status << "\nCompile entry: " << entryPoint;
        status << "\nCompile status: " << (result.success ? "OK" : "FAILED");
        for (size_t i = 0; i < result.diagnostics.size() && i < 12; ++i) {
            status << "\n- " << result.diagnostics[i].message;
        }
    } else {
        status << "\nCompile status: unavailable (compilation service not initialized)";
    }

    m_ubershaderPath = m_currentProjectPath;
    m_ubershaderSource = source;
    m_ubershaderStatus = status.str();
    m_ubershaderTextEditor.SetText(m_ubershaderSource);
    m_showUbershaderPopup = true;
    ImGui::OpenPopup("Ubershader Viewer");
}

void ShaderLabIDE::ShowUbershaderPopup() {
    if (!m_showUbershaderPopup) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(980.0f, 680.0f), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Ubershader Viewer", &m_showUbershaderPopup, ImGuiWindowFlags_NoCollapse)) {
        if (!m_ubershaderStatus.empty()) {
            ImGui::TextWrapped("%s", m_ubershaderStatus.c_str());
            ImGui::Separator();
        }

        ImVec2 editorSize = ImGui::GetContentRegionAvail();
        editorSize.y -= ImGui::GetFrameHeightWithSpacing() + 6.0f;
        if (editorSize.y < 120.0f) {
            editorSize.y = 120.0f;
        }
        m_ubershaderTextEditor.Render("##UbershaderSource", editorSize, true);

        if (LabeledActionButton("CloseUbershaderViewer", OpenFontIcons::kX, "Close", "Close ubershader viewer", ImVec2(110.0f, 0.0f))) {
            m_showUbershaderPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}
