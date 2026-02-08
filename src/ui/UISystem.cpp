// Force include standard headers first to avoid namespace pollution
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ratio>
#include <filesystem>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <fstream>
#include <climits>
#include <future>
#include <mutex>
#include <cstdint>
#include <cstring>

#include "ShaderLab/UI/UISystem.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Audio/AudioSystem.h"
#include "ShaderLab/Core/BuildPipeline.h"
#include "ShaderLab/Core/RuntimeExporter.h"
#include "ShaderLab/Core/Serializer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <imgui_internal.h>

namespace ShaderLab {

namespace fs = std::filesystem;

UISystem::UISystem() {
    // Store application root (assumed CWD at launch)
    m_appRoot = fs::current_path().string();

    CreateDefaultScene();
    CreateDefaultTrack();

    // Initialize custom HLSL language definition to ensure everything works
    TextEditor::LanguageDefinition langDef;

    static const char* const keywords[] = {
        "AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer", "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile", "compile_fragment",
        "CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else",
        "export", "extern", "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj",
        "linear", "LineStream", "matrix", "min16float", "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset",
        "pass", "pixelfragment", "PixelShader", "point", "PointStream", "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer",
        "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state",
        "static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS",
        "Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment",
        "VertexShader", "void", "volatile", "while",
        "bool1","bool2","bool3","bool4","double1","double2","double3","double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout",
        "uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4",
        "float1x1","float2x1","float3x1","float4x1","float1x2","float2x2","float3x2","float4x2",
        "float1x3","float2x3","float3x3","float4x3","float1x4","float2x4","float3x4","float4x4",
        "half1x1","half2x1","half3x1","half4x1","half1x2","half2x2","half3x2","half4x2",
        "half1x3","half2x3","half3x3","half4x3","half1x4","half2x4","half3x4","half4x4",
    };

    for (auto& k : keywords) {
        langDef.mKeywords.insert(k);
    }

    static const char* const identifiers[] = {
        "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint",
        "asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx",
        "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
        "distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2",
        "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount",
        "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange",
        "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan",
        "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf",
        "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
        "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin",
        "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step",
        "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj",
        "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
    };

    for (auto& k : identifiers) {
        TextEditor::Identifier id;
        id.mDeclaration = "Built-in function";
        langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
    }

    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
    langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", TextEditor::PaletteIndex::Punctuation));

    langDef.mCommentStart = "/*";
    langDef.mCommentEnd = "*/";
    langDef.mSingleLineComment = "//";
    langDef.mCaseSensitive = true;
    langDef.mAutoIndentation = true;
    langDef.mName = "HLSL";

    m_textEditor.SetLanguageDefinition(langDef);

    // Create enhanced dark palette with vivid colors (IM_COL32 AABBGGRR format)
    auto palette = TextEditor::GetDarkPalette();
    palette[(int)TextEditor::PaletteIndex::Keyword] = 0xffd69c56;         // #569cd6 (Blue)
    palette[(int)TextEditor::PaletteIndex::KnownIdentifier] = 0xffb0c94e; // #4ec9b0 (Teal)
    palette[(int)TextEditor::PaletteIndex::Number] = 0xffa8ceb5;          // #b5cea8 (Light Green)
    palette[(int)TextEditor::PaletteIndex::String] = 0xff7891ce;          // #ce9178 (Orange)
    palette[(int)TextEditor::PaletteIndex::Comment] = 0xff55996a;         // #6a9955 (Green)
    palette[(int)TextEditor::PaletteIndex::MultiLineComment] = 0xff55996a;// #6a9955
    palette[(int)TextEditor::PaletteIndex::Identifier] = 0xffdcdcdc;      // #dcdcdc (White/Grey)
    palette[(int)TextEditor::PaletteIndex::Punctuation] = 0xffdcdcdc;
    palette[(int)TextEditor::PaletteIndex::Preprocessor] = 0xff9b9b9b;
    m_textEditor.SetPalette(palette);

    m_textEditor.SetShowWhitespaces(false);
}

std::string UISystem::GetProjectName() const {
    if (m_currentProjectPath.empty()) {
        return "untitled";
    }

    fs::path path(m_currentProjectPath);
    std::string stem = path.stem().string();
    return stem.empty() ? "untitled" : stem;
}

UISystem::~UISystem() {
    Shutdown();
}

void UISystem::SetActiveScene(int index) {
    if (index >= (int)m_scenes.size()) return;

    m_activeSceneIndex = index;
    if (index >= 0) {
        auto& scene = m_scenes[index];
        // Editor
        m_shaderState.text = scene.shaderCode;
        m_textEditor.SetText(scene.shaderCode);
        m_shaderState.status = CompileStatus::Clean;
    } else {
        // Clear / Null Scene
        m_shaderState.text = "// No Active Scene";
        m_textEditor.SetText(m_shaderState.text);
        m_shaderState.status = CompileStatus::Clean;
    }
}

void UISystem::SyncPostFxEditorToSelection() {
    if (m_postFxSelectedIndex < 0 || m_postFxSelectedIndex >= (int)m_postFxDraftChain.size()) {
        m_shaderState.text = "// No post fx selected";
        m_textEditor.SetText(m_shaderState.text);
        m_shaderState.status = CompileStatus::Clean;
        m_shaderState.diagnostics.clear();
        return;
    }

    auto& effect = m_postFxDraftChain[m_postFxSelectedIndex];
    m_shaderState.text = effect.shaderCode;
    m_textEditor.SetText(effect.shaderCode);
    m_shaderState.status = effect.isDirty ? CompileStatus::Dirty : CompileStatus::Clean;
    m_shaderState.diagnostics.clear();
}

void UISystem::AppendDemoLog(const std::string& message) {
    m_demoLog.push_back(message);
    if (m_demoLog.size() > 400) {
        m_demoLog.erase(m_demoLog.begin(), m_demoLog.begin() + (m_demoLog.size() - 400));
    }
}

bool UISystem::Initialize(HWND hwnd, Device* device, Swapchain* swapchain) {
    if (!device || !device->IsValid() || !swapchain || !hwnd) {
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    m_context = ImGui::CreateContext();
    ImGui::SetCurrentContext(m_context);

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Load custom fonts for editor
    // Path to fonts relative to executable (in editor_assets/fonts/)
    std::string fontPath = m_appRoot + "/editor_assets/fonts/";
    
    // Hacked font for logo (large)
    m_fontHackedLogo = io.Fonts->AddFontFromFileTTF((fontPath + "Hacked-KerX.ttf").c_str(), 48.0f);
    if (!m_fontHackedLogo) {
        // Fallback to default if font doesn't load
        m_fontHackedLogo = io.Fonts->AddFontDefault();
    }
    
    // Hacked font for headings (medium)
    m_fontHackedHeading = io.Fonts->AddFontFromFileTTF((fontPath + "Hacked-KerX.ttf").c_str(), 20.0f);
    if (!m_fontHackedHeading) {
        m_fontHackedHeading = io.Fonts->AddFontDefault();
    }
    
    // Orbitron for regular text
    m_fontOrbitronText = io.Fonts->AddFontFromFileTTF((fontPath + "OrbitronMedium-Bz9B.ttf").c_str(), 15.0f);
    if (!m_fontOrbitronText) {
        m_fontOrbitronText = io.Fonts->AddFontDefault();
    }
    
    // Erbos Draco for numerical fields
    m_fontErbosDracoNumbers = io.Fonts->AddFontFromFileTTF((fontPath + "ErbosDraco1StNbpRegular-99V5.ttf").c_str(), 14.0f);
    if (!m_fontErbosDracoNumbers) {
        m_fontErbosDracoNumbers = io.Fonts->AddFontDefault();
    }

    // Build font atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Setup style
    SetupImGuiStyle();

    // Create descriptor heap for ImGui
    CreateDescriptorHeap(device);

    // Setup platform/renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(
        device->GetDevice(),
        Swapchain::BUFFER_COUNT,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_srvHeap.Get(),
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvHeap->GetGPUDescriptorHandleForHeapStart()
    );

    // Store device reference for texture creation
    m_deviceRef = device;
    m_swapchainRef = swapchain;

    m_initialized = true;
    return true;
}

void UISystem::Shutdown() {
    if (m_initialized) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(m_context);
        m_context = nullptr;
    }

