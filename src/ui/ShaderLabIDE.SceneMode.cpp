#include "ShaderLab/UI/ShaderLabIDE.h"

namespace ShaderLab {

void ShaderLabIDE::ShowSceneModeWindows() {
    ShowSceneList();
    ShowSnippetBin();
    ShowScenePostStack();
    ShowSceneTexturesAndChannels();

    ShowShaderEditor();
    ShowDiagnostics();
    ShowPreviewWindow();
}

} // namespace ShaderLab
