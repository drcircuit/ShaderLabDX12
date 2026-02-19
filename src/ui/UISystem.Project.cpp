#include "ShaderLab/UI/UISystem.h"

#include <filesystem>
#include <fstream>

#include "ShaderLab/Audio/AudioSystem.h"
#include "ShaderLab/Core/RuntimeExporter.h"
#include "ShaderLab/Core/Serializer.h"

#include <nlohmann/json.hpp>

#include <commdlg.h>
#include <imgui.h>
#include <shellapi.h>

namespace ShaderLab {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
fs::path GetProjectUiSettingsPath(const std::string& projectPath) {
    if (projectPath.empty()) {
        return {};
    }
    const fs::path project(projectPath);
    const fs::path parent = project.parent_path();
    const std::string stem = project.stem().string();
    return parent / (stem + ".shaderlab.user.json");
}

SizeTargetPreset ParseSizePresetFromString(const std::string& value) {
    if (value == "1k") return SizeTargetPreset::K1;
    if (value == "2k") return SizeTargetPreset::K2;
    if (value == "4k") return SizeTargetPreset::K4;
    if (value == "16k") return SizeTargetPreset::K16;
    if (value == "32k") return SizeTargetPreset::K32;
    if (value == "64k") return SizeTargetPreset::K64;
    return SizeTargetPreset::None;
}

std::string SizePresetToString(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K1: return "1k";
        case SizeTargetPreset::K2: return "2k";
        case SizeTargetPreset::K4: return "4k";
        case SizeTargetPreset::K16: return "16k";
        case SizeTargetPreset::K32: return "32k";
        case SizeTargetPreset::K64: return "64k";
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
} // namespace

void UISystem::LoadProjectUiSettings() {
    const fs::path settingsPath = GetProjectUiSettingsPath(m_currentProjectPath);
    if (settingsPath.empty()) {
        return;
    }

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
    m_buildSettingsTargetKind = ParseBuildTargetKindFromString(build.value("targetKind", std::string("selfcontained")));
    const std::string modeStr = build.value("mode", std::string("release"));
    m_buildSettingsMode = (modeStr == "crinkled" || modeStr == "release-crinkled")
        ? BuildMode::ReleaseCrinkled
        : BuildMode::Release;
    m_buildSettingsSizeTarget = ParseSizePresetFromString(build.value("sizeTarget", std::string("none")));
    m_buildSettingsRestrictedCompactTrack = build.value("restrictedCompactTrack", m_buildSettingsRestrictedCompactTrack);
    m_buildSettingsRuntimeDebugLog = build.value("runtimeDebugLog", m_buildSettingsRuntimeDebugLog);
    m_buildSettingsCompactTrackDebugLog = build.value("compactTrackDebugLog", m_buildSettingsCompactTrackDebugLog);
    m_buildSettingsMicroDeveloperBuild = build.value("microDeveloperBuild", m_buildSettingsMicroDeveloperBuild);
    m_buildSettingsCleanSolutionRootPath = build.value("cleanSolutionRootPath", m_buildSettingsCleanSolutionRootPath);
    m_microUbershaderKeepEntrypointsBySignature.clear();
    const json keepMap = build.value("microUbershaderKeepEntrypoints", json::object());
    if (keepMap.is_object()) {
        for (auto it = keepMap.begin(); it != keepMap.end(); ++it) {
            if (!it.value().is_array()) {
                continue;
            }

            std::vector<std::string> keep;
            for (const auto& entry : it.value()) {
                if (entry.is_string()) {
                    keep.push_back(entry.get<std::string>());
                }
            }
            if (!keep.empty()) {
                m_microUbershaderKeepEntrypointsBySignature[it.key()] = std::move(keep);
            }
        }
    }
    m_microUbershaderConflictsDirty = true;
    m_buildSettingsRefreshRequested = true;
}

void UISystem::SaveProjectUiSettings() const {
    if (m_currentProjectPath.empty()) {
        return;
    }

    const fs::path settingsPath = GetProjectUiSettingsPath(m_currentProjectPath);
    std::ofstream out(settingsPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }

    json root;
    json microKeep = json::object();
    for (const auto& [signature, keep] : m_microUbershaderKeepEntrypointsBySignature) {
        if (keep.empty()) {
            continue;
        }
        microKeep[signature] = keep;
    }
    root["build"] = {
        {"targetKind", BuildTargetKindToString(m_buildSettingsTargetKind)},
        {"mode", (m_buildSettingsMode == BuildMode::ReleaseCrinkled) ? "crinkled" : "release"},
        {"sizeTarget", SizePresetToString(m_buildSettingsSizeTarget)},
        {"restrictedCompactTrack", m_buildSettingsRestrictedCompactTrack},
        {"runtimeDebugLog", m_buildSettingsRuntimeDebugLog},
        {"compactTrackDebugLog", m_buildSettingsCompactTrackDebugLog},
        {"microDeveloperBuild", m_buildSettingsMicroDeveloperBuild},
        {"cleanSolutionRootPath", m_buildSettingsCleanSolutionRootPath},
        {"microUbershaderKeepEntrypoints", microKeep}
    };

    out << root.dump(2);
}

void UISystem::SaveProject() {
    if (m_currentProjectPath.empty()) {
        SaveProjectAs();
        return;
    }

    ProjectData data;
    data.scenes = m_scenes;
    data.track = m_track;
    data.transport = m_transport;
    data.audioLibrary = m_audioLibrary;

    fs::path projectRoot = fs::path(m_currentProjectPath).parent_path();
    if (Serializer::ConsolidateProject(data, projectRoot.string())) {
        m_scenes = data.scenes;
        m_audioLibrary = data.audioLibrary;

        if (Serializer::SaveProject(data, m_currentProjectPath)) {
            SaveProjectUiSettings();
        }
    }
}

void UISystem::SaveProjectAs() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "JSON Project (*.json)\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        m_currentProjectPath = szFile;
        SaveProject();
        SaveProjectUiSettings();
    }
}