    m_previewTexture.Reset();
    m_previewRtvHeap.Reset();
    m_srvHeap.Reset();
    m_initialized = false;
}

void UISystem::CreateDescriptorHeap(Device* device) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 128;  // ImGui font (0), Preview (1), Thumbnails (2+)
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap));
}

void UISystem::CreatePreviewTexture(uint32_t width, uint32_t height) {
    if (!m_deviceRef || width == 0 || height == 0) {
        return;
    }

    // Only recreate if size changed
    if (m_previewTexture && m_previewTextureWidth == width && m_previewTextureHeight == height) {
        return;
    }

    m_previewTexture.Reset();
    m_previewRtvHeap.Reset();

    // Create render target texture
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    clearValue.Color[0] = 0.0f;
    clearValue.Color[1] = 0.0f;
    clearValue.Color[2] = 0.0f;
    clearValue.Color[3] = 1.0f;

    HRESULT hr = m_deviceRef->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        IID_PPV_ARGS(&m_previewTexture)
    );

    if (FAILED(hr)) {
        return;
    }

    // Create RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    m_deviceRef->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_previewRtvHeap));

    // Create RTV
    m_previewRtvHandle = m_previewRtvHeap->GetCPUDescriptorHandleForHeapStart();
    m_deviceRef->GetDevice()->CreateRenderTargetView(m_previewTexture.Get(), nullptr, m_previewRtvHandle);

    // Create SRV in ImGui's descriptor heap (descriptor index 1, after ImGui's font texture at 0)
    if (m_srvHeap) {
        UINT descriptorSize = m_deviceRef->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
        srvHandle.ptr += descriptorSize;  // Skip ImGui's font texture descriptor

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_deviceRef->GetDevice()->CreateShaderResourceView(
            m_previewTexture.Get(),
            &srvDesc,
            srvHandle
        );

        // Store GPU handle for ImGui::Image
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += descriptorSize;
        m_previewSrvGpuHandle = gpuHandle;
    }

    m_previewTextureWidth = width;
    m_previewTextureHeight = height;

    // Also create dummy texture if not exists
    CreateDummyTexture();
}

