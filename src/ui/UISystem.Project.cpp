#include "ShaderLab/UI/UISystem.h"

#include <filesystem>

#include "ShaderLab/Audio/AudioSystem.h"
#include "ShaderLab/Core/RuntimeExporter.h"
#include "ShaderLab/Core/Serializer.h"

#include <commdlg.h>
#include <imgui.h>
#include <shellapi.h>

namespace ShaderLab {

namespace fs = std::filesystem;

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
        }
    }
}

void UISystem::BuildProject() {
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