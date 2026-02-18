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
const char* BuildModeName(BuildMode mode) {
    return mode == BuildMode::ReleaseCrinkled ? "Release Crinkled" : "Release";
}

const char* SizePresetName(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K1: return "1K";
        case SizeTargetPreset::K2: return "2K";
        case SizeTargetPreset::K4: return "4K";
        case SizeTargetPreset::K16: return "16K";
        case SizeTargetPreset::K32: return "32K";
        case SizeTargetPreset::K64: return "64K";
        default: return "None";
    }
}

uint64_t SizePresetToBytes(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K1: return 1024ull;
        case SizeTargetPreset::K2: return 2048ull;
        case SizeTargetPreset::K4: return 4096ull;
        case SizeTargetPreset::K16: return 16ull * 1024ull;
        case SizeTargetPreset::K32: return 32ull * 1024ull;
        case SizeTargetPreset::K64: return 64ull * 1024ull;
        default: return 0ull;
    }
}

bool IsTinySizeTarget(SizeTargetPreset preset) {
    switch (preset) {
        case SizeTargetPreset::K1:
        case SizeTargetPreset::K2:
        case SizeTargetPreset::K4:
        case SizeTargetPreset::K16:
        case SizeTargetPreset::K32:
        case SizeTargetPreset::K64:
            return true;
        default:
            return false;
    }
}

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