void UISystem::OpenProject() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "JSON Project (*.json)\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        m_currentProjectPath = szFile;
        ProjectData data;
        if (Serializer::LoadProject(m_currentProjectPath, data)) {
            m_scenes = data.scenes;
            m_audioLibrary = data.audioLibrary;
            m_track = data.track;
            m_transport.bpm = data.transport.bpm;

            fs::current_path(fs::path(m_currentProjectPath).parent_path());

            if (m_audioSystem) {
                m_audioSystem->Stop();
            }

            if (m_deviceRef) {
                for(auto& scene : m_scenes) {
                    for(auto& bind : scene.bindings) {
                        if (bind.bindingType == BindingType::File && !bind.filePath.empty()) {
                            LoadTextureFromFile(bind.filePath, bind.textureResource);
                        }
                    }
                }
            }

            LoadProjectUiSettings();
        }
    }
}

void UISystem::BuildProject() {
    m_buildSettingsTargetKind = BuildTargetKind::SelfContainedDemo;
    m_showBuildSettings = true;
    m_buildSettingsRefreshRequested = true;
}

void UISystem::BuildScreenSaverProject() {
    m_buildSettingsTargetKind = BuildTargetKind::SelfContainedScreenSaver;
    m_showBuildSettings = true;
    m_buildSettingsRefreshRequested = true;
}

void UISystem::BuildPackagedDemoProject() {
    m_buildSettingsTargetKind = BuildTargetKind::PackagedDemo;
    m_showBuildSettings = true;
    m_buildSettingsRefreshRequested = true;
}

void UISystem::BuildMicroDemoProject() {
    m_buildSettingsTargetKind = BuildTargetKind::MicroDemo;
    m_buildSettingsMicroDeveloperBuild = false;
    m_microUbershaderConflictsDirty = true;
    m_showBuildSettings = true;
    m_buildSettingsRefreshRequested = true;
}

void UISystem::BuildMicroDeveloperDemoProject() {
    m_buildSettingsTargetKind = BuildTargetKind::MicroDemo;
    m_buildSettingsMicroDeveloperBuild = true;
    m_buildSettingsRuntimeDebugLog = true;
    m_microUbershaderConflictsDirty = true;
    m_showBuildSettings = true;
    m_buildSettingsRefreshRequested = true;
}

void UISystem::ExportRuntimePackage() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Executable (*.exe)\0*.exe\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "exe";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (!m_currentProjectPath.empty()) {
        std::string name = fs::path(m_currentProjectPath).stem().string();
        strcpy_s(szFile, name.c_str());
    } else {
        strcpy_s(szFile, "MyDemo");
    }

    if (GetSaveFileNameA(&ofn)) {
        ProjectData data;
        data.scenes = m_scenes;
        data.track = m_track;
        data.transport = m_transport;
        data.audioLibrary = m_audioLibrary;

        RuntimeExportRequest request;
        request.appRoot = m_appRoot;
        request.destExePath = szFile;
        request.data = data;

        RuntimeExportResult result = RuntimeExporter::Export(request);
        if (result.success) {
            MessageBoxA(NULL, result.message.c_str(), "Export Complete", MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL, result.message.c_str(), "Export Error", MB_ICONERROR);
        }
    }
}

} // namespace ShaderLab