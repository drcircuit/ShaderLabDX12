#include "ShaderLab/Core/BuildPipeline.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "ShaderLab/Core/Serializer.h"
#include "ShaderLab/Core/ShaderLabData.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Shader/ShaderCompiler.h"

namespace ShaderLab {

namespace fs = std::filesystem;

namespace {
std::string TrimString(const std::string& value) {
    const char* whitespace = " \t\r\n";
    size_t start = value.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = value.find_last_not_of(whitespace);
    return value.substr(start, end - start + 1);
}

bool FileExists(const fs::path& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

bool FindOnPath(const std::string& exeName) {
    DWORD size = GetEnvironmentVariableA("PATH", nullptr, 0);
    if (size == 0) return false;
    std::string buffer(size, '\0');
    GetEnvironmentVariableA("PATH", buffer.data(), size);

    size_t start = 0;
    while (start < buffer.size()) {
        size_t end = buffer.find(';', start);
        if (end == std::string::npos) end = buffer.size();
        std::string dir = buffer.substr(start, end - start);
        dir = TrimString(dir);
        if (!dir.empty()) {
            fs::path candidate = fs::path(dir) / exeName;
            if (FileExists(candidate)) return true;
        }
        start = end + 1;
    }
    return false;
}

bool FindOnPathFull(const std::string& exeName, std::string& outPath) {
    DWORD size = GetEnvironmentVariableA("PATH", nullptr, 0);
    if (size == 0) return false;
    std::string buffer(size, '\0');
    GetEnvironmentVariableA("PATH", buffer.data(), size);

    size_t start = 0;
    while (start < buffer.size()) {
        size_t end = buffer.find(';', start);
        if (end == std::string::npos) end = buffer.size();
        std::string dir = buffer.substr(start, end - start);
        dir = TrimString(dir);
        if (!dir.empty()) {
            fs::path candidate = fs::path(dir) / exeName;
            if (FileExists(candidate)) {
                outPath = candidate.string();
                return true;
            }
        }
        start = end + 1;
    }
    return false;
}

bool FindVcVars(std::string& outPath) {
    const char* knownPaths[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools"
    };

    for (const char* base : knownPaths) {
        fs::path vcvars = fs::path(base) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
        if (FileExists(vcvars)) {
            outPath = vcvars.string();
            return true;
        }
    }

    fs::path vswhere = fs::path("C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe");
    if (!FileExists(vswhere)) return false;

    std::string cmd = "\"" + vswhere.string() + "\" -latest -property installationPath";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buffer[512];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    _pclose(pipe);

    output = TrimString(output);
    if (output.empty()) return false;

    fs::path vcvars = fs::path(output) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat";
    if (FileExists(vcvars)) {
        outPath = vcvars.string();
        return true;
    }

    return false;
}

bool HasMsvcToolsetPrefix(const fs::path& vcvarsPath, const std::string& prefix) {
    std::error_code ec;
    fs::path vcDir = vcvarsPath.parent_path().parent_path().parent_path();
    fs::path toolsDir = vcDir / "Tools" / "MSVC";
    if (!fs::exists(toolsDir, ec) || ec) return false;

    for (const auto& entry : fs::directory_iterator(toolsDir, ec)) {
        if (ec || !entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

bool HasWindowsSdk(std::string& outPath) {
    fs::path base = "C:\\Program Files (x86)\\Windows Kits\\10\\Include";
    if (!FileExists(base)) return false;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(base, ec)) {
        if (ec || !entry.is_directory()) continue;
        fs::path header = entry.path() / "um" / "d3d12.h";
        if (FileExists(header)) {
            outPath = entry.path().string();
            return true;
        }
    }
    return false;
}

bool ResolveCrinklerPath(std::string& outPath) {
    auto envToPath = [&](const char* varName) -> bool {
        char buffer[MAX_PATH] = {};
        DWORD len = GetEnvironmentVariableA(varName, buffer, MAX_PATH);
        if (len == 0 || len >= MAX_PATH) return false;
        std::string value = TrimString(buffer);
        if (value.empty()) return false;

        fs::path candidate(value);
        std::error_code ec;
        if (fs::is_directory(candidate, ec) && !ec) {
            fs::path exePath = candidate / "crinkler.exe";
            if (FileExists(exePath)) {
                outPath = exePath.string();
                return true;
            }
        } else if (FileExists(candidate)) {
            outPath = candidate.string();
            return true;
        }
        return false;
    };

    if (envToPath("SHADERLAB_CRINKLER")) return true;
    if (envToPath("CRINKLER_PATH")) return true;

    if (FindOnPathFull("crinkler.exe", outPath)) return true;

    const char* knownPaths[] = {
        "C:\\Program Files\\Crinkler\\crinkler.exe",
        "C:\\Program Files (x86)\\Crinkler\\crinkler.exe"
    };
    for (const char* path : knownPaths) {
        if (FileExists(path)) {
            outPath = path;
            return true;
        }
    }
    return false;
}

bool HasDxcRuntime(const fs::path& appRoot) {
    const char* dllName = "dxcompiler.dll";
    if (FindOnPath(dllName)) {
        return true;
    }

    const fs::path roots[] = {
        appRoot,
        appRoot / "build" / "bin",
        appRoot / "bin",
        appRoot / "dev_kit" / "bin"
    };

    for (const auto& root : roots) {
        if (FileExists(root / dllName)) {
            return true;
        }
    }

    return false;
}

std::string BuildPixelShaderSource(const std::string& shaderSource,
                                   const std::vector<PreviewRenderer::TextureDecl>& textureDecls,
                                   bool flipFragCoord = false) {
    std::string wrapped = R"(
cbuffer Constants : register(b0) {
    float iTime;
    float2 iResolution;
};
)";

    for (int i = 0; i < 8; ++i) {
        std::string type = "Texture2D";
        for (const auto& decl : textureDecls) {
            if (decl.slot == i) {
                type = decl.type;
                break;
            }
        }
        wrapped += type + " iChannel" + std::to_string(i) + " : register(t" + std::to_string(i) + ");\n";
    }

    wrapped += "\n";
    for (int i = 0; i < 8; ++i) {
        wrapped += "SamplerState iSampler" + std::to_string(i) + " : register(s" + std::to_string(i) + ");\n";
    }

    wrapped += R"(

struct PSInput {
    float4 pos : SV_POSITION;
    float2 fragCoord : TEXCOORD0;
};

)";
    wrapped += shaderSource;
    wrapped += R"(

float4 PSMain(PSInput input) : SV_TARGET {
)";
    if (flipFragCoord) {
        wrapped += "    float2 fragCoord = float2(input.fragCoord.x, iResolution.y - input.fragCoord.y);\n";
    } else {
        wrapped += "    float2 fragCoord = input.fragCoord;\n";
    }
    wrapped += R"(
    return main(fragCoord, iResolution, iTime);
}
)";
    return wrapped;
}

bool WriteBinaryFile(const fs::path& path, const std::vector<uint8_t>& data, std::string& outError) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        outError = "Failed to write: " + path.string();
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file.good()) {
        outError = "Failed to write: " + path.string();
        return false;
    }
    return true;
}

