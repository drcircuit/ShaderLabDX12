#include "ShaderLab/UI/ShaderLabIDE.h"

namespace ShaderLab {

void ShaderLabIDE::ShowPostFxModeWindows() {
    ShowPostEffectsWindows();
    ShowShaderEditor();
    ShowDiagnostics();
    ShowPreviewWindow();
}

} // namespace ShaderLab
