#include "ShaderLab/UI/ShaderLabIDE.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "ShaderLab/UI/UIConfig.h"
#include "ShaderLab/Graphics/Device.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <imgui_internal.h>

namespace ShaderLab {

namespace {
struct PerformanceOverlayModel {
    const char* modeName = "Demo";
    float fps = 0.0f;
    float frameMs = 0.0f;
    uint32_t previewWidth = 0;
    uint32_t previewHeight = 0;
    double vramUsageGB = 0.0;
    double vramBudgetGB = 0.0;
    double vramPercent = 0.0;
    int activeCompute = 0;
    bool showComputeLine = false;
};

struct PerformanceOverlayStyle {
    float padX = 8.0f;
    float padY = 6.0f;
    float boxWidth = 320.0f;
    float barHeight = 14.0f;
    float barGapFromText = 6.0f;
    float cornerRounding = 4.0f;
    ImU32 background = IM_COL32(0, 0, 0, 176);
    ImU32 border = IM_COL32(0, 200, 200, 210);
    ImU32 barBackground = IM_COL32(35, 35, 35, 220);
    ImU32 barBorder = IM_COL32(160, 160, 160, 220);
    ImU32 barText = IM_COL32(240, 240, 240, 255);
    ImU32 vramOk = IM_COL32(60, 220, 100, 255);
    ImU32 vramWarn = IM_COL32(240, 205, 70, 255);
    ImU32 vramCritical = IM_COL32(245, 90, 90, 255);
    float vramWarnThreshold = 70.0f;
    float vramCriticalThreshold = 90.0f;
};

PerformanceOverlayStyle BuildPerformanceOverlayStyle(const UIThemeColors& theme) {
    PerformanceOverlayStyle style;
    float dpiScale = 1.0f;
    if (ImGuiViewport* viewport = ImGui::GetMainViewport()) {
        dpiScale = (std::max)(1.0f, viewport->DpiScale);
    }

    style.background = ImGui::GetColorU32(theme.PerfOverlayBackground);
    style.border = ImGui::GetColorU32(theme.PerfOverlayBorder);
    style.barBackground = ImGui::GetColorU32(theme.PerfOverlayBarBackground);
    style.barBorder = ImGui::GetColorU32(theme.PerfOverlayBarBorder);
    style.barText = ImGui::GetColorU32(theme.PerfOverlayBarText);
    style.vramOk = ImGui::GetColorU32(theme.PerfOverlayVramOk);
    style.vramWarn = ImGui::GetColorU32(theme.PerfOverlayVramWarn);
    style.vramCritical = ImGui::GetColorU32(theme.PerfOverlayVramCritical);
    style.padX *= dpiScale;
    style.padY *= dpiScale;
    style.boxWidth *= dpiScale;
    style.barHeight *= dpiScale;
    style.barGapFromText *= dpiScale;
    style.cornerRounding *= dpiScale;
    return style;
}

ImU32 GetVramUsageColor(double vramPercent, const PerformanceOverlayStyle& style) {
    if (vramPercent < static_cast<double>(style.vramWarnThreshold)) {
        return style.vramOk;
    }
    if (vramPercent < static_cast<double>(style.vramCriticalThreshold)) {
        return style.vramWarn;
    }
    return style.vramCritical;
}

void DrawPerformanceOverlay(ImDrawList* drawList,
                            const ImVec2& overlayPos,
                            const PerformanceOverlayModel& model,
                            const PerformanceOverlayStyle& style,
                            ImU32 textColor) {
    if (!drawList) {
        return;
    }

    char line0[128] = {};
    char line1[128] = {};
    char line2[128] = {};
    char line3[128] = {};
    char line4[128] = {};
    char line5[128] = {};
    char line6[128] = {};
    std::snprintf(line0, sizeof(line0), "FPS: %.1f", model.fps);
    std::snprintf(line1, sizeof(line1), "Frame: %.2f ms", model.frameMs);
    std::snprintf(line2, sizeof(line2), "Preview: %ux%u", model.previewWidth, model.previewHeight);
    std::snprintf(line3, sizeof(line3), "Mode: %s", model.modeName);
    std::snprintf(line4,
                  sizeof(line4),
                  "VRAM: %.2f / %.2f GB (%.1f%%)",
                  model.vramUsageGB,
                  model.vramBudgetGB,
                  model.vramPercent);
    std::snprintf(line5, sizeof(line5), "Compute: %d active", model.activeCompute);
    std::snprintf(line6, sizeof(line6), "Alt+D stats | Alt+V vsync");

    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const int lineCount = model.showComputeLine ? 7 : 6;
    const float boxHeight = style.padY * 2.0f +
                            lineHeight * static_cast<float>(lineCount) +
                            style.barHeight +
                            style.barGapFromText;

    const ImU32 vramColor = GetVramUsageColor(model.vramPercent, style);
    const float vramRatio = static_cast<float>((std::clamp)(model.vramPercent / 100.0, 0.0, 1.0));

    drawList->AddRectFilled(
        overlayPos,
        ImVec2(overlayPos.x + style.boxWidth, overlayPos.y + boxHeight),
        style.background,
        style.cornerRounding);
    drawList->AddRect(
        overlayPos,
        ImVec2(overlayPos.x + style.boxWidth, overlayPos.y + boxHeight),
        style.border,
        style.cornerRounding,
        0,
        1.0f);

    ImVec2 textPos(overlayPos.x + style.padX, overlayPos.y + style.padY);
    drawList->AddText(textPos, textColor, line0); textPos.y += lineHeight;
    drawList->AddText(textPos, textColor, line1); textPos.y += lineHeight;
    drawList->AddText(textPos, textColor, line2); textPos.y += lineHeight;
    drawList->AddText(textPos, textColor, line3); textPos.y += lineHeight;
    if (model.showComputeLine) {
        drawList->AddText(textPos, textColor, line5);
        textPos.y += lineHeight;
    }
    drawList->AddText(textPos, textColor, line6);

    const ImVec2 barMin(overlayPos.x + style.padX, overlayPos.y + boxHeight - style.padY - style.barHeight);
    const ImVec2 barMax(overlayPos.x + style.boxWidth - style.padX, barMin.y + style.barHeight);
    drawList->AddRectFilled(barMin, barMax, style.barBackground, 3.0f);
    const float barFillWidth = (barMax.x - barMin.x) * vramRatio;
    if (barFillWidth > 0.0f) {
        drawList->AddRectFilled(barMin, ImVec2(barMin.x + barFillWidth, barMax.y), vramColor, 3.0f);
    }
    drawList->AddRect(barMin, barMax, style.barBorder, 3.0f, 0, 1.0f);

    char barText[96] = {};
    std::snprintf(barText,
                  sizeof(barText),
                  "VRAM %.1f%%  (%.2f / %.2f GB)",
                  model.vramPercent,
                  model.vramUsageGB,
                  model.vramBudgetGB);
    const ImVec2 barTextSize = ImGui::CalcTextSize(barText);
    const ImVec2 barTextPos(
        barMin.x + ((barMax.x - barMin.x) - barTextSize.x) * 0.5f,
        barMin.y + ((barMax.y - barMin.y) - barTextSize.y) * 0.5f);
    drawList->AddText(barTextPos, style.barText, barText);
}

bool IsModifierKey(ImGuiKey key) {
    return key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
           key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
           key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt ||
           key == ImGuiKey_LeftSuper || key == ImGuiKey_RightSuper;
}

std::string BaseScreenKeyName(ImGuiKey key) {
    switch (key) {
        case ImGuiKey_Space: return "Space";
        case ImGuiKey_Tab: return "Tab";
        case ImGuiKey_Enter: return "Enter";
        case ImGuiKey_KeypadEnter: return "Numpad Enter";
        case ImGuiKey_Backspace: return "Backspace";

        case ImGuiKey_LeftCtrl:
        case ImGuiKey_RightCtrl:
            return "Ctrl";
        case ImGuiKey_LeftShift:
        case ImGuiKey_RightShift:
            return "Shift";
        case ImGuiKey_LeftAlt:
        case ImGuiKey_RightAlt:
            return "Alt";
        case ImGuiKey_LeftSuper:
        case ImGuiKey_RightSuper:
            return "Super";
        default:
            break;
    }

    const char* name = ImGui::GetKeyName(key);
    return (name && name[0] != '\0') ? std::string(name) : std::string();
}

std::string FormatScreenKeyEntry(ImGuiKey key, const ImGuiIO& io) {
    const std::string base = BaseScreenKeyName(key);
    if (base.empty()) {
        return {};
    }

    if (IsModifierKey(key)) {
        return base;
    }

    std::string out;
    if (io.KeyCtrl) out += "Ctrl+";
    if (io.KeyShift) out += "Shift+";
    if (io.KeyAlt) out += "Alt+";
    if (io.KeySuper) out += "Super+";
    out += base;
    return out;
}
}

void ShaderLabIDE::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    m_aboutTimeSeconds = ImGui::GetTime();