std::string GetTransitionShaderSourceForBuild(TransitionType type) {
    std::string common = R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
    float2 uv = fragCoord / iResolution;
    float t = saturate(iTime);
    float4 colA = iChannel0.Sample(iSampler0, uv);
    float4 colB = iChannel1.Sample(iSampler1, uv);
)";
    switch (type) {
        case TransitionType::Crossfade: return common + R"(
    return lerp(colA, colB, t);
}
)";
        case TransitionType::DipToBlack: return common + R"(
    return (t < 0.5) ? lerp(colA, float4(0,0,0,1), t*2.0) : lerp(float4(0,0,0,1), colB, (t-0.5)*2.0);
}
)";
        case TransitionType::FadeOut: return common + R"(
    return lerp(colA, float4(0,0,0,1), t);
}
)";
        case TransitionType::FadeIn: return common + R"(
    return lerp(float4(0,0,0,1), colB, t);
}
)";
        case TransitionType::Glitch: return common + R"(
    float offset = iTime * 10.0;
    float noise = frac(sin(dot(float2(floor(uv.y * 20.0) + offset, offset), float2(12.9898, 78.233))) * 43758.5453);
    float disp = (noise - 0.5) * 0.1 * sin(t * 3.14159);
    float2 uv2 = uv + float2(disp, 0);
    colA = iChannel0.Sample(iSampler0, uv2);
    colB = iChannel1.Sample(iSampler1, uv2);
    return lerp(colA, colB, t);
}
)";
        case TransitionType::Pixelate: return common + R"(
    float p = sin(t * 3.14159);
    float n = 50.0 * (1.0 - p) + 1.0;
    float2 uvP = floor(uv * n) / n;
    colA = iChannel0.Sample(iSampler0, uvP);
    colB = iChannel1.Sample(iSampler1, uvP);
    return lerp(colA, colB, t);
}
)";
        default:
            return common + R"(
    return lerp(colA, colB, t);
}
)";
    }
}

