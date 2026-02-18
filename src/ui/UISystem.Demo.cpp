#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UIConfig.h"
#include "ShaderLab/UI/OpenFontIcons.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/Audio/AudioSystem.h"

#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace ShaderLab {

namespace {
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

    drawList->AddText(font, fontSize, ImVec2(std::floor(textX), std::floor(textY)), ImGui::GetColorU32(UIConfig::ColorCheckMark), icon.c_str());

    if (ImGui::IsItemHovered() && tooltip && *tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return pressed;
}

ImVec2 CompactIconSquareSize() {
    const float side = ImGui::GetFrameHeight();
    return ImVec2(side, side);
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

void UISystem::CreateDefaultTrack() {
    m_track.name = "Main Track";
    m_track.bpm = 120.0f;
    m_track.lengthBeats = 512;

    // Start scene 0 at beat 0
    TrackerRow startRow;
    startRow.rowId = 0;
    startRow.sceneIndex = 0;
    m_track.rows.push_back(startRow);
}

void UISystem::ShowAudioLibrary() {
    if (ImGui::Begin("Audio Library")) {
        // Add Button
        if (IconButton("AddAudioFile", OpenFontIcons::kFilePlus, "Add audio file")) {
             OPENFILENAMEA ofn = {0};
             char szFile[260] = {0};
             ofn.lStructSize = sizeof(ofn);
             ofn.hwndOwner = nullptr;
             ofn.lpstrFile = szFile;
             ofn.nMaxFile = sizeof(szFile);
             ofn.lpstrFilter = "Audio\0*.mp3;*.wav;*.ogg\0All\0*.*\0";
             ofn.nFilterIndex = 1;
             ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
             if (GetOpenFileNameA(&ofn)) {
                 AudioClip clip;
                 clip.path = szFile;
                 // Extract filename for name
                 std::string pathStr = szFile;
                 size_t lastSlash = pathStr.find_last_of("\\/");
                 if (lastSlash != std::string::npos)
                     clip.name = pathStr.substr(lastSlash + 1);
                 else
                     clip.name = pathStr;

                 // Smart guess type (wav/ogg often sfx, mp3 often music)
                 // Keeping default Music for now.
                 m_audioLibrary.push_back(clip);
             }
        }

        ImGui::Separator();

        // List
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
        if (ImGui::BeginTable("AudioLibTable", 5, flags)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("BPM", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)m_audioLibrary.size(); ++i) {
                ImGui::PushID(i);
                ImGui::TableNextRow();
                auto& clip = m_audioLibrary[i];

                ImGui::TableSetColumnIndex(0);
                PushNumericFont();
                ImGui::Text("%d", i);
                PopNumericFont();

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", clip.name.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", clip.path.c_str());

                ImGui::TableSetColumnIndex(2);
                const char* types[] = { "Music", "OneShot" };
                int currentType = (clip.type == AudioType::Music) ? 0 : 1;
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::Combo("##Type", &currentType, types, 2)) {
                    clip.type = (currentType == 0) ? AudioType::Music : AudioType::OneShot;
                }

                ImGui::TableSetColumnIndex(3);
                 if (clip.type == AudioType::Music) {
                     ImGui::SetNextItemWidth(-FLT_MIN);
                     // BPM of the specific audio file - This should remain editable as it's a property of the file
                     PushNumericFont();
                     ImGui::InputFloat("##BPM", &clip.bpm, 0.0f, 0.0f, "%.1f");
                     PopNumericFont();
                 } else {
                     ImGui::TextDisabled("-");
                }

                ImGui::TableSetColumnIndex(4);
                if (IconButton("DeleteAudio", OpenFontIcons::kTrash2, "Delete audio clip")) {
                     // Check if this clip is currently playing and stop it
                     if (m_audioSystem && m_audioSystem->IsPlaying() && m_currentMode == UIMode::Demo) {
                         // We don't have a way to check WHICH clip is playing exactly by index easily here
                         // without tracking it. But safest is to Stop audio if we are deleting.
                         // Or better, let's just assume if users are deleting from library, they accept a glitch/stop.
                         // Actually, checking filename match is cleaner if AudioSystem exposed it.
                         // For now, force Stop to be safe against ghost audio.
                         m_audioSystem->Stop();
                     }

                     if (m_activeMusicIndex == i) {
                         m_activeMusicIndex = -1;
                     } else if (m_activeMusicIndex > i) {
                         m_activeMusicIndex -= 1;
                     }

                     m_audioLibrary.erase(m_audioLibrary.begin() + i);
                     // Note: References by index in Track/Grid will break!
                     // Ideally we would use UUIDs, but for strict prototype index fixup is skipped.
                     // Warning: Deleting items shifts indices.
                     i--;
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void UISystem::ShowDemoPlaylist() {
    // Only use m_track now
    // Create default if empty/fresh? But m_track is a member, so it exists.
    // Just ensure it has some rows maybe.
    if (m_track.rows.empty()) {
        TrackerRow startRow; startRow.rowId = 0;
        m_track.rows.push_back(startRow);
    }

    if (ImGui::Begin("Demo: Playlist")) {
        // --- Single Track Editor ---
        auto& track = m_track;

        auto ScrubToBeat = [&](int targetBeat) {
            if (m_transport.state == TransportState::Playing) {
                m_transport.state = TransportState::Paused;
                if (m_audioSystem) m_audioSystem->Pause();
            }
            SeekToBeat(targetBeat);
        };

        ImGui::Text("Tracking Demo: %s", GetProjectName().c_str());

        ImGui::SameLine();
        ImGui::Text("BPM");
        ImGui::SameLine(0.0f, 4.0f);
        SetNextNumericFieldWidth(60.0f);
        PushNumericFont();
        if (ImGui::InputFloat("##TrackBPM", &track.bpm, 0.0f, 0.0f, "%.1f")) {
            if (track.bpm < 1.0f) track.bpm = 1.0f;
        }
        PopNumericFont();
        ImGui::SameLine(0.0f, 2.0f);
        const ImVec2 spinnerSize = CompactIconSquareSize();
        auto SpinnerIconButton = [&](const char* id, uint32_t icon, const char* tooltip) -> bool {
            return IconButton(id, icon, tooltip, spinnerSize);
        };
        if (SpinnerIconButton("BpmDec", OpenFontIcons::kMinus, "Decrease BPM")) {
            if (track.bpm > 1.0f) track.bpm -= 1.0f;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (SpinnerIconButton("BpmInc", OpenFontIcons::kPlus, "Increase BPM")) {
            track.bpm += 1.0f;
        }

        ImGui::SameLine(0.0f, 12.0f);
        ImGui::Text("Bars");
        ImGui::SameLine(0.0f, 4.0f);
        SetNextNumericFieldWidth(70.0f);
        int bars = (track.lengthBeats + 3) / 4;
        PushNumericFont();
        if (ImGui::InputInt("##TrackBars", &bars, 4, 16)) {
            if (bars < 1) bars = 1;
            track.lengthBeats = bars * 4;
        }
        PopNumericFont();
        ImGui::SameLine(0.0f, 6.0f);
        PushNumericFont();
        ImGui::TextUnformatted("(");
        ImGui::SameLine(0.0f, 0.0f);
        PushNumericFont();
        ImGui::Text("%d", track.lengthBeats);
        PopNumericFont();
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextUnformatted(" beats)");
        PopNumericFont();

        ImGui::Separator();

        // 2. Tracker Grid
        // We want a table: [Beat] [Scene] [Transition] [Music] [OneShot]
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("TrackerGrid", 5, flags)) {
            ImGui::TableSetupColumn("Bar:Beat", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Scene", ImGuiTableColumnFlags_WidthFixed, 210.0f);
            ImGui::TableSetupColumn("Transition", ImGuiTableColumnFlags_WidthFixed, 180.0f);
            ImGui::TableSetupColumn("Music", ImGuiTableColumnFlags_WidthFixed, 210.0f);
            ImGui::TableSetupColumn("OneShot", ImGuiTableColumnFlags_WidthFixed, 60.0f); // Narrower for icon
            ImGui::TableHeadersRow();

            if (m_transport.state == TransportState::Playing && track.lengthBeats > 0) {
                const float rowHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().CellPadding.y * 2.0f;
                const float windowHeight = ImGui::GetWindowHeight();
                const float rowMin = rowHeight * track.currentBeat;
                const float rowMax = rowMin + rowHeight;
                const float scrollY = ImGui::GetScrollY();
                const float scrollMax = ImGui::GetScrollMaxY();

                if (rowMin < scrollY || rowMax > scrollY + windowHeight - rowHeight) {
                    float targetY = rowMin - rowHeight * 2.0f;
                    if (targetY < 0.0f) targetY = 0.0f;
                    if (targetY > scrollMax) targetY = scrollMax;
                    ImGui::SetScrollY(targetY);
                }
            }

            // Prepare scene names for Combos
            std::vector<const char*> sceneNames;
            sceneNames.reserve(m_scenes.size() + 1);
            sceneNames.push_back("(Hold)"); // Index -1 means "continue previous"
            for (const auto& s : m_scenes) sceneNames.push_back(s.name.c_str());

            const char* transNames[] = { "None", "Crossfade", "DipToBlack", "FadeOut", "FadeIn", "Glitch", "Pixel" }; // Replaced Cut and added explicit Fades

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive()) {
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
                    ScrubToBeat(track.currentBeat - 1);
                } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
                    ScrubToBeat(track.currentBeat + 1);
                }
            }

            // Iterate through every beat
            ImGuiListClipper clipper;
            clipper.Begin(track.lengthBeats);
            while (clipper.Step()) {
                for (int beat = clipper.DisplayStart; beat < clipper.DisplayEnd; beat++) {
                    ImGui::PushID(beat);

                    ImGui::TableNextRow();

                    // Column 0: Beat (Bar:Beat)
                    int bar = (beat / 4) + 1;
                    int subBeat = (beat % 4) + 1;

                    // Highlight logic
                    bool isCurrent = (beat == track.currentBeat);
                    bool isBarStart = (subBeat == 1);
                    bool isBeatThree = (subBeat == 3);

                    if (isBarStart) {
                        // Darker styling for first beat of bar
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.1f, 0.1f, 0.1f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
                    }

                    if (isCurrent) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(0, 150, 255, 120));
                    } else if (isBarStart) {
                        // Darker row background for bar markers
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(0, 0, 0, 80));
                    } else if (isBeatThree) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
                    }

                    ImGui::TableSetColumnIndex(0);

                    // Helper to get row safely
                    auto GetRow = [&]() -> TrackerRow* {
                        for (auto& r : track.rows) { if (r.rowId == beat) return &r; }
                        return nullptr;
                    };

                    // Helper to ensure row exists
                    auto EnsureRow = [&]() {
                        if (GetRow()) return;
                        TrackerRow newRow;
                        newRow.rowId = beat;
                        track.rows.push_back(newRow);
                    };

                    TrackerRow* row = GetRow();

                    // "Stop" Button/Indicator
                    if (row && row->stop) {
                        if (IconButton("StopMarker", OpenFontIcons::kStop, "Clear STOP marker", ImVec2(-FLT_MIN, 0))) {
                             row->stop = false;
                        }
                    } else {
                        PushNumericFont();
                        if (isBarStart) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%02d:%02d", bar, subBeat);
                        else ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%02d:%02d", bar, subBeat);
                        PopNumericFont();

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                            ScrubToBeat(beat);
                        }

                        // Right-click to toggle STOP
                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                            EnsureRow();
                            row = GetRow();
                            if (row) row->stop = !row->stop;
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Right-click to toggle STOP command");
                    }

                    // Column 1: Scene
                    ImGui::TableSetColumnIndex(1);
                    ImGui::BeginGroup();
                    int currentSceneSel = (row) ? row->sceneIndex : -1;
                    int comboIdx = currentSceneSel + 1;
                    float currentOffset = (row) ? row->timeOffset : 0.0f;

                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90); // make room for offset input
                    if (ImGui::Combo("##Scene", &comboIdx, sceneNames.data(), (int)sceneNames.size())) {
                        EnsureRow();
                        row = GetRow();
                        if (row) row->sceneIndex = comboIdx - 1;
                    }

