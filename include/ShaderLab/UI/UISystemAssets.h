#pragma once

#include <cstddef>

namespace ShaderLab {

struct PostFxPreset {
    const char* name;
    const char* code;
};

extern const int kPostFxHistoryCount;
extern const int kMaxPostFxChain;
extern const int kAboutSrvIndex;

extern const char* kAboutLogoShader;
extern const char* kAboutGlitchPostFx;

extern const PostFxPreset kPostFxPresets[];
extern const size_t kPostFxPresetCount;

} // namespace ShaderLab
