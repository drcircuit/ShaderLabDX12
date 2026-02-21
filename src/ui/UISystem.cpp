// Force include standard headers first to avoid namespace pollution
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ratio>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <fstream>
#include <system_error>
#include <climits>
#include <future>
#include <mutex>
#include <cstdint>

#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UIConfig.h"
#include "ShaderLab/UI/OpenFontIcons.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Audio/AudioSystem.h"
#include "ShaderLab/DevKit/BuildPipeline.h"
#include "ShaderLab/DevKit/RuntimeExporter.h"
#include "ShaderLab/Core/Serializer.h"
#include <nlohmann/json.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <imgui_internal.h>

namespace ShaderLab {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
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

fs::path GetGlobalSnippetBaseDir(const std::string& appRoot) {
    fs::path baseDir;
    char* appData = nullptr;
    size_t appDataLen = 0;
    if (_dupenv_s(&appData, &appDataLen, "APPDATA") == 0 && appData && *appData) {
        baseDir = fs::path(appData) / "ShaderLab";
    } else {
        baseDir = fs::path(appRoot) / ".shaderlab";
    }
    if (appData) {
        free(appData);
    }
    return baseDir;
}

fs::path GetGlobalSnippetDirectory(const std::string& appRoot) {
    return GetGlobalSnippetBaseDir(appRoot) / "snippets";
}

fs::path GetUiSettingsPath(const std::string& appRoot) {
    return GetGlobalSnippetBaseDir(appRoot) / "ui_settings.json";
}

SizeTargetPreset ParseSizePresetFromString(const std::string& value) {
    if (value == "1k" || value == "2k" || value == "4k" || value == "16k" || value == "32k") return SizeTargetPreset::K64;
    if (value == "64k") return SizeTargetPreset::K64;
    if (value == "128k") return SizeTargetPreset::K128;
    if (value == "256k") return SizeTargetPreset::K256;
    if (value == "512k") return SizeTargetPreset::K512;
    if (value == "1024k" || value == "1m") return SizeTargetPreset::K1024;
    return SizeTargetPreset::None;
}

std::string SizePresetToString(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K64: return "64k";
        case SizeTargetPreset::K128: return "128k";
        case SizeTargetPreset::K256: return "256k";
        case SizeTargetPreset::K512: return "512k";
        case SizeTargetPreset::K1024: return "1024k";
        default: return "none";
    }
}

BuildTargetKind ParseBuildTargetKindFromString(const std::string& value) {
    if (value == "packaged") return BuildTargetKind::PackagedDemo;
    if (value == "selfcontained-screensaver") return BuildTargetKind::SelfContainedScreenSaver;
    if (value == "micro") return BuildTargetKind::MicroDemo;
    return BuildTargetKind::SelfContainedDemo;
}

std::string BuildTargetKindToString(BuildTargetKind kind) {
    switch (kind) {
        case BuildTargetKind::PackagedDemo: return "packaged";
        case BuildTargetKind::SelfContainedScreenSaver: return "selfcontained-screensaver";
        case BuildTargetKind::MicroDemo: return "micro";
        default: return "selfcontained";
    }
}

ImVec4 JsonToColor(const json& value, const ImVec4& fallback) {
    if (!value.is_array() || value.size() < 4) {
        return fallback;
    }
    ImVec4 color = fallback;
    for (size_t i = 0; i < 4; ++i) {
        if (value[i].is_number()) {
            (&color.x)[i] = value[i].get<float>();
        }
    }
    return color;
}

json ColorToJson(const ImVec4& color) {
    return json::array({ color.x, color.y, color.z, color.w });
}

UIThemeColors DefaultUiThemeColors() {
    return UIThemeColors{};
}

std::vector<NamedUITheme> BuiltInThemes() {
    std::vector<NamedUITheme> themes;

    {
        NamedUITheme t;
        t.name = "ShaderPunk";
        t.colors = DefaultUiThemeColors();
        t.colors.PanelOpacity = 0.90f;
        t.colors.PanelHeadingOpacity = 0.95f;
        themes.push_back(t);
    }

    {
        NamedUITheme t;
        t.name = "SandTracker";
        t.colors = DefaultUiThemeColors();
        t.colors.LinesAccentColorDim = ImVec4(0.46f, 0.36f, 0.23f, 0.75f);
        t.colors.ControlBackground = ImVec4(0.22f, 0.18f, 0.13f, 1.0f);
        t.colors.ControlFontColor = ImVec4(0.96f, 0.90f, 0.76f, 1.0f);
        t.colors.IconColor = ImVec4(0.92f, 0.75f, 0.43f, 1.0f);
        t.colors.ButtonIconColor = ImVec4(0.97f, 0.84f, 0.51f, 1.0f);
        t.colors.ButtonLabelColor = ImVec4(0.94f, 0.88f, 0.72f, 1.0f);
        t.colors.ButtonBackgroundColor = ImVec4(0.28f, 0.22f, 0.15f, 1.0f);
        t.colors.PanelBackground = ImVec4(0.15f, 0.12f, 0.09f, 0.95f);
        t.colors.WindowBackground = ImVec4(0.11f, 0.09f, 0.07f, 1.0f);
        t.colors.PanelTitleBackground = ImVec4(0.24f, 0.18f, 0.12f, 1.0f);
        t.colors.LogoFontColor = ImVec4(0.93f, 0.77f, 0.45f, 1.0f);
        t.colors.PanelOpacity = 0.86f;
        t.colors.PanelHeadingOpacity = 0.94f;
        themes.push_back(t);
    }

    {
        NamedUITheme t;
        t.name = "16BitWarrior";
        t.colors = DefaultUiThemeColors();
        t.colors.LinesAccentColorDim = ImVec4(0.22f, 0.77f, 0.42f, 0.75f);
        t.colors.ControlBackground = ImVec4(0.07f, 0.14f, 0.10f, 1.0f);
        t.colors.ControlFontColor = ImVec4(0.74f, 1.0f, 0.70f, 1.0f);
        t.colors.IconColor = ImVec4(0.37f, 1.0f, 0.58f, 1.0f);
        t.colors.ButtonBackgroundColor = ImVec4(0.12f, 0.24f, 0.16f, 1.0f);
        t.colors.PanelBackground = ImVec4(0.04f, 0.11f, 0.07f, 0.95f);
        t.colors.WindowBackground = ImVec4(0.03f, 0.08f, 0.05f, 1.0f);
        t.colors.PanelTitleBackground = ImVec4(0.08f, 0.20f, 0.12f, 1.0f);
        t.colors.ActiveTabBackground = ImVec4(0.14f, 0.40f, 0.23f, 1.0f);
        t.colors.LogoFontColor = ImVec4(0.44f, 1.0f, 0.65f, 1.0f);
        t.colors.PanelOpacity = 0.84f;
        t.colors.PanelHeadingOpacity = 0.92f;
        themes.push_back(t);
    }

    {
        NamedUITheme t;
        t.name = "CodeNinja";
        t.colors = DefaultUiThemeColors();
        t.colors.LinesAccentColorDim = ImVec4(0.24f, 0.25f, 0.27f, 0.75f);
        t.colors.ControlBackground = ImVec4(0.09f, 0.10f, 0.11f, 1.0f);
        t.colors.ControlFontColor = ImVec4(0.87f, 0.89f, 0.92f, 1.0f);
        t.colors.IconColor = ImVec4(0.61f, 0.67f, 0.74f, 1.0f);
        t.colors.ButtonIconColor = ImVec4(0.86f, 0.93f, 1.0f, 1.0f);
        t.colors.ButtonBackgroundColor = ImVec4(0.14f, 0.15f, 0.17f, 1.0f);
        t.colors.PanelBackground = ImVec4(0.07f, 0.08f, 0.10f, 0.95f);
        t.colors.WindowBackground = ImVec4(0.05f, 0.06f, 0.08f, 1.0f);
        t.colors.PanelTitleBackground = ImVec4(0.11f, 0.12f, 0.15f, 1.0f);
        t.colors.ActivePanelTitleColor = ImVec4(0.23f, 0.25f, 0.31f, 1.0f);
        t.colors.LogoFontColor = ImVec4(0.84f, 0.90f, 0.98f, 1.0f);
        t.colors.PanelOpacity = 0.88f;
        t.colors.PanelHeadingOpacity = 0.95f;
        themes.push_back(t);
    }

    {
        NamedUITheme t;
        t.name = "PinkHack";
        t.colors = DefaultUiThemeColors();
        t.colors.LinesAccentColorDim = ImVec4(0.72f, 0.18f, 0.50f, 0.80f);
        t.colors.ControlBackground = ImVec4(0.20f, 0.07f, 0.16f, 1.0f);
        t.colors.ControlFontColor = ImVec4(1.0f, 0.86f, 0.95f, 1.0f);
        t.colors.IconColor = ImVec4(1.0f, 0.45f, 0.86f, 1.0f);
        t.colors.ButtonIconColor = ImVec4(1.0f, 0.66f, 0.92f, 1.0f);
        t.colors.ButtonBackgroundColor = ImVec4(0.30f, 0.10f, 0.24f, 1.0f);
        t.colors.PanelBackground = ImVec4(0.12f, 0.04f, 0.10f, 0.95f);
        t.colors.WindowBackground = ImVec4(0.09f, 0.03f, 0.08f, 1.0f);
        t.colors.PanelTitleBackground = ImVec4(0.24f, 0.08f, 0.20f, 1.0f);
        t.colors.ActiveTabBackground = ImVec4(0.52f, 0.16f, 0.42f, 1.0f);
        t.colors.LogoFontColor = ImVec4(1.0f, 0.58f, 0.89f, 1.0f);
        t.colors.PanelOpacity = 0.84f;
        t.colors.PanelHeadingOpacity = 0.93f;
        themes.push_back(t);
    }

    {
        NamedUITheme t;
        t.name = "Galaxy";
        t.colors = DefaultUiThemeColors();
        t.colors.LinesAccentColorDim = ImVec4(0.32f, 0.36f, 0.69f, 0.72f);
        t.colors.ControlBackground = ImVec4(0.08f, 0.09f, 0.21f, 1.0f);
        t.colors.ControlFontColor = ImVec4(0.86f, 0.90f, 1.0f, 1.0f);
        t.colors.IconColor = ImVec4(0.51f, 0.72f, 1.0f, 1.0f);
        t.colors.ButtonBackgroundColor = ImVec4(0.12f, 0.13f, 0.29f, 1.0f);
        t.colors.PanelBackground = ImVec4(0.05f, 0.06f, 0.14f, 0.95f);
        t.colors.WindowBackground = ImVec4(0.03f, 0.04f, 0.10f, 1.0f);
        t.colors.PanelTitleBackground = ImVec4(0.10f, 0.12f, 0.30f, 1.0f);
        t.colors.ActiveTabBackground = ImVec4(0.20f, 0.30f, 0.62f, 1.0f);
        t.colors.LogoFontColor = ImVec4(0.69f, 0.82f, 1.0f, 1.0f);
        t.colors.BackgroundImage = "editor_assets/themes/galaxy_tile.ppm";
        t.colors.PanelOpacity = 0.80f;
        t.colors.PanelHeadingOpacity = 0.92f;
        themes.push_back(t);
    }

    {
        NamedUITheme t;
        t.name = "Noizr";
        t.colors = DefaultUiThemeColors();
        t.colors.LinesAccentColorDim = ImVec4(0.25f, 0.26f, 0.27f, 0.65f);
        t.colors.ControlBackground = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
        t.colors.ControlFontColor = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        t.colors.IconColor = ImVec4(0.72f, 0.76f, 0.80f, 1.0f);
        t.colors.ButtonBackgroundColor = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
        t.colors.PanelBackground = ImVec4(0.07f, 0.07f, 0.07f, 0.95f);
        t.colors.WindowBackground = ImVec4(0.04f, 0.04f, 0.04f, 1.0f);
        t.colors.PanelTitleBackground = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
        t.colors.LogoFontColor = ImVec4(0.88f, 0.88f, 0.88f, 1.0f);
        t.colors.BackgroundImage = "editor_assets/themes/noizr_tile.ppm";
        t.colors.PanelOpacity = 0.78f;
        t.colors.PanelHeadingOpacity = 0.90f;
        themes.push_back(t);
    }

    return themes;
}

void ApplyThemeColorFromJson(UIThemeColors& colors, const json& src) {
    colors.LinesAccentColorDim = JsonToColor(src.value("LinesAccentColorDim", json::array()), colors.LinesAccentColorDim);
    colors.ControlBackground = JsonToColor(src.value("ControlBackground", json::array()), colors.ControlBackground);
    colors.ControlFontColor = JsonToColor(src.value("ControlFontColor", json::array()), colors.ControlFontColor);
    colors.IconColor = JsonToColor(src.value("IconColor", json::array()), colors.IconColor);
    colors.ButtonIconColor = JsonToColor(src.value("ButtonIconColor", json::array()), colors.ButtonIconColor);
    colors.ButtonLabelColor = JsonToColor(src.value("ButtonLabelColor", json::array()), colors.ButtonLabelColor);
    colors.ButtonBackgroundColor = JsonToColor(src.value("ButtonBackgroundColor", json::array()), colors.ButtonBackgroundColor);
    colors.PanelBackground = JsonToColor(src.value("PanelBackground", json::array()), colors.PanelBackground);
    colors.WindowBackground = JsonToColor(src.value("WindowBackground", json::array()), colors.WindowBackground);
    colors.TrackerHeadingBackground = JsonToColor(src.value("TrackerHeadingBackground", json::array()), colors.TrackerHeadingBackground);
    colors.TrackerHeadingFontColor = JsonToColor(src.value("TrackerHeadingFontColor", json::array()), colors.TrackerHeadingFontColor);
    colors.TrackerAccentBeatBackground = JsonToColor(src.value("TrackerAccentBeatBackground", json::array()), colors.TrackerAccentBeatBackground);
    colors.TrackerAccentBeatFontColor = JsonToColor(src.value("TrackerAccentBeatFontColor", json::array()), colors.TrackerAccentBeatFontColor);
    colors.TrackerBeatBackground = JsonToColor(src.value("TrackerBeatBackground", json::array()), colors.TrackerBeatBackground);
    colors.TrackerBeatFontColor = JsonToColor(src.value("TrackerBeatFontColor", json::array()), colors.TrackerBeatFontColor);
    colors.LabelFontColor = JsonToColor(src.value("LabelFontColor", json::array()), colors.LabelFontColor);
    colors.PanelTitleFontColor = JsonToColor(src.value("PanelTitleFontColor", json::array()), colors.PanelTitleFontColor);
    colors.PanelTitleBackground = JsonToColor(src.value("PanelTitleBackground", json::array()), colors.PanelTitleBackground);
    colors.ActiveTabFontColor = JsonToColor(src.value("ActiveTabFontColor", json::array()), colors.ActiveTabFontColor);
    colors.ActiveTabBackground = JsonToColor(src.value("ActiveTabBackground", json::array()), colors.ActiveTabBackground);
    colors.PassiveTabFontColor = JsonToColor(src.value("PassiveTabFontColor", json::array()), colors.PassiveTabFontColor);
    colors.PassiveTabBackground = JsonToColor(src.value("PassiveTabBackground", json::array()), colors.PassiveTabBackground);
    colors.ActivePanelTitleColor = JsonToColor(src.value("ActivePanelTitleColor", json::array()), colors.ActivePanelTitleColor);
    colors.ActivePanelBackground = JsonToColor(src.value("ActivePanelBackground", json::array()), colors.ActivePanelBackground);
    colors.PassivePanelTitleColor = JsonToColor(src.value("PassivePanelTitleColor", json::array()), colors.PassivePanelTitleColor);
    colors.PassivePanelBackground = JsonToColor(src.value("PassivePanelBackground", json::array()), colors.PassivePanelBackground);
    colors.LogoFontColor = JsonToColor(src.value("LogoFontColor", json::array()), colors.LogoFontColor);
    colors.ConsoleFontColor = JsonToColor(src.value("ConsoleFontColor", json::array()), colors.ConsoleFontColor);
    colors.ConsoleBackground = JsonToColor(src.value("ConsoleBackground", json::array()), colors.ConsoleBackground);
    colors.StatusFontColor = JsonToColor(src.value("StatusFontColor", json::array()), colors.StatusFontColor);
    if (src.contains("ControlOpacity") && src["ControlOpacity"].is_number()) {
        colors.ControlOpacity = (std::clamp)(src["ControlOpacity"].get<float>(), 0.0f, 1.0f);
    }
    if (src.contains("PanelOpacity") && src["PanelOpacity"].is_number()) {
        colors.PanelOpacity = (std::clamp)(src["PanelOpacity"].get<float>(), 0.0f, 1.0f);
    }
    if (src.contains("PanelHeadingOpacity") && src["PanelHeadingOpacity"].is_number()) {
        colors.PanelHeadingOpacity = (std::clamp)(src["PanelHeadingOpacity"].get<float>(), 0.0f, 1.0f);
    }
    colors.BackgroundImage = src.value("BackgroundImage", colors.BackgroundImage);
}