                    if (comboIdx > 0) { // If scene selected
                        ImGui::SameLine();
                        SetNextNumericFieldWidth(80.0f);
                        PushNumericFont();
                        if (ImGui::InputFloat("##Off", &currentOffset, 1.0f, 4.0f, "%.1f")) {
                            EnsureRow();
                            row = GetRow();
                            if (row) row->timeOffset = currentOffset;
                        }
                        PopNumericFont();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Time Offset (Beats)");
                    }
                    ImGui::EndGroup();

                    // Column 2: Transition
                    ImGui::TableSetColumnIndex(2);
                    int currentTrans = (row) ? (int)row->transition : 0;
                    float currentDur = (row) ? row->transitionDuration : 1.0f;

                    ImGui::SetNextItemWidth(85);
                    if (ImGui::Combo("##Trans", &currentTrans, transNames, IM_ARRAYSIZE(transNames))) {
                        EnsureRow();
                        row = GetRow();
                        if (row) row->transition = (TransitionType)currentTrans;
                    }
                    if (currentTrans != 0) { // If not Cut
                        ImGui::SameLine();
                        SetNextNumericFieldWidth(85.0f);
                        PushNumericFont();
                        // Using InputFloat with step=0.5 to show arrows
                        if (ImGui::InputFloat("##Dur", &currentDur, 0.5f, 1.0f, "%.1f")) {
                            EnsureRow();
                            row = GetRow();
                            if (row) row->transitionDuration = currentDur;
                        }
                        PopNumericFont();
                    }

