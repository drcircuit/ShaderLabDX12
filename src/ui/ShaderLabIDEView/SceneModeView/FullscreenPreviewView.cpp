#include "ShaderLab/UI/ShaderLabIDE.h"

#include <imgui.h>

#include <algorithm>

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

void ShaderLabIDE::ShowFullscreenPreview() {
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

}