json ThemeColorsToJson(const UIThemeColors& colors) {
    return {
        {"LinesAccentColorDim", ColorToJson(colors.LinesAccentColorDim)},
        {"ControlBackground", ColorToJson(colors.ControlBackground)},
        {"ControlFontColor", ColorToJson(colors.ControlFontColor)},
        {"IconColor", ColorToJson(colors.IconColor)},
        {"ButtonIconColor", ColorToJson(colors.ButtonIconColor)},
        {"ButtonLabelColor", ColorToJson(colors.ButtonLabelColor)},
        {"ButtonBackgroundColor", ColorToJson(colors.ButtonBackgroundColor)},
        {"PanelBackground", ColorToJson(colors.PanelBackground)},
        {"WindowBackground", ColorToJson(colors.WindowBackground)},
        {"TrackerHeadingBackground", ColorToJson(colors.TrackerHeadingBackground)},
        {"TrackerHeadingFontColor", ColorToJson(colors.TrackerHeadingFontColor)},
        {"TrackerAccentBeatBackground", ColorToJson(colors.TrackerAccentBeatBackground)},
        {"TrackerAccentBeatFontColor", ColorToJson(colors.TrackerAccentBeatFontColor)},
        {"TrackerBeatBackground", ColorToJson(colors.TrackerBeatBackground)},
        {"TrackerBeatFontColor", ColorToJson(colors.TrackerBeatFontColor)},
        {"LabelFontColor", ColorToJson(colors.LabelFontColor)},
        {"PanelTitleFontColor", ColorToJson(colors.PanelTitleFontColor)},
        {"PanelTitleBackground", ColorToJson(colors.PanelTitleBackground)},
        {"ActiveTabFontColor", ColorToJson(colors.ActiveTabFontColor)},
        {"ActiveTabBackground", ColorToJson(colors.ActiveTabBackground)},
        {"PassiveTabFontColor", ColorToJson(colors.PassiveTabFontColor)},
        {"PassiveTabBackground", ColorToJson(colors.PassiveTabBackground)},
        {"ActivePanelTitleColor", ColorToJson(colors.ActivePanelTitleColor)},
        {"ActivePanelBackground", ColorToJson(colors.ActivePanelBackground)},
        {"PassivePanelTitleColor", ColorToJson(colors.PassivePanelTitleColor)},
        {"PassivePanelBackground", ColorToJson(colors.PassivePanelBackground)},
        {"LogoFontColor", ColorToJson(colors.LogoFontColor)},
        {"ConsoleFontColor", ColorToJson(colors.ConsoleFontColor)},
        {"ConsoleBackground", ColorToJson(colors.ConsoleBackground)},
        {"StatusFontColor", ColorToJson(colors.StatusFontColor)},
        {"ControlOpacity", colors.ControlOpacity},
        {"PanelOpacity", colors.PanelOpacity},
        {"PanelHeadingOpacity", colors.PanelHeadingOpacity},
        {"BackgroundImage", colors.BackgroundImage}
    };
}

void LoadUiBuildSettings(
    const std::string& appRoot,
    BuildTargetKind& targetKind,
    BuildMode& mode,
    SizeTargetPreset& sizeTarget,
    bool& restrictedCompactTrack,
    bool& runtimeDebugLog,
    bool& compactTrackDebugLog,
    bool& microDeveloperBuild,
    std::string& cleanSolutionRootPath,
    std::string& crinklerPath) {
    const fs::path settingsPath = GetUiSettingsPath(appRoot);
    std::ifstream in(settingsPath, std::ios::binary);
    if (!in.is_open()) {
        return;
    }

    json root;
    try {
        in >> root;
    } catch (...) {
        return;
    }

    const json build = root.value("build", json::object());
    targetKind = ParseBuildTargetKindFromString(build.value("targetKind", std::string("selfcontained")));
    const std::string modeStr = build.value("mode", std::string("release"));
    mode = (modeStr == "crinkled" || modeStr == "release-crinkled")
        ? BuildMode::ReleaseCrinkled
        : BuildMode::Release;

    sizeTarget = ParseSizePresetFromString(build.value("sizeTarget", std::string("none")));
    restrictedCompactTrack = build.value("restrictedCompactTrack", false);
    runtimeDebugLog = build.value("runtimeDebugLog", false);
    compactTrackDebugLog = build.value("compactTrackDebugLog", false);
    microDeveloperBuild = build.value("microDeveloperBuild", false);
    cleanSolutionRootPath = build.value("cleanSolutionRootPath", std::string());
    crinklerPath = build.value("crinklerPath", std::string());

    if (!crinklerPath.empty()) {
        SetEnvironmentVariableA("SHADERLAB_CRINKLER", crinklerPath.c_str());
    }
}

void SaveUiBuildSettings(
    const std::string& appRoot,
    BuildTargetKind targetKind,
    BuildMode mode,
    SizeTargetPreset sizeTarget,
    bool restrictedCompactTrack,
    bool runtimeDebugLog,
    bool compactTrackDebugLog,
    bool microDeveloperBuild,
    const std::string& cleanSolutionRootPath,
    const std::string& crinklerPath) {
    const fs::path settingsPath = GetUiSettingsPath(appRoot);
    std::error_code ec;
    fs::create_directories(settingsPath.parent_path(), ec);

    json root;
    std::ifstream in(settingsPath, std::ios::binary);
    if (in.is_open()) {
        try {
            in >> root;
        } catch (...) {
            root = json::object();
        }
    }

    json build;
    build["targetKind"] = BuildTargetKindToString(targetKind);
    build["mode"] = (mode == BuildMode::ReleaseCrinkled) ? "crinkled" : "release";
    build["sizeTarget"] = SizePresetToString(sizeTarget);
    build["restrictedCompactTrack"] = restrictedCompactTrack;
    build["runtimeDebugLog"] = runtimeDebugLog;
    build["compactTrackDebugLog"] = compactTrackDebugLog;
    build["microDeveloperBuild"] = microDeveloperBuild;
    build["cleanSolutionRootPath"] = cleanSolutionRootPath;
    build["crinklerPath"] = crinklerPath;
    root["build"] = build;

    std::ofstream out(settingsPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << root.dump(2);
}

std::string SanitizeSnippetFileStem(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-';
        result.push_back(ok ? c : '_');
    }
    if (result.empty()) {
        result = "Folder";
    }
    return result;
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

const char* BuildModeLabel(BuildMode mode) {
    return mode == BuildMode::ReleaseCrinkled ? "Release Crinkled" : "Release";
}

const char* SizePresetLabel(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K64: return "64K";
        case SizeTargetPreset::K128: return "128K";
        case SizeTargetPreset::K256: return "256K";
        case SizeTargetPreset::K512: return "512K";
        case SizeTargetPreset::K1024: return "1024K";
        default: return "None";
    }
}

uint64_t SizePresetBytes(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K64: return 64ull * 1024ull;
        case SizeTargetPreset::K128: return 128ull * 1024ull;
        case SizeTargetPreset::K256: return 256ull * 1024ull;
        case SizeTargetPreset::K512: return 512ull * 1024ull;
        case SizeTargetPreset::K1024: return 1024ull * 1024ull;
        default: return 0ull;
    }
}

void OpenExternal(const std::string& target) {
    ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
}

UISystem::UISystem() {
    // Resolve application root from executable location first (supports installed/portable layouts).
    char exePath[MAX_PATH] = {};
    DWORD exePathLen = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (exePathLen > 0 && exePathLen < MAX_PATH) {
        m_appRoot = fs::path(std::string(exePath, exePathLen)).parent_path().string();
    }
    if (m_appRoot.empty()) {
        m_appRoot = fs::current_path().string();
    }

    LoadUiBuildSettings(
        m_appRoot,
        m_buildSettingsTargetKind,
        m_buildSettingsMode,
        m_buildSettingsSizeTarget,
        m_buildSettingsRestrictedCompactTrack,
        m_buildSettingsRuntimeDebugLog,
        m_buildSettingsCompactTrackDebugLog,
        m_buildSettingsMicroDeveloperBuild,
        m_buildSettingsCleanSolutionRootPath,
        m_buildSettingsCrinklerPath);

    LoadUiThemeSettings();

    CreateDefaultScene();
    CreateDefaultTrack();

    // Initialize custom HLSL language definition to ensure everything works
    TextEditor::LanguageDefinition langDef;

    static const char* const keywords[] = {
        "AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
        "CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
        "export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
        "linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
        "pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
        "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
        "static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
        "Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
        "VertexShader", "void", "volatile", "while",
        "bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
        "uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
        "float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
        "float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
        "half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
        "half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4",
    };

    for (auto& k : keywords) {
        langDef.mKeywords.insert(k);
    }

    static const char* const identifiers[] = {
        "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint",
        "asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx",
        "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
        "distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2",
        "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount",
        "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange",
        "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan",
        "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf",
        "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
        "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin",
        "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step",
        "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj",
        "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
    };

    for (auto& k : identifiers) {
        TextEditor::Identifier id;
        id.mDeclaration = "Built-in function";
        langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", TextEditor::PaletteIndex::Punctuation));

    langDef.mCommentStart = "/*";
    langDef.mCommentEnd = "*/";
    langDef.mSingleLineComment = "//";
    langDef.mCaseSensitive = true;
    langDef.mAutoIndentation = true;
    langDef.mName = "HLSL";

    m_textEditor.SetLanguageDefinition(langDef);
    m_snippetTextEditor.SetLanguageDefinition(langDef);

    // Create enhanced dark palette with vivid colors (IM_COL32 AABBGGRR format)
    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Keyword] = 0xffd69c56;         // #569cd6 (Blue)
    palette[(int)TextEditor::PaletteIndex::KnownIdentifier] = 0xffb0c94e; // #4ec9b0 (Teal)
    palette[(int)TextEditor::PaletteIndex::Number] = 0xffa8ceb5;          // #b5cea8 (Light Green)
    palette[(int)TextEditor::PaletteIndex::String] = 0xff7891ce;          // #ce9178 (Orange)
    palette[(int)TextEditor::PaletteIndex::Comment] = 0xff55996a;         // #6a9955 (Green)
    palette[(int)TextEditor::PaletteIndex::MultiLineComment] = 0xff55996a;// #6a9955
    palette[(int)TextEditor::PaletteIndex::Identifier] = 0xffdcdcdc;      // #dcdcdc (White/Grey)
    palette[(int)TextEditor::PaletteIndex::Punctuation] = 0xffdcdcdc;
    palette[(int)TextEditor::PaletteIndex::Preprocessor] = 0xff9b9b9b;
    m_textEditor.SetPalette(palette);
    m_snippetTextEditor.SetPalette(palette);
    ApplyCodeEditorControlOpacity();

    m_textEditor.SetShowWhitespaces(false);
    m_snippetTextEditor.SetShowWhitespaces(false);
    m_snippetTextEditor.SetReadOnly(true);

    LoadGlobalSnippets();
}

std::string UISystem::GetProjectName() const {
    if (m_currentProjectPath.empty()) {
        return "untitled";
    }

    fs::path path(m_currentProjectPath);
    std::string stem = path.stem().string();
    return stem.empty() ? "untitled" : stem;
}

UISystem::~UISystem() {
    Shutdown();
}

void UISystem::SetActiveScene(int index) {
    if (index >= (int)m_scenes.size()) return;

    m_activeSceneIndex = index;
    if (index >= 0) {
        auto& scene = m_scenes[index];
        // Editor
        m_shaderState.text = scene.shaderCode;
        m_textEditor.SetText(scene.shaderCode);
        m_shaderState.status = CompileStatus::Clean;
    } else {
        // Clear / Null Scene
        m_shaderState.text = "// No Active Scene";
        m_textEditor.SetText(m_shaderState.text);
        m_shaderState.status = CompileStatus::Clean;
    }
}

void UISystem::SyncPostFxEditorToSelection() {
    if (m_postFxSelectedIndex < 0 || m_postFxSelectedIndex >= (int)m_postFxDraftChain.size()) {
        m_shaderState.text = "// No post fx selected";
        m_textEditor.SetText(m_shaderState.text);
        m_shaderState.status = CompileStatus::Clean;
        m_shaderState.diagnostics.clear();
        return;
    }

    auto& effect = m_postFxDraftChain[m_postFxSelectedIndex];
    m_shaderState.text = effect.shaderCode;
    m_textEditor.SetText(effect.shaderCode);
    m_shaderState.status = effect.isDirty ? CompileStatus::Dirty : CompileStatus::Clean;
    m_shaderState.diagnostics.clear();
}

void UISystem::AppendDemoLog(const std::string& message) {
    m_demoLog.push_back(message);
    if (m_demoLog.size() > 400) {
        m_demoLog.erase(m_demoLog.begin(), m_demoLog.begin() + (m_demoLog.size() - 400));
    }
}