                    // Column 3: Music
                    ImGui::TableSetColumnIndex(3);
                    std::string currentMusicName = "";
                    int currentMusicIdx = (row) ? row->musicIndex : -1;
                    if (currentMusicIdx >= 0 && currentMusicIdx < (int)m_audioLibrary.size()) {
                        auto& clip = m_audioLibrary[currentMusicIdx];
                        // Append BPM to display name for info
                        if (clip.bpm > 0.0f) {
                             // Simplified display for compactness
                            currentMusicName = clip.name;
                        } else {
                            currentMusicName = clip.name;
                        }
                    } else if (currentMusicIdx == -1) {
                        currentMusicName = "(Hold)";
                    }

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo("##Music", currentMusicName.c_str())) {
                        if (ImGui::Selectable("(Hold)", currentMusicIdx == -1)) {
                            EnsureRow();
                            row = GetRow();
                            if (row) row->musicIndex = -1;
                        }
                        for (int n = 0; n < (int)m_audioLibrary.size(); n++) {
                            auto& clip = m_audioLibrary[n];
                            char label[128];
                            if (clip.bpm > 0.0f) {
                                snprintf(label, sizeof(label), "%s (%.1f)", clip.name.c_str(), clip.bpm);
                            } else {
                                strncpy_s(label, clip.name.c_str(), _TRUNCATE);
                            }
                            if (strlen(label) == 0) strcpy_s(label, "Untitled");

                            bool is_selected = (currentMusicIdx == n);
                            if (ImGui::Selectable(label, is_selected)) {
                                EnsureRow();
                                row = GetRow();
                                if (row) row->musicIndex = n;
                            }
                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    // Column 4: One Shot
                    ImGui::TableSetColumnIndex(4);

                    std::string currentOSName = "(None)";
                    int currentOSIdx = (row) ? row->oneShotIndex : -1;
                    if (currentOSIdx >= 0 && currentOSIdx < (int)m_audioLibrary.size()) {
                        currentOSName = m_audioLibrary[currentOSIdx].name;
                    }

                    // Compact Symbol Button
                    const char* btnSymbol = (currentOSIdx >= 0) ? "[!]" : ".";
                    // Center the button roughly
                    float avail = ImGui::GetContentRegionAvail().x;
                    float btnSize = 30.0f;
                    if (avail > btnSize) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnSize) * 0.5f);

                    if (ImGui::Button(btnSymbol, ImVec2(btnSize, 0))) {
                        ImGui::OpenPopup("OneShotSelect");
                    }

                    // Tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", currentOSName.c_str());
                    }

                    if (ImGui::BeginPopup("OneShotSelect")) {
                         if (ImGui::Selectable("(None)", currentOSIdx == -1)) {
                             EnsureRow();
                             row = GetRow();
                             if (row) row->oneShotIndex = -1;
                         }
                         for (int n = 0; n < (int)m_audioLibrary.size(); ++n) {
                             std::string label = m_audioLibrary[n].name;
                             if (label.empty()) label = "Untitled";
                             bool is_selected = (currentOSIdx == n);
                             if (ImGui::Selectable(label.c_str(), is_selected)) {
                                 EnsureRow();
                                 row = GetRow();
                                 if (row) row->oneShotIndex = n;
                             }
                             if (is_selected) ImGui::SetItemDefaultFocus();
                         }
                         ImGui::EndPopup();
                    }

                    if (isBarStart) {
                        ImGui::PopStyleColor(2);
                    }

                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void UISystem::UpdateTransport(double wallNowSeconds, float dtSeconds) {
    if (m_transport.state == TransportState::Playing && !m_transport.freezeTime) {
        // Sync with Audio if available
        if (m_audioSystem && m_audioSystem->IsPlaying()) {
            m_transport.timeSeconds = (double)m_audioSystem->GetPlaybackTime();
        } else {
            m_transport.timeSeconds += dtSeconds;
        }

        // Demo Track Logic
        // Check triggers
        // Note: m_track is now the single source of truth
        auto& track = m_track;

        auto HasMusicIndex = [&](int index) -> bool {
            for (const auto& row : track.rows) {
                if (row.musicIndex == index) return true;
            }
            return false;
        };

        if (m_activeMusicIndex >= 0) {
            if (m_activeMusicIndex >= (int)m_audioLibrary.size() || !HasMusicIndex(m_activeMusicIndex)) {
                if (m_audioSystem) m_audioSystem->Stop();
                m_activeMusicIndex = -1;
            }
        }

        float targetBpm = track.bpm;
        if (m_activeMusicIndex >= 0 && m_activeMusicIndex < (int)m_audioLibrary.size()) {
            const auto& clip = m_audioLibrary[m_activeMusicIndex];
            if (clip.bpm > 0.0f && m_audioSystem && m_audioSystem->IsPlaying()) {
                targetBpm = clip.bpm;
            }
        }
        m_transport.bpm = targetBpm;

        float beatsPerSec = m_transport.bpm / 60.0f;

        // Update current beat
        float exactBeat = (float)(m_transport.timeSeconds * beatsPerSec);
        track.currentBeat = (int)std::floor(exactBeat);

        // When editing Scene/PostFX, keep transport time/beat in sync but avoid
        // track-driven scene switches that can overwrite in-progress editor text.
        if (m_currentMode == UIMode::Scene || m_currentMode == UIMode::PostFX) {
            return;
        }

        if (m_transitionActive) {
            double transitionEndBeat = m_transitionStartBeat + m_transitionDurationBeats;
            if (exactBeat >= transitionEndBeat) {
                m_transitionActive = false;
                if (m_pendingActiveScene != -2) {
                    std::ostringstream msg;
                    msg << "[beat " << track.currentBeat << "] Transition complete -> scene " << m_pendingActiveScene;
                    AppendDemoLog(msg.str());
                    SetActiveScene(m_pendingActiveScene);
                    m_activeSceneStartBeat = m_transitionToStartBeat;
                    m_activeSceneOffset = (m_pendingActiveScene >= 0) ? m_transitionToOffset : 0.0f;
                    m_pendingActiveScene = -2;
                    m_transitionJustCompletedBeat = track.currentBeat;
                }
            }
        }

        // Auto-Stop at end of track
        if (track.lengthBeats > 0 && track.currentBeat >= track.lengthBeats) {
             m_transport.state = TransportState::Stopped;
             // Don't reset time/beat here, so we freeze at the end frame (e.g. Black)
             // m_transport.timeSeconds = 0.0;
             // track.currentBeat = 0;
             // track.lastTriggeredBeat = -1;

             // Stop audio
             if (m_audioSystem) m_audioSystem->Stop();
             m_activeMusicIndex = -1;

             return;
        }

        // Check for events if we crossed a beat boundary (or multiple)
        if (track.currentBeat > track.lastTriggeredBeat) {
            // Process all beats from last+1 to current
            for (int b = track.lastTriggeredBeat + 1; b <= track.currentBeat; ++b) {
                // Find if there is a row for this beat (Simple search)
                for (const auto& row : track.rows) {
                    if (row.rowId == b) {
                        // Scene Change
                        if (row.sceneIndex >= 0 || (row.transition != TransitionType::None && row.transitionDuration > 0.0f)) {
                            if (m_transitionJustCompletedBeat == row.rowId &&
                                row.transition == TransitionType::None &&
                                row.sceneIndex >= 0 &&
                                row.sceneIndex == m_activeSceneIndex) {
                                continue;
                            }
                            if (m_transitionActive &&
                                row.transition == TransitionType::None &&
                                row.sceneIndex >= 0 &&
                                row.sceneIndex == m_pendingActiveScene) {
                                continue;
                            }
                            if (row.transition != TransitionType::None && row.transitionDuration > 0.0f) {
                                // Start Transition
                                m_transitionActive = true;
                                m_transitionFromIndex = m_activeSceneIndex;
                                m_transitionFromOffset = m_activeSceneOffset;
                                m_transitionFromStartBeat = m_activeSceneStartBeat;
                                m_transitionToStartBeat = static_cast<double>(b);

                                int targetIndex = row.sceneIndex;
                                float targetOffset = row.timeOffset;

                                // Logic Fix for "(Hold)":
                                // If the command is (Hold) [-1], it usually means "Keep Current Scene".
                                // However, previously this was interpreted as "Transition to Null/Black".
                                // We now default to "Target = Current" for effects (Glitch/Pixelate/Crossfade/FadeIn/DipToBlack),
                                // effectively applying the transition 'inplace'.
                                // ONLY "FadeOut" explicitly targets Black/Null when (Hold) is used.
                                if (targetIndex == -1 && row.transition == TransitionType::Crossfade) {
                                    const TrackerRow* nextRow = FindNextSceneRow(track, b);
                                    if (nextRow) {
                                        targetIndex = nextRow->sceneIndex;
                                        targetOffset = nextRow->timeOffset;
                                    }
                                }
                                if (targetIndex == -1 && row.transition != TransitionType::FadeOut) {
                                    targetIndex = m_activeSceneIndex;
                                    targetOffset = m_activeSceneOffset;
                                    m_transitionToStartBeat = m_activeSceneStartBeat;
                                } else if (targetIndex == m_activeSceneIndex) {
                                    targetOffset = m_activeSceneOffset;
                                    m_transitionToStartBeat = m_activeSceneStartBeat;
                                }

                                m_transitionToIndex = targetIndex;
                                m_transitionToOffset = targetOffset;
                                m_transitionStartBeat = (double)b;
                                m_transitionDurationBeats = (double)row.transitionDuration;
                                m_currentTransitionType = row.transition;

                                // Defer scene switch until transition ends
                                m_pendingActiveScene = targetIndex;

                                std::ostringstream msg;
                                msg << "[beat " << b << "] Transition " << TransitionName(row.transition)
                                    << " from " << m_transitionFromIndex << " to " << targetIndex
                                    << " dur " << row.transitionDuration;
                                AppendDemoLog(msg.str());
                            } else if (row.sceneIndex >= 0) {
                                // Validate scene index before switching
                                if (row.sceneIndex < (int)m_scenes.size()) {
                                    m_transitionActive = false;
                                    SetActiveScene(row.sceneIndex);
                                    m_activeSceneStartBeat = static_cast<double>(b);
                                    m_activeSceneOffset = row.timeOffset;
                                    std::ostringstream msg;
                                    msg << "[beat " << b << "] Scene set to " << row.sceneIndex;
                                    AppendDemoLog(msg.str());
                                }
                            }
                        }

                        // Music Change
                        if (row.musicIndex >= 0 && row.musicIndex < (int)m_audioLibrary.size() && m_audioSystem) {
                             auto& clip = m_audioLibrary[row.musicIndex];
                             m_audioSystem->LoadAudio(clip.path);
                             if (m_transport.state == TransportState::Playing) {
                                 m_audioSystem->Play();
                             }
                             m_activeMusicIndex = row.musicIndex;
                             // Propagate BPM
                             if (clip.bpm > 0.0f) {
                                 m_transport.bpm = clip.bpm;
                                 // Recalculate beatsPerSec immediately?
                                 // This might cause a jump in beat calculation for the *next* frame
                                 // or even this frame if we relied on it.
                                 // For now, let it take effect next frame.
                             }

                                std::ostringstream msg;
                                msg << "[beat " << b << "] Music " << row.musicIndex;
                                AppendDemoLog(msg.str());
                        }

                        // One Shot
                        if (row.oneShotIndex >= 0 && row.oneShotIndex < (int)m_audioLibrary.size() && m_audioSystem) {
                            m_audioSystem->PlayOneShot(m_audioLibrary[row.oneShotIndex].path);
                            std::ostringstream msg;
                            msg << "[beat " << b << "] OneShot " << row.oneShotIndex;
                            AppendDemoLog(msg.str());
                        }

                        // Stop Command
                        if (row.stop) {
                             m_transport.state = TransportState::Stopped;
                             if (m_audioSystem) m_audioSystem->Stop();
                                m_activeMusicIndex = -1;
                                AppendDemoLog("[stop] Track stopped");
                             // Do NOT reset time/beat, so it freezes exactly here.
                             // But we must stop processing further.
                             return;
                        }
                    }
                }
            }
            track.lastTriggeredBeat = track.currentBeat;
        }
        m_transitionJustCompletedBeat = -1;
    } else {
         auto& track = m_track;
         float targetBpm = track.bpm;
         if (m_activeMusicIndex >= 0 && m_activeMusicIndex < (int)m_audioLibrary.size()) {
             const auto& clip = m_audioLibrary[m_activeMusicIndex];
             if (clip.bpm > 0.0f && m_audioSystem && m_audioSystem->IsPlaying()) {
                 targetBpm = clip.bpm;
             }
         }
         m_transport.bpm = targetBpm;

         // Sync Transport BPM to active track during pause too
         float beatsPerSec = m_transport.bpm / 60.0f;
         track.currentBeat = (int)std::floor(m_transport.timeSeconds * beatsPerSec);

         // Reset trigger tracking on rewind
         if (track.currentBeat < track.lastTriggeredBeat) {
              track.lastTriggeredBeat = track.currentBeat - 1;
         }
    }
    m_transport.lastFrameWallSeconds = wallNowSeconds;
}