void UISystem::CreateDummyTexture() {
    auto device = m_deviceRef->GetDevice();

    // 1. Texture2D Dummy
    if (!m_dummyTexture) {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;

        // Initialize to solid black
        unsigned char blackPixel[4] = {0,0,0,255};
        CreateTextureFromData(blackPixel, 1, 1, 4, m_dummyTexture);

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeap));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(m_dummyTexture.Get(), &srvDesc, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // 2. TextureCube Dummy
    if (!m_dummyTextureCube) {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 6; // Cube faces
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;

        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dummyTextureCube));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeapCube));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // Cube view
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = 1;
        device->CreateShaderResourceView(m_dummyTextureCube.Get(), &srvDesc, m_dummySrvHeapCube->GetCPUDescriptorHandleForHeapStart());
    }

    // 3. Texture3D Dummy
    if (!m_dummyTexture3D) {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        texDesc.Width = 1;
        texDesc.Height = 1;
        texDesc.DepthOrArraySize = 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;

        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_dummyTexture3D));

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeap3D));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D; // 3D view
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture3D.MipLevels = 1;
        device->CreateShaderResourceView(m_dummyTexture3D.Get(), &srvDesc, m_dummySrvHeap3D->GetCPUDescriptorHandleForHeapStart());
    }
}

void UISystem::SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Geometry - Sharp, industrial look
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.TabRounding = 0.0f;

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(5, 3);
    style.ItemSpacing = ImVec2(6, 4);

    // Demoscene / Cyberpunk Palette
    // Deep blacks, grays, and electric accents (Cyan/Teal/Orange)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f); // Nearly black
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Input Fields
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);

    // Title Bars
    colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 0.45f, 0.45f, 1.00f); // Teal Accent
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);

    // Menus
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);

    // Sliders & Checks
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.90f, 0.90f, 1.00f); // Bright Cyan
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.90f, 0.90f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.55f, 0.55f, 1.00f); // Teal

    // Headers (Collapsing Headers, Tree Nodes)
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.50f, 0.50f, 1.00f);

    // Separators
    colors[ImGuiCol_Separator] = ImVec4(0.40f, 0.40f, 0.40f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);

    // Resize Grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.00f, 0.55f, 0.55f, 1.00f); // Teal
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    // Plots
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 0.80f, 0.50f, 1.00f); // Green-ish
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 0.60f, 1.00f);

    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.55f, 0.55f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.00f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
}