    ImGuiIO& io = ImGui::GetIO();
    const bool altDown = io.KeyAlt;
    const bool ctrlDown = io.KeyCtrl;
    const bool shiftDown = io.KeyShift;
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        m_shaderState.showPerformanceOverlay = !m_shaderState.showPerformanceOverlay;
    }
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        m_previewFullscreen = !m_previewFullscreen;
    }
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
        m_previewVsyncEnabled = !m_previewVsyncEnabled;
    }
    if (ctrlDown && !altDown && !io.KeySuper && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        OpenProject();
    }
    if (ctrlDown && !altDown && !io.KeySuper && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveProject();
    }
    if (ctrlDown && shiftDown && ImGui::IsKeyPressed(ImGuiKey_K, false)) {
        m_screenKeysOverlayEnabled = !m_screenKeysOverlayEnabled;
    }

    if (m_screenKeysOverlayEnabled && m_currentMode == UIMode::Scene) {
        for (int keyValue = (int)ImGuiKey_Keyboard_BEGIN; keyValue < (int)ImGuiKey_Keyboard_END; ++keyValue) {
            const ImGuiKey key = static_cast<ImGuiKey>(keyValue);
            if (!ImGui::IsKeyPressed(key, false)) {
                continue;
            }

            if (key == ImGuiKey_K && ctrlDown && shiftDown) {
                continue;
            }

            const bool isShortcutChord = io.KeyCtrl || io.KeyAlt || io.KeySuper;
            if (isShortcutChord) {
                continue;
            }

            std::string entry = FormatScreenKeyEntry(key, io);
            if (entry.empty()) {
                continue;
            }

            m_screenKeyLog.push_back(entry);
            if (m_screenKeyLog.size() > 160) {
                m_screenKeyLog.erase(m_screenKeyLog.begin(), m_screenKeyLog.begin() + (m_screenKeyLog.size() - 160));
            }
        }
    }

    // Setup fullscreen dockspace
    EnsureThemeBackgroundTexture();
    DrawThemeBackgroundTiled();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 titlebarPad(UIConfig::TitlebarPadX, UIConfig::TitlebarPadY);
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + titlebarPad.x, viewport->Pos.y + titlebarPad.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - titlebarPad.x * 2.0f, viewport->Size.y - titlebarPad.y * 2.0f));
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::SetNextWindowBgAlpha(0.0f);
    ImGui::Begin("MainDockspace", nullptr, windowFlags);
    ImGui::PopStyleVar(2);

    // Show menu bar first
    ShowMainMenuBar();
    ShowThemeEditorPopup();

    // Show mode tabs below menu bar
    UIMode pendingMode = m_currentMode;
    UIMode requestedMode = m_currentMode;
    bool forceSelect = false;
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        requestedMode = UIMode::Demo;
        forceSelect = true;
    } else if (altDown && ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        requestedMode = UIMode::Scene;
        forceSelect = true;
    } else if (altDown && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        requestedMode = UIMode::PostFX;
        forceSelect = true;
    }

    if (forceSelect) {
        pendingMode = requestedMode;
    }

    if (ImGui::BeginTabBar("ModeTabBar", ImGuiTabBarFlags_None)) {
        const bool allowTabSwitch = !forceSelect;
        ImGuiTabItemFlags demoFlags = (forceSelect && requestedMode == UIMode::Demo) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        ImGui::PushStyleColor(ImGuiCol_Text, (m_currentMode == UIMode::Demo) ? m_uiThemeColors.ActiveTabFontColor : m_uiThemeColors.PassiveTabFontColor);
        if (ImGui::BeginTabItem("Demo", nullptr, demoFlags)) {
            if (allowTabSwitch) {
                pendingMode = UIMode::Demo;
            }
            ImGui::EndTabItem();
        }
        ImGui::PopStyleColor();
        ImGuiTabItemFlags sceneFlags = (forceSelect && requestedMode == UIMode::Scene) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        ImGui::PushStyleColor(ImGuiCol_Text, (m_currentMode == UIMode::Scene) ? m_uiThemeColors.ActiveTabFontColor : m_uiThemeColors.PassiveTabFontColor);
        if (ImGui::BeginTabItem("Scene", nullptr, sceneFlags)) {
            if (allowTabSwitch) {
                pendingMode = UIMode::Scene;
            }
            ImGui::EndTabItem();
        }
        ImGui::PopStyleColor();
        ImGuiTabItemFlags postFlags = (forceSelect && requestedMode == UIMode::PostFX) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        ImGui::PushStyleColor(ImGuiCol_Text, (m_currentMode == UIMode::PostFX) ? m_uiThemeColors.ActiveTabFontColor : m_uiThemeColors.PassiveTabFontColor);
        if (ImGui::BeginTabItem("Post FX", nullptr, postFlags)) {
            if (allowTabSwitch) {
                pendingMode = UIMode::PostFX;
            }
            ImGui::EndTabItem();
        }
        ImGui::PopStyleColor();
        ImGui::EndTabBar();
    }

    m_titlebarHeight = ImGui::GetCursorPosY();

    // Create dockspace below tabs
    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Apply mode change immediately so layout + windows stay in sync
    if (pendingMode != m_currentMode) {
        m_currentMode = pendingMode;
    }

    // Build layout if mode changed or first run
    bool modeChanged = (m_currentMode != m_lastMode);
    const float kModeFlashDuration = 0.25f;
    if (!m_layoutBuilt || modeChanged) {
        BuildLayout(m_currentMode);
        m_layoutBuilt = true;
        m_lastMode = m_currentMode;
        m_modeChangeFlashSeconds = kModeFlashDuration;
    }

    ImGui::End();

    // Show transport controls
    ShowTransportControls();

    // Sync editor state on mode change to avoid cross-mode text bleeding
    if (modeChanged) {
        if (m_currentMode == UIMode::PostFX) {
            if (m_postFxSelectedIndex < 0 && !m_postFxDraftChain.empty()) {
                m_postFxSelectedIndex = 0;
            }
            SyncPostFxEditorToSelection();
        } else if (m_currentMode == UIMode::Scene || m_currentMode == UIMode::Demo) {
            SetActiveScene(m_activeSceneIndex);
        }
    }

    // Show mode-specific windows
    ShowModeWindows();

    if (m_showAbout) {
        ShowAboutWindow();
    }

    if (m_showBuildSettings) {
        ShowBuildSettingsWindow();
    }

    if (m_shaderState.showPerformanceOverlay) {
        ImGuiViewport* overlayViewport = ImGui::GetMainViewport();
        if (overlayViewport) {
            ImDrawList* fg = ImGui::GetForegroundDrawList(overlayViewport);
            const ImVec2 overlayPos(overlayViewport->WorkPos.x + 10.0f, overlayViewport->WorkPos.y + 10.0f);

            PerformanceOverlayModel overlayModel;
            overlayModel.modeName = m_currentMode == UIMode::Demo
                ? "Demo"
                : (m_currentMode == UIMode::Scene ? "Scene" : "PostFX");
            overlayModel.showComputeLine = (m_currentMode == UIMode::PostFX);
            if (overlayModel.showComputeLine) {
                for (const auto& fx : m_computeEffectDraftChain) {
                    if (fx.enabled) {
                        ++overlayModel.activeCompute;
                    }
                }
            }

            overlayModel.fps = ImGui::GetIO().Framerate;
            overlayModel.frameMs = (overlayModel.fps > 0.0f) ? (1000.0f / overlayModel.fps) : 0.0f;
            overlayModel.previewWidth = m_previewTextureWidth;
            overlayModel.previewHeight = m_previewTextureHeight;
            if (m_deviceRef) {
                const auto mem = m_deviceRef->GetVideoMemoryInfo();
                constexpr double kBytesPerGB = 1024.0 * 1024.0 * 1024.0;
                overlayModel.vramUsageGB = static_cast<double>(mem.usage) / kBytesPerGB;
                overlayModel.vramBudgetGB = static_cast<double>(mem.budget) / kBytesPerGB;
                if (mem.budget > 0) {
                    overlayModel.vramPercent = (static_cast<double>(mem.usage) * 100.0) / static_cast<double>(mem.budget);
                }
            }

            const PerformanceOverlayStyle overlayStyle = BuildPerformanceOverlayStyle(m_uiThemeColors);
            DrawPerformanceOverlay(
                fg,
                overlayPos,
                overlayModel,
                overlayStyle,
                ImGui::GetColorU32(m_uiThemeColors.PerfOverlayFontColor));
        }
    }

    if (m_modeChangeFlashSeconds > 0.0f) {
        m_modeChangeFlashSeconds = (std::max)(0.0f, m_modeChangeFlashSeconds - ImGui::GetIO().DeltaTime);
        float t = (kModeFlashDuration > 0.0f) ? (m_modeChangeFlashSeconds / kModeFlashDuration) : 0.0f;
        float alpha = 0.35f * t;

        ImVec4 color = ImVec4(0.0f, 0.75f, 0.75f, alpha);
        if (m_currentMode == UIMode::Demo) {
            color = ImVec4(0.2f, 0.6f, 1.0f, alpha);
        } else if (m_currentMode == UIMode::PostFX) {
            color = ImVec4(1.0f, 0.6f, 0.2f, alpha);
        }

        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImVec2 min = mainViewport->WorkPos;
        ImVec2 max = ImVec2(min.x + mainViewport->WorkSize.x, min.y + mainViewport->WorkSize.y);
        ImU32 col = ImGui::GetColorU32(color);
        ImGui::GetForegroundDrawList()->AddRect(min, max, col, 0.0f, 0, 3.0f);
    }

    UpdateBuildLogic();
}

void ShaderLabIDE::EndFrame() {
    ImGui::Render();
}

} // namespace ShaderLab
