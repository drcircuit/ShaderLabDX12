#pragma once

#include <windows.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace ShaderLab {

enum class ShaderCompileMode {
    Live,    // Debug, no optimization, fast compile
    Build    // Release, O3, stripped for final demo
};

struct ShaderDiagnostic {
    std::string message;
    std::string filename;
    uint32_t line = 0;
    uint32_t column = 0;
    bool isError = false;
};

struct ShaderCompileResult {
    std::vector<uint8_t> bytecode;
    std::vector<ShaderDiagnostic> diagnostics;
    bool success = false;
};

class ShaderCompiler {
public:
    struct BindingDecl {
        int slot;
        std::string type;
    };

    static std::string WrapShaderSource(const std::string& fragmentSource, const std::vector<BindingDecl>& bindings) {
        std::string wrapped = R"(
cbuffer Constants : register(b0) {
    float iTime;
    float2 iResolution;
    float iBeat;
    float iBar;
    float fBarBeat;
};
)";
        // Generate texture declarations (Always 8 slots to match root signature/standard)
        for(int i=0; i<8; ++i) {
            std::string type = "Texture2D";
            for(const auto& decl : bindings) {
                if (decl.slot == i) {
                    type = decl.type;
                    break;
                }
            }
            wrapped += type + " iChannel" + std::to_string(i) + " : register(t" + std::to_string(i) + ");\n";
        }
        wrapped += "\n";

        // Generate samplers
        for(int i=0; i<8; ++i) {
             wrapped += "SamplerState iChannel" + std::to_string(i) + "Sampler : register(s" + std::to_string(i) + ");\n";
        }
        wrapped += "\n";
        wrapped += fragmentSource;
        return wrapped;
    }

    ShaderCompiler();
    ~ShaderCompiler();

    bool Initialize();
    void Shutdown();

    ShaderCompileResult CompileFromFile(const std::wstring& filepath,
                                        const std::string& entryPoint,
                                        const std::string& target,
                                        ShaderCompileMode mode = ShaderCompileMode::Live);

    ShaderCompileResult CompileFromSource(const std::string& source,
                                          const std::string& entryPoint,
                                          const std::string& target,
                                          const std::wstring& sourceName = L"shader.hlsl",
                                          ShaderCompileMode mode = ShaderCompileMode::Live);

private:
    using DxcCreateInstanceProc = HRESULT (WINAPI*)(REFCLSID, REFIID, LPVOID*);

    void ParseDiagnostics(IDxcBlobEncoding* errorBlob, 
                         std::vector<ShaderDiagnostic>& outDiagnostics);
    
    std::vector<std::wstring> GetCompileArguments(ShaderCompileMode mode);

    ComPtr<IDxcUtils> m_utils;
    ComPtr<IDxcCompiler3> m_compiler;
    ComPtr<IDxcIncludeHandler> m_includeHandler;
    HMODULE m_dxcModule = nullptr;
    DxcCreateInstanceProc m_dxcCreateInstance = nullptr;
};

} // namespace ShaderLab
