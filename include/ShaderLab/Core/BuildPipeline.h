#pragma once

#include <functional>
#include <string>

namespace ShaderLab {

struct BuildPrereqReport {
    bool ok = false;
    std::string message;
};

struct BuildRequest {
    std::string appRoot;
    std::string projectPath;
    std::string targetExePath;
};

struct BuildResult {
    bool success = false;
};

class BuildPipeline {
public:
    static BuildPrereqReport CheckPrereqs(const std::string& appRoot);
    static BuildResult BuildSelfContained(
        const BuildRequest& request,
        const std::function<void(const std::string&)>& log);
};

} // namespace ShaderLab