const char* GetTransitionPackedPath(TransitionType type) {
    switch (type) {
        case TransitionType::Crossfade: return "assets/shaders/transition_crossfade.cso";
        case TransitionType::DipToBlack: return "assets/shaders/transition_dip_to_black.cso";
        case TransitionType::FadeOut: return "assets/shaders/transition_fade_out.cso";
        case TransitionType::FadeIn: return "assets/shaders/transition_fade_in.cso";
        case TransitionType::Glitch: return "assets/shaders/transition_glitch.cso";
        case TransitionType::Pixelate: return "assets/shaders/transition_pixelate.cso";
        default: return "";
    }
}

const char* GetPackedVertexShaderPath() {
    return "assets/shaders/vertex.cso";
}

bool RunCommand(const std::string& command, const std::function<void(const std::string&)>& log) {
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = NULL;
    HANDLE writePipe = NULL;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        log("Error: Failed to create pipe for command.");
        return false;
    }

    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;

    PROCESS_INFORMATION pi = {};
    std::string cmdLine = "cmd.exe /C " + command;

    BOOL created = CreateProcessA(
        NULL,
        cmdLine.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(writePipe);

    if (!created) {
        CloseHandle(readPipe);
        log("Error: Failed to launch build command.");
        return false;
    }

    char buffer[256];
    DWORD bytesRead = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        log(std::string(buffer));
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);

    return exitCode == 0;
}
} // namespace

BuildPrereqReport BuildPipeline::CheckPrereqs(const std::string& appRoot) {
    std::vector<std::string> missing;
    std::vector<std::string> guidance;
    std::vector<std::string> optionalMissing;

    std::string vcvarsPath;
    if (!FindVcVars(vcvarsPath)) {
        missing.push_back("Visual Studio C++ Build Tools (vcvars64.bat)");
        guidance.push_back("Install Visual Studio 2022 with 'Desktop development with C++'.");
    }

    std::string sdkPath;
    if (!HasWindowsSdk(sdkPath)) {
        missing.push_back("Windows SDK 10 (d3d12.h)");
        guidance.push_back("Install Windows 10/11 SDK via Visual Studio Installer.");
    }

    if (!FindOnPath("cmake.exe") && !FileExists("C:\\Program Files\\CMake\\bin\\cmake.exe")) {
        missing.push_back("CMake");
        guidance.push_back("Install CMake from https://cmake.org/download/ and add it to PATH.");
    }

    if (!HasDxcRuntime(fs::path(appRoot))) {
        missing.push_back("DXC (dxcompiler.dll)");
        guidance.push_back("Install DirectX Shader Compiler or place dxcompiler.dll next to the editor executable.");
    }

    std::string crinklerPath;
    if (!ResolveCrinklerPath(crinklerPath)) {
        optionalMissing.push_back("Crinkler (optional, for size-optimized builds)");
        guidance.push_back("Set SHADERLAB_CRINKLER or CRINKLER_PATH to the Crinkler folder or crinkler.exe.");
    }

    std::ostringstream message;
    if (!missing.empty()) {
        message << "Missing build prerequisites:\n";
        for (const auto& item : missing) {
            message << " - " << item << "\n";
        }

        if (!guidance.empty()) {
            message << "\nSetup guidance:\n";
            for (const auto& item : guidance) {
                message << " - " << item << "\n";
            }
            message << "\nAfter installing, restart the editor and try again.";
        }

        return { false, message.str() };
    }

    if (!optionalMissing.empty()) {
        message << "Optional tools not found:\n";
        for (const auto& item : optionalMissing) {
            message << " - " << item << "\n";
        }
        if (!guidance.empty()) {
            message << "\nSetup guidance:\n";
            for (const auto& item : guidance) {
                message << " - " << item << "\n";
            }
        }
        message << "\nYou can still build, but output may be larger.";
        return { true, message.str() };
    }

    return { true, std::string() };
}

