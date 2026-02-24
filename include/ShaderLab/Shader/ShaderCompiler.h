#pragma once

#include "ShaderLab/Platform/Platform.h"
#include "ShaderLab/Shader/ShaderBase.h"
#include <string>
#include <vector>

// DXC is available on Windows (via LoadLibrary) and on Linux/macOS (as a
// native shared library from the DirectXShaderCompiler open-source project).
// On Windows we use WRL ComPtr; on other platforms we use the CComPtr shim
// that ships with the DXC SDK headers.
#ifdef _WIN32
    #include <windows.h>
    #include <dxcapi.h>
    #include <wrl/client.h>
    using Microsoft::WRL::ComPtr;
#else
    // Linux/macOS: install the DirectXShaderCompiler package.
    //   Ubuntu/Debian: apt install directx-shader-compiler
    //   or build from https://github.com/microsoft/DirectXShaderCompiler
    // The DXC SDK headers provide WinAdapter.h (HRESULT, REFCLSID …) and
    // a CComPtr<> template compatible with ComPtr<> usage below.
    #include <dxc/dxcapi.h>
    #include <dxc/WinAdapter.h>
    template<typename T>
    using ComPtr = CComPtr<T>;
#endif

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
        std::vector<ShaderBase::TextureBindingDecl> sharedBindings;
        sharedBindings.reserve(bindings.size());
        for (const auto& binding : bindings) {
            sharedBindings.push_back({binding.slot, binding.type});
        }
        return ShaderBase::BuildFragmentShaderTemplate(fragmentSource, sharedBindings);
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
#ifdef _WIN32
    // On Windows, DXC is loaded dynamically from dxcompiler.dll.
    using DxcCreateInstanceProc = HRESULT (WINAPI*)(REFCLSID, REFIID, LPVOID*);
    HMODULE              m_dxcModule         = nullptr;
    DxcCreateInstanceProc m_dxcCreateInstance = nullptr;
#endif

    void ParseDiagnostics(IDxcBlobEncoding* errorBlob,
                          std::vector<ShaderDiagnostic>& outDiagnostics);

    std::vector<std::wstring> GetCompileArguments(ShaderCompileMode mode);

    ComPtr<IDxcUtils>          m_utils;
    ComPtr<IDxcCompiler3>      m_compiler;
    ComPtr<IDxcIncludeHandler> m_includeHandler;
};

} // namespace ShaderLab