void UISystem::BeginFrame() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    m_aboutTimeSeconds = ImGui::GetTime();

    ImGuiIO& io = ImGui::GetIO();
    const bool altDown = io.KeyAlt;
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        m_shaderState.showPerformanceOverlay = !m_shaderState.showPerformanceOverlay;
    }
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        m_previewFullscreen = !m_previewFullscreen;
    }

    // Setup fullscreen dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    windowFlags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("MainDockspace", nullptr, windowFlags);
    ImGui::PopStyleVar(2);

    // Show menu bar first
    ShowMainMenuBar();

    // Show mode tabs below menu bar
    UIMode pendingMode = m_currentMode;
    UIMode requestedMode = m_currentMode;
    bool forceSelect = false;
    if (altDown && ImGui::IsKeyPressed(ImGuiKey_Q, false)) {
        requestedMode = UIMode::Demo;
        forceSelect = true;
    } else if (altDown && ImGui::IsKeyPressed(ImGuiKey_W, false)) {
        requestedMode = UIMode::Scene;
        forceSelect = true;
    } else if (altDown && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
        requestedMode = UIMode::PostFX;
        forceSelect = true;
    }

    if (forceSelect) {
        pendingMode = requestedMode;
    }

    if (ImGui::BeginTabBar("ModeTabBar", ImGuiTabBarFlags_None)) {
        const bool allowTabSwitch = !forceSelect;
        ImGuiTabItemFlags demoFlags = (forceSelect && requestedMode == UIMode::Demo) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Demo", nullptr, demoFlags)) {
            if (allowTabSwitch) {
                pendingMode = UIMode::Demo;
            }
            ImGui::EndTabItem();
        }
        ImGuiTabItemFlags sceneFlags = (forceSelect && requestedMode == UIMode::Scene) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Scene", nullptr, sceneFlags)) {
            if (allowTabSwitch) {
                pendingMode = UIMode::Scene;
            }
            ImGui::EndTabItem();
        }
        ImGuiTabItemFlags postFlags = (forceSelect && requestedMode == UIMode::PostFX) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
        if (ImGui::BeginTabItem("Post FX", nullptr, postFlags)) {
            if (allowTabSwitch) {
                pendingMode = UIMode::PostFX;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    // Create dockspace below tabs
    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    // Apply mode change immediately so layout + windows stay in sync
    if (pendingMode != m_currentMode) {
        m_currentMode = pendingMode;
    }

    // Build layout if mode changed or first run
    bool modeChanged = (m_currentMode != m_lastMode);
    const float kModeFlashDuration = 0.25f;
    if (!m_layoutBuilt || modeChanged) {
        BuildLayout(m_currentMode);
        m_layoutBuilt = true;
        m_lastMode = m_currentMode;
        m_modeChangeFlashSeconds = kModeFlashDuration;
    }

    ImGui::End();

    // Show transport controls
    ShowTransportControls();

    // Sync editor state on mode change to avoid cross-mode text bleeding
    if (modeChanged) {
        if (m_currentMode == UIMode::PostFX) {
            if (m_postFxSelectedIndex < 0 && !m_postFxDraftChain.empty()) {
                m_postFxSelectedIndex = 0;
            }
            SyncPostFxEditorToSelection();
        } else if (m_currentMode == UIMode::Scene || m_currentMode == UIMode::Demo) {
            SetActiveScene(m_activeSceneIndex);
        }
    }

    // Show mode-specific windows
    ShowModeWindows();

    if (m_showAbout) {
        ShowAboutWindow();
    }

    if (m_modeChangeFlashSeconds > 0.0f) {
        m_modeChangeFlashSeconds = (std::max)(0.0f, m_modeChangeFlashSeconds - ImGui::GetIO().DeltaTime);
        float t = (kModeFlashDuration > 0.0f) ? (m_modeChangeFlashSeconds / kModeFlashDuration) : 0.0f;
        float alpha = 0.35f * t;

        ImVec4 color = ImVec4(0.0f, 0.75f, 0.75f, alpha);
        if (m_currentMode == UIMode::Demo) {
            color = ImVec4(0.2f, 0.6f, 1.0f, alpha);
        } else if (m_currentMode == UIMode::PostFX) {
            color = ImVec4(1.0f, 0.6f, 0.2f, alpha);
        }

        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImVec2 min = mainViewport->WorkPos;
        ImVec2 max = ImVec2(min.x + mainViewport->WorkSize.x, min.y + mainViewport->WorkSize.y);
        ImU32 col = ImGui::GetColorU32(color);
        ImGui::GetForegroundDrawList()->AddRect(min, max, col, 0.0f, 0, 3.0f);
    }

    UpdateBuildLogic();
}

void UISystem::ShowMainMenuBar() {
    if (ImGui::BeginMenuBar()) {
        // ShaderLab logo in top left using Hacked font
        if (m_fontHackedHeading) {
            ImGui::PushFont(m_fontHackedHeading);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.90f, 0.90f, 1.0f)); // Bright cyan
            ImGui::Text("SHADERLAB");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Separator();
        }
        
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                m_scenes.clear();
                CreateDefaultScene();
                m_track = DemoTrack();
                CreateDefaultTrack();
                m_audioLibrary.clear();
                m_currentProjectPath.clear();
                if (m_audioSystem) m_audioSystem->Stop();
            }
            if (ImGui::MenuItem("Open", "Ctrl+O")) {
                OpenProject();
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                SaveProject();
            }
            if (ImGui::MenuItem("Save As...")) {
                SaveProjectAs();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Build Self-Contained EXE...")) {
                BuildProject();
            }
            if (ImGui::MenuItem("Export Runtime Package...")) {
                ExportRuntimePackage();
            }
            ImGui::Separator();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Demo Mode", nullptr, m_currentMode == UIMode::Demo)) {
                m_currentMode = UIMode::Demo;
            }
            if (ImGui::MenuItem("Scene Mode", nullptr, m_currentMode == UIMode::Scene)) {
                m_currentMode = UIMode::Scene;
            }
            if (ImGui::MenuItem("Post FX Mode", nullptr, m_currentMode == UIMode::PostFX)) {
                m_currentMode = UIMode::PostFX;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Device")) {
            auto adapters = Device::GetAvailableAdapters();
            int currentIdx = -1;
            if (m_deviceRef) currentIdx = m_deviceRef->GetAdapterIndex();

            for (const auto& adapter : adapters) {
                std::string name;
                name.reserve(adapter.name.length());
                for (wchar_t c : adapter.name) name.push_back(static_cast<char>(c));

                // Calculate VRAM in GB for display
                float vramGB = static_cast<float>(adapter.videoMemory) / (1024.0f * 1024.0f * 1024.0f);
                std::string label = name + " (" + std::to_string(vramGB).substr(0, 4) + " GB)";

                bool isSelected = (currentIdx != -1 && (int)adapter.index == currentIdx);

                if (ImGui::MenuItem(label.c_str(), nullptr, isSelected)) {
                    if (m_restartCallback) {
                        m_restartCallback((int)adapter.index);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                m_showAbout = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void UISystem::EndFrame() {
    ImGui::Render();
}

bool UISystem::CompileScene(int sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_scenes.size()) return false;
    auto& scene = m_scenes[sceneIndex];

    // Only compile if we have a renderer
    if (!m_previewRenderer) return false;

    // Collect texture declarations
    std::vector<PreviewRenderer::TextureDecl> decls;
    for(const auto& b : scene.bindings) {
        if (!b.enabled) continue;

        PreviewRenderer::TextureDecl decl;
        decl.slot = b.channelIndex;

        if (b.type == TextureType::TextureCube) decl.type = "TextureCube";
        else if (b.type == TextureType::Texture3D) decl.type = "Texture3D";
        else decl.type = "Texture2D";

        decls.push_back(decl);
    }

    // Compile
    std::vector<std::string> errors;
    auto pso = m_previewRenderer->CompileShader(scene.shaderCode, decls, errors);
    bool success = (pso != nullptr);

    // Update Scene state
    if (success) {
        scene.pipelineState = pso;
        scene.isDirty = false;
    }

    // If this is the active scene, update the editor UI state too
    if (sceneIndex == m_activeSceneIndex) {
        m_shaderState.status = success ? CompileStatus::Success : CompileStatus::Error;
        m_shaderState.diagnostics.clear();
        for (const auto& msg : errors) {
            Diagnostic d;
            d.line = 0;
            d.column = 0;
            d.message = msg;
            m_shaderState.diagnostics.push_back(d);
        }

        if (success) {
            m_shaderState.lastCompiledText = scene.shaderCode;
        }
    }

    return success;
}

void UISystem::Render(ID3D12GraphicsCommandList* commandList) {
    // Only attempt preview rendering if we have all required components initialized
    bool previewRendered = false;
    if (m_previewRenderer && m_swapchainRef && m_deviceRef) {
        if (m_showAbout) {
            RenderAboutLogo(commandList);
        }
        previewRendered = RenderPreviewTexture(commandList);

        // If preview was rendered, restore render target and viewport for ImGui
        if (previewRendered) {
            // Reset render target to backbuffer after preview rendering
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapchainRef->GetCurrentRTV();
            commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

            // Restore viewport and scissor rect to full window size
            D3D12_VIEWPORT viewport{};
            viewport.Width = static_cast<float>(m_swapchainRef->GetWidth());
            viewport.Height = static_cast<float>(m_swapchainRef->GetHeight());
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            commandList->RSSetViewports(1, &viewport);

            D3D12_RECT scissor{};
            scissor.right = static_cast<LONG>(m_swapchainRef->GetWidth());
            scissor.bottom = static_cast<LONG>(m_swapchainRef->GetHeight());
            commandList->RSSetScissorRects(1, &scissor);
        }
    }

    // Set descriptor heap for ImGui (must be set before rendering)
    if (m_srvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
    }

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

bool UISystem::LoadTextureFromFile(const std::string& path, ComPtr<ID3D12Resource>& outResource) {
    if (path.empty()) return false;

    int w, h, channels;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) return false;

    // TODO: Handle failure in creation?
    // Using a local helper for synchronous upload
    CreateTextureFromData(data, w, h, 4, outResource);

    stbi_image_free(data);
    return outResource != nullptr;
}

void UISystem::CreateTextureFromData(const void* data, int width, int height, int channels, ComPtr<ID3D12Resource>& outResource) {
    (void)channels;
    auto device = m_deviceRef->GetDevice();

    // 1. Create Default Heap Resource (Dest)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};

    if (FAILED(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&outResource)))) {
        return;
    }

    // 2. Create Upload Buffer
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT numRows;
    UINT64 rowSizeInBytes;
    UINT64 totalBytes;

    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

    D3D12_HEAP_PROPERTIES uploadHeapProps = {D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.SampleDesc.Count = 1;

    ComPtr<ID3D12Resource> uploadBuffer;
    if (FAILED(device->CreateCommittedResource(
        &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadBuffer)))) {
        return;
    }

    // 3. Copy Memory
    void* mappedData;
    uploadBuffer->Map(0, nullptr, &mappedData);

    const uint8_t* srcData = (const uint8_t*)data;
    uint8_t* dstData = (uint8_t*)mappedData;

    for (UINT i = 0; i < numRows; ++i) {
        memcpy(dstData + footprint.Footprint.RowPitch * i,
               srcData + width * 4 * i,
               width * 4);
    }
    uploadBuffer->Unmap(0, nullptr);

    // 4. Create Short Lived Command Queue/List
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue));

    ComPtr<ID3D12CommandAllocator> allocator;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));

    ComPtr<ID3D12GraphicsCommandList> cmdList;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));

    // 5. Record Copy
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = outResource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = uploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = outResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    cmdList->ResourceBarrier(1, &barrier);
    cmdList->Close();

    // 6. Execute and Wait
    ID3D12CommandList* ppCommandLists[] = { cmdList.Get() };
    queue->ExecuteCommandLists(1, ppCommandLists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    queue->Signal(fence.Get(), 1);
    fence->SetEventOnCompletion(1, fenceEvent);
    WaitForSingleObject(fenceEvent, INFINITE);
    CloseHandle(fenceEvent);
}