bool UISystem::Initialize(HWND hwnd, Device* device, Swapchain* swapchain) {
    if (!device || !device->IsValid() || !swapchain || !hwnd) {
        return false;
    }

    m_hwnd = hwnd;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    m_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_context);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Load custom fonts for editor
    // Path to fonts relative to executable (in editor_assets/fonts/)
    std::string fontPath = m_appRoot + "/editor_assets/fonts/";
    std::string iconFontFile;
    const std::vector<fs::path> iconFontCandidates = {
        fs::path(m_appRoot) / "third_party" / "OpenFontIcons" / UIConfig::FontFileOpenFontIcons,
        fs::path(m_appRoot) / "editor_assets" / "fonts" / UIConfig::FontFileOpenFontIcons,
        fs::path(m_appRoot) / ".." / "third_party" / "OpenFontIcons" / UIConfig::FontFileOpenFontIcons
    };
    for (const auto& candidate : iconFontCandidates) {
        std::error_code ec;
        if (fs::exists(candidate, ec) && !ec) {
            iconFontFile = candidate.lexically_normal().string();
            break;
        }
    }
    
    // Hacked font for logo (large)
    m_fontHackedLogo = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileHacked).c_str(), UIConfig::FontLogo);
    if (!m_fontHackedLogo) {
        // Fallback to default if font doesn't load
        m_fontHackedLogo = io.Fonts->AddFontDefault();
    }
    
    // Hacked font for headings (medium)
    m_fontHackedHeading = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileHacked).c_str(), UIConfig::FontHeading);
    if (!m_fontHackedHeading) {
        m_fontHackedHeading = io.Fonts->AddFontDefault();
    }
    
    // Orbitron for regular text
    m_fontOrbitronText = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileOrbitron).c_str(), UIConfig::FontText);
    if (!m_fontOrbitronText) {
        m_fontOrbitronText = io.Fonts->AddFontDefault();
    }
    static const ImWchar iconRanges[] = { 0xE000, 0xE0FF, 0 };
    constexpr float iconFontScale = 1.22f;
    constexpr float iconGlyphOffsetY = 1.0f;
    ImFontConfig iconConfigText;
    iconConfigText.MergeMode = true;
    iconConfigText.PixelSnapH = true;
    iconConfigText.GlyphOffset.y = iconGlyphOffsetY;
    ImFont* iconTextMerge = nullptr;
    if (!iconFontFile.empty()) {
        iconTextMerge = io.Fonts->AddFontFromFileTTF(iconFontFile.c_str(), UIConfig::FontText * iconFontScale, &iconConfigText, iconRanges);
    }
    
    // Erbos Draco for numerical fields
    m_fontErbosDracoNumbers = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileErbosOpen).c_str(), UIConfig::FontNumeric);
    if (!m_fontErbosDracoNumbers) {
        m_fontErbosDracoNumbers = io.Fonts->AddFontDefault();
    }

    // Orbitron for menu (smaller)
    m_fontMenuSmall = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileOrbitron).c_str(), UIConfig::FontMenu);
    if (!m_fontMenuSmall) {
        m_fontMenuSmall = io.Fonts->AddFontDefault();
    }
    ImFontConfig iconConfigMenu;
    iconConfigMenu.MergeMode = true;
    iconConfigMenu.PixelSnapH = true;
    iconConfigMenu.GlyphOffset.y = iconGlyphOffsetY;
    ImFont* iconMenuMerge = nullptr;
    if (!iconFontFile.empty()) {
        iconMenuMerge = io.Fonts->AddFontFromFileTTF(iconFontFile.c_str(), UIConfig::FontMenu * iconFontScale, &iconConfigMenu, iconRanges);
    }

    (void)iconTextMerge;
    (void)iconMenuMerge;

    const float codeFontSizes[5] = { 11.0f, 12.0f, 13.0f, 15.0f, 17.0f };
    for (int i = 0; i < 5; ++i) {
        m_fontCodeSizes[i] = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileCode).c_str(), codeFontSizes[i]);
        if (!m_fontCodeSizes[i]) {
            m_fontCodeSizes[i] = io.Fonts->AddFontDefault();
        }
    }
    m_fontCode = m_fontCodeSizes[(int)CodeFontSize::M];

    m_fontCodeItalic = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileCodeItalic).c_str(), UIConfig::FontCode);
    if (!m_fontCodeItalic) {
        m_fontCodeItalic = m_fontCode;
    }

    // Build font atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    
    // Set Orbitron as the default font for the UI
    if (m_fontOrbitronText) {
        io.FontDefault = m_fontOrbitronText;
    }

    m_textEditor.SetCommentFont(m_fontCodeItalic, UIConfig::FontCode);
    m_snippetTextEditor.SetCommentFont(m_fontCodeItalic, UIConfig::FontCode);

    // Setup style
    SetupImGuiStyle();

    // Create descriptor heap for ImGui
    CreateDescriptorHeap(device);

    // Setup platform/renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(
        device->GetDevice(),
        Swapchain::BUFFER_COUNT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_srvHeap.Get(),
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvHeap->GetGPUDescriptorHandleForHeapStart()
    );

    // Store device reference for texture creation
    m_deviceRef = device;
    m_swapchainRef = swapchain;
    CreateTitlebarIconTexture();

    m_initialized = true;
    return true;
}

void UISystem::Shutdown() {
    SaveUiThemeSettings();

    SaveUiBuildSettings(
        m_appRoot,
        m_buildSettingsTargetKind,
        m_buildSettingsMode,
        m_buildSettingsSizeTarget,
        m_buildSettingsRestrictedCompactTrack,
        m_buildSettingsRuntimeDebugLog,
        m_buildSettingsCompactTrackDebugLog,
        m_buildSettingsMicroDeveloperBuild,
        m_buildSettingsCleanSolutionRootPath,
        m_buildSettingsCrinklerPath);

    if (m_initialized) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(m_context);
        m_context = nullptr;
    }

    m_previewTexture.Reset();
    m_titlebarIconTexture.Reset();
    m_titlebarIconSrvGpuHandle = {};
    m_themeBackgroundTexture.Reset();
    m_themeBackgroundSrvGpuHandle = {};
    m_themeBackgroundWidth = 0;
    m_themeBackgroundHeight = 0;
    m_loadedThemeBackgroundPath.clear();
    m_previewRtvHeap.Reset();
    m_srvHeap.Reset();
    m_initialized = false;
}

void UISystem::CreateDescriptorHeap(Device* device) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 128;  // ImGui font (0), Preview (1), Thumbnails (2+)
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap));
}

void UISystem::CreateTitlebarIconTexture() {
    m_titlebarIconTexture.Reset();
    m_titlebarIconSrvGpuHandle = {};

    if (!m_deviceRef || !m_srvHeap) {
        return;
    }

    fs::path iconPath;
    const fs::path preferred = fs::path(m_appRoot) / "editor_assets" / "shaderlab.ico.ico";
    if (fs::exists(preferred)) {
        iconPath = preferred;
    } else {
        const fs::path iconDir = fs::path(m_appRoot) / "editor_assets";
        std::error_code ec;
        if (fs::exists(iconDir, ec)) {
            for (const auto& entry : fs::directory_iterator(iconDir, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_regular_file()) {
                    continue;
                }
                const fs::path candidate = entry.path();
                if (candidate.has_extension() && candidate.extension() == ".ico") {
                    iconPath = candidate;
                    break;
                }
            }
        }
    }

    if (iconPath.empty()) {
        return;
    }

    HICON icon = static_cast<HICON>(LoadImageW(
        nullptr,
        iconPath.wstring().c_str(),
        IMAGE_ICON,
        32,
        32,
        LR_LOADFROMFILE));
    if (!icon) {
        return;
    }

    ICONINFO iconInfo = {};
    if (!GetIconInfo(icon, &iconInfo)) {
        DestroyIcon(icon);
        return;
    }

    BITMAP bitmap = {};
    GetObject(iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask, sizeof(bitmap), &bitmap);
    int width = (bitmap.bmWidth > 0) ? bitmap.bmWidth : 32;
    int height = (bitmap.bmHeight > 0) ? bitmap.bmHeight : 32;
    if (!iconInfo.hbmColor && height > 1) {
        height /= 2;
    }
    width = (std::max)(16, width);
    height = (std::max)(16, height);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = CreateCompatibleDC(nullptr);
    void* dibPixels = nullptr;
    HBITMAP dib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
    HGDIOBJ oldObject = SelectObject(hdc, dib);
    PatBlt(hdc, 0, 0, width, height, BLACKNESS);
    DrawIconEx(hdc, 0, 0, icon, width, height, 0, nullptr, DI_NORMAL);

    std::vector<unsigned char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
    if (dibPixels) {
        const unsigned char* bgra = static_cast<const unsigned char*>(dibPixels);
        for (size_t i = 0; i < static_cast<size_t>(width) * static_cast<size_t>(height); ++i) {
            rgba[i * 4 + 0] = bgra[i * 4 + 2];
            rgba[i * 4 + 1] = bgra[i * 4 + 1];
            rgba[i * 4 + 2] = bgra[i * 4 + 0];
            rgba[i * 4 + 3] = bgra[i * 4 + 3];
        }
    }

    SelectObject(hdc, oldObject);
    DeleteObject(dib);
    DeleteDC(hdc);
    if (iconInfo.hbmColor) {
        DeleteObject(iconInfo.hbmColor);
    }
    if (iconInfo.hbmMask) {
        DeleteObject(iconInfo.hbmMask);
    }
    DestroyIcon(icon);

    CreateTextureFromData(rgba.data(), width, height, 4, m_titlebarIconTexture);
    if (!m_titlebarIconTexture) {
        return;
    }

    constexpr UINT kTitlebarIconSrvIndex = 127;
    const UINT descriptorSize = m_deviceRef->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += static_cast<SIZE_T>(kTitlebarIconSrvIndex) * descriptorSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    m_deviceRef->GetDevice()->CreateShaderResourceView(m_titlebarIconTexture.Get(), &srvDesc, cpuHandle);

    m_titlebarIconSrvGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    m_titlebarIconSrvGpuHandle.ptr += static_cast<SIZE_T>(kTitlebarIconSrvIndex) * descriptorSize;
}

void UISystem::CreatePreviewTexture(uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) {
        return;
    }

    // Only recreate if size changed
    if (m_previewTexture && m_previewTextureWidth == width && m_previewTextureHeight == height) {
        return;
    }

    m_previewTexture.Reset();
    m_previewRtvHeap.Reset();

    // Create render target texture
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    HRESULT hr = m_deviceRef->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&m_previewTexture)
    );

    if (FAILED(hr)) {
        return;
    }

    // Create RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_previewRtvHeap));

    // Create RTV
    m_previewRtvHandle = m_previewRtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_deviceRef->GetDevice()->CreateRenderTargetView(m_previewTexture.Get(), nullptr, m_previewRtvHandle);

    // Create SRV in ImGui's descriptor heap (descriptor index 1, after ImGui's font texture at 0)
    if (m_srvHeap) {
        UINT descriptorSize = m_deviceRef->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += descriptorSize;  // Skip ImGui's font texture descriptor

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_deviceRef->GetDevice()->CreateShaderResourceView(
            m_previewTexture.Get(),
            &srvDesc,
            srvHandle
        );

        // Store GPU handle for ImGui::Image
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += descriptorSize;
        m_previewSrvGpuHandle = gpuHandle;
    }

    m_previewTextureWidth = width;
    m_previewTextureHeight = height;

    // Also create dummy texture if not exists
    CreateDummyTexture();
}

void UISystem::CreateDummyTexture() {
    auto device = m_deviceRef->GetDevice();

    // 1. Texture2D Dummy
    if (!m_dummyTexture) {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;

        // Initialize to solid black
        unsigned char blackPixel[4] = {0,0,0,255};
        CreateTextureFromData(blackPixel, 1, 1, 4, m_dummyTexture);

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeap));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(m_dummyTexture.Get(), &srvDesc, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // 2. TextureCube Dummy
    if (!m_dummyTextureCube) {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 6; // Cube faces
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;

        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dummyTextureCube));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeapCube));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // Cube view
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = 1;
        device->CreateShaderResourceView(m_dummyTextureCube.Get(), &srvDesc, m_dummySrvHeapCube->GetCPUDescriptorHandleForHeapStart());
    }

    // 3. Texture3D Dummy
    if (!m_dummyTexture3D) {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;

        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dummyTexture3D));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeap3D));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D; // 3D view
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture3D.MipLevels = 1;
        device->CreateShaderResourceView(m_dummyTexture3D.Get(), &srvDesc, m_dummySrvHeap3D->GetCPUDescriptorHandleForHeapStart());
    }
}

void UISystem::SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry - Sharp, industrial look
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(5, 3);
    style.ItemSpacing = ImVec2(6, 4);

    ApplyUiTheme();
}

void UISystem::ApplyUiTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    const UIThemeColors& theme = m_uiThemeColors;
    const float controlOpacity = (std::clamp)(theme.ControlOpacity, 0.0f, 1.0f);
    const float panelOpacity = (std::clamp)(theme.PanelOpacity, 0.0f, 1.0f);
    const float headingOpacity = (std::clamp)(theme.PanelHeadingOpacity, 0.0f, 1.0f);
    auto WithAlpha = [](ImVec4 color, float alpha) {
        color.w = alpha;
        return color;
    };

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = theme.LabelFontColor;
    colors[ImGuiCol_TextDisabled] = theme.StatusFontColor;
    colors[ImGuiCol_TextLink] = theme.ButtonLabelColor;
    colors[ImGuiCol_WindowBg] = WithAlpha(theme.WindowBackground, panelOpacity);
    colors[ImGuiCol_ChildBg] = WithAlpha(theme.PanelBackground, panelOpacity);
    colors[ImGuiCol_PopupBg] = WithAlpha(theme.ControlBackground, controlOpacity);
    colors[ImGuiCol_Border] = theme.LinesAccentColorDim;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = WithAlpha(theme.ControlBackground, controlOpacity);
    colors[ImGuiCol_FrameBgHovered] = WithAlpha(theme.ActivePanelBackground, controlOpacity);
    colors[ImGuiCol_FrameBgActive] = WithAlpha(theme.ActivePanelTitleColor, controlOpacity);

    colors[ImGuiCol_TitleBg] = WithAlpha(theme.PassivePanelTitleColor, headingOpacity);
    colors[ImGuiCol_TitleBgActive] = WithAlpha(theme.ActivePanelTitleColor, headingOpacity);
    colors[ImGuiCol_TitleBgCollapsed] = WithAlpha(theme.PassivePanelTitleColor, headingOpacity * 0.9f);

    colors[ImGuiCol_MenuBarBg] = WithAlpha(theme.PanelTitleBackground, headingOpacity);

    colors[ImGuiCol_ScrollbarBg] = theme.PanelBackground;
    colors[ImGuiCol_ScrollbarGrab] = theme.LinesAccentColorDim;
    colors[ImGuiCol_ScrollbarGrabHovered] = theme.ActivePanelTitleColor;
    colors[ImGuiCol_ScrollbarGrabActive] = theme.ActiveTabBackground;

    colors[ImGuiCol_CheckMark] = theme.ButtonIconColor;
    colors[ImGuiCol_SliderGrab] = theme.IconColor;
    colors[ImGuiCol_SliderGrabActive] = theme.ButtonIconColor;

    colors[ImGuiCol_Button] = WithAlpha(theme.ButtonBackgroundColor, panelOpacity);
    colors[ImGuiCol_ButtonHovered] = WithAlpha(theme.ActivePanelBackground, panelOpacity);
    colors[ImGuiCol_ButtonActive] = WithAlpha(theme.ActivePanelTitleColor, headingOpacity);

    colors[ImGuiCol_Header] = WithAlpha(theme.PanelTitleBackground, headingOpacity);
    colors[ImGuiCol_HeaderHovered] = WithAlpha(theme.ActivePanelBackground, panelOpacity);
    colors[ImGuiCol_HeaderActive] = WithAlpha(theme.ActivePanelTitleColor, headingOpacity);

    colors[ImGuiCol_Separator] = theme.LinesAccentColorDim;
    colors[ImGuiCol_SeparatorHovered] = theme.ActivePanelTitleColor;
    colors[ImGuiCol_SeparatorActive] = theme.ActiveTabBackground;

    colors[ImGuiCol_ResizeGrip] = theme.LinesAccentColorDim;
    colors[ImGuiCol_ResizeGripHovered] = theme.ActivePanelTitleColor;
    colors[ImGuiCol_ResizeGripActive] = theme.ActiveTabBackground;

    colors[ImGuiCol_Tab] = WithAlpha(theme.PassiveTabBackground, headingOpacity);
    colors[ImGuiCol_TabHovered] = WithAlpha(theme.ActivePanelBackground, headingOpacity);
    colors[ImGuiCol_TabActive] = WithAlpha(theme.ActiveTabBackground, headingOpacity);
    colors[ImGuiCol_TabUnfocused] = WithAlpha(theme.PassiveTabBackground, headingOpacity);
    colors[ImGuiCol_TabUnfocusedActive] = WithAlpha(theme.ActiveTabBackground, headingOpacity);

    colors[ImGuiCol_TableHeaderBg] = WithAlpha(theme.TrackerHeadingBackground, headingOpacity);
    colors[ImGuiCol_TableBorderStrong] = theme.LinesAccentColorDim;
    colors[ImGuiCol_TableBorderLight] = theme.LinesAccentColorDim;
    colors[ImGuiCol_TableRowBg] = WithAlpha(theme.TrackerBeatBackground, panelOpacity);
    colors[ImGuiCol_TableRowBgAlt] = WithAlpha(theme.PanelBackground, panelOpacity);

    colors[ImGuiCol_TextSelectedBg] = theme.ActiveTabBackground;
    colors[ImGuiCol_DragDropTarget] = theme.IconColor;
    colors[ImGuiCol_NavHighlight] = theme.ActivePanelTitleColor;
    colors[ImGuiCol_NavWindowingHighlight] = theme.ControlFontColor;
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);

    colors[ImGuiCol_PlotLines] = theme.IconColor;
    colors[ImGuiCol_PlotLinesHovered] = theme.ButtonIconColor;
    colors[ImGuiCol_PlotHistogram] = theme.IconColor;
    colors[ImGuiCol_PlotHistogramHovered] = theme.ButtonIconColor;

    ApplyCodeEditorControlOpacity();
    EnsureThemeBackgroundTexture();
}

