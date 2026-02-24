// ---------------------------------------------------------------------------
// ShaderCompiler_posix.cpp
//
// Linux / macOS implementation of ShaderCompiler.
// Replaces ShaderCompiler.cpp on non-Windows builds.
//
// Key differences from the Windows version:
//   - dxcompiler is loaded via dlopen() instead of LoadLibraryA().
//   - The -spirv flag is added to produce SPIR-V output for Vulkan.
//   - CComPtr<> (provided by dxc/WinAdapter.h) is used instead of
//     Microsoft::WRL::ComPtr<>.
//
// Install requirements:
//   Ubuntu/Debian : apt install directx-shader-compiler
//   Fedora        : dnf install directx-shader-compiler
//   macOS         : brew install directx-shader-compiler
//
// DXC shared library name:
//   Linux : libdxcompiler.so
//   macOS : libdxcompiler.dylib  (or via MoltenVK distribution)
// ---------------------------------------------------------------------------

#include "ShaderLab/Shader/ShaderCompiler.h"

#include <dlfcn.h>   // dlopen, dlsym, dlclose

#include <exception>
#include <sstream>
#include <cstdio>

namespace ShaderLab {

namespace {

using DxcCreateInstanceProc = HRESULT (*)(REFCLSID, REFIID, LPVOID*);

// The shared-library handle kept alive for the process lifetime.
static void*                 g_dxcHandle          = nullptr;
static DxcCreateInstanceProc g_dxcCreateInstance  = nullptr;

bool LoadDxcLibrary() {
    if (g_dxcHandle) return true; // already loaded

#ifdef __APPLE__
    const char* libNames[] = { "libdxcompiler.dylib", nullptr };
#else
    const char* libNames[] = { "libdxcompiler.so", "libdxcompiler.so.3", nullptr };
#endif

    for (const char** name = libNames; *name; ++name) {
        g_dxcHandle = dlopen(*name, RTLD_LAZY | RTLD_LOCAL);
        if (g_dxcHandle) break;
    }

    if (!g_dxcHandle) {
        std::fprintf(stderr,
            "[ShaderCompiler] Failed to load DXC library.  "
            "Install directx-shader-compiler or set LD_LIBRARY_PATH.  "
            "Error: %s\n", dlerror());
        return false;
    }

    g_dxcCreateInstance = reinterpret_cast<DxcCreateInstanceProc>(
        dlsym(g_dxcHandle, "DxcCreateInstance"));
    if (!g_dxcCreateInstance) {
        std::fprintf(stderr, "[ShaderCompiler] DxcCreateInstance symbol not found\n");
        dlclose(g_dxcHandle);
        g_dxcHandle = nullptr;
        return false;
    }
    return true;
}

std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) return {};
    // On Linux/macOS wchar_t is 4 bytes; a simple ASCII-range conversion
    // suffices for shader identifiers.  For full Unicode use iconv or ICU.
    std::string out;
    out.reserve(value.size());
    for (wchar_t c : value) {
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

} // namespace

ShaderCompiler::ShaderCompiler()  = default;
ShaderCompiler::~ShaderCompiler() { Shutdown(); }

bool ShaderCompiler::Initialize() {
#if !defined(SHADERLAB_ENABLE_DXC)
#define SHADERLAB_ENABLE_DXC 1
#endif
#if !SHADERLAB_ENABLE_DXC
    return false;
#else
    Shutdown();

    if (!LoadDxcLibrary()) return false;

    HRESULT hr = g_dxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils));
    if (FAILED(hr)) return false;

    hr = g_dxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_compiler));
    if (FAILED(hr)) return false;

    hr = m_utils->CreateDefaultIncludeHandler(&m_includeHandler);
    return SUCCEEDED(hr);
#endif
}

void ShaderCompiler::Shutdown() {
    m_includeHandler.Release();
    m_compiler.Release();
    m_utils.Release();
    // Note: we intentionally keep the dlopen handle alive (process lifetime).
}

// Compile from source – delegates to the shared implementation below.
ShaderCompileResult ShaderCompiler::CompileFromFile(const std::wstring& filepath,
                                                     const std::string&  entryPoint,
                                                     const std::string&  target,
                                                     ShaderCompileMode   mode) {
    ShaderCompileResult result;
    if (!m_utils || !m_compiler || !m_includeHandler) {
        ShaderDiagnostic diag;
        diag.message = "Shader compiler is not initialized";
        diag.isError = true;
        result.diagnostics.push_back(diag);
        return result;
    }

    ComPtr<IDxcBlobEncoding> sourceBlob;
    HRESULT hr = m_utils->LoadFile(filepath.c_str(), nullptr, &sourceBlob);
    if (FAILED(hr)) {
        ShaderDiagnostic diag;
        diag.message  = "Failed to load shader file";
        diag.filename = WideToUtf8(filepath);
        diag.isError  = true;
        result.diagnostics.push_back(diag);
        return result;
    }

    return CompileFromSource(
        std::string(
            static_cast<const char*>(sourceBlob->GetBufferPointer()),
            sourceBlob->GetBufferSize()),
        entryPoint, target, filepath, mode);
}