ProjectState UISystem::CaptureState() {
    ProjectState state;
    state.scenes = m_scenes;
    state.audioLibrary = m_audioLibrary;
    state.track = m_track;
    state.transport = m_transport;
    state.currentMode = m_currentMode;
    state.shaderState = m_shaderState;
    state.activeSceneIndex = m_activeSceneIndex;

    // Strip GPU resources from the saved state to ensure they don't dangle
    for (auto& scene : state.scenes) {
        scene.texture.Reset();
        scene.srvHeap.Reset();
        scene.pipelineState.Reset();
        scene.textureValid = false;
        scene.postFxTextureA.Reset();
        scene.postFxTextureB.Reset();
        scene.postFxSrvHeap.Reset();
        scene.postFxRtvHeap.Reset();
        scene.postFxValid = false;
        for (auto& fx : scene.postFxChain) {
            fx.pipelineState.Reset();
            fx.isDirty = true;
            fx.historyIndex = 0;
            fx.historyInitialized = false;
            fx.historyTextures.clear();
        }

        for (auto& binding : scene.bindings) {
             binding.textureResource.Reset();
             binding.fileTextureValid = false;
        }
    }

    return state;
}

void UISystem::RestoreState(const ProjectState& state) {
    m_scenes = state.scenes;
    m_audioLibrary = state.audioLibrary;
    m_track = state.track;
    m_transport = state.transport;
    m_currentMode = state.currentMode;
    m_shaderState = state.shaderState;
    m_activeSceneIndex = state.activeSceneIndex;

    // Reload files and clear runtime resources (they belong to old device)
    for (auto& scene : m_scenes) {
        scene.pipelineState = nullptr;
        scene.texture = nullptr;
        scene.srvHeap = nullptr;
        scene.isDirty = true; // Force compile on next use
        scene.postFxTextureA = nullptr;
        scene.postFxTextureB = nullptr;
        scene.postFxSrvHeap = nullptr;
        scene.postFxRtvHeap = nullptr;
        scene.postFxValid = false;
        for (auto& fx : scene.postFxChain) {
            fx.pipelineState = nullptr;
            fx.isDirty = true;
            fx.historyIndex = 0;
            fx.historyInitialized = false;
            fx.historyTextures.clear();
        }

        for (auto& binding : scene.bindings) {
            binding.textureResource = nullptr;

            if (binding.bindingType == BindingType::File && !binding.filePath.empty()) {
                 if (LoadTextureFromFile(binding.filePath, binding.textureResource)) {
                     binding.fileTextureValid = true;
                 }
            }
        }
    }

    // Restore text editor
    m_textEditor.SetText(m_shaderState.text);

    // Mark as dirty so user knows to recompile
    if (m_shaderState.status == CompileStatus::Success) {
        m_shaderState.status = CompileStatus::Dirty;
    }

    // Force layout rebuild
    m_layoutBuilt = false;
}