void UISystem::ApplyCodeEditorControlOpacity() {
    const float controlOpacity = (std::clamp)(m_uiThemeColors.ControlOpacity, 0.0f, 1.0f);
    auto ApplyOpacityToEditorBackground = [controlOpacity](TextEditor& editor) {
        TextEditor::Palette palette = editor.GetPalette();
        const int bgIndex = static_cast<int>(TextEditor::PaletteIndex::Background);
        ImVec4 bg = ImGui::ColorConvertU32ToFloat4(palette[bgIndex]);
        bg.w = controlOpacity;
        palette[bgIndex] = ImGui::ColorConvertFloat4ToU32(bg);
        editor.SetPalette(palette);
    };

    ApplyOpacityToEditorBackground(m_textEditor);
    ApplyOpacityToEditorBackground(m_snippetTextEditor);
}

bool UISystem::AddOrReplaceCustomTheme(const std::string& name, const UIThemeColors& colors) {
    if (name.empty()) {
        return false;
    }

    auto it = std::find_if(m_customThemes.begin(), m_customThemes.end(), [&](const NamedUITheme& item) {
        return item.name == name;
    });

    if (it != m_customThemes.end()) {
        it->colors = colors;
        return true;
    }

    m_customThemes.push_back({ name, colors });
    return true;
}

void UISystem::LoadUiThemeSettings() {
    m_uiThemeColors = DefaultUiThemeColors();
    m_customThemes.clear();
    for (const auto& theme : BuiltInThemes()) {
        AddOrReplaceCustomTheme(theme.name, theme.colors);
    }
    m_activeThemeName = "ShaderPunk";
    std::snprintf(m_themeNameBuffer, sizeof(m_themeNameBuffer), "%s", "");

    const fs::path settingsPath = GetUiSettingsPath(m_appRoot);
    std::ifstream in(settingsPath, std::ios::binary);
    if (!in.is_open()) {
        return;
    }

    json root;
    try {
        in >> root;
    } catch (...) {
        return;
    }

    const json themeRoot = root.value("theme", json::object());
    m_activeThemeName = themeRoot.value("active", std::string("ShaderPunk"));
    ApplyThemeColorFromJson(m_uiThemeColors, themeRoot.value("activeColors", json::object()));

    const json themes = themeRoot.value("themes", json::array());
    if (themes.is_array()) {
        for (const auto& item : themes) {
            if (!item.is_object()) {
                continue;
            }
            const std::string name = item.value("name", std::string());
            if (name.empty()) {
                continue;
            }

            UIThemeColors colors = DefaultUiThemeColors();
            auto existing = std::find_if(m_customThemes.begin(), m_customThemes.end(), [&](const NamedUITheme& t) { return t.name == name; });
            if (existing != m_customThemes.end()) {
                colors = existing->colors;
            }
            ApplyThemeColorFromJson(colors, item.value("colors", json::object()));
            AddOrReplaceCustomTheme(name, colors);
        }
    }

    auto it = std::find_if(m_customThemes.begin(), m_customThemes.end(), [&](const NamedUITheme& item) {
        return item.name == m_activeThemeName;
    });
    if (it != m_customThemes.end()) {
        m_uiThemeColors = it->colors;
    } else {
        m_activeThemeName = "ShaderPunk";
        auto fallback = std::find_if(m_customThemes.begin(), m_customThemes.end(), [&](const NamedUITheme& item) {
            return item.name == m_activeThemeName;
        });
        if (fallback != m_customThemes.end()) {
            m_uiThemeColors = fallback->colors;
        }
    }
}

