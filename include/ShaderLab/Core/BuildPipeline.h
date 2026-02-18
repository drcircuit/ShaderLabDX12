#pragma once

#include <functional>
#include <cstdint>
#include <string>

namespace ShaderLab {

enum class BuildMode {
    Release,
    ReleaseCrinkled
};

enum class SizeTargetPreset {
    None,
    K1,
    K2,
    K4,
    K16,
    K32,
    K64
};

struct BuildPrereqReport {
    bool ok = false;
    std::string message;
    bool hasVisualStudioTools = false;
    bool hasWindowsSdk = false;
    bool hasCMake = false;
    bool hasDxcRuntime = false;
    bool hasCrinkler = false;
    bool hasNinja = false;
};

struct BuildRequest {
    std::string appRoot;
    std::string projectPath;
    std::string targetExePath;
    BuildMode mode = BuildMode::Release;
    SizeTargetPreset sizeTarget = SizeTargetPreset::None;
    bool restrictedCompactTrack = false;
    bool runtimeDebugLog = false;
    bool compactTrackDebugLog = false;
};

struct BuildResult {
    bool success = false;
    bool budgetHit = true;
    uint64_t finalExeBytes = 0;
    uint64_t budgetBytes = 0;
    std::string report;
};

class BuildPipeline {
public:
    static BuildPrereqReport CheckPrereqs(const std::string& appRoot, BuildMode mode = BuildMode::Release);
    static BuildResult BuildSelfContained(
        const BuildRequest& request,
        const std::function<void(const std::string&)>& log);
};

} // namespace ShaderLab