void UISystem::SeekToBeat(int beat) {
    auto& track = m_track;
    if (track.lengthBeats <= 0) return;

    if (beat < 0) beat = 0;
    if (beat >= track.lengthBeats) beat = track.lengthBeats - 1;

    if (m_transport.bpm <= 0.0f) {
        m_transport.bpm = track.bpm > 0.0f ? track.bpm : 120.0f;
    }

    const double beatSeconds = 60.0 / static_cast<double>(m_transport.bpm);
    m_transport.timeSeconds = static_cast<double>(beat) * beatSeconds;
    track.currentBeat = beat;
    track.lastTriggeredBeat = beat - 1;

    m_transitionActive = false;
    m_pendingActiveScene = -2;
    m_transitionFromIndex = -1;
    m_transitionToIndex = -1;
    m_transitionFromOffset = 0.0f;
    m_transitionToOffset = 0.0f;
    m_transitionStartBeat = 0.0;
    m_transitionDurationBeats = 1.0;
    m_currentTransitionType = TransitionType::None;
    m_activeSceneIndex = -1;
    m_activeSceneOffset = 0.0f;
    m_activeSceneStartBeat = 0.0;
    m_transitionFromStartBeat = 0.0;
    m_transitionToStartBeat = 0.0;
    int ignoreSceneBeat = -1;

    for (int b = 0; b <= beat; ++b) {
        for (const auto& row : track.rows) {
            if (row.rowId != b) continue;

            if (row.transition != TransitionType::None && row.transitionDuration > 0.0f) {
                m_transitionActive = true;
                m_transitionFromIndex = m_activeSceneIndex;
                m_transitionFromOffset = m_activeSceneOffset;
                m_transitionFromStartBeat = m_activeSceneStartBeat;
                m_transitionToStartBeat = static_cast<double>(b);
                m_transitionStartBeat = static_cast<double>(b);
                m_transitionDurationBeats = static_cast<double>(row.transitionDuration);
                m_currentTransitionType = row.transition;

                int targetIndex = row.sceneIndex;
                float targetOffset = row.timeOffset;
                if (targetIndex == -1 && row.transition == TransitionType::Crossfade) {
                    const TrackerRow* nextRow = FindNextSceneRow(track, b);
                    if (nextRow) {
                        targetIndex = nextRow->sceneIndex;
                        targetOffset = nextRow->timeOffset;
                    }
                }
                if (targetIndex == -1 && row.transition != TransitionType::FadeOut) {
                    targetIndex = m_activeSceneIndex;
                    targetOffset = m_activeSceneOffset;
                    m_transitionToStartBeat = m_activeSceneStartBeat;
                } else if (targetIndex == m_activeSceneIndex) {
                    targetOffset = m_activeSceneOffset;
                    m_transitionToStartBeat = m_activeSceneStartBeat;
                }

                m_transitionToIndex = targetIndex;
                m_transitionToOffset = targetOffset;
                m_pendingActiveScene = targetIndex;

                const double transitionEndBeat = m_transitionStartBeat + m_transitionDurationBeats;
                if (beat >= transitionEndBeat && m_pendingActiveScene >= 0) {
                    m_transitionActive = false;
                    SetActiveScene(m_pendingActiveScene);
                    m_activeSceneStartBeat = m_transitionToStartBeat;
                    m_activeSceneOffset = m_transitionToOffset;
                    m_pendingActiveScene = -2;
                    ignoreSceneBeat = static_cast<int>(transitionEndBeat);
                }
            } else if (row.sceneIndex >= 0) {
                if (ignoreSceneBeat == row.rowId && row.sceneIndex == m_activeSceneIndex) {
                    continue;
                }
                if (m_transitionActive && row.sceneIndex == m_pendingActiveScene) {
                    continue;
                }
                m_transitionActive = false;
                m_pendingActiveScene = -2;
                m_activeSceneIndex = row.sceneIndex;
                m_activeSceneStartBeat = static_cast<double>(b);
                m_activeSceneOffset = row.timeOffset;
            }
        }
    }

    SetActiveScene(m_activeSceneIndex);
}