void UISystem::SaveUiThemeSettings() const {
    const fs::path settingsPath = GetUiSettingsPath(m_appRoot);
    std::error_code ec;
    fs::create_directories(settingsPath.parent_path(), ec);

    json root;
    std::ifstream in(settingsPath, std::ios::binary);
    if (in.is_open()) {
        try {
            in >> root;
        } catch (...) {
            root = json::object();
        }
    }

    json themes = json::array();
    for (const auto& item : m_customThemes) {
        themes.push_back({
            {"name", item.name},
            {"colors", ThemeColorsToJson(item.colors)}
        });
    }

    root["theme"] = {
        {"active", m_activeThemeName},
        {"themes", themes},
        {"activeColors", ThemeColorsToJson(m_uiThemeColors)}
    };

    std::ofstream out(settingsPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << root.dump(2);
}

void UISystem::EnsureThemeBackgroundTexture() {
    std::string requestedPath = m_uiThemeColors.BackgroundImage;
    if (requestedPath.empty()) {
        m_themeBackgroundTexture.Reset();
        m_themeBackgroundSrvGpuHandle = {};
        m_themeBackgroundWidth = 0;
        m_themeBackgroundHeight = 0;
        m_loadedThemeBackgroundPath.clear();
        return;
    }

    fs::path resolvedPath(requestedPath);
    if (resolvedPath.is_relative()) {
        resolvedPath = fs::path(m_appRoot) / resolvedPath;
    }
    resolvedPath = resolvedPath.lexically_normal();
    requestedPath = resolvedPath.string();

    if (requestedPath == m_loadedThemeBackgroundPath && m_themeBackgroundTexture && m_themeBackgroundSrvGpuHandle.ptr != 0) {
        return;
    }

    m_themeBackgroundTexture.Reset();
    m_themeBackgroundSrvGpuHandle = {};
    m_themeBackgroundWidth = 0;
    m_themeBackgroundHeight = 0;
    m_loadedThemeBackgroundPath.clear();

    if (!m_deviceRef || !m_srvHeap || !fs::exists(resolvedPath)) {
        return;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = stbi_load(requestedPath.c_str(), &width, &height, &channels, 4);
    if (!data || width <= 0 || height <= 0) {
        if (data) stbi_image_free(data);
        return;
    }

    CreateTextureFromData(data, width, height, 4, m_themeBackgroundTexture);
    stbi_image_free(data);
    if (!m_themeBackgroundTexture) {
        return;
    }

    constexpr UINT kThemeBackgroundSrvIndex = 126;
    const UINT descriptorSize = m_deviceRef->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += static_cast<SIZE_T>(kThemeBackgroundSrvIndex) * descriptorSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    m_deviceRef->GetDevice()->CreateShaderResourceView(m_themeBackgroundTexture.Get(), &srvDesc, cpuHandle);

    m_themeBackgroundSrvGpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
    m_themeBackgroundSrvGpuHandle.ptr += static_cast<SIZE_T>(kThemeBackgroundSrvIndex) * descriptorSize;
    m_themeBackgroundWidth = width;
    m_themeBackgroundHeight = height;
    m_loadedThemeBackgroundPath = requestedPath;
}

void UISystem::DrawThemeBackgroundTiled() {
    if (!m_themeBackgroundTexture || m_themeBackgroundSrvGpuHandle.ptr == 0 || m_themeBackgroundWidth <= 0 || m_themeBackgroundHeight <= 0) {
        return;
    }

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    if (!viewport) {
        return;
    }

    ImDrawList* bg = ImGui::GetBackgroundDrawList(viewport);
    const ImVec2 origin = viewport->Pos;
    const ImVec2 maxPos = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    const float tileW = static_cast<float>(m_themeBackgroundWidth);
    const float tileH = static_cast<float>(m_themeBackgroundHeight);

    if (tileW < 1.0f || tileH < 1.0f) {
        return;
    }

    for (float y = origin.y; y < maxPos.y; y += tileH) {
        for (float x = origin.x; x < maxPos.x; x += tileW) {
            const ImVec2 pMin(x, y);
            const ImVec2 pMax((std::min)(x + tileW, maxPos.x), (std::min)(y + tileH, maxPos.y));

            const float uMax = (pMax.x - pMin.x) / tileW;
            const float vMax = (pMax.y - pMin.y) / tileH;
            bg->AddImage((ImTextureID)m_themeBackgroundSrvGpuHandle.ptr, pMin, pMax, ImVec2(0.0f, 0.0f), ImVec2(uMax, vMax), IM_COL32(255, 255, 255, 255));
        }
    }
}

void UISystem::ShowThemeEditorPopup() {
    if (m_showThemeEditor) {
        ImGui::OpenPopup("Theme Editor");
        m_showThemeEditor = false;
    }

    if (!ImGui::BeginPopupModal("Theme Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted("Theme name");
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputText("##ThemeName", m_themeNameBuffer, sizeof(m_themeNameBuffer));

    char backgroundPathBuffer[512];
    std::snprintf(backgroundPathBuffer, sizeof(backgroundPathBuffer), "%s", m_uiThemeColors.BackgroundImage.c_str());
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::InputText("BackgroundImage", backgroundPathBuffer, sizeof(backgroundPathBuffer))) {
        m_uiThemeColors.BackgroundImage = backgroundPathBuffer;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        char filePath[512] = {};
        if (!m_uiThemeColors.BackgroundImage.empty()) {
            fs::path currentPath(m_uiThemeColors.BackgroundImage);
            if (currentPath.is_relative()) {
                currentPath = fs::path(m_appRoot) / currentPath;
            }
            const std::string currentPathString = currentPath.lexically_normal().string();
            std::strncpy(filePath, currentPathString.c_str(), sizeof(filePath) - 1);
        }
        const std::string defaultThemeDir = (fs::path(m_appRoot) / "editor_assets" / "themes").lexically_normal().string();
        char initialDir[MAX_PATH] = {};
        std::strncpy(initialDir, defaultThemeDir.c_str(), sizeof(initialDir) - 1);
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = sizeof(filePath);
        ofn.lpstrInitialDir = initialDir;
        ofn.lpstrFilter = "Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.ppm\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            fs::path selected = fs::path(filePath).lexically_normal();
            std::error_code ec;
            fs::path relative = fs::relative(selected, fs::path(m_appRoot), ec);
            const std::string relativeStr = relative.generic_string();
            if (!ec && !relative.empty() && relativeStr.rfind("..", 0) != 0) {
                m_uiThemeColors.BackgroundImage = relative.generic_string();
            } else {
                m_uiThemeColors.BackgroundImage = selected.string();
            }
        }
    }
    ImGui::TextDisabled("Relative paths resolve from app root (e.g. editor_assets/themes/tile.png)");
    ImGui::SetNextItemWidth(240.0f);
    ImGui::SliderFloat("ControlOpacity", &m_uiThemeColors.ControlOpacity, 0.15f, 1.0f, "%.2f");
    ImGui::SetNextItemWidth(240.0f);
    ImGui::SliderFloat("PanelOpacity", &m_uiThemeColors.PanelOpacity, 0.15f, 1.0f, "%.2f");
    ImGui::SetNextItemWidth(240.0f);
    ImGui::SliderFloat("PanelHeadingOpacity", &m_uiThemeColors.PanelHeadingOpacity, 0.15f, 1.0f, "%.2f");

    ImGui::SeparatorText("Colors");

    const ImGuiColorEditFlags pickerFlags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf;
    ImGui::BeginChild("ThemeColorList", ImVec2(520.0f, 420.0f), true);
    ImGui::ColorEdit4("LinesAccentColorDim", &m_uiThemeColors.LinesAccentColorDim.x, pickerFlags);
    ImGui::ColorEdit4("ControlBackground", &m_uiThemeColors.ControlBackground.x, pickerFlags);
    ImGui::ColorEdit4("ControlFontColor", &m_uiThemeColors.ControlFontColor.x, pickerFlags);
    ImGui::ColorEdit4("IconColor", &m_uiThemeColors.IconColor.x, pickerFlags);
    ImGui::ColorEdit4("ButtonIconColor", &m_uiThemeColors.ButtonIconColor.x, pickerFlags);
    ImGui::ColorEdit4("ButtonLabelColor", &m_uiThemeColors.ButtonLabelColor.x, pickerFlags);
    ImGui::ColorEdit4("ButtonBackgroundColor", &m_uiThemeColors.ButtonBackgroundColor.x, pickerFlags);
    ImGui::ColorEdit4("PanelBackground", &m_uiThemeColors.PanelBackground.x, pickerFlags);
    ImGui::ColorEdit4("WindowBackground", &m_uiThemeColors.WindowBackground.x, pickerFlags);
    ImGui::ColorEdit4("TrackerHeadingBackground", &m_uiThemeColors.TrackerHeadingBackground.x, pickerFlags);
    ImGui::ColorEdit4("TrackerHeadingFontColor", &m_uiThemeColors.TrackerHeadingFontColor.x, pickerFlags);
    ImGui::ColorEdit4("TrackerAccentBeatBackground", &m_uiThemeColors.TrackerAccentBeatBackground.x, pickerFlags);
    ImGui::ColorEdit4("TrackerAccentBeatFontColor", &m_uiThemeColors.TrackerAccentBeatFontColor.x, pickerFlags);
    ImGui::ColorEdit4("TrackerBeatBackground", &m_uiThemeColors.TrackerBeatBackground.x, pickerFlags);
    ImGui::ColorEdit4("TrackerBeatFontColor", &m_uiThemeColors.TrackerBeatFontColor.x, pickerFlags);
    ImGui::ColorEdit4("LabelFontColor", &m_uiThemeColors.LabelFontColor.x, pickerFlags);
    ImGui::ColorEdit4("PanelTitleFontColor", &m_uiThemeColors.PanelTitleFontColor.x, pickerFlags);
    ImGui::ColorEdit4("PanelTitleBackground", &m_uiThemeColors.PanelTitleBackground.x, pickerFlags);
    ImGui::ColorEdit4("ActiveTabFontColor", &m_uiThemeColors.ActiveTabFontColor.x, pickerFlags);
    ImGui::ColorEdit4("ActiveTabBackground", &m_uiThemeColors.ActiveTabBackground.x, pickerFlags);
    ImGui::ColorEdit4("PassiveTabFontColor", &m_uiThemeColors.PassiveTabFontColor.x, pickerFlags);
    ImGui::ColorEdit4("PassiveTabBackground", &m_uiThemeColors.PassiveTabBackground.x, pickerFlags);
    ImGui::ColorEdit4("ActivePanelTitleColor", &m_uiThemeColors.ActivePanelTitleColor.x, pickerFlags);
    ImGui::ColorEdit4("ActivePanelBackground", &m_uiThemeColors.ActivePanelBackground.x, pickerFlags);
    ImGui::ColorEdit4("PassivePanelTitleColor", &m_uiThemeColors.PassivePanelTitleColor.x, pickerFlags);
    ImGui::ColorEdit4("PassivePanelBackground", &m_uiThemeColors.PassivePanelBackground.x, pickerFlags);
    ImGui::ColorEdit4("LogoFontColor", &m_uiThemeColors.LogoFontColor.x, pickerFlags);
    ImGui::ColorEdit4("ConsoleFontColor", &m_uiThemeColors.ConsoleFontColor.x, pickerFlags);
    ImGui::ColorEdit4("ConsoleBackground", &m_uiThemeColors.ConsoleBackground.x, pickerFlags);
    ImGui::ColorEdit4("StatusFontColor", &m_uiThemeColors.StatusFontColor.x, pickerFlags);
    ImGui::EndChild();

    ApplyUiTheme();

    if (ImGui::Button("Save Theme", ImVec2(120.0f, 0.0f))) {
        std::string themeName = m_themeNameBuffer;
        if (!themeName.empty() && AddOrReplaceCustomTheme(themeName, m_uiThemeColors)) {
            m_activeThemeName = themeName;
            SaveUiThemeSettings();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to Default", ImVec2(140.0f, 0.0f))) {
        auto shaderPunk = std::find_if(m_customThemes.begin(), m_customThemes.end(), [&](const NamedUITheme& t) {
            return t.name == "ShaderPunk";
        });
        m_uiThemeColors = (shaderPunk != m_customThemes.end()) ? shaderPunk->colors : DefaultUiThemeColors();
        m_activeThemeName = "ShaderPunk";
        ApplyUiTheme();
        SaveUiThemeSettings();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(100.0f, 0.0f))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void UISystem::PushNumericFont() {
    if (m_fontErbosDracoNumbers) {
        ImGui::PushFont(m_fontErbosDracoNumbers);
    }
}

void UISystem::PopNumericFont() {
    if (m_fontErbosDracoNumbers) {
        ImGui::PopFont();
    }
}

float UISystem::GetNumericFieldMinWidth() const {
    ImGuiStyle& style = ImGui::GetStyle();
    float width = 0.0f;
    if (m_fontErbosDracoNumbers) {
        ImGui::PushFont(m_fontErbosDracoNumbers);
    }
    width = ImGui::CalcTextSize("000.0").x + style.FramePadding.x * 2.0f;
    if (m_fontErbosDracoNumbers) {
        ImGui::PopFont();
    }
    return width;
}

void UISystem::SetNextNumericFieldWidth(float requestedWidth) {
    const float minWidth = GetNumericFieldMinWidth();
    const float width = (requestedWidth > 0.0f) ? (std::max)(requestedWidth, minWidth) : minWidth;
    ImGui::SetNextItemWidth(width);
}

void UISystem::BeginFrame() {
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

void UISystem::ShowMainMenuBar() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(UIConfig::MenuFramePadX, UIConfig::MenuFramePadY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(UIConfig::MenuItemSpacingX, UIConfig::MenuItemSpacingY));
    if (ImGui::BeginMenuBar()) {

        const float startY = ImGui::GetCursorPosY();
        const float frameHeight = ImGui::GetFrameHeight();
        const float minHeight = 0.0f;
        const float autoHeight = (std::max)(frameHeight + UIConfig::MenuTopPad + UIConfig::MenuBottomPad, minHeight);
        const float targetHeight = UIConfig::MenuBarHeight > 0.0f
            ? (std::max)(UIConfig::MenuBarHeight, minHeight)
            : autoHeight;
        ImGui::SetCursorPosY(startY + UIConfig::MenuTopPad);

        ImGui::Dummy(ImVec2(UIConfig::MenuLeftPad, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);

        if (m_fontMenuSmall) {
            ImGui::PushFont(m_fontMenuSmall);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, m_uiThemeColors.LabelFontColor);

        float menuMaxX = ImGui::GetCursorScreenPos().x;
        
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                m_scenes.clear();
                CreateDefaultScene();
                m_track = DemoTrack();
                CreateDefaultTrack();
                m_audioLibrary.clear();
                m_demoTitle = "Untitled Demo";
                m_demoAuthor.clear();
                m_demoDescription.clear();
                m_currentProjectPath.clear();
                if (m_audioSystem) m_audioSystem->Stop();
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                OpenProject();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveProject();
            }
            if (ImGui::MenuItem("Save As...")) {
                SaveProjectAs();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Packaged Demo...")) {
                BuildPackagedDemoProject();
            }
            if (ImGui::MenuItem("Build Self-Contained Demo...")) {
                BuildProject();
            }
            if (ImGui::MenuItem("Build Self-Contained Screensaver...")) {
                BuildScreenSaverProject();
            }
            if (ImGui::MenuItem("Build Micro-Demo...")) {
                BuildMicroDemoProject();
            }
            if (ImGui::MenuItem("Build Micro-Demo (Developer Overlay)...")) {
                BuildMicroDeveloperDemoProject();
            }
            if (ImGui::MenuItem("Build Settings...")) {
                m_showBuildSettings = true;
                m_buildSettingsRefreshRequested = true;
            }
            if (ImGui::MenuItem("Export Runtime Package...")) {
                ExportRuntimePackage();
            }
            ImGui::Separator();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Demo Mode", nullptr, m_currentMode == UIMode::Demo)) {
                m_currentMode = UIMode::Demo;
            }
            if (ImGui::MenuItem("Scene Mode", nullptr, m_currentMode == UIMode::Scene)) {
                m_currentMode = UIMode::Scene;
            }
            if (ImGui::MenuItem("Post FX Mode", nullptr, m_currentMode == UIMode::PostFX)) {
                m_currentMode = UIMode::PostFX;
            }
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
        if (ImGui::BeginMenu("Theme")) {
            if (!m_customThemes.empty()) {
                for (const auto& theme : m_customThemes) {
                    const bool selected = (m_activeThemeName == theme.name);
                    if (ImGui::MenuItem(theme.name.c_str(), nullptr, selected)) {
                        m_uiThemeColors = theme.colors;
                        m_activeThemeName = theme.name;
                        ApplyUiTheme();
                        SaveUiThemeSettings();
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Edit Theme...")) {
                std::snprintf(m_themeNameBuffer, sizeof(m_themeNameBuffer), "%s", m_activeThemeName.c_str());
                m_showThemeEditor = true;
            }
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
        if (ImGui::BeginMenu("Device")) {
            auto adapters = Device::GetAvailableAdapters();
            int currentIdx = -1;
            if (m_deviceRef) currentIdx = m_deviceRef->GetAdapterIndex();

            for (const auto& adapter : adapters) {
                std::string name;
                name.reserve(adapter.name.length());
                for (wchar_t c : adapter.name) name.push_back(static_cast<char>(c));

                // Calculate VRAM in GB for display
                float vramGB = static_cast<float>(adapter.videoMemory) / (1024.0f * 1024.0f * 1024.0f);
                std::string label = name + " (" + std::to_string(vramGB).substr(0, 4) + " GB)";

                bool isSelected = (currentIdx != -1 && (int)adapter.index == currentIdx);

                if (ImGui::MenuItem(label.c_str(), nullptr, isSelected)) {
                    if (m_restartCallback) {
                        m_restartCallback((int)adapter.index);
                    }
                }
            }
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                m_showAbout = true;
            }
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);

        const float controlsStartX = menuMaxX + UIConfig::MenuItemSpacingX * 2.0f;
        const float controlsStartY = ImGui::GetCursorScreenPos().y;
        const float vsyncSlotWidth = 90.0f;
        const float controlsSpacing = UIConfig::MenuItemSpacingX * 2.0f;
        const float fpsSlotWidth = 90.0f;

        ImGui::SetCursorScreenPos(ImVec2(controlsStartX, controlsStartY));
        ImGui::Checkbox("VSync", &m_previewVsyncEnabled);

        const float fpsX = controlsStartX + vsyncSlotWidth + controlsSpacing;
        ImGui::SetCursorScreenPos(ImVec2(fpsX, controlsStartY));
        ImFont* fpsFont = m_fontCodeSizes[static_cast<int>(CodeFontSize::XS)];
        if (fpsFont) {
            ImGui::PushFont(fpsFont);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, m_uiThemeColors.StatusFontColor);
        ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
        ImGui::PopStyleColor();
        if (fpsFont) {
            ImGui::PopFont();
        }

        const float controlsEndX = fpsX + fpsSlotWidth;

        if (m_fontMenuSmall) {
            ImGui::PopFont();
        }
        ImGui::PopStyleColor();

        const float endY = ImGui::GetCursorPosY();
        const float extra = targetHeight - (endY - startY);
        if (extra > 0.0f) {
            ImGui::Dummy(ImVec2(0.0f, extra));
        }

        (void)controlsEndX;

        ImGui::EndMenuBar();
    }
    ImGui::PopStyleVar(2);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float stripHeight = UIConfig::TitlebarPadY;
    if (viewport && stripHeight > 4.0f) {
        ImDrawList* fg = ImGui::GetWindowDrawList();
        const ImVec2 stripMin = viewport->Pos;
        const ImVec2 stripMax = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + stripHeight);
        fg->PushClipRect(stripMin, stripMax, false);
        ImVec4 stripColor = m_uiThemeColors.PanelTitleBackground;
        stripColor.w = (std::clamp)(m_uiThemeColors.PanelHeadingOpacity, 0.0f, 1.0f);
        fg->AddRectFilled(stripMin, stripMax, ImGui::GetColorU32(stripColor));
        fg->AddLine(ImVec2(stripMin.x, stripMax.y - 1.0f), ImVec2(stripMax.x, stripMax.y - 1.0f), ImGui::GetColorU32(m_uiThemeColors.LinesAccentColorDim), 1.0f);

        const float edgePad = UIConfig::MenuRightPad;
        const float itemPad = UIConfig::MenuItemSpacingX;
        const float buttonSize = (std::max)(14.0f, stripHeight - 8.0f);
        const float buttonY = stripMin.y + (stripHeight - buttonSize) * 0.5f;
        const float closeX = stripMax.x - edgePad - buttonSize;
        const float minX = closeX - itemPad - buttonSize;

        auto DrawTitlebarButton = [&](float x, float y, float size, uint32_t iconCode, bool hovered) {
            const ImVec2 rectMin(x, y);
            const ImVec2 rectMax(x + size, y + size);
            const ImU32 bg = ImGui::GetColorU32(hovered ? m_uiThemeColors.ActivePanelBackground : m_uiThemeColors.ButtonBackgroundColor);
            fg->AddRectFilled(rectMin, rectMax, bg, 0.0f);
            fg->AddRect(rectMin, rectMax, ImGui::GetColorU32(m_uiThemeColors.LinesAccentColorDim), 0.0f);

            const std::string icon = OpenFontIcons::ToUtf8(iconCode);
            ImFont* iconFont = m_fontMenuSmall ? m_fontMenuSmall : ImGui::GetFont();
            const float iconFontSize = ImGui::GetFontSize();

            float textX = x;
            float textY = y;
            bool usedGlyphBounds = false;
            if (iconFont) {
                if (ImFontBaked* baked = iconFont->GetFontBaked(iconFontSize)) {
                    if (ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(iconCode))) {
                        const float glyphW = glyph->X1 - glyph->X0;
                        const float glyphH = glyph->Y1 - glyph->Y0;
                        textX = x + (size - glyphW) * 0.5f - glyph->X0;
                        textY = y + (size - glyphH) * 0.5f - glyph->Y0 + 1.0f;
                        if (iconCode == OpenFontIcons::kChevronDown) {
                            textX -= 2.0f;
                            textY += 1.0f;
                        }
                        if (iconCode == OpenFontIcons::kXCircle) {
                            textX += 0.5f;
                        }
                        usedGlyphBounds = true;
                    }
                }
            }
            if (!usedGlyphBounds) {
                const ImVec2 textSize = ImGui::CalcTextSize(icon.c_str());
                textX = x + (size - textSize.x) * 0.5f;
                textY = y + (size - textSize.y) * 0.5f + 1.0f;
                if (iconCode == OpenFontIcons::kChevronDown) {
                    textX -= 2.0f;
                    textY += 1.0f;
                }
                if (iconCode == OpenFontIcons::kXCircle) {
                    textX += 0.5f;
                }
            }
            fg->AddText(iconFont, iconFontSize, ImVec2(std::floor(textX), std::floor(textY)), ImGui::GetColorU32(m_uiThemeColors.ButtonIconColor), icon.c_str());
        };

        const bool canUseWindow = m_hwnd != nullptr;
        const ImVec2 minButtonMin(minX, buttonY);
        const ImVec2 minButtonMax(minX + buttonSize, buttonY + buttonSize);
        const ImVec2 closeButtonMin(closeX, buttonY);
        const ImVec2 closeButtonMax(closeX + buttonSize, buttonY + buttonSize);

        const bool hoverMin = ImGui::IsMouseHoveringRect(minButtonMin, minButtonMax, false);
        const bool hoverClose = ImGui::IsMouseHoveringRect(closeButtonMin, closeButtonMax, false);
        const bool leftClicked = (GetAsyncKeyState(VK_LBUTTON) & 0x0001) != 0;
        const bool minPressed = hoverMin && leftClicked;
        const bool closePressed = hoverClose && leftClicked;

        DrawTitlebarButton(minX, buttonY, buttonSize, OpenFontIcons::kChevronDown, hoverMin);
        DrawTitlebarButton(closeX, buttonY, buttonSize, OpenFontIcons::kXCircle, hoverClose);

        if (canUseWindow) {
            if (minPressed) {
                ShowWindow(m_hwnd, SW_MINIMIZE);
            }
            if (closePressed) {
                PostMessage(m_hwnd, WM_CLOSE, 0, 0);
            }
        }

        float iconRightX = stripMin.x + edgePad;
        if (m_titlebarIconTexture && m_titlebarIconSrvGpuHandle.ptr != 0) {
            const float iconSize = buttonSize;
            const float iconX = stripMin.x + edgePad;
            const float iconY = stripMin.y + (stripHeight - iconSize) * 0.5f;
            fg->AddImage((ImTextureID)m_titlebarIconSrvGpuHandle.ptr, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize));
            iconRightX = iconX + iconSize + itemPad;
        }

        std::string demoTitle = m_demoTitle;
        if (demoTitle.empty()) {
            demoTitle = GetProjectName();
        }
        if (demoTitle.empty()) {
            demoTitle = "Untitled Demo";
        }

        if (m_fontMenuSmall) {
            ImGui::PushFont(m_fontMenuSmall);
        }
        const ImVec2 titleSize = ImGui::CalcTextSize(demoTitle.c_str());
        const float titleX = viewport->Pos.x + (viewport->Size.x - titleSize.x) * 0.5f;
        const float titleY = viewport->Pos.y + (stripHeight - titleSize.y) * 0.5f + 1.0f;
        fg->AddText(
            ImVec2(std::floor(titleX), std::floor(titleY)),
            ImGui::GetColorU32(m_uiThemeColors.PanelTitleFontColor),
            demoTitle.c_str());
        fg->PopClipRect();
        if (m_fontMenuSmall) {
            ImGui::PopFont();
        }

        (void)iconRightX;
    }
}

void UISystem::EndFrame() {
    ImGui::Render();
}

bool UISystem::CompileScene(int sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_scenes.size()) return false;
    auto& scene = m_scenes[sceneIndex];

    // Only compile if we have a renderer
    if (!m_previewRenderer) return false;

    // Collect texture declarations
    std::vector<PreviewRenderer::TextureDecl> decls;
    for(const auto& b : scene.bindings) {
        if (!b.enabled) continue;

        PreviewRenderer::TextureDecl decl;
        decl.slot = b.channelIndex;

        if (b.type == TextureType::TextureCube) decl.type = "TextureCube";
        else if (b.type == TextureType::Texture3D) decl.type = "Texture3D";
        else decl.type = "Texture2D";

        decls.push_back(decl);
    }

    // Compile
    std::vector<std::string> errors;
    auto pso = m_previewRenderer->CompileShader(scene.shaderCode, decls, errors);
    bool success = (pso != nullptr);

    // Update Scene state
    if (success) {
        scene.pipelineState = pso;
        scene.compiledShaderBytes = m_previewRenderer->GetLastCompiledPixelShaderSize();
        scene.isDirty = false;
        m_playbackBlockedByCompileError = false;
    } else {
        scene.compiledShaderBytes = 0;
        m_playbackBlockedByCompileError = true;
        if (m_transport.state == TransportState::Playing) {
            m_transport.state = TransportState::Stopped;
            if (m_audioSystem) {
                m_audioSystem->Stop();
            }
            m_activeMusicIndex = -1;
        }
    }

    // If this is the active scene, update the editor UI state too
    if (sceneIndex == m_activeSceneIndex) {
        m_shaderState.status = success ? CompileStatus::Success : CompileStatus::Error;
        m_shaderState.diagnostics.clear();
        for (const auto& msg : errors) {
            Diagnostic d;
            d.line = 0;
            d.column = 0;
            d.message = msg;
            m_shaderState.diagnostics.push_back(d);
        }

        if (success) {
            m_shaderState.lastCompiledText = scene.shaderCode;
        }
    }

    return success;
}

void UISystem::LoadGlobalSnippets() {
    m_snippetFolders.clear();
    m_selectedSnippetFolderIndex = -1;
    m_selectedSnippetIndex = -1;
    m_nextSnippetId = 1;

    const fs::path snippetsDir = GetGlobalSnippetDirectory(m_appRoot);
    m_snippetsDirectoryPath = snippetsDir.string();

    std::error_code ec;
    fs::create_directories(snippetsDir, ec);

    const auto loadFromFile = [this](const fs::path& filePath, const std::string& fallbackFolderName) {
        std::ifstream in(filePath);
        if (!in.is_open()) {
            return;
        }

        json root;
        try {
            in >> root;
        } catch (...) {
            return;
        }

        if (!root.contains("snippets") || !root["snippets"].is_array()) {
            return;
        }

        ShaderSnippetFolder folder;
        folder.name = root.value("folder", fallbackFolderName);
        folder.filePath = filePath.string();

        for (const auto& item : root["snippets"]) {
            if (!item.is_object()) {
                continue;
            }

            ShaderSnippet snippet;
            snippet.name = item.value("name", std::string{});
            snippet.code = item.value("code", std::string{});

            if (snippet.name.empty() || snippet.code.empty()) {
                continue;
            }

            folder.snippets.push_back(std::move(snippet));
            m_nextSnippetId = (std::max)(m_nextSnippetId, static_cast<int>(folder.snippets.size()) + 1);
        }

        m_snippetFolders.push_back(std::move(folder));
    };

    if (fs::exists(snippetsDir)) {
        std::vector<fs::path> jsonFiles;
        for (const auto& entry : fs::directory_iterator(snippetsDir, ec)) {
            if (ec) {
                break;
            }

            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension() == ".json") {
                jsonFiles.push_back(entry.path());
            }
        }

        std::sort(jsonFiles.begin(), jsonFiles.end());
        for (const auto& path : jsonFiles) {
            loadFromFile(path, path.stem().string());
        }
    }

    const fs::path legacyPath = GetGlobalSnippetBaseDir(m_appRoot) / "snippets.json";
    if (m_snippetFolders.empty() && fs::exists(legacyPath)) {
        loadFromFile(legacyPath, "General");
    }

    if (m_snippetFolders.empty()) {
        ShaderSnippetFolder folder;
        folder.name = "General";
        folder.filePath = (snippetsDir / "General.json").string();
        m_snippetFolders.push_back(std::move(folder));
    }

    m_selectedSnippetFolderIndex = 0;
    if (!m_snippetFolders[0].snippets.empty()) {
        m_selectedSnippetIndex = 0;
    }
}