void UISystem::SaveProject() {
    if (m_currentProjectPath.empty()) {
        SaveProjectAs();
        return;
    }

    // Prepare data
    ProjectData data;
    data.scenes = m_scenes;
    data.track = m_track;
    data.transport = m_transport;
    data.audioLibrary = m_audioLibrary;

    // Consolidate Assets (Copy external files to project/assets)
    // We assume m_currentProjectPath is a file path "path/to/project.json"
    fs::path projectRoot = fs::path(m_currentProjectPath).parent_path();
    if (Serializer::ConsolidateProject(data, projectRoot.string())) {
        // Update local state with Consolidated paths (so UI reflects "assets/...")
        m_scenes = data.scenes;
        m_audioLibrary = data.audioLibrary;
        // Track and Transport technically don't contain paths but we keep consistency

        // Save
        if (Serializer::SaveProject(data, m_currentProjectPath)) {
            // Optional: Notification "Saved"
        }
    }
}

void UISystem::SaveProjectAs() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "JSON Project (*.json)\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        m_currentProjectPath = szFile;
        SaveProject();
    }
}

void UISystem::OpenProject() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "JSON Project (*.json)\0*.json\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "json";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        m_currentProjectPath = szFile;
        ProjectData data;
        if (Serializer::LoadProject(m_currentProjectPath, data)) {
            m_scenes = data.scenes;
            m_audioLibrary = data.audioLibrary;
            m_track = data.track;
            m_transport.bpm = data.transport.bpm;

            // Adjust CWD to project root for relative paths?
            // Or just resolve relative paths against project root.
            fs::current_path(fs::path(m_currentProjectPath).parent_path());

            // Reload Audio (Basic re-init)
            if (m_audioSystem) {
                m_audioSystem->Stop();
                // We don't auto-play or auto-load everything until needed,
                // but textures must be reloaded for previews.
            }

            // Reload Textures
            if (m_deviceRef) {
                for(auto& scene : m_scenes) {
                    for(auto& bind : scene.bindings) {
                        if (bind.bindingType == BindingType::File && !bind.filePath.empty()) {
                            LoadTextureFromFile(bind.filePath, bind.textureResource);
                        }
                    }
                }
            }
        }
    }
}