BuildResult BuildPipeline::BuildSelfContained(
    const BuildRequest& request,
    const std::function<void(const std::string&)>& log) {
    BuildResult result{};

    log("Application Root: " + request.appRoot);
    log("Project Path: " + request.projectPath);
    log("Target Output: " + request.targetExePath);

    fs::path sourceDir = fs::path(request.appRoot) / "dev_kit";
    bool isDevKit = fs::exists(sourceDir) && fs::exists(sourceDir / "CMakeLists.txt");

    if (!isDevKit) {
        fs::path appRootCmake = fs::path(request.appRoot) / "CMakeLists.txt";
        log("Dev Kit not found at " + (fs::path(request.appRoot) / "dev_kit").string());
        if (fs::exists(appRootCmake)) {
            sourceDir = request.appRoot;
            log("Falling back to App Root (Development Mode)");
        } else {
            log("Error: Dev Kit missing. Distributed builds require dev_kit with CMakeLists.txt.");
            return result;
        }
    } else {
        log("Using Dev Kit source: " + sourceDir.string());
    }

    const bool hasNinja = FindOnPath("ninja.exe");
    std::string crinklerPath;
    const bool hasCrinkler = ResolveCrinklerPath(crinklerPath);
    const bool useCrinkler = hasNinja && hasCrinkler;

    if (useCrinkler) {
        log("Crinkler: enabled (" + crinklerPath + ")");
    } else if (hasCrinkler && !hasNinja) {
        log("Crinkler detected but Ninja not found; skipping Crinkler.");
    }
    fs::path buildDir = hasNinja
        ? (fs::path(request.appRoot) / "build_selfcontained_ninja")
        : (fs::path(request.appRoot) / "build_selfcontained_vs2022");
    std::string cmd = "cmake -S \"" + sourceDir.string() + "\" -B \"" + buildDir.string() + "\"";
    if (hasNinja) {
        cmd += " -G Ninja -DCMAKE_BUILD_TYPE=Release";
    } else {
        cmd += " -G \"Visual Studio 17 2022\" -A x64";
    }
    cmd += " -DSHADERLAB_BUILD_RUNTIME=ON -DSHADERLAB_BUILD_EDITOR=OFF";
    cmd += " -DSHADERLAB_ENABLE_DXC=OFF -DSHADERLAB_STATIC_RUNTIME=ON";

    if (useCrinkler) {
        std::string crinklerArg = crinklerPath;
        std::replace(crinklerArg.begin(), crinklerArg.end(), '\\', '/');
        cmd += " -DSHADERLAB_USE_CRINKLER=ON";
        cmd += " -DCRINKLER_PATH=\"" + crinklerArg + "\"";
        cmd += " -DCMAKE_LINKER=\"" + crinklerArg + "\"";
    }

    std::string vcvars;
    if (!FindVcVars(vcvars)) {
        log("Error: vcvars64.bat not found. Install Visual Studio Build Tools 2022.");
        return result;
    }

    std::string vcvarsArgs;
    if (useCrinkler && HasMsvcToolsetPrefix(vcvars, "14.29")) {
        vcvarsArgs = " -vcvars_ver=14.29";
        log("Crinkler: using MSVC v142 toolset (14.29)");
    } else if (useCrinkler) {
        log("Crinkler: MSVC v142 toolset not found; using default toolset");
    }

    std::string cmdWithEnv = "call \"" + vcvars + "\"" + vcvarsArgs + " >nul && " + cmd;

    log("----------------------------------------");
    log("Step 1: Configuring CMake");
    log("Command: " + cmdWithEnv);

    if (!RunCommand(cmdWithEnv, log)) {
        log("CMake Configuration Failed.");
        return result;
    }

    log("----------------------------------------");
    log("Step 2: Building Player Target");
    std::string buildCmd = "cmake --build \"" + buildDir.string() + "\" --target ShaderLabPlayer --config Release";
    std::string buildCmdWithEnv = "call \"" + vcvars + "\"" + vcvarsArgs + " >nul && " + buildCmd;
    log("Command: " + buildCmdWithEnv);

    if (!RunCommand(buildCmdWithEnv, log)) {
        log("Build Failed. Trying Debug Configuration...");
        buildCmd = "cmake --build \"" + buildDir.string() + "\" --target ShaderLabPlayer --config Debug";
        buildCmdWithEnv = "call \"" + vcvars + "\"" + vcvarsArgs + " >nul && " + buildCmd;
        if (!RunCommand(buildCmdWithEnv, log)) {
            log("Build Failed.");
            return result;
        }
    }

    log("----------------------------------------");
    log("Step 3: Verifying Executable");

    fs::path buildBinPath = buildDir / "bin";
    fs::path playerExe = buildBinPath / "ShaderLabPlayer.exe";

    if (!fs::exists(playerExe)) {
        if (fs::exists(buildBinPath / "Debug" / "ShaderLabPlayer.exe")) playerExe = buildBinPath / "Debug" / "ShaderLabPlayer.exe";
        else if (fs::exists(buildBinPath / "Release" / "ShaderLabPlayer.exe")) playerExe = buildBinPath / "Release" / "ShaderLabPlayer.exe";
    }

    if (!fs::exists(playerExe)) {
        log("Error: Could not find ShaderLabPlayer.exe in build artifacts.");
        log("Expected locations checked:");
        log(" - " + (buildBinPath / "ShaderLabPlayer.exe").string());
        log(" - " + (buildBinPath / "Debug" / "ShaderLabPlayer.exe").string());
        log(" - " + (buildBinPath / "Release" / "ShaderLabPlayer.exe").string());
        return result;
    }

    log("Found Player EXE: " + playerExe.string());

    log("----------------------------------------");
    log("Step 4: Preparing Packed Project");

    ProjectData project;
    if (!Serializer::LoadProject(request.projectPath, project)) {
        log("Error: Failed to load project for packaging.");
        return result;
    }

    fs::path packRoot = fs::path(request.appRoot) / "build_selfcontained_pack";
    std::error_code ec;
    fs::remove_all(packRoot, ec);
    fs::create_directories(packRoot, ec);

    if (!Serializer::ConsolidateProject(project, packRoot.string())) {
        log("Error: Failed to consolidate assets for packaging.");
        return result;
    }

    ShaderCompiler compiler;
    if (!compiler.Initialize()) {
        log("Error: DXC not available. Cannot precompile shaders for self-contained build.");
        return result;
    }

    auto logDiagnostics = [&](const std::vector<ShaderDiagnostic>& diags) {
        for (const auto& diag : diags) {
            log(diag.message);
        }
    };

    log("Precompiling vertex shader");
    const char* vertexShaderSource = R"(
cbuffer Constants : register(b0) {
    float iTime;
    float2 iResolution;
};

struct VSInput {
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 fragCoord : TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.pos = float4(input.pos, 1.0);
    output.fragCoord = float2(input.uv.x, 1.0 - input.uv.y) * iResolution;
    return output;
}
)";

    auto vsResult = compiler.CompileFromSource(vertexShaderSource, "main", "vs_6_0", L"vertex.hlsl", ShaderCompileMode::Build);
    if (!vsResult.success) {
        log("Error: Vertex shader precompile failed.");
        logDiagnostics(vsResult.diagnostics);
        return result;
    }

    std::vector<Serializer::PackedExtraFile> extraFiles;
    fs::path vertexPath = packRoot / GetPackedVertexShaderPath();
    std::string writeError;
    if (!WriteBinaryFile(vertexPath, vsResult.bytecode, writeError)) {
        log("Error: " + writeError);
        return result;
    }
    extraFiles.push_back({vertexPath.string(), GetPackedVertexShaderPath()});

    log("Precompiling transitions");
    const TransitionType transitions[] = {
        TransitionType::Crossfade,
        TransitionType::DipToBlack,
        TransitionType::FadeOut,
        TransitionType::FadeIn,
        TransitionType::Glitch,
        TransitionType::Pixelate
    };

    for (TransitionType type : transitions) {
        const char* packedPath = GetTransitionPackedPath(type);
        if (!packedPath || !*packedPath) continue;
        std::string shaderSource = GetTransitionShaderSourceForBuild(type);
        std::vector<PreviewRenderer::TextureDecl> decls = { {0, "Texture2D"}, {1, "Texture2D"} };
        std::string wrapped = BuildPixelShaderSource(shaderSource, decls);
        auto psResult = compiler.CompileFromSource(wrapped, "PSMain", "ps_6_0", L"transition.hlsl", ShaderCompileMode::Build);
        if (!psResult.success) {
            log("Error: Transition shader precompile failed.");
            logDiagnostics(psResult.diagnostics);
            return result;
        }
        fs::path transitionPath = packRoot / packedPath;
        if (!WriteBinaryFile(transitionPath, psResult.bytecode, writeError)) {
            log("Error: " + writeError);
            return result;
        }
        extraFiles.push_back({transitionPath.string(), packedPath});
    }

    log("Precompiling scene and post FX shaders");
    for (size_t i = 0; i < project.scenes.size(); ++i) {
        auto& scene = project.scenes[i];
        log("Scene " + std::to_string(i) + ": " + scene.name);
        std::vector<PreviewRenderer::TextureDecl> decls;
        for (const auto& b : scene.bindings) {
            if (!b.enabled) continue;
            PreviewRenderer::TextureDecl decl;
            decl.slot = b.channelIndex;
            if (b.type == TextureType::TextureCube) decl.type = "TextureCube";
            else if (b.type == TextureType::Texture3D) decl.type = "Texture3D";
            else decl.type = "Texture2D";
            decls.push_back(decl);
        }

        std::string wrapped = BuildPixelShaderSource(scene.shaderCode, decls);
        log("  Compiling scene shader -> assets/shaders/scene_" + std::to_string(i) + ".cso");
        auto psResult = compiler.CompileFromSource(wrapped, "PSMain", "ps_6_0", L"scene.hlsl", ShaderCompileMode::Build);
        if (!psResult.success) {
            log("Error: Scene shader precompile failed: " + scene.name);
            logDiagnostics(psResult.diagnostics);
            return result;
        }

        std::string scenePackedPath = "assets/shaders/scene_" + std::to_string(i) + ".cso";
        fs::path scenePath = packRoot / scenePackedPath;
        if (!WriteBinaryFile(scenePath, psResult.bytecode, writeError)) {
            log("Error: " + writeError);
            return result;
        }
        scene.precompiledPath = scenePackedPath;
        extraFiles.push_back({scenePath.string(), scenePackedPath});
        log("  Packed scene shader: " + scenePackedPath);

        for (size_t fxIndex = 0; fxIndex < scene.postFxChain.size(); ++fxIndex) {
            auto& fx = scene.postFxChain[fxIndex];
            log("  Compiling post FX [" + std::to_string(fxIndex) + "] " + fx.name + " -> assets/shaders/scene_" + std::to_string(i) + "_fx_" + std::to_string(fxIndex) + ".cso");
            std::string fxWrapped = BuildPixelShaderSource(fx.shaderCode, {}, true);
            auto fxResult = compiler.CompileFromSource(fxWrapped, "PSMain", "ps_6_0", L"postfx.hlsl", ShaderCompileMode::Build);
            if (!fxResult.success) {
                log("Error: Post FX precompile failed: " + fx.name);
                logDiagnostics(fxResult.diagnostics);
                return result;
            }

            std::string fxPackedPath = "assets/shaders/scene_" + std::to_string(i) + "_fx_" + std::to_string(fxIndex) + ".cso";
            fs::path fxPath = packRoot / fxPackedPath;
            if (!WriteBinaryFile(fxPath, fxResult.bytecode, writeError)) {
                log("Error: " + writeError);
                return result;
            }
            fx.precompiledPath = fxPackedPath;
            extraFiles.push_back({fxPath.string(), fxPackedPath});
            log("  Packed post FX shader: " + fxPackedPath);
        }
    }

    fs::path packProjectPath = packRoot / "project.json";
    if (!Serializer::SaveProject(project, packProjectPath.string())) {
        log("Error: Failed to write packed project manifest.");
        return result;
    }

    log("----------------------------------------");
    log("Step 5: Packing Executable");
    if (Serializer::PackExecutable(playerExe.string(), request.targetExePath, packProjectPath.string(), extraFiles)) {
        log("----------------------------------------");
        log("BUILD SUCCESSFUL");
        result.success = true;
    } else {
        log("Error: Failed to pack executable.");
    }

    return result;
}

} // namespace ShaderLab