void UISystem::SaveGlobalSnippets() const {
    if (m_snippetsDirectoryPath.empty()) {
        return;
    }

    const fs::path snippetsDir(m_snippetsDirectoryPath);
    std::error_code ec;
    fs::create_directories(snippetsDir, ec);

    for (const auto& folder : m_snippetFolders) {
        fs::path filePath = folder.filePath.empty()
            ? (snippetsDir / (SanitizeSnippetFileStem(folder.name) + ".json"))
            : fs::path(folder.filePath);

        json root;
        root["version"] = 1;
        root["folder"] = folder.name;
        root["snippets"] = json::array();

        for (const auto& snippet : folder.snippets) {
            if (snippet.name.empty() || snippet.code.empty()) {
                continue;
            }

            root["snippets"].push_back({
                {"name", snippet.name},
                {"code", snippet.code}
            });
        }

        std::ofstream out(filePath);
        if (!out.is_open()) {
            continue;
        }
        out << root.dump(2);
    }
}

void UISystem::InsertSnippetIntoEditor(const std::string& snippetCode) {
    if (snippetCode.empty()) {
        return;
    }

    std::string insertText = snippetCode;
    if (!insertText.empty() && insertText.back() != '\n') {
        insertText.push_back('\n');
    }

    m_textEditor.InsertText(insertText);
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

    m_shaderState.status = CompileStatus::Dirty;
}

void UISystem::Render(ID3D12GraphicsCommandList* commandList) {
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

bool UISystem::LoadTextureFromFile(const std::string& path, ComPtr<ID3D12Resource>& outResource) {
    if (path.empty()) return false;

    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return false;

    // TODO: Handle failure in creation?
    // Using a local helper for synchronous upload
    CreateTextureFromData(data, w, h, 4, outResource);

    stbi_image_free(data);
    return outResource != nullptr;
}

void UISystem::CreateTextureFromData(const void* data, int width, int height, int channels, ComPtr<ID3D12Resource>& outResource) {
    (void)channels;
    auto device = m_deviceRef->GetDevice();

    // 1. Create Default Heap Resource (Dest)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};

    if (FAILED(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&outResource)))) {
        return;
    }

    // 2. Create Upload Buffer
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;

    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    D3D12_HEAP_PROPERTIES uploadHeapProps = {D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.SampleDesc.Count = 1;

    ComPtr<ID3D12Resource> uploadBuffer;
    if (FAILED(device->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadBuffer)))) {
        return;
    }

    // 3. Copy Memory
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);

    const uint8_t* srcData = (const uint8_t*)data;
    uint8_t* dstData = (uint8_t*)mappedData;

    for (UINT i = 0; i < numRows; ++i) {
        memcpy(dstData + footprint.Footprint.RowPitch * i,
               srcData + width * 4 * i,
               width * 4);
    }
    uploadBuffer->Unmap(0, nullptr);

    // 4. Create Short Lived Command Queue/List
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));

    ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));

    ComPtr<ID3D12GraphicsCommandList> cmdList;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));

    // 5. Record Copy
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = outResource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = outResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // 6. Execute and Wait
    ID3D12CommandList* ppCommandLists[] = { cmdList.Get() };
    queue->ExecuteCommandLists(1, ppCommandLists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    queue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);
}

ProjectState UISystem::CaptureState() {
    ProjectState state;
    state.scenes = m_scenes;
    state.audioLibrary = m_audioLibrary;
    state.track = m_track;
    state.transport = m_transport;
    state.demoTitle = m_demoTitle;
    state.demoAuthor = m_demoAuthor;
    state.demoDescription = m_demoDescription;
    state.currentMode = m_currentMode;
    state.shaderState = m_shaderState;
    state.activeSceneIndex = m_activeSceneIndex;

    // Strip GPU resources from the saved state to ensure they don't dangle
    for (auto& scene : state.scenes) {
        scene.texture.Reset();
        scene.srvHeap.Reset();
        scene.pipelineState.Reset();
        scene.textureValid = false;
        scene.postFxTextureA.Reset();
        scene.postFxTextureB.Reset();
        scene.postFxSrvHeap.Reset();
        scene.postFxRtvHeap.Reset();
        scene.postFxValid = false;
        for (auto& fx : scene.postFxChain) {
            fx.pipelineState.Reset();
            fx.isDirty = true;
            fx.historyIndex = 0;
            fx.historyInitialized = false;
            fx.historyTextures.clear();
        }

        for (auto& binding : scene.bindings) {
             binding.textureResource.Reset();
             binding.fileTextureValid = false;
        }
    }

    return state;
}

void UISystem::RestoreState(const ProjectState& state) {
    m_scenes = state.scenes;
    m_audioLibrary = state.audioLibrary;
    m_track = state.track;
    m_transport = state.transport;
    m_demoTitle = state.demoTitle;
    m_demoAuthor = state.demoAuthor;
    m_demoDescription = state.demoDescription;
    m_currentMode = state.currentMode;
    m_shaderState = state.shaderState;
    m_activeSceneIndex = state.activeSceneIndex;

    // Reload files and clear runtime resources (they belong to old device)
    for (auto& scene : m_scenes) {
        scene.pipelineState = nullptr;
        scene.texture = nullptr;
        scene.srvHeap = nullptr;
        scene.isDirty = true; // Force compile on next use
        scene.postFxTextureA = nullptr;
        scene.postFxTextureB = nullptr;
        scene.postFxSrvHeap = nullptr;
        scene.postFxRtvHeap = nullptr;
        scene.postFxValid = false;
        for (auto& fx : scene.postFxChain) {
            fx.pipelineState = nullptr;
            fx.isDirty = true;
            fx.historyIndex = 0;
            fx.historyInitialized = false;
            fx.historyTextures.clear();
        }

        for (auto& binding : scene.bindings) {
            binding.textureResource = nullptr;

            if (binding.bindingType == BindingType::File && !binding.filePath.empty()) {
                 if (LoadTextureFromFile(binding.filePath, binding.textureResource)) {
                     binding.fileTextureValid = true;
                 }
            }
        }
    }

    // Restore text editor
    m_textEditor.SetText(m_shaderState.text);

    // Mark as dirty so user knows to recompile
    if (m_shaderState.status == CompileStatus::Success) {
        m_shaderState.status = CompileStatus::Dirty;
    }

    // Force layout rebuild
    m_layoutBuilt = false;
}

void UISystem::RefreshMicroUbershaderConflictCache() {
    m_microUbershaderConflicts.clear();

    if (m_currentProjectPath.empty()) {
        m_microUbershaderConflictsDirty = false;
        return;
    }

    m_microUbershaderConflicts = BuildPipeline::AnalyzeMicroUbershaderConflicts(m_currentProjectPath);

    std::unordered_set<std::string> activeKeys;
    for (const auto& conflict : m_microUbershaderConflicts) {
        activeKeys.insert(conflict.signatureKey);

        std::vector<std::string> validEntrypoints;
        validEntrypoints.reserve(conflict.options.size());
        for (const auto& option : conflict.options) {
            validEntrypoints.push_back(option.moduleEntrypoint);
        }

        auto it = m_microUbershaderKeepEntrypointsBySignature.find(conflict.signatureKey);
        if (it == m_microUbershaderKeepEntrypointsBySignature.end()) {
            m_microUbershaderKeepEntrypointsBySignature[conflict.signatureKey] = validEntrypoints;
            continue;
        }

        std::vector<std::string> filtered;
        filtered.reserve(it->second.size());
        for (const std::string& kept : it->second) {
            if (std::find(validEntrypoints.begin(), validEntrypoints.end(), kept) != validEntrypoints.end()) {
                filtered.push_back(kept);
            }
        }
        if (filtered.empty()) {
            filtered = validEntrypoints;
        }
        it->second = std::move(filtered);
    }

    for (auto it = m_microUbershaderKeepEntrypointsBySignature.begin(); it != m_microUbershaderKeepEntrypointsBySignature.end();) {
        if (activeKeys.find(it->first) == activeKeys.end()) {
            it = m_microUbershaderKeepEntrypointsBySignature.erase(it);
        } else {
            ++it;
        }
    }

    m_microUbershaderConflictsDirty = false;
}