void UISystem::BuildProject() {
    BuildPrereqReport prereq = BuildPipeline::CheckPrereqs(m_appRoot);
    if (!prereq.ok) {
        MessageBoxA(nullptr, prereq.message.c_str(), "Build Prerequisites Missing", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!prereq.message.empty()) {
        MessageBoxA(nullptr, prereq.message.c_str(), "Build Prerequisites Notice", MB_OK | MB_ICONINFORMATION);
    }

    if (m_currentMode == UIMode::PostFX && m_postFxSourceSceneIndex >= 0 && m_postFxSourceSceneIndex < (int)m_scenes.size()) {
        m_scenes[m_postFxSourceSceneIndex].postFxChain = m_postFxDraftChain;
    }

    if (m_currentProjectPath.empty()) {
        // Enforce Save First
        int ret = MessageBoxA(NULL, "Project must be saved before building. Save now?", "Build Requirement", MB_YESNO | MB_ICONQUESTION);
        if (ret == IDYES) {
            SaveProjectAs();
            if (m_currentProjectPath.empty()) return; // Cancelled
        } else {
            return;
        }
    } else {
        // Auto-save before build
        SaveProject();
    }

    // Now proceed with build using m_currentProjectPath
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Executable (*.exe)\0*.exe\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "exe";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        // Start Async Build
        m_isBuilding = true;
        m_buildComplete = false;
        m_buildSuccess = false;
        m_buildLog = "Initializing Build Process...\n";

        std::string targetExePath = szFile;
        std::string projectPath = m_currentProjectPath;
        std::string appRoot = m_appRoot;

        m_buildFuture = std::async(std::launch::async, [this, targetExePath, projectPath, appRoot]() {
            auto Log = [&](const std::string& msg) {
                std::lock_guard<std::mutex> lock(m_buildLogMutex);
                m_buildLog += msg;
                if (msg.empty() || msg.back() != '\n') {
                    m_buildLog += "\n";
                }
            };

            BuildRequest request;
            request.appRoot = appRoot;
            request.projectPath = projectPath;
            request.targetExePath = targetExePath;

            BuildResult result = BuildPipeline::BuildSelfContained(request, Log);
            m_buildSuccess = result.success;
            m_buildComplete = true;
        });
    }
}