bool FindVcVarsScript(const char* scriptName, std::string& outPath) {
    const char* knownPaths[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools"
    };

    for (const char* base : knownPaths) {
        fs::path vcvars = fs::path(base) / "VC" / "Auxiliary" / "Build" / scriptName;
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

    fs::path vcvars = fs::path(output) / "VC" / "Auxiliary" / "Build" / scriptName;
    if (FileExists(vcvars)) {
        outPath = vcvars.string();
        return true;
    }

    return false;
}

bool FindVcVars(std::string& outPath) {
    return FindVcVarsScript("vcvars64.bat", outPath);
}

bool FindVcVars32(std::string& outPath) {
    return FindVcVarsScript("vcvars32.bat", outPath);
}

bool FindVcVarsAll(std::string& outPath) {
    return FindVcVarsScript("vcvarsall.bat", outPath);
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

bool IsTruthyEnvVar(const char* name) {
    char buffer[32] = {};
    DWORD len = GetEnvironmentVariableA(name, buffer, static_cast<DWORD>(sizeof(buffer)));
    if (len == 0 || len >= sizeof(buffer)) return false;
    std::string value = TrimString(buffer);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value == "1" || value == "true" || value == "yes" || value == "on";
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

constexpr uint32_t kEmbeddedTrackMagic = 0x4B52544Du; // 'MTRK'

struct EmbeddedTrackFooter {
    uint32_t magic = kEmbeddedTrackMagic;
    uint32_t payloadSize = 0;
    uint32_t version = 1;
};

template<typename T>
void AppendValue(std::vector<uint8_t>& output, const T& value) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
    output.insert(output.end(), ptr, ptr + sizeof(T));
}

bool BuildCompactTrackBinary(const DemoTrack& track, std::vector<uint8_t>& outData) {
    outData.clear();
    outData.reserve(10 + track.rows.size() * 9);

    const auto appendU8 = [&outData](uint8_t value) {
        outData.push_back(value);
    };
    const auto appendI8 = [&outData](int8_t value) {
        outData.push_back(static_cast<uint8_t>(value));
    };
    const auto appendU16 = [&appendU8](uint16_t value) {
        appendU8(static_cast<uint8_t>(value & 0xFFu));
        appendU8(static_cast<uint8_t>((value >> 8) & 0xFFu));
    };
    const auto appendI16 = [&appendU16](int16_t value) {
        appendU16(static_cast<uint16_t>(value));
    };

    const uint16_t bpmQ8 = static_cast<uint16_t>(
        (std::max)(0.0f, (std::min)(65535.0f, track.bpm * 256.0f)));
    const uint16_t lengthBeats = static_cast<uint16_t>(
        (std::max)(0, (std::min)(65535, track.lengthBeats)));
    const uint16_t rowCount = static_cast<uint16_t>(
        (std::min)(static_cast<size_t>(65535), track.rows.size()));

    // Header v2 (10 bytes): magic('TKR2'), bpmQ8, lengthBeats, rowCount
    appendU16(0x4B54u); // 'TK'
    appendU16(0x3252u); // 'R2'
    appendU16(bpmQ8);
    appendU16(lengthBeats);
    appendU16(rowCount);

    for (size_t i = 0; i < rowCount; ++i) {
        const auto& src = track.rows[i];
        const int16_t rowId = static_cast<int16_t>((std::max)(-32768, (std::min)(32767, src.rowId)));
        const int16_t sceneIndex = static_cast<int16_t>((std::max)(-1, (std::min)(32767, src.sceneIndex)));
        const uint8_t transition = static_cast<uint8_t>(src.transition);
        uint8_t flags = 0;
        if (src.stop) flags |= 0x1u;

        const uint8_t transitionDurationQ4 = static_cast<uint8_t>(
            (std::max)(0.0f, (std::min)(255.0f, src.transitionDuration * 16.0f)));
        const int8_t timeOffsetQ4 = static_cast<int8_t>(
            (std::max)(-128.0f, (std::min)(127.0f, src.timeOffset * 16.0f)));
        const int8_t musicIndex = static_cast<int8_t>((std::max)(-1, (std::min)(127, src.musicIndex)));

        // Row v2 (9 bytes): rowId, sceneIndex, transition, flags, transitionDurationQ4, timeOffsetQ4, musicIndex
        appendI16(rowId);
        appendI16(sceneIndex);
        appendU8(transition);
        appendU8(flags);
        appendU8(transitionDurationQ4);
        appendI8(timeOffsetQ4);
        appendI8(musicIndex);
    }

    return true;
}

bool WriteCompactTrackBinary(const DemoTrack& track, const fs::path& outputPath, std::string& outError) {
    std::vector<uint8_t> data;
    if (!BuildCompactTrackBinary(track, data)) {
        outError = "Failed to build compact track binary.";
        return false;
    }
    return WriteBinaryFile(outputPath, data, outError);
}

bool EmbedCompactTrackIntoExecutable(const DemoTrack& track, const fs::path& exePath, std::string& outError) {
    std::vector<uint8_t> payload;
    if (!BuildCompactTrackBinary(track, payload)) {
        outError = "Failed to build compact track payload.";
        return false;
    }

    EmbeddedTrackFooter footer;
    footer.payloadSize = static_cast<uint32_t>(payload.size());

    std::ofstream file(exePath, std::ios::binary | std::ios::app);
    if (!file.is_open()) {
        outError = "Failed to open executable for embedded track write: " + exePath.string();
        return false;
    }

    file.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    file.write(reinterpret_cast<const char*>(&footer), static_cast<std::streamsize>(sizeof(footer)));
    if (!file.good()) {
        outError = "Failed to append embedded track payload to executable.";
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

BuildPrereqReport BuildPipeline::CheckPrereqs(const std::string& appRoot, BuildMode mode) {
    BuildPrereqReport report{};
    std::vector<std::string> missing;
    std::vector<std::string> guidance;
    std::vector<std::string> optionalMissing;

    std::string vcvarsPath;
    report.hasVisualStudioTools = FindVcVars(vcvarsPath);
    if (!report.hasVisualStudioTools) {
        missing.push_back("Visual Studio C++ Build Tools (vcvars64.bat)");
        guidance.push_back("Install Visual Studio 2022 with 'Desktop development with C++'.");
    }

    std::string sdkPath;
    report.hasWindowsSdk = HasWindowsSdk(sdkPath);
    if (!report.hasWindowsSdk) {
        missing.push_back("Windows SDK 10 (d3d12.h)");
        guidance.push_back("Install Windows 10/11 SDK via Visual Studio Installer.");
    }

    report.hasCMake = FindOnPath("cmake.exe") || FileExists("C:\\Program Files\\CMake\\bin\\cmake.exe");
    if (!report.hasCMake) {
        missing.push_back("CMake");
        guidance.push_back("Install CMake from https://cmake.org/download/ and add it to PATH.");
    }

    report.hasDxcRuntime = HasDxcRuntime(fs::path(appRoot));
    if (!report.hasDxcRuntime) {
        missing.push_back("DXC (dxcompiler.dll)");
        guidance.push_back("Install DirectX Shader Compiler or place dxcompiler.dll next to the editor executable.");
    }

    std::string crinklerPath;
    report.hasCrinkler = ResolveCrinklerPath(crinklerPath);
    report.hasNinja = FindOnPath("ninja.exe");
    if (mode == BuildMode::ReleaseCrinkled) {
        if (!report.hasCrinkler) {
            missing.push_back("Crinkler (crinkler.exe)");
            guidance.push_back("See README-CRINKLER.txt for setup instructions.");
            guidance.push_back("Set SHADERLAB_CRINKLER or CRINKLER_PATH to crinkler.exe (or its folder).");
        }
        if (!report.hasNinja) {
            missing.push_back("Ninja (ninja.exe)");
            guidance.push_back("Install Ninja and ensure ninja.exe is on PATH.");
        }
    } else if (!report.hasCrinkler) {
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

        report.ok = false;
        report.message = message.str();
        return report;
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
        report.ok = true;
        report.message = message.str();
        return report;
    }

    report.ok = true;
    return report;
}

BuildResult BuildPipeline::BuildSelfContained(
    const BuildRequest& request,
    const std::function<void(const std::string&)>& log) {
    BuildResult result{};

    log(std::string("Build Mode: ") + BuildModeName(request.mode));
    log(std::string("Size Target: ") + SizePresetName(request.sizeTarget));
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
    const bool wantCrinkler = request.mode == BuildMode::ReleaseCrinkled;
    const bool useCrinkler = wantCrinkler && hasNinja && hasCrinkler;
    const bool tinyProfile = IsTinySizeTarget(request.sizeTarget);
    const bool useMicroPlayer = tinyProfile;
    const bool useX86Target = useMicroPlayer;
    const bool staticRuntime = !tinyProfile;

    if (tinyProfile) {
        log("Tiny Player Profile: ON (1K-64K size target)");
        log("Tiny Profile Runtime: dynamic CRT (/MD) for maximum size reduction");
    } else {
        log("Tiny Player Profile: OFF (full runtime profile)");
    }

    log(std::string("Target Architecture: ") + (useX86Target ? "x86" : "x64"));

    if (useMicroPlayer) {
        if (useCrinkler) {
            log("MicroPlayer mode: ON (tiny preset path, ShaderLabMicroPlayer + Crinkler)");
        } else {
            log("MicroPlayer mode: ON (tiny preset path, ShaderLabMicroPlayer)");
            log("Note: Crinkler is disabled; this is a tiny benchmark runtime path.");
        }
    } else {
        log("MicroPlayer mode: OFF (full runtime path)");
    }

    if (wantCrinkler && !useCrinkler) {
        if (!hasCrinkler) {
            log("Error: Release Crinkled requires Crinkler, but crinkler.exe was not found.");
        }
        if (!hasNinja) {
            log("Error: Release Crinkled currently requires Ninja (ninja.exe on PATH).");
        }
        log("See README-CRINKLER.txt for setup instructions.");
        return result;
    }

    if (useCrinkler) {
        log("Crinkler: enabled (" + crinklerPath + ")");
    } else if (hasCrinkler && !hasNinja) {
        log("Crinkler detected but Ninja not found; skipping Crinkler.");
    } else {
        log("Crinkler: disabled (standard release linker)");
    }
    fs::path buildDir;
    if (hasNinja) {
        if (useMicroPlayer && useCrinkler) {
            buildDir = fs::path(request.appRoot) / "build_selfcontained_ninja_crinkled_micro_x86";
        } else if (useMicroPlayer) {
            buildDir = fs::path(request.appRoot) / "build_selfcontained_ninja_release_micro";
        } else {
            buildDir = fs::path(request.appRoot) /
                (useCrinkler ? "build_selfcontained_ninja_crinkled" : "build_selfcontained_ninja_release");
        }
    } else {
        if (useMicroPlayer) {
            buildDir = fs::path(request.appRoot) /
                (useCrinkler ? "build_selfcontained_vs2022_crinkled_micro" : "build_selfcontained_vs2022_release_micro");
        } else {
            buildDir = fs::path(request.appRoot) /
                (useCrinkler ? "build_selfcontained_vs2022_crinkled" : "build_selfcontained_vs2022_release");
        }
    }
    std::string cmd = "cmake -S \"" + sourceDir.string() + "\" -B \"" + buildDir.string() + "\"";
    if (hasNinja) {
        cmd += " -G Ninja -DCMAKE_BUILD_TYPE=Release";
    } else {
        cmd += useX86Target ? " -G \"Visual Studio 17 2022\" -A Win32" : " -G \"Visual Studio 17 2022\" -A x64";
    }
    if (useMicroPlayer) {
        cmd += " -DSHADERLAB_BUILD_RUNTIME=OFF -DSHADERLAB_BUILD_EDITOR=OFF -DSHADERLAB_BUILD_MICRO_PLAYER=ON";
    } else {
        cmd += " -DSHADERLAB_BUILD_RUNTIME=ON -DSHADERLAB_BUILD_EDITOR=OFF -DSHADERLAB_BUILD_MICRO_PLAYER=OFF";
    }
    cmd += " -DSHADERLAB_ENABLE_DXC=OFF";
    cmd += staticRuntime ? " -DSHADERLAB_STATIC_RUNTIME=ON" : " -DSHADERLAB_STATIC_RUNTIME=OFF";
    cmd += " -DSHADERLAB_RUNTIME_IMGUI=OFF";
    cmd += request.runtimeDebugLog ? " -DSHADERLAB_RUNTIME_DEBUG_LOG=ON" : " -DSHADERLAB_RUNTIME_DEBUG_LOG=OFF";
    cmd += request.compactTrackDebugLog ? " -DSHADERLAB_COMPACT_TRACK_DEBUG=ON" : " -DSHADERLAB_COMPACT_TRACK_DEBUG=OFF";
    cmd += tinyProfile ? " -DSHADERLAB_TINY_PLAYER=ON" : " -DSHADERLAB_TINY_PLAYER=OFF";

    if (useCrinkler) {
        std::string crinklerArg = crinklerPath;
        std::replace(crinklerArg.begin(), crinklerArg.end(), '\\', '/');
        cmd += " -DSHADERLAB_USE_CRINKLER=ON";
        cmd += " -DSHADERLAB_CRINKLER_TINYIMPORT=ON";
        cmd += " -DCRINKLER_PATH=\"" + crinklerArg + "\"";
        cmd += " -DCMAKE_LINKER=\"" + crinklerArg + "\"";
        cmd += " -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY";
        cmd += " -DCMAKE_EXE_LINKER_FLAGS=\"/MANIFEST:NO\"";
    } else {
        cmd += " -DSHADERLAB_USE_CRINKLER=OFF";
        cmd += " -DSHADERLAB_CRINKLER_TINYIMPORT=OFF";
        cmd += " -DCRINKLER_PATH=\"\"";
        cmd += " -DCMAKE_LINKER=link.exe";
    }

    std::string vcvars;
    std::string vcvarsFallback;
    bool canUseVcvarsFallback = false;
    bool usingVcvarsFallback = false;
    std::string vcvarsBaseArgs;
    std::string vcvarsLabel;
    const bool preferX86Vcvars = useX86Target;
    bool foundVcvars = false;
    if (preferX86Vcvars) {
        foundVcvars = FindVcVars32(vcvars);
        canUseVcvarsFallback = FindVcVarsAll(vcvarsFallback);
        if (foundVcvars) {
            log("Using x86 MSVC environment (vcvars32) for MicroPlayer Crinkler build.");
            vcvarsLabel = "vcvars32";
        } else if (canUseVcvarsFallback) {
            vcvars = vcvarsFallback;
            usingVcvarsFallback = true;
            vcvarsBaseArgs = " x86";
            foundVcvars = true;
            log("vcvars32.bat not found, falling back to vcvarsall.bat x86.");
            vcvarsLabel = "vcvarsall x86";
        }
    } else {
        foundVcvars = FindVcVars(vcvars);
        if (foundVcvars) {
            vcvarsLabel = "vcvars64";
        }
    }

    if (!foundVcvars) {
        if (preferX86Vcvars) {
            log("Error: neither vcvars32.bat nor vcvarsall.bat were found. Install Visual Studio Build Tools 2022.");
        } else {
            log("Error: vcvars64.bat not found. Install Visual Studio Build Tools 2022.");
        }
        return result;
    }

    std::string vcvarsArgs;
    if (useCrinkler && HasMsvcToolsetPrefix(vcvars, "14.29")) {
        vcvarsArgs = " -vcvars_ver=14.29";
        log("Crinkler: using MSVC v142 toolset (14.29)");
    } else if (useCrinkler) {
        log("Crinkler: MSVC v142 toolset not found; using default toolset");
    }

    if (!vcvarsArgs.empty()) {
        std::string scriptName = fs::path(vcvars).filename().string();
        std::transform(scriptName.begin(), scriptName.end(), scriptName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (scriptName != "vcvarsall.bat") {
            std::string vcvarsAll;
            if (FindVcVarsAll(vcvarsAll)) {
                vcvars = vcvarsAll;
                vcvarsBaseArgs = preferX86Vcvars ? " x86" : " x64";
                vcvarsLabel = preferX86Vcvars ? "vcvarsall x86" : "vcvarsall x64";
                log("Crinkler: switching to " + vcvarsLabel + " for toolset pinning.");
            } else {
                log("Warning: vcvarsall.bat not found; pinned toolset may fail with current vcvars script.");
            }
        }
    }

    if (!vcvarsLabel.empty()) {
        log("MSVC environment: " + vcvarsLabel);
    }

    auto WrapWithVcVars = [&](const std::string& innerCmd, const std::string& args) {
        return "call \"" + vcvars + "\"" + args + " >nul && " + innerCmd;
    };

    std::string activeVcvarsArgs = vcvarsBaseArgs + vcvarsArgs;
    const bool hasPinnedToolset = !vcvarsArgs.empty();
    std::string cmdWithEnv = WrapWithVcVars(cmd, activeVcvarsArgs);

    log("----------------------------------------");
    log("Step 1: Configuring CMake");
    log("Command: " + cmdWithEnv);

    if (!RunCommand(cmdWithEnv, log)) {
        if (hasPinnedToolset) {
            log("CMake configure failed with pinned MSVC toolset; retrying with default toolset.");
            activeVcvarsArgs = vcvarsBaseArgs;
            cmdWithEnv = WrapWithVcVars(cmd, activeVcvarsArgs);
            log("Retry Command: " + cmdWithEnv);
            if (!RunCommand(cmdWithEnv, log)) {
                if (preferX86Vcvars && !usingVcvarsFallback && canUseVcvarsFallback) {
                    log("vcvars32 configure failed; retrying with vcvarsall.bat x86.");
                    vcvars = vcvarsFallback;
                    usingVcvarsFallback = true;
                    vcvarsBaseArgs = " x86";
                    vcvarsLabel = "vcvarsall x86";

                    std::string fallbackArgs;
                    if (useCrinkler && HasMsvcToolsetPrefix(vcvars, "14.29")) {
                        fallbackArgs = " -vcvars_ver=14.29";
                    }
                    activeVcvarsArgs = vcvarsBaseArgs + fallbackArgs;
                    cmdWithEnv = WrapWithVcVars(cmd, activeVcvarsArgs);
                    log("Fallback Command: " + cmdWithEnv);
                    if (!RunCommand(cmdWithEnv, log)) {
                        log("CMake Configuration Failed.");
                        return result;
                    }
                    log("CMake configure succeeded with vcvarsall.bat x86 fallback.");
                    log("MSVC environment: " + vcvarsLabel);
                } else {
                    log("CMake Configuration Failed.");
                    return result;
                }
            }
            log("CMake configure succeeded with default MSVC toolset.");
        } else {
            if (preferX86Vcvars && !usingVcvarsFallback && canUseVcvarsFallback) {
                log("vcvars32 configure failed; retrying with vcvarsall.bat x86.");
                vcvars = vcvarsFallback;
                usingVcvarsFallback = true;
                vcvarsBaseArgs = " x86";
                vcvarsLabel = "vcvarsall x86";
                std::string fallbackArgs;
                if (useCrinkler && HasMsvcToolsetPrefix(vcvars, "14.29")) {
                    fallbackArgs = " -vcvars_ver=14.29";
                }
                activeVcvarsArgs = vcvarsBaseArgs + fallbackArgs;
                cmdWithEnv = WrapWithVcVars(cmd, activeVcvarsArgs);
                log("Fallback Command: " + cmdWithEnv);
                if (!RunCommand(cmdWithEnv, log)) {
                    log("CMake Configuration Failed.");
                    return result;
                }
                log("CMake configure succeeded with vcvarsall.bat x86 fallback.");
                log("MSVC environment: " + vcvarsLabel);
            } else {
                log("CMake Configuration Failed.");
                return result;
            }
        }
    }

    log("----------------------------------------");
    log("Step 2: Building Player Target");
    const std::string targetName = useMicroPlayer ? "ShaderLabMicroPlayer" : "ShaderLabPlayer";
    std::string buildCmd = "cmake --build \"" + buildDir.string() + "\" --target " + targetName + " --config Release";
    std::string buildCmdWithEnv = WrapWithVcVars(buildCmd, activeVcvarsArgs);
    log("Command: " + buildCmdWithEnv);

    bool releaseBuildOk = RunCommand(buildCmdWithEnv, log);
    if (!releaseBuildOk && hasPinnedToolset) {
        log("Build failed with pinned MSVC toolset; retrying with default toolset environment.");
        activeVcvarsArgs = vcvarsBaseArgs;
        buildCmdWithEnv = WrapWithVcVars(buildCmd, activeVcvarsArgs);
        log("Retry Build Command: " + buildCmdWithEnv);
        releaseBuildOk = RunCommand(buildCmdWithEnv, log);
        if (releaseBuildOk) {
            log("Build succeeded with default MSVC toolset environment.");
        }
    }

    if (!releaseBuildOk && useMicroPlayer && useCrinkler) {
        log("Crinkler link failed for MicroPlayer. Retrying with /TINYIMPORT disabled for stability.");
        std::string stableCmd = cmd + " -DSHADERLAB_CRINKLER_TINYIMPORT=OFF";
        std::string stableCmdWithEnv = WrapWithVcVars(stableCmd, activeVcvarsArgs);
        log("Retry Configure Command: " + stableCmdWithEnv);

        if (RunCommand(stableCmdWithEnv, log)) {
            log("Retry Build Command: " + buildCmdWithEnv);
            releaseBuildOk = RunCommand(buildCmdWithEnv, log);
            if (releaseBuildOk) {
                log("Crinkler stability fallback succeeded (/TINYIMPORT disabled).");
            }
        }
    }

    if (!releaseBuildOk) {
        log("Build Failed. Trying Debug Configuration...");
        buildCmd = "cmake --build \"" + buildDir.string() + "\" --target " + targetName + " --config Debug";
        buildCmdWithEnv = WrapWithVcVars(buildCmd, activeVcvarsArgs);
        if (!RunCommand(buildCmdWithEnv, log)) {
            log("Build Failed.");
            return result;
        }
    }

    log("----------------------------------------");
    log("Step 3: Verifying Executable");

    fs::path buildBinPath = buildDir / "bin";
    fs::path playerExe = buildBinPath / (targetName + ".exe");

    if (!fs::exists(playerExe)) {
        if (fs::exists(buildBinPath / "Debug" / (targetName + ".exe"))) playerExe = buildBinPath / "Debug" / (targetName + ".exe");
        else if (fs::exists(buildBinPath / "Release" / (targetName + ".exe"))) playerExe = buildBinPath / "Release" / (targetName + ".exe");
    }

    if (!fs::exists(playerExe)) {
        log("Error: Could not find " + targetName + ".exe in build artifacts.");
        log("Expected locations checked:");
        log(" - " + (buildBinPath / (targetName + ".exe")).string());
        log(" - " + (buildBinPath / "Debug" / (targetName + ".exe")).string());
        log(" - " + (buildBinPath / "Release" / (targetName + ".exe")).string());
        return result;
    }

    log("Found Player EXE: " + playerExe.string());

    if (useMicroPlayer) {
        log("----------------------------------------");
        log("Step 4: Exporting MicroPlayer Binary");

        std::error_code copyEc;
        fs::create_directories(fs::path(request.targetExePath).parent_path(), copyEc);
        copyEc.clear();
        fs::copy_file(playerExe, request.targetExePath, fs::copy_options::overwrite_existing, copyEc);
        if (copyEc) {
            log("Error: Failed to copy MicroPlayer output to target path.");
            log("Details: " + copyEc.message());
            return result;
        }

        std::error_code sizeEc;
        const uint64_t finalSize = fs::file_size(request.targetExePath, sizeEc);
        if (!sizeEc) {
            result.finalExeBytes = finalSize;
            log("Final EXE size: " + std::to_string(finalSize) + " bytes");
        }

        const uint64_t budgetBytes = SizePresetToBytes(request.sizeTarget);
        result.budgetBytes = budgetBytes;
        if (budgetBytes > 0 && !sizeEc) {
            if (finalSize <= budgetBytes) {
                result.budgetHit = true;
                const uint64_t bytesLeft = budgetBytes - finalSize;
                result.report = "Size target " + std::string(SizePresetName(request.sizeTarget)) + " hit. " + std::to_string(bytesLeft) + " bytes left in budget.";
                log(result.report);
            } else {
                result.budgetHit = false;
                const uint64_t overshoot = finalSize - budgetBytes;
                result.report = "Size target " + std::string(SizePresetName(request.sizeTarget)) + " missed. Overshot by " + std::to_string(overshoot) + " bytes.";
                log(result.report);
            }
        }

        if (request.restrictedCompactTrack) {
            log("Restricted optimization: embedding compact track timeline into MicroPlayer executable");
            ProjectData project;
            if (!Serializer::LoadProject(request.projectPath, project)) {
                log("Error: Failed to load project for compact track export.");
                return result;
            }

            std::string embedError;
            if (!EmbedCompactTrackIntoExecutable(project.track, fs::path(request.targetExePath), embedError)) {
                log("Error: " + embedError);
                return result;
            }
            log("Embedded compact track payload into output executable.");
        }

        log("----------------------------------------");
        log("Runtime Target Path: MicroPlayer (x86 tiny preset path)");
        log("Output Executable: " + request.targetExePath);
        log("BUILD SUCCESSFUL");
        result.success = true;
        return result;
    }

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

    if (request.restrictedCompactTrack) {
        log("Restricted optimization: compact track binary enabled");
        fs::path compactTrackPath = packRoot / "assets" / "track.bin";
        if (!WriteCompactTrackBinary(project.track, compactTrackPath, writeError)) {
            log("Error: " + writeError);
            return result;
        }
        extraFiles.push_back({ compactTrackPath.string(), "assets/track.bin" });

        project.track.name.clear();
        project.track.rows.clear();
        project.track.currentBeat = 0;
        project.track.lastTriggeredBeat = -1;

        for (auto& scene : project.scenes) {
            scene.name.clear();
            scene.shaderCode.clear();
            for (auto& fx : scene.postFxChain) {
                fx.name.clear();
                fx.shaderCode.clear();
            }
        }
        for (auto& clip : project.audioLibrary) {
            clip.name.clear();
        }
    }

    if (!Serializer::SaveProject(project, packProjectPath.string())) {
        log("Error: Failed to write packed project manifest.");
        return result;
    }

    log("----------------------------------------");
    log("Step 5: Packing Executable");
    if (Serializer::PackExecutable(playerExe.string(), request.targetExePath, packProjectPath.string(), extraFiles)) {
        std::error_code sizeEc;
        const uint64_t finalSize = fs::file_size(request.targetExePath, sizeEc);
        if (!sizeEc) {
            result.finalExeBytes = finalSize;
            log("Final EXE size: " + std::to_string(finalSize) + " bytes");
        }

        const uint64_t budgetBytes = SizePresetToBytes(request.sizeTarget);
        result.budgetBytes = budgetBytes;
        if (budgetBytes > 0 && !sizeEc) {
            if (finalSize <= budgetBytes) {
                result.budgetHit = true;
                const uint64_t bytesLeft = budgetBytes - finalSize;
                result.report = "Size target " + std::string(SizePresetName(request.sizeTarget)) + " hit. " + std::to_string(bytesLeft) + " bytes left in budget.";
                log(result.report);
            } else {
                result.budgetHit = false;
                const uint64_t overshoot = finalSize - budgetBytes;
                result.report = "Size target " + std::string(SizePresetName(request.sizeTarget)) + " missed. Overshot by " + std::to_string(overshoot) + " bytes.";
                log(result.report);
            }
        }

        log("----------------------------------------");
        log("Runtime Target Path: Full Runtime Player (x64 open/free demo path)");
        log("Output Executable: " + request.targetExePath);
        log("BUILD SUCCESSFUL");
        result.success = true;
    } else {
        log("Error: Failed to pack executable.");
    }

    return result;
}

} // namespace ShaderLab