void UISystem::ShowBuildSettingsWindow() {
    const BuildTargetKind prevTargetKind = m_buildSettingsTargetKind;
    const BuildMode prevMode = m_buildSettingsMode;
    const SizeTargetPreset prevSizeTarget = m_buildSettingsSizeTarget;
    const bool prevRestrictedCompactTrack = m_buildSettingsRestrictedCompactTrack;
    const bool prevRuntimeDebugLog = m_buildSettingsRuntimeDebugLog;
    const bool prevCompactTrackDebugLog = m_buildSettingsCompactTrackDebugLog;
    const bool prevMicroDeveloperBuild = m_buildSettingsMicroDeveloperBuild;
    const std::string prevCleanSolutionRootPath = m_buildSettingsCleanSolutionRootPath;

    if (m_buildSettingsRefreshRequested) {
        m_buildSettingsPrereq = BuildPipeline::CheckPrereqs(m_appRoot, m_buildSettingsMode);
        m_buildSettingsRefreshRequested = false;
    }
    if (m_buildSettingsTargetKind == BuildTargetKind::MicroDemo && m_microUbershaderConflictsDirty) {
        RefreshMicroUbershaderConflictCache();
    }

    int unresolvedMicroConflictCount = 0;
    if (m_buildSettingsTargetKind == BuildTargetKind::MicroDemo) {
        for (const auto& conflict : m_microUbershaderConflicts) {
            const auto it = m_microUbershaderKeepEntrypointsBySignature.find(conflict.signatureKey);
            if (it == m_microUbershaderKeepEntrypointsBySignature.end() || it->second.empty()) {
                ++unresolvedMicroConflictCount;
            }
        }
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 windowSize(760.0f, 620.0f);
    ImGui::SetNextWindowPos(ImVec2(
        viewport->Pos.x + (viewport->Size.x - windowSize.x) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - windowSize.y) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

    bool canCloseBuildWindow = !m_isBuilding;
    bool* openPtr = canCloseBuildWindow ? &m_showBuildSettings : nullptr;
    if (!ImGui::Begin("Build Settings", openPtr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    auto LabeledActionButton = [](const char* id, uint32_t iconCodepoint, const char* label, const char* tooltip, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
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

        const ImVec2 iconSize = ImGui::CalcTextSize(icon.c_str());
        const ImVec2 labelSize = ImGui::CalcTextSize(safeLabel);
        const float gap = safeLabel[0] ? style.ItemInnerSpacing.x : 0.0f;
        const float contentWidth = iconSize.x + gap + labelSize.x;
        const float baseX = min.x + (buttonSize.x - contentWidth) * 0.5f;
        const float iconY = min.y + (buttonSize.y - iconSize.y) * 0.5f +
            ((iconCodepoint == OpenFontIcons::kPlus || iconCodepoint == OpenFontIcons::kMinus) ? 1.0f : 0.0f);
        const float labelY = min.y + (buttonSize.y - labelSize.y) * 0.5f;

        drawList->AddText(ImVec2(std::floor(baseX), std::floor(iconY)), ImGui::GetColorU32(ImGuiCol_CheckMark), icon.c_str());
        if (safeLabel[0]) {
            drawList->AddText(ImVec2(std::floor(baseX + iconSize.x + gap), std::floor(labelY)), ImGui::GetColorU32(ImGuiCol_TextLink), safeLabel);
        }

        if (ImGui::IsItemHovered() && tooltip && *tooltip) {
            ImGui::SetTooltip("%s", tooltip);
        }
        return pressed;
    };

    bool buildModeChanged = false;
    int modeIndex = (m_buildSettingsMode == BuildMode::ReleaseCrinkled) ? 1 : 0;
    const char* modeLabels[] = { "Release (standard)", "Release Crinkled (smaller size, requires Crinkler + Ninja)" };
    ImGui::TextUnformatted("Build Mode");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##BuildMode", &modeIndex, modeLabels, 2)) {
        m_buildSettingsMode = modeIndex == 1 ? BuildMode::ReleaseCrinkled : BuildMode::Release;
        m_buildSettingsRefreshRequested = true;
        buildModeChanged = true;
        m_buildSettingsAutoSwitchedToCrinkled = false;
    }

    int targetIndex = 1;
    switch (m_buildSettingsTargetKind) {
        case BuildTargetKind::PackagedDemo: targetIndex = 0; break;
        case BuildTargetKind::SelfContainedDemo: targetIndex = 1; break;
        case BuildTargetKind::SelfContainedScreenSaver: targetIndex = 2; break;
        case BuildTargetKind::MicroDemo: targetIndex = 3; break;
    }
    const char* targetLabels[] = {
        "Packaged (.zip)",
        "Self-Contained (.exe)",
        "Screen Saver (.scr)",
        "Micro-Demo (.exe)"
    };
    ImGui::TextUnformatted("Build Target");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::Combo("##BuildTarget", &targetIndex, targetLabels, 4)) {
        switch (targetIndex) {
            case 0: m_buildSettingsTargetKind = BuildTargetKind::PackagedDemo; break;
            case 2: m_buildSettingsTargetKind = BuildTargetKind::SelfContainedScreenSaver; break;
            case 3: m_buildSettingsTargetKind = BuildTargetKind::MicroDemo; break;
            default: m_buildSettingsTargetKind = BuildTargetKind::SelfContainedDemo; break;
        }
        m_microUbershaderConflictsDirty = true;
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("File menu builds use this target.");
    ImGui::PopTextWrapPos();

    ImGui::SeparatorText("Size Budget");
    static const SizeTargetPreset presets[] = {
        SizeTargetPreset::None,
        SizeTargetPreset::K64,
        SizeTargetPreset::K128,
        SizeTargetPreset::K256,
        SizeTargetPreset::K512,
        SizeTargetPreset::K1024
    };
    bool sizePresetChanged = false;
    for (SizeTargetPreset preset : presets) {
        ImGui::PushID((int)preset);
        const bool selected = (m_buildSettingsSizeTarget == preset);
        const std::string label = std::string(SizePresetLabel(preset)) +
            (preset == SizeTargetPreset::None ? " (No target)" : " (" + std::to_string(SizePresetBytes(preset)) + " bytes)");
        if (ImGui::RadioButton(label.c_str(), selected)) {
            m_buildSettingsSizeTarget = preset;
            sizePresetChanged = true;
            m_buildSettingsAutoSwitchedToCrinkled = false;
        }
        ImGui::PopID();
    }

    const bool tinyTargetSelected = (m_buildSettingsSizeTarget != SizeTargetPreset::None) || m_buildSettingsTargetKind == BuildTargetKind::MicroDemo;
    bool autoSwitchedToCrinkled = false;
    const bool canAutoSwitchToCrinkled =
        tinyTargetSelected &&
        m_buildSettingsMode == BuildMode::Release &&
        m_buildSettingsPrereq.hasCrinkler &&
        m_buildSettingsPrereq.hasNinja;
    if (canAutoSwitchToCrinkled && (sizePresetChanged || buildModeChanged)) {
        m_buildSettingsMode = BuildMode::ReleaseCrinkled;
        m_buildSettingsRefreshRequested = true;
        autoSwitchedToCrinkled = true;
        m_buildSettingsAutoSwitchedToCrinkled = true;
    }

    if (autoSwitchedToCrinkled || m_buildSettingsAutoSwitchedToCrinkled) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.4f, 1.0f), "Switched to Release Crinkled for this size budget.");
        ImGui::PopTextWrapPos();
    }

    if (m_buildSettingsMode == BuildMode::Release && m_buildSettingsSizeTarget != SizeTargetPreset::None) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Size budgets use MicroPlayer (x86). No budget uses full runtime (x64).");
        ImGui::PopTextWrapPos();
    }

    ImGui::Checkbox("Compact track mode", &m_buildSettingsRestrictedCompactTrack);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Stores track data in assets/track.bin and removes extra JSON fields.");
    ImGui::Checkbox("Runtime debug logs", &m_buildSettingsRuntimeDebugLog);
    ImGui::TextDisabled("Adds runtime log text (increases build size).");
    ImGui::Checkbox("Compact-track debug logs", &m_buildSettingsCompactTrackDebugLog);
    ImGui::TextDisabled("Adds compact-track diagnostics (increases build size).");
    ImGui::PopTextWrapPos();

    if (m_buildSettingsTargetKind == BuildTargetKind::MicroDemo) {
        ImGui::Checkbox("Micro developer mode (overlay)", &m_buildSettingsMicroDeveloperBuild);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Shows on-screen MicroPlayer diagnostics for debugging.");
        ImGui::PopTextWrapPos();
        if (m_buildSettingsMicroDeveloperBuild && !m_buildSettingsRuntimeDebugLog) {
            m_buildSettingsRuntimeDebugLog = true;
        }

        bool microConflictSelectionChanged = false;
        ImGui::SeparatorText("Micro Ubershader Conflicts");
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Duplicate helper signatures are listed side by side. Pick what to keep.");
        ImGui::PopTextWrapPos();

        if (LabeledActionButton("ResetMicroConflictKeepAll", OpenFontIcons::kRefresh, "Reset All", "Keep all implementations for all conflicts")) {
            for (const auto& conflict : m_microUbershaderConflicts) {
                auto& keep = m_microUbershaderKeepEntrypointsBySignature[conflict.signatureKey];
                keep.clear();
                keep.reserve(conflict.options.size());
                for (const auto& option : conflict.options) {
                    keep.push_back(option.moduleEntrypoint);
                }
            }
            microConflictSelectionChanged = true;
        }

        if (m_currentProjectPath.empty()) {
            ImGui::TextDisabled("Save the project to analyze micro ubershader conflicts.");
        } else if (m_microUbershaderConflicts.empty()) {
            ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.4f, 1.0f), "No duplicate local function signatures found.");
        } else {
            for (size_t conflictIndex = 0; conflictIndex < m_microUbershaderConflicts.size(); ++conflictIndex) {
                auto& conflict = m_microUbershaderConflicts[conflictIndex];
                auto& keep = m_microUbershaderKeepEntrypointsBySignature[conflict.signatureKey];

                ImGui::PushID(static_cast<int>(conflictIndex));
                ImGui::SeparatorText(conflict.signatureDisplay.c_str());
                if (keep.empty()) {
                    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Unresolved: choose one or more implementations.");
                }
                const int columnCount = (std::max)(1, static_cast<int>(conflict.options.size()));
                if (ImGui::BeginTable("MicroConflictOptions", columnCount, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
                    for (const auto& option : conflict.options) {
                        std::string colName = option.moduleLabel + " (" + option.moduleEntrypoint + ")";
                        ImGui::TableSetupColumn(colName.c_str());
                    }
                    ImGui::TableNextRow();

                    for (size_t optionIndex = 0; optionIndex < conflict.options.size(); ++optionIndex) {
                        const auto& option = conflict.options[optionIndex];
                        ImGui::TableSetColumnIndex(static_cast<int>(optionIndex));
                        const bool selected = std::find(keep.begin(), keep.end(), option.moduleEntrypoint) != keep.end();

                        std::string toggleLabel = std::string(selected ? "[KEEP] " : "[skip] ") + option.moduleEntrypoint;
                        if (ImGui::Selectable(toggleLabel.c_str(), selected, 0, ImVec2(-FLT_MIN, 0.0f))) {
                            if (selected) {
                                keep.erase(std::remove(keep.begin(), keep.end(), option.moduleEntrypoint), keep.end());
                                microConflictSelectionChanged = true;
                            } else {
                                keep.push_back(option.moduleEntrypoint);
                                microConflictSelectionChanged = true;
                            }
                        }

                        ImGui::BeginChild(("Snippet_" + std::to_string(optionIndex)).c_str(), ImVec2(0.0f, 110.0f), true);
                        ImGui::TextUnformatted(option.snippet.c_str());
                        ImGui::EndChild();
                    }

                    ImGui::EndTable();
                }

                if (conflict.options.size() >= 2) {
                    const std::string left = conflict.options[0].moduleEntrypoint;
                    const std::string right = conflict.options[1].moduleEntrypoint;
                    if (ImGui::Button("Keep Left")) {
                        keep = {left};
                        microConflictSelectionChanged = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Keep Right")) {
                        keep = {right};
                        microConflictSelectionChanged = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Keep Both")) {
                        keep = {left, right};
                        microConflictSelectionChanged = true;
                    }
                }
                ImGui::PopID();
            }
        }

        if (microConflictSelectionChanged && !m_currentProjectPath.empty()) {
            SaveProjectUiSettings();
        }
    } else {
        m_buildSettingsMicroDeveloperBuild = false;
    }

    ImGui::SeparatorText("Clean Solution Export");
    {
        static char cleanRootBuffer[1024] = {};
        static bool initialized = false;
        if (!initialized) {
            const size_t copyLen = (std::min)(m_buildSettingsCleanSolutionRootPath.size(), sizeof(cleanRootBuffer) - 1);
            if (copyLen > 0) {
                memcpy(cleanRootBuffer, m_buildSettingsCleanSolutionRootPath.c_str(), copyLen);
            }
            cleanRootBuffer[copyLen] = '\0';
            initialized = true;
        }

        ImGui::TextUnformatted("Solution Root");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##SolutionRoot", cleanRootBuffer, sizeof(cleanRootBuffer))) {
            m_buildSettingsCleanSolutionRootPath = cleanRootBuffer;
        }
        if (LabeledActionButton("ClearSolutionRoot", OpenFontIcons::kTrash2, "Clear", "Clear solution root")) {
            m_buildSettingsCleanSolutionRootPath.clear();
            cleanRootBuffer[0] = '\0';
        }
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Required. Existing content is versioned and replaced each build.");
        ImGui::PopTextWrapPos();
    }

    if (m_buildSettingsRuntimeDebugLog && m_buildSettingsSizeTarget != SizeTargetPreset::None) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.25f, 1.0f), "Runtime debug logs can make you miss the size budget.");
        ImGui::PopTextWrapPos();
    }
    if (m_buildSettingsCompactTrackDebugLog && m_buildSettingsSizeTarget != SizeTargetPreset::None) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.25f, 1.0f), "Compact-track debug logs can make you miss the size budget.");
        ImGui::PopTextWrapPos();
    }

    ImGui::SeparatorText("Dependencies");
    if (LabeledActionButton("RefreshDeps", OpenFontIcons::kRefresh, "Refresh", "Refresh dependency status")) {
        m_buildSettingsRefreshRequested = true;
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("Mode: %s", BuildModeLabel(m_buildSettingsMode));
    const char* targetLabel = "Self-Contained Demo (.exe)";
    if (m_buildSettingsTargetKind == BuildTargetKind::PackagedDemo) targetLabel = "Packaged Demo (.zip)";
    else if (m_buildSettingsTargetKind == BuildTargetKind::SelfContainedScreenSaver) targetLabel = "Self-Contained Screen Saver (.scr)";
    else if (m_buildSettingsTargetKind == BuildTargetKind::MicroDemo) targetLabel = "Micro-Demo (.exe)";
    ImGui::TextDisabled("Target: %s", targetLabel);
    ImGui::PopTextWrapPos();

    const bool crinklerPossible =
        (m_buildSettingsMode == BuildMode::ReleaseCrinkled) &&
        m_buildSettingsPrereq.hasCrinkler &&
        m_buildSettingsPrereq.hasNinja;
    const char* activeLinker = (m_buildSettingsMode == BuildMode::Release)
        ? "MSVC link.exe"
        : (crinklerPossible ? "Crinkler" : "Unavailable (missing Crinkler or Ninja)");
    ImGui::Text("Linker: %s", activeLinker);

    struct DepRow {
        const char* name;
        bool present;
        bool required;
        const char* configureLabel;
        std::string configureTarget;
    };

    const bool crinklerRequired = (m_buildSettingsMode == BuildMode::ReleaseCrinkled);
    const bool ninjaRequired = (m_buildSettingsMode == BuildMode::ReleaseCrinkled);
    const std::vector<DepRow> deps = {
        { "Visual Studio C++ Build Tools", m_buildSettingsPrereq.hasVisualStudioTools, true, "Install", "https://visualstudio.microsoft.com/downloads/" },
        { "Windows SDK", m_buildSettingsPrereq.hasWindowsSdk, true, "Install", "https://developer.microsoft.com/windows/downloads/windows-sdk/" },
        { "CMake", m_buildSettingsPrereq.hasCMake, true, "Install", "https://cmake.org/download/" },
        { "DXC Runtime (dxcompiler.dll)", m_buildSettingsPrereq.hasDxcRuntime, true, "Download", "https://github.com/microsoft/DirectXShaderCompiler/releases" },
        { "Crinkler", m_buildSettingsPrereq.hasCrinkler, crinklerRequired, "Setup Guide", m_appRoot + "\\README-CRINKLER.txt" },
        { "Ninja", m_buildSettingsPrereq.hasNinja, ninjaRequired, "Install", "https://github.com/ninja-build/ninja/releases" }
    };

    if (ImGui::BeginTable("BuildDeps", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Dependency");
        ImGui::TableSetupColumn("Required");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Configure");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < deps.size(); ++i) {
            const auto& dep = deps[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(dep.name);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(dep.required ? "Yes" : "Optional");

            ImGui::TableSetColumnIndex(2);
            if (dep.present) {
                ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.4f, 1.0f), "Detected");
            } else if (dep.required) {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Missing");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.25f, 1.0f), "Missing (Optional)");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID((int)i + 1000);
            if (!dep.present && LabeledActionButton("Cfg", OpenFontIcons::kFolder, dep.configureLabel, dep.configureLabel, ImVec2(-FLT_MIN, 0.0f))) {
                OpenExternal(dep.configureTarget);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!m_buildSettingsPrereq.hasCrinkler) {
        if (LabeledActionButton("PickCrinklerExe", OpenFontIcons::kFolder, "Crinkler Override", "Use a custom crinkler.exe (bundled copy is preferred)")) {
            char pathBuf[512] = { 0 };
            OPENFILENAMEA ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
            ofn.lpstrFile = pathBuf;
            ofn.nMaxFile = sizeof(pathBuf);
            ofn.lpstrFilter = "Crinkler (crinkler.exe)\0crinkler.exe\0Executables (*.exe)\0*.exe\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) {
                m_buildSettingsCrinklerPath = pathBuf;
                SetEnvironmentVariableA("SHADERLAB_CRINKLER", pathBuf);
                m_buildSettingsRefreshRequested = true;
            }
        }
    }

    if (!m_buildSettingsPrereq.hasNinja) {
        if (LabeledActionButton("CopyNinjaWinget", OpenFontIcons::kCopy, "Copy Ninja Command", "Copy Ninja install command")) {
            ImGui::SetClipboardText("winget install Ninja-build.Ninja");
        }
    }

    if (!m_buildSettingsPrereq.message.empty()) {
        ImGui::Separator();
        ImGui::BeginChild("PrereqMsg", ImVec2(0, 120), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(m_buildSettingsPrereq.message.c_str());
        ImGui::EndChild();
    }

    ImGui::Separator();
    if (!canCloseBuildWindow) {
        ImGui::BeginDisabled();
    }
    if (LabeledActionButton("CloseBuildSettings", OpenFontIcons::kXCircle, "Close", "Close", ImVec2(140.0f, 0.0f)) && canCloseBuildWindow) {
        m_showBuildSettings = false;
    }
    if (!canCloseBuildWindow) {
        ImGui::EndDisabled();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("Build in progress. This window will close when done.");
        ImGui::PopTextWrapPos();
    }

    const bool hasCleanRoot = !m_buildSettingsCleanSolutionRootPath.empty();
    const bool canBuild = m_buildSettingsPrereq.ok && hasCleanRoot;
    const bool canStartBuild = canBuild && !m_isBuilding;
    if (!canStartBuild) {
        ImGui::BeginDisabled();
    }
    if (LabeledActionButton("BuildFromSettings", OpenFontIcons::kPlay, "Build Now", "Build to the selected solution root", ImVec2(180.0f, 0.0f))) {
        if (m_currentMode == UIMode::PostFX && m_postFxSourceSceneIndex >= 0 && m_postFxSourceSceneIndex < (int)m_scenes.size()) {
            m_scenes[m_postFxSourceSceneIndex].postFxChain = m_postFxDraftChain;
        }

        if (m_currentProjectPath.empty()) {
            SaveProjectAs();
            if (m_currentProjectPath.empty()) {
                if (!canBuild) {
                    ImGui::EndDisabled();
                }
                ImGui::End();
                return;
            }
        } else {
            SaveProject();
        }

        const BuildTargetKind buildTargetKind = m_buildSettingsTargetKind;
        const bool buildScreenSaver = buildTargetKind == BuildTargetKind::SelfContainedScreenSaver;
        const bool buildPackaged = buildTargetKind == BuildTargetKind::PackagedDemo;
        std::string targetOutputPath;
        const std::string defaultBinaryName = (m_currentProjectPath.empty() ? "MyDemo" : fs::path(m_currentProjectPath).stem().string()) + (buildPackaged ? ".zip" : (buildScreenSaver ? ".scr" : ".exe"));

        fs::path outputDir = fs::path(m_buildSettingsCleanSolutionRootPath);
        std::error_code makeDirEc;
        fs::create_directories(outputDir, makeDirEc);
        if (makeDirEc) {
            std::lock_guard<std::mutex> lock(m_buildLogMutex);
            m_buildLog += "Error: Failed to create clean solution root directory for output binary.\n";
            m_buildLog += "Path: " + outputDir.string() + "\n";
            m_buildLog += "Details: " + makeDirEc.message() + "\n";
            ImGui::End();
            return;
        }
        targetOutputPath = (outputDir / defaultBinaryName).string();

        if (!targetOutputPath.empty()) {
            if (buildTargetKind == BuildTargetKind::MicroDemo) {
                {
                    std::lock_guard<std::mutex> lock(m_buildLogMutex);
                    m_buildLog += "[Micro Step 1/2] Preflight: assembling conflict state for ubershader...\n";
                }
                m_microUbershaderConflictsDirty = true;
                RefreshMicroUbershaderConflictCache();

                bool hasUnresolvedConflicts = false;
                for (const auto& conflict : m_microUbershaderConflicts) {
                    const auto it = m_microUbershaderKeepEntrypointsBySignature.find(conflict.signatureKey);
                    if (it == m_microUbershaderKeepEntrypointsBySignature.end() || it->second.empty()) {
                        hasUnresolvedConflicts = true;
                        break;
                    }
                }

                if (hasUnresolvedConflicts) {
                    std::lock_guard<std::mutex> lock(m_buildLogMutex);
                    m_buildLog += "[Micro Step 1/2] Conflict(s) detected and unresolved. Resolve them in 'Micro Ubershader Conflicts' before build.\n";
                    m_buildLog += "Build not started.\n";
                    ImGui::End();
                    return;
                }

                {
                    std::error_code cleanupEc;
                    const fs::path stalePackShaders = fs::path(m_buildSettingsCleanSolutionRootPath) / "build_selfcontained_pack" / "assets" / "shaders";
                    fs::remove(stalePackShaders / "ubershader.hlsl", cleanupEc);
                    cleanupEc.clear();
                    fs::remove(stalePackShaders / "ubershader.bin", cleanupEc);
                }

                {
                    std::lock_guard<std::mutex> lock(m_buildLogMutex);
                    m_buildLog += "[Micro Step 2/2] Conflicts resolved. Rebuilding ubershader + runtime from scratch...\n";
                }
            }

            m_isBuilding = true;
            m_buildComplete = false;
            m_buildSuccess = false;
            m_buildLog = "Initializing Build Process...\n";
            m_buildLog += std::string("Build Mode: ") + BuildModeLabel(m_buildSettingsMode) + "\n";
            m_buildLog += std::string("Size Target: ") + SizePresetLabel(m_buildSettingsSizeTarget) + "\n";
            m_buildLog += std::string("Restricted Compact Track: ") + (m_buildSettingsRestrictedCompactTrack ? "Enabled" : "Disabled") + "\n";
            m_buildLog += std::string("Runtime Debug Logs: ") + (m_buildSettingsRuntimeDebugLog ? "Enabled" : "Disabled") + "\n";
            m_buildLog += std::string("Compact Track Debug Logs: ") + (m_buildSettingsCompactTrackDebugLog ? "Enabled" : "Disabled") + "\n";
            m_buildLog += std::string("Output Type: ") + (buildPackaged ? "Packaged Demo (.zip)" : (buildScreenSaver ? "Screen Saver (.scr)" : "Executable (.exe)")) + "\n";
            m_buildLog += std::string("Clean Solution Root: ") + m_buildSettingsCleanSolutionRootPath + "\n";
            m_buildLog += std::string("Output Binary: ") + targetOutputPath + "\n";
            if (m_buildSettingsAutoSwitchedToCrinkled) {
                m_buildLog += "Build Mode Auto-Switch: Release -> Release Crinkled (size budget with Crinkler+Ninja detected)\n";
                m_buildSettingsAutoSwitchedToCrinkled = false;
            }

            const std::string targetExePath = targetOutputPath;
            const std::string projectPath = m_currentProjectPath;
            const std::string appRoot = m_appRoot;
            const BuildTargetKind selectedTargetKind = m_buildSettingsTargetKind;
            const BuildMode selectedMode = m_buildSettingsMode;
            const SizeTargetPreset selectedSizeTarget = m_buildSettingsSizeTarget;
            const bool selectedRestrictedCompactTrack = m_buildSettingsRestrictedCompactTrack;
            const bool selectedRuntimeDebugLog = m_buildSettingsRuntimeDebugLog;
            const bool selectedCompactTrackDebugLog = m_buildSettingsCompactTrackDebugLog;
            const bool selectedMicroDeveloperBuild = m_buildSettingsMicroDeveloperBuild;
            const std::string selectedCleanSolutionRootPath = m_buildSettingsCleanSolutionRootPath;
            const auto selectedMicroKeepEntrypointsBySignature = m_microUbershaderKeepEntrypointsBySignature;

            m_buildFuture = std::async(std::launch::async, [this, targetExePath, projectPath, appRoot, selectedTargetKind, selectedMode, selectedSizeTarget, selectedRestrictedCompactTrack, selectedRuntimeDebugLog, selectedCompactTrackDebugLog, selectedMicroDeveloperBuild, selectedCleanSolutionRootPath, selectedMicroKeepEntrypointsBySignature]() {
                auto Log = [&](const std::string& msg) {
                    std::lock_guard<std::mutex> lock(m_buildLogMutex);
                    m_buildLog += msg;
                    if (msg.empty() || msg.back() != '\n') {
                        m_buildLog += "\n";
                    }
                };

                BuildRequest request;
                request.appRoot = appRoot;
                request.projectPath = projectPath;
                request.targetExePath = targetExePath;
                request.targetKind = selectedTargetKind;
                request.mode = selectedMode;
                request.sizeTarget = selectedSizeTarget;
                request.restrictedCompactTrack = selectedRestrictedCompactTrack;
                request.runtimeDebugLog = selectedRuntimeDebugLog;
                request.compactTrackDebugLog = selectedCompactTrackDebugLog;
                request.microDeveloperBuild = selectedMicroDeveloperBuild;
                request.cleanSolutionRootPath = selectedCleanSolutionRootPath;
                if (selectedTargetKind == BuildTargetKind::MicroDemo) {
                    request.microUbershaderKeepEntrypointsBySignature = selectedMicroKeepEntrypointsBySignature;
                }

                BuildResult result = BuildPipeline::BuildSelfContained(request, Log);
                m_buildSuccess = result.success;
                m_buildComplete = true;
            });
        }
    }
    if (!canStartBuild) {
        ImGui::EndDisabled();
        if (!canBuild) {
            ImGui::PushTextWrapPos(0.0f);
            if (!hasCleanRoot) {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Set Solution Root to enable build.");
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Install missing required dependencies to enable build.");
            }
            ImGui::PopTextWrapPos();
        } else if (m_isBuilding) {
            ImGui::TextDisabled("Build already running.");
        }
    }
    if (m_buildSettingsTargetKind == BuildTargetKind::MicroDemo && unresolvedMicroConflictCount > 0) {
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Unresolved conflicts: %d", unresolvedMicroConflictCount);
    }

    if (m_isBuilding || !m_buildLog.empty()) {
        static bool autoCopyOnFailure = false;
        static bool didAutoCopy = false;

        ImGui::SeparatorText("Build Console");
        ImGui::PushStyleColor(ImGuiCol_ChildBg, m_uiThemeColors.ConsoleBackground);
        ImGui::PushStyleColor(ImGuiCol_Text, m_uiThemeColors.ConsoleFontColor);
        ImGui::BeginChild("BuildConsoleRegion", ImVec2(0.0f, 220.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
        {
            std::lock_guard<std::mutex> lock(m_buildLogMutex);
            ImGui::TextUnformatted(m_buildLog.c_str());
            if (m_isBuilding && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);

        if (!m_isBuilding && m_buildComplete) {
            if (m_buildSuccess) {
                ImGui::TextColored(m_uiThemeColors.StatusFontColor, "Build Completed Successfully!");
            } else {
                ImGui::TextColored(m_uiThemeColors.StatusFontColor, "Build Failed.");
                if (autoCopyOnFailure && !didAutoCopy) {
                    std::lock_guard<std::mutex> lock(m_buildLogMutex);
                    ImGui::SetClipboardText(m_buildLog.c_str());
                    didAutoCopy = true;
                }
            }
        } else if (m_isBuilding) {
            didAutoCopy = false;
            static float time = 0.0f;
            time += ImGui::GetIO().DeltaTime;
            const char* dots = (int(time * 2) % 4) == 0 ? ".   " : (int(time * 2) % 4) == 1 ? "..  " : (int(time * 2) % 4) == 2 ? "... " : "....";
            ImGui::Text("Building%s", dots);
        }

        if (LabeledActionButton("CopyBuildLogInline", OpenFontIcons::kCopy, "Copy Log", "Copy build log text", ImVec2(150.0f, 0.0f))) {
            std::lock_guard<std::mutex> lock(m_buildLogMutex);
            ImGui::SetClipboardText(m_buildLog.c_str());
        }
        ImGui::Checkbox("Auto copy on failure", &autoCopyOnFailure);
    }

    const bool settingsChanged =
        prevTargetKind != m_buildSettingsTargetKind ||
        prevMode != m_buildSettingsMode ||
        prevSizeTarget != m_buildSettingsSizeTarget ||
        prevRestrictedCompactTrack != m_buildSettingsRestrictedCompactTrack ||
        prevRuntimeDebugLog != m_buildSettingsRuntimeDebugLog ||
        prevCompactTrackDebugLog != m_buildSettingsCompactTrackDebugLog ||
        prevMicroDeveloperBuild != m_buildSettingsMicroDeveloperBuild ||
        prevCleanSolutionRootPath != m_buildSettingsCleanSolutionRootPath;
    if (settingsChanged) {
        SaveUiBuildSettings(
            m_appRoot,
            m_buildSettingsTargetKind,
            m_buildSettingsMode,
            m_buildSettingsSizeTarget,
            m_buildSettingsRestrictedCompactTrack,
            m_buildSettingsRuntimeDebugLog,
            m_buildSettingsCompactTrackDebugLog,
            m_buildSettingsMicroDeveloperBuild,
            m_buildSettingsCleanSolutionRootPath,
            m_buildSettingsCrinklerPath);
        if (!m_currentProjectPath.empty()) {
            SaveProjectUiSettings();
        }
    }

    ImGui::End();
}

void UISystem::UpdateBuildLogic() {
    if (m_isBuilding && m_buildComplete) {
        m_isBuilding = false;
    }
}

} // namespace ShaderLab