void UISystem::ShowTransportControls() {
    ImGui::Begin("Transport", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    if (m_fontHackedLogo) {
        ImGui::PushFont(m_fontHackedLogo);
        ImVec2 logoSize = ImGui::CalcTextSize("SHADERLAB");
        ImGui::PopFont();

        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImVec2 contentMax = ImGui::GetWindowContentRegionMax();
        ImVec2 logoPos(
            windowPos.x + contentMax.x - logoSize.x,
            windowPos.y + contentMin.y + (std::min)(UIConfig::MenuLogoBaselineOffset, 0.0f)
        );

        ImGui::PushFont(m_fontHackedLogo);
        ImGui::GetWindowDrawList()->AddText(logoPos, IM_COL32(0, 230, 230, 255), "SHADERLAB");
        ImGui::PopFont();
    }

    const bool altDown = ImGui::IsKeyDown(ImGuiKey_LeftAlt) || ImGui::IsKeyDown(ImGuiKey_RightAlt);
    const bool hotkeyToggle = altDown && ImGui::IsKeyPressed(ImGuiKey_Space, false);
    const bool hotkeyStop = altDown && ImGui::IsKeyPressed(ImGuiKey_X, false);

    // Play/Pause/Stop buttons
    ImGui::Button(m_transport.state == TransportState::Playing ? "Pause" : "Play");
    const bool playPausePressed = ImGui::IsItemClicked(ImGuiMouseButton_Left) || hotkeyToggle;
    if (playPausePressed) {
        if (m_transport.state == TransportState::Playing) {
            m_transport.state = TransportState::Paused;
            if (m_audioSystem) m_audioSystem->Pause();
            if (m_currentMode == UIMode::Demo) {
                AppendDemoLog("[pause] Transport paused");
            }
        } else {
            bool compilationFailed = false;

            // In Demo mode, ensure all scenes are compiled
            if (m_currentMode == UIMode::Demo) {
                for (int i = 0; i < (int)m_scenes.size(); ++i) {
                    if (m_scenes[i].isDirty || m_scenes[i].pipelineState == nullptr) {
                        if (!CompileScene(i)) {
                            // Compilation failed - switch to scene mode to show error
                            m_currentMode = UIMode::Scene;
                            m_activeSceneIndex = i;

                            // Load shader source into editor
                            m_shaderState.text = m_scenes[i].shaderCode;
                            m_textEditor.SetText(m_scenes[i].shaderCode);

                            // Re-compile (with active index set) to populate UI diagnostics
                            CompileScene(i);

                            compilationFailed = true;
                            break;
                        }
                    }

                    for (auto& fx : m_scenes[i].postFxChain) {
                        if (!fx.pipelineState || fx.isDirty) {
                            std::vector<std::string> errors;
                            if (!CompilePostFxEffect(fx, errors)) {
                                m_currentMode = UIMode::PostFX;
                                m_postFxSourceSceneIndex = i;
                                m_postFxDraftChain = m_scenes[i].postFxChain;
                                m_postFxSelectedIndex = m_postFxDraftChain.empty() ? -1 : 0;
                                SyncPostFxEditorToSelection();
                                m_shaderState.status = CompileStatus::Error;
                                m_shaderState.diagnostics.clear();
                                for (const auto& error : errors) {
                                    Diagnostic diag;
                                    diag.message = error;
                                    m_shaderState.diagnostics.push_back(diag);
                                }
                                compilationFailed = true;
                                break;
                            }
                        }
                    }
                    if (compilationFailed) break;
                }
            }

            if (compilationFailed && m_currentMode == UIMode::Demo) {
                m_hasDemoCompiledSize = false;
                m_lastDemoCompiledSizeBytes = 0;
            }

            if (!compilationFailed) {
                if (m_currentMode == UIMode::Demo) {
                    std::vector<bool> referencedScenes(m_scenes.size(), false);
                    for (const auto& row : m_track.rows) {
                        if (row.sceneIndex >= 0 && row.sceneIndex < (int)m_scenes.size()) {
                            referencedScenes[row.sceneIndex] = true;
                        }
                    }

                    size_t totalBytes = 0;
                    for (int i = 0; i < (int)m_scenes.size(); ++i) {
                        if (!referencedScenes[i]) continue;
                        const auto& scene = m_scenes[i];
                        if (scene.pipelineState && scene.compiledShaderBytes > 0) {
                            totalBytes += scene.compiledShaderBytes;
                        }
                        for (const auto& fx : scene.postFxChain) {
                            if (fx.enabled && fx.pipelineState && fx.compiledShaderBytes > 0) {
                                totalBytes += fx.compiledShaderBytes;
                            }
                        }
                    }

                    m_lastDemoCompiledSizeBytes = totalBytes;
                    m_hasDemoCompiledSize = (totalBytes > 0);
                }

                if (m_transport.state == TransportState::Stopped) {
                    // Reset Transport
                    m_transport.timeSeconds = 0.0;
                    m_transport.lastFrameWallSeconds = 0.0;
                    m_track.currentBeat = 0;
                    m_track.lastTriggeredBeat = -1;
                    m_transitionActive = false;
                    m_pendingActiveScene = -2;
                    m_transitionFromIndex = -1;
                    m_transitionToIndex = -1;
                    m_transitionFromOffset = 0.0f;
                    m_transitionToOffset = 0.0f;
                    m_transitionStartBeat = 0.0;
                    m_transitionDurationBeats = 1.0;
                    m_currentTransitionType = TransitionType::None;

                    m_activeSceneIndex = -1;
                    m_activeSceneOffset = 0.0f;
                    m_activeSceneStartBeat = 0.0;
                    m_transitionFromStartBeat = 0.0;
                    m_transitionToStartBeat = 0.0;
                    m_transitionJustCompletedBeat = -1;

                    if (m_audioSystem) m_audioSystem->Stop();
                    m_activeMusicIndex = -1;

                    SeekToBeat(0);
                }
                m_transport.state = TransportState::Playing;
                m_transport.lastFrameWallSeconds = 0.0;
                if (m_currentMode == UIMode::Demo) {
                    AppendDemoLog("[play] Transport playing");
                }
                // Start Audio
                if (m_activeMusicIndex >= 0 && m_activeMusicIndex < (int)m_audioLibrary.size()) {
                    auto& clip = m_audioLibrary[m_activeMusicIndex];
                    m_audioSystem->LoadAudio(clip.path);
                    m_audioSystem->Play();
                }
            }
        }
    }
    ImGui::SameLine();

    bool stopPressed = ImGui::Button("Stop");
    if (stopPressed || hotkeyStop) {
        m_transport.state = TransportState::Stopped;
        m_transport.timeSeconds = 0.0;
        m_transport.lastFrameWallSeconds = 0.0;
        m_track.currentBeat = 0;
        m_track.lastTriggeredBeat = -1;
        m_transitionActive = false;
        m_pendingActiveScene = -2;
        m_transitionFromIndex = -1;
        m_transitionToIndex = -1;
        m_transitionFromOffset = 0.0f;
        m_transitionToOffset = 0.0f;
        m_transitionStartBeat = 0.0;
        m_transitionDurationBeats = 1.0;
        m_currentTransitionType = TransitionType::None;

        m_activeSceneIndex = -1;
        m_activeSceneOffset = 0.0f;
        m_activeSceneStartBeat = 0.0;
        m_transitionFromStartBeat = 0.0;
        m_transitionToStartBeat = 0.0;
        m_transitionJustCompletedBeat = -1;

        if (m_audioSystem) m_audioSystem->Stop();
        m_activeMusicIndex = -1;

        if (m_currentMode == UIMode::Demo) {
            AppendDemoLog("[stop] Transport stopped");
        }
    }
    ImGui::SameLine();

    PushNumericFont();
    ImGui::TextUnformatted("Time:");
    ImGui::SameLine();
    PushNumericFont();
    ImGui::Text("%.2f", m_transport.timeSeconds);
    PopNumericFont();
    PopNumericFont();

    // Show beat counter based on active BPM
    float beatsPerSec = m_transport.bpm / 60.0f;
    float exactBeat = (float)(m_transport.timeSeconds * beatsPerSec);
    int bar = (int)exactBeat / 4 + 1;
    int beat = (int)exactBeat % 4 + 1;
    ImGui::SameLine();
    PushNumericFont();
    ImGui::TextUnformatted("Beat:");
    ImGui::SameLine();
    PushNumericFont();
    ImGui::Text("%02d:%02d", bar, beat);
    PopNumericFont();
    PopNumericFont();

    if (m_track.lengthBeats > 0) {
        ImGui::SameLine();
        PushNumericFont();
        ImGui::TextUnformatted("/");
        ImGui::SameLine(0.0f, 0.0f);
        PushNumericFont();
        ImGui::Text("%d", m_track.lengthBeats);
        PopNumericFont();
        PopNumericFont();
    }

    ImGui::SameLine();
    SetNextNumericFieldWidth(80.0f);
    float bpm = m_transport.bpm;
    PushNumericFont();
    if (ImGui::InputFloat("BPM", &bpm, 0.0f, 0.0f, "%.1f")) {
        if (bpm < 1.0f) bpm = 1.0f;
        m_transport.bpm = bpm;
    }
    PopNumericFont();

    if (m_currentMode == UIMode::Demo && m_hasDemoCompiledSize) {
        ImGui::SameLine();
        ImGui::TextDisabled("Compiled: %s", FormatByteSize(m_lastDemoCompiledSizeBytes).c_str());
    }

    ImGui::End();
}

} // namespace ShaderLab