ShaderCompileResult ShaderCompiler::CompileFromSource(const std::string&  source,
                                                       const std::string&  entryPoint,
                                                       const std::string&  target,
                                                       const std::wstring& sourceName,
                                                       ShaderCompileMode   mode) {
    ShaderCompileResult result;
#if !SHADERLAB_ENABLE_DXC
    ShaderDiagnostic diag;
    diag.message = "DXC is disabled in this build";
    diag.isError = true;
    result.diagnostics.push_back(diag);
    return result;
#else
    if (!m_utils || !m_compiler || !m_includeHandler) {
        ShaderDiagnostic diag;
        diag.message = "Shader compiler is not initialized";
        diag.isError = true;
        result.diagnostics.push_back(diag);
        return result;
    }
    try {
        ComPtr<IDxcBlobEncoding> sourceBlob;
        HRESULT hr = m_utils->CreateBlob(source.c_str(),
                                         static_cast<UINT32>(source.size()),
                                         DXC_CP_ACP, &sourceBlob);
        if (FAILED(hr)) {
            ShaderDiagnostic diag; diag.message = "Failed to create source blob"; diag.isError = true;
            result.diagnostics.push_back(diag); return result;
        }

        std::wstring wEntry(entryPoint.begin(), entryPoint.end());
        std::wstring wTarget(target.begin(), target.end());

        std::vector<std::wstring> args = GetCompileArguments(mode);
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == L"-E" && i + 1 < args.size()) args[i + 1] = wEntry;
            if (args[i] == L"-T" && i + 1 < args.size()) args[i + 1] = wTarget;
        }

        std::vector<LPCWSTR> argPtrs;
        argPtrs.reserve(args.size());
        for (const auto& a : args) argPtrs.push_back(a.c_str());

        DxcBuffer buf{ sourceBlob->GetBufferPointer(), sourceBlob->GetBufferSize(), DXC_CP_ACP };
        ComPtr<IDxcResult> compileResult;
        hr = m_compiler->Compile(&buf, argPtrs.data(),
                                  static_cast<UINT32>(argPtrs.size()),
                                  m_includeHandler.p, IID_PPV_ARGS(&compileResult));
        if (FAILED(hr)) {
            ShaderDiagnostic diag; diag.message = "Shader compilation failed"; diag.isError = true;
            result.diagnostics.push_back(diag); return result;
        }

        HRESULT status; compileResult->GetStatus(&status);
        ComPtr<IDxcBlobUtf8> errors;
        compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
        if (errors && errors->GetStringLength() > 0)
            ParseDiagnostics(errors.p, result.diagnostics);

        if (SUCCEEDED(status)) {
            ComPtr<IDxcBlob> bytecode;
            hr = compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&bytecode), nullptr);
            if (SUCCEEDED(hr) && bytecode) {
                const uint8_t* data = static_cast<const uint8_t*>(bytecode->GetBufferPointer());
                result.bytecode.assign(data, data + bytecode->GetBufferSize());
                result.success = true;
            }
        }
        return result;
    } catch (const std::exception& ex) {
        ShaderDiagnostic diag;
        diag.message = std::string("Shader compilation exception: ") + ex.what();
        diag.filename = WideToUtf8(sourceName);
        diag.isError  = true;
        result.diagnostics.push_back(diag);
        return result;
    } catch (...) {
        ShaderDiagnostic diag;
        diag.message  = "Shader compilation exception: unknown";
        diag.filename = WideToUtf8(sourceName);
        diag.isError  = true;
        result.diagnostics.push_back(diag);
        return result;
    }
#endif
}

void ShaderCompiler::ParseDiagnostics(IDxcBlobEncoding*          errorBlob,
                                       std::vector<ShaderDiagnostic>& outDiagnostics) {
    if (!errorBlob || errorBlob->GetBufferSize() == 0) return;

    std::string errorString(static_cast<const char*>(errorBlob->GetBufferPointer()),
                            errorBlob->GetBufferSize());
    std::istringstream stream(errorString);
    std::string line;
    while (std::getline(stream, line)) {
        ShaderDiagnostic diag;
        diag.message = line;
        diag.isError = (line.find("error") != std::string::npos);
        outDiagnostics.push_back(diag);
    }
}

std::vector<std::wstring> ShaderCompiler::GetCompileArguments(ShaderCompileMode mode) {
    std::vector<std::wstring> args;
    args.push_back(L"-E");  args.push_back(L"main");
    args.push_back(L"-T");  args.push_back(L"ps_6_0");
    args.push_back(L"-HV"); args.push_back(L"2021");

    // Output SPIR-V for the Vulkan backend.
    args.push_back(L"-spirv");
    args.push_back(L"-fspv-target-env=vulkan1.2");

    if (mode == ShaderCompileMode::Live) {
        args.push_back(L"-Od");
        args.push_back(L"-Zi");
        args.push_back(L"-Qembed_debug");
    } else {
        args.push_back(L"-O3");
        args.push_back(L"-Qstrip_debug");
        args.push_back(L"-Qstrip_reflect");
        args.push_back(L"-Qstrip_priv");
    }
    return args;
}

} // namespace ShaderLab