void UISystem::UpdateBuildLogic() {
    if (m_isBuilding) {
        static bool autoCopyOnFailure = false;
        static bool didAutoCopy = false;

        if (ImGui::Begin("Build Status", &m_isBuilding, ImGuiWindowFlags_AlwaysAutoResize)) {
            // Progress Log
            ImGui::BeginChild("LogRegion", ImVec2(500, 300), true, ImGuiWindowFlags_HorizontalScrollbar);
            {
                std::lock_guard<std::mutex> lock(m_buildLogMutex);
                ImGui::TextUnformatted(m_buildLog.c_str());
                if (m_buildComplete && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            if (!m_buildComplete) {
                didAutoCopy = false;
            }

            if (ImGui::Button("Copy Log", ImVec2(120, 0))) {
                std::lock_guard<std::mutex> lock(m_buildLogMutex);
                ImGui::SetClipboardText(m_buildLog.c_str());
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-copy on failure", &autoCopyOnFailure);

            if (m_buildComplete) {
                if (m_buildSuccess) {
                    ImGui::TextColored(ImVec4(0,1,0,1), "Build Completed Successfully!");
                } else {
                    ImGui::TextColored(ImVec4(1,0,0,1), "Build Failed.");
                    if (autoCopyOnFailure && !didAutoCopy) {
                        std::lock_guard<std::mutex> lock(m_buildLogMutex);
                        ImGui::SetClipboardText(m_buildLog.c_str());
                        didAutoCopy = true;
                    }
                }
                if (ImGui::Button("Close", ImVec2(120, 0))) {
                    m_isBuilding = false;
                }
            } else {
                static float time = 0.0f;
                time += ImGui::GetIO().DeltaTime;
                const char* dots = (int(time * 2) % 4) == 0 ? ".   " : (int(time * 2) % 4) == 1 ? "..  " : (int(time * 2) % 4) == 2 ? "... " : "....";
                ImGui::Text("Building%s", dots);
            }
        }
        ImGui::End();
    }
}

void UISystem::ExportRuntimePackage() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Executable (*.exe)\0*.exe\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = "exe";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    // Suggest a name based on project or "Demo"
    if (!m_currentProjectPath.empty()) {
        std::string name = fs::path(m_currentProjectPath).stem().string();
        strcpy_s(szFile, name.c_str());
    } else {
        strcpy_s(szFile, "MyDemo");
    }

    if (GetSaveFileNameA(&ofn)) {
        ProjectData data;
        data.scenes = m_scenes;
        data.track = m_track;
        data.transport = m_transport;
        data.audioLibrary = m_audioLibrary;

        RuntimeExportRequest request;
        request.appRoot = m_appRoot;
        request.destExePath = szFile;
        request.data = data;

        RuntimeExportResult result = RuntimeExporter::Export(request);
        if (result.success) {
            MessageBoxA(NULL, result.message.c_str(), "Export Complete", MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL, result.message.c_str(), "Export Error", MB_ICONERROR);
        }
    }
}

} // namespace ShaderLab
