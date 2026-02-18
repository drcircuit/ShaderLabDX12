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
#include "ShaderLab/UI/UIConfig.h"
#include "ShaderLab/UI/OpenFontIcons.h"
#include "ShaderLab/UI/UISystemDemoUtils.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Audio/AudioSystem.h"
#include "ShaderLab/Core/BuildPipeline.h"
#include "ShaderLab/Core/RuntimeExporter.h"
#include "ShaderLab/Core/Serializer.h"
#include <nlohmann/json.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <commdlg.h>
#include <shellapi.h>
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Shell32.lib")

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <imgui_internal.h>

namespace ShaderLab {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
fs::path GetGlobalSnippetBaseDir(const std::string& appRoot) {
    fs::path baseDir;
    char* appData = nullptr;
    size_t appDataLen = 0;
    if (_dupenv_s(&appData, &appDataLen, "APPDATA") == 0 && appData && *appData) {
        baseDir = fs::path(appData) / "ShaderLab";
    } else {
        baseDir = fs::path(appRoot) / ".shaderlab";
    }
    if (appData) {
        free(appData);
    }
    return baseDir;
}

fs::path GetGlobalSnippetDirectory(const std::string& appRoot) {
    return GetGlobalSnippetBaseDir(appRoot) / "snippets";
}

std::string SanitizeSnippetFileStem(const std::string& name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-';
        result.push_back(ok ? c : '_');
    }
    if (result.empty()) {
        result = "Folder";
    }
    return result;
}

bool IconButton(const char* id, uint32_t iconCodepoint, const char* tooltip, const ImVec2& size = ImVec2(0.0f, 0.0f)) {
    const std::string icon = OpenFontIcons::ToUtf8(iconCodepoint);
    const std::string buttonId = std::string("##") + id;

    ImVec2 buttonSize = size;
    if (buttonSize.x <= 0.0f || buttonSize.y <= 0.0f) {
        const ImVec2 textSize = ImGui::CalcTextSize(icon.c_str());
        const ImVec2 pad = ImGui::GetStyle().FramePadding;
        if (buttonSize.x <= 0.0f) buttonSize.x = textSize.x + pad.x * 2.0f;
        if (buttonSize.y <= 0.0f) buttonSize.y = textSize.y + pad.y * 2.0f;
    }

    const bool pressed = ImGui::InvisibleButton(buttonId.c_str(), buttonSize);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();

    const ImU32 bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    drawList->AddRectFilled(min, max, bg, style.FrameRounding);
    if (style.FrameBorderSize > 0.0f) {
        drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();

    float textX = min.x;
    float textY = min.y;
    bool usedGlyphBounds = false;
    if (font) {
        if (ImFontBaked* baked = font->GetFontBaked(fontSize)) {
            if (ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(iconCodepoint))) {
                const float glyphW = glyph->X1 - glyph->X0;
                const float glyphH = glyph->Y1 - glyph->Y0;
                textX = min.x + (buttonSize.x - glyphW) * 0.5f - glyph->X0;
                textY = min.y + (buttonSize.y - glyphH) * 0.5f - glyph->Y0;
                usedGlyphBounds = true;
            }
        }
    }
    if (!usedGlyphBounds) {
        const ImVec2 textSize = ImGui::CalcTextSize(icon.c_str());
        textX = min.x + (buttonSize.x - textSize.x) * 0.5f;
        textY = min.y + (buttonSize.y - textSize.y) * 0.5f;
    }

    drawList->AddText(font, fontSize, ImVec2(std::floor(textX), std::floor(textY)), ImGui::GetColorU32(UIConfig::ColorCheckMark), icon.c_str());

    if (ImGui::IsItemHovered() && tooltip && *tooltip) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return pressed;
}

const char* BuildModeLabel(BuildMode mode) {
    return mode == BuildMode::ReleaseCrinkled ? "Release Crinkled" : "Release";
}

const char* SizePresetLabel(SizeTargetPreset preset) {
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

uint64_t SizePresetBytes(SizeTargetPreset preset) {
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

void OpenExternal(const std::string& target) {
    ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
}

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
    m_snippetTextEditor.SetLanguageDefinition(langDef);

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
    m_snippetTextEditor.SetPalette(palette);

    m_textEditor.SetShowWhitespaces(false);
    m_snippetTextEditor.SetShowWhitespaces(false);
    m_snippetTextEditor.SetReadOnly(true);

    LoadGlobalSnippets();
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

    m_hwnd = hwnd;

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
    std::string iconFontPath = m_appRoot + "/third_party/OpenFontIcons/";
    std::string iconFontFile = iconFontPath + UIConfig::FontFileOpenFontIcons;
    if (!fs::exists(iconFontFile)) {
        iconFontFile = fontPath + UIConfig::FontFileOpenFontIcons;
    }
    
    // Hacked font for logo (large)
    m_fontHackedLogo = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileHacked).c_str(), UIConfig::FontLogo);
    if (!m_fontHackedLogo) {
        // Fallback to default if font doesn't load
        m_fontHackedLogo = io.Fonts->AddFontDefault();
    }
    
    // Hacked font for headings (medium)
    m_fontHackedHeading = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileHacked).c_str(), UIConfig::FontHeading);
    if (!m_fontHackedHeading) {
        m_fontHackedHeading = io.Fonts->AddFontDefault();
    }
    
    // Orbitron for regular text
    m_fontOrbitronText = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileOrbitron).c_str(), UIConfig::FontText);
    if (!m_fontOrbitronText) {
        m_fontOrbitronText = io.Fonts->AddFontDefault();
    }
    static const ImWchar iconRanges[] = { 0xE000, 0xE0FF, 0 };
    constexpr float iconFontScale = 1.22f;
    constexpr float iconGlyphOffsetY = 1.0f;
    ImFontConfig iconConfigText;
    iconConfigText.MergeMode = true;
    iconConfigText.PixelSnapH = true;
    iconConfigText.GlyphOffset.y = iconGlyphOffsetY;
    io.Fonts->AddFontFromFileTTF(iconFontFile.c_str(), UIConfig::FontText * iconFontScale, &iconConfigText, iconRanges);
    
    // Erbos Draco for numerical fields
    m_fontErbosDracoNumbers = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileErbosOpen).c_str(), UIConfig::FontNumeric);
    if (!m_fontErbosDracoNumbers) {
        m_fontErbosDracoNumbers = io.Fonts->AddFontDefault();
    }

    // Orbitron for menu (smaller)
    m_fontMenuSmall = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileOrbitron).c_str(), UIConfig::FontMenu);
    if (!m_fontMenuSmall) {
        m_fontMenuSmall = io.Fonts->AddFontDefault();
    }
    ImFontConfig iconConfigMenu;
    iconConfigMenu.MergeMode = true;
    iconConfigMenu.PixelSnapH = true;
    iconConfigMenu.GlyphOffset.y = iconGlyphOffsetY;
    io.Fonts->AddFontFromFileTTF(iconFontFile.c_str(), UIConfig::FontMenu * iconFontScale, &iconConfigMenu, iconRanges);

    const float codeFontSizes[5] = { 11.0f, 12.0f, 13.0f, 15.0f, 17.0f };
    for (int i = 0; i < 5; ++i) {
        m_fontCodeSizes[i] = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileCode).c_str(), codeFontSizes[i]);
        if (!m_fontCodeSizes[i]) {
            m_fontCodeSizes[i] = io.Fonts->AddFontDefault();
        }
    }
    m_fontCode = m_fontCodeSizes[(int)CodeFontSize::M];

    m_fontCodeItalic = io.Fonts->AddFontFromFileTTF((fontPath + UIConfig::FontFileCodeItalic).c_str(), UIConfig::FontCode);
    if (!m_fontCodeItalic) {
        m_fontCodeItalic = m_fontCode;
    }

    // Build font atlas
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    
    // Set Orbitron as the default font for the UI
    if (m_fontOrbitronText) {
        io.FontDefault = m_fontOrbitronText;
    }

    m_textEditor.SetCommentFont(m_fontCodeItalic, UIConfig::FontCode);
    m_snippetTextEditor.SetCommentFont(m_fontCodeItalic, UIConfig::FontCode);

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

    // Demoscene / Cyberpunk Palette - Enhanced futuristic cyan/teal theme
    // Deep blacks, dark grays, and electric cyan accents
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = UIConfig::ColorText;
    colors[ImGuiCol_TextDisabled] = UIConfig::ColorTextDisabled;
    colors[ImGuiCol_WindowBg] = UIConfig::ColorWindowBg;
    colors[ImGuiCol_ChildBg] = UIConfig::ColorChildBg;
    colors[ImGuiCol_PopupBg] = UIConfig::ColorPopupBg;
    colors[ImGuiCol_Border] = UIConfig::ColorBorder;
    colors[ImGuiCol_BorderShadow] = UIConfig::ColorBorderShadow;

    // Input Fields - darker with cyan tint
    colors[ImGuiCol_FrameBg] = UIConfig::ColorFrameBg;
    colors[ImGuiCol_FrameBgHovered] = UIConfig::ColorFrameBgHovered;
    colors[ImGuiCol_FrameBgActive] = UIConfig::ColorFrameBgActive;

    // Title Bars - darker with bright cyan accent
    colors[ImGuiCol_TitleBg] = UIConfig::ColorTitleBg;
    colors[ImGuiCol_TitleBgActive] = UIConfig::ColorTitleBgActive;
    colors[ImGuiCol_TitleBgCollapsed] = UIConfig::ColorTitleBgCollapsed;

    // Menus - very dark
    colors[ImGuiCol_MenuBarBg] = UIConfig::ColorMenuBarBg;

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = UIConfig::ColorScrollbarBg;
    colors[ImGuiCol_ScrollbarGrab] = UIConfig::ColorScrollbarGrab;
    colors[ImGuiCol_ScrollbarGrabHovered] = UIConfig::ColorScrollbarGrabHovered;
    colors[ImGuiCol_ScrollbarGrabActive] = UIConfig::ColorScrollbarGrabActive;

    // Sliders & Checks - bright cyan
    colors[ImGuiCol_CheckMark] = UIConfig::ColorCheckMark;
    colors[ImGuiCol_SliderGrab] = UIConfig::ColorSliderGrab;
    colors[ImGuiCol_SliderGrabActive] = UIConfig::ColorSliderGrabActive;

    // Buttons - dark with cyan accents
    colors[ImGuiCol_Button] = UIConfig::ColorButton;
    colors[ImGuiCol_ButtonHovered] = UIConfig::ColorButtonHovered;
    colors[ImGuiCol_ButtonActive] = UIConfig::ColorButtonActive;

    // Headers (Collapsing Headers, Tree Nodes)
    colors[ImGuiCol_Header] = UIConfig::ColorHeader;
    colors[ImGuiCol_HeaderHovered] = UIConfig::ColorHeaderHovered;
    colors[ImGuiCol_HeaderActive] = UIConfig::ColorHeaderActive;

    // Separators - cyan
    colors[ImGuiCol_Separator] = UIConfig::ColorSeparator;
    colors[ImGuiCol_SeparatorHovered] = UIConfig::ColorSeparatorHovered;
    colors[ImGuiCol_SeparatorActive] = UIConfig::ColorSeparatorActive;

    // Resize Grip - cyan
    colors[ImGuiCol_ResizeGrip] = UIConfig::ColorResizeGrip;
    colors[ImGuiCol_ResizeGripHovered] = UIConfig::ColorResizeGripHovered;
    colors[ImGuiCol_ResizeGripActive] = UIConfig::ColorResizeGripActive;

    // Tabs - dark with cyan active state
    colors[ImGuiCol_Tab] = UIConfig::ColorTab;
    colors[ImGuiCol_TabHovered] = UIConfig::ColorTabHovered;
    colors[ImGuiCol_TabActive] = UIConfig::ColorTabActive;
    colors[ImGuiCol_TabUnfocused] = UIConfig::ColorTabUnfocused;
    colors[ImGuiCol_TabUnfocusedActive] = UIConfig::ColorTabUnfocusedActive;

    // Plots - cyan theme
    colors[ImGuiCol_PlotLines] = UIConfig::ColorPlotLines;
    colors[ImGuiCol_PlotLinesHovered] = UIConfig::ColorPlotLinesHovered;
    colors[ImGuiCol_PlotHistogram] = UIConfig::ColorPlotHistogram;
    colors[ImGuiCol_PlotHistogramHovered] = UIConfig::ColorPlotHistogramHovered;

    colors[ImGuiCol_TextSelectedBg] = UIConfig::ColorTextSelectedBg;
    colors[ImGuiCol_DragDropTarget] = UIConfig::ColorDragDropTarget;
    colors[ImGuiCol_NavHighlight] = UIConfig::ColorNavHighlight;
    colors[ImGuiCol_NavWindowingHighlight] = UIConfig::ColorNavWindowingHighlight;
    colors[ImGuiCol_NavWindowingDimBg] = UIConfig::ColorNavWindowingDimBg;
    colors[ImGuiCol_ModalWindowDimBg] = UIConfig::ColorModalWindowDimBg;
}

void UISystem::PushNumericFont() {
    if (m_fontErbosDracoNumbers) {
        ImGui::PushFont(m_fontErbosDracoNumbers);
    }
}

void UISystem::PopNumericFont() {
    if (m_fontErbosDracoNumbers) {
        ImGui::PopFont();
    }
}

float UISystem::GetNumericFieldMinWidth() const {
    ImGuiStyle& style = ImGui::GetStyle();
    float width = 0.0f;
    if (m_fontErbosDracoNumbers) {
        ImGui::PushFont(m_fontErbosDracoNumbers);
    }
    width = ImGui::CalcTextSize("000.0").x + style.FramePadding.x * 2.0f;
    if (m_fontErbosDracoNumbers) {
        ImGui::PopFont();
    }
    return width;
}

void UISystem::SetNextNumericFieldWidth(float requestedWidth) {
    const float minWidth = GetNumericFieldMinWidth();
    const float width = (requestedWidth > 0.0f) ? (std::max)(requestedWidth, minWidth) : minWidth;
    ImGui::SetNextItemWidth(width);
}

bool UISystem::IsPointInTitlebarButtons(POINT screenPt) const {
    return screenPt.x >= m_titlebarButtonsMin.x && screenPt.x <= m_titlebarButtonsMax.x &&
           screenPt.y >= m_titlebarButtonsMin.y && screenPt.y <= m_titlebarButtonsMax.y;
}

bool UISystem::IsPointInTitlebarDrag(POINT screenPt) const {
    if (m_titlebarDragMin.x > m_titlebarDragMax.x || m_titlebarDragMin.y > m_titlebarDragMax.y) {
        return false;
    }
    return screenPt.x >= m_titlebarDragMin.x && screenPt.x <= m_titlebarDragMax.x &&
           screenPt.y >= m_titlebarDragMin.y && screenPt.y <= m_titlebarDragMax.y;
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
    ImVec2 titlebarPad(UIConfig::TitlebarPadX, UIConfig::TitlebarPadY);
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + titlebarPad.x, viewport->Pos.y + titlebarPad.y));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - titlebarPad.x * 2.0f, viewport->Size.y - titlebarPad.y * 2.0f));
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

    m_titlebarHeight = ImGui::GetCursorPosY();

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

    if (m_showBuildSettings) {
        ShowBuildSettingsWindow();
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(UIConfig::MenuFramePadX, UIConfig::MenuFramePadY));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(UIConfig::MenuItemSpacingX, UIConfig::MenuItemSpacingY));

        const float startY = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(startY + UIConfig::MenuTopPad);

        ImGui::Dummy(ImVec2(UIConfig::MenuLeftPad, 0.0f));
        ImGui::SameLine(0.0f, 0.0f);

        m_titlebarDragMin = ImVec2(FLT_MAX, FLT_MAX);
        m_titlebarDragMax = ImVec2(-FLT_MAX, -FLT_MAX);

        if (m_fontMenuSmall) {
            ImGui::PushFont(m_fontMenuSmall);
        }

        float menuMaxX = ImGui::GetCursorScreenPos().x;
        
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
            if (ImGui::MenuItem("Build Settings...")) {
                m_showBuildSettings = true;
                m_buildSettingsRefreshRequested = true;
            }
            if (ImGui::MenuItem("Export Runtime Package...")) {
                ExportRuntimePackage();
            }
            ImGui::Separator();
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {}
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
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
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
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
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                m_showAbout = true;
            }
            ImGui::EndMenu();
        }
        menuMaxX = (std::max)(menuMaxX, ImGui::GetItemRectMax().x);

        float buttonSize = ImGui::GetFrameHeight();
        float totalButtonsWidth = buttonSize * 2.0f + ImGui::GetStyle().ItemSpacing.x;
        float cursorX = ImGui::GetCursorPosX();
        float avail = ImGui::GetContentRegionAvail().x - UIConfig::MenuRightPad;
        if (avail > totalButtonsWidth) {
            ImGui::SetCursorPosX(cursorX + (avail - totalButtonsWidth));
        }

        const bool canUseWindow = m_hwnd != nullptr;
        m_titlebarButtonsMin = ImVec2(FLT_MAX, FLT_MAX);
        m_titlebarButtonsMax = ImVec2(-FLT_MAX, -FLT_MAX);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIConfig::ColorFrameBgHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIConfig::ColorFrameBgActive);
        if (IconButton("TitlebarMinimize", OpenFontIcons::kChevronDown, "Minimize", ImVec2(buttonSize, buttonSize)) && canUseWindow) {
            SendMessage(m_hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        }
        ImGui::PopStyleColor(2);
        {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            m_titlebarButtonsMin.x = (std::min)(m_titlebarButtonsMin.x, min.x);
            m_titlebarButtonsMin.y = (std::min)(m_titlebarButtonsMin.y, min.y);
            m_titlebarButtonsMax.x = (std::max)(m_titlebarButtonsMax.x, max.x);
            m_titlebarButtonsMax.y = (std::max)(m_titlebarButtonsMax.y, max.y);
        }
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UIConfig::ColorHeaderHovered);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, UIConfig::ColorHeaderActive);
        if (IconButton("TitlebarClose", OpenFontIcons::kXCircle, "Close", ImVec2(buttonSize, buttonSize)) && canUseWindow) {
            SendMessage(m_hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
        }
        ImGui::PopStyleColor(2);
        {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            m_titlebarButtonsMin.x = (std::min)(m_titlebarButtonsMin.x, min.x);
            m_titlebarButtonsMin.y = (std::min)(m_titlebarButtonsMin.y, min.y);
            m_titlebarButtonsMax.x = (std::max)(m_titlebarButtonsMax.x, max.x);
            m_titlebarButtonsMax.y = (std::max)(m_titlebarButtonsMax.y, max.y);
        }
        if (m_fontMenuSmall) {
            ImGui::PopFont();
        }

        const float endY = ImGui::GetCursorPosY();
        float minHeight = 0.0f;
        const float autoHeight = (std::max)(endY - startY + UIConfig::MenuBottomPad, minHeight);
        const float targetHeight = UIConfig::MenuBarHeight > 0.0f
            ? (std::max)(UIConfig::MenuBarHeight, minHeight)
            : autoHeight;
        const float extra = targetHeight - (endY - startY);
        if (extra > 0.0f) {
            ImGui::Dummy(ImVec2(0.0f, extra));
        }

        ImVec2 barMin = ImGui::GetWindowPos();
        barMin.y += startY;
        ImVec2 barMax = ImVec2(barMin.x + ImGui::GetWindowWidth(), barMin.y + targetHeight);
        float dragMinX = menuMaxX + UIConfig::MenuItemSpacingX;
        float dragMaxX = m_titlebarButtonsMin.x - UIConfig::MenuItemSpacingX;
        if (dragMaxX > dragMinX) {
            m_titlebarDragMin = ImVec2(dragMinX, barMin.y);
            m_titlebarDragMax = ImVec2(dragMaxX, barMax.y);
        } else {
            m_titlebarDragMin = ImVec2(FLT_MAX, FLT_MAX);
            m_titlebarDragMax = ImVec2(-FLT_MAX, -FLT_MAX);
        }

        ImGui::PopStyleVar(2);
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
        scene.compiledShaderBytes = m_previewRenderer->GetLastCompiledPixelShaderSize();
        scene.isDirty = false;
    } else {
        scene.compiledShaderBytes = 0;
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

void UISystem::LoadGlobalSnippets() {
    m_snippetFolders.clear();
    m_selectedSnippetFolderIndex = -1;
    m_selectedSnippetIndex = -1;
    m_nextSnippetId = 1;

    const fs::path snippetsDir = GetGlobalSnippetDirectory(m_appRoot);
    m_snippetsDirectoryPath = snippetsDir.string();

    std::error_code ec;
    fs::create_directories(snippetsDir, ec);

    const auto loadFromFile = [this](const fs::path& filePath, const std::string& fallbackFolderName) {
        std::ifstream in(filePath);
        if (!in.is_open()) {
            return;
        }

        json root;
        try {
            in >> root;
        } catch (...) {
            return;
        }

        if (!root.contains("snippets") || !root["snippets"].is_array()) {
            return;
        }

        ShaderSnippetFolder folder;
        folder.name = root.value("folder", fallbackFolderName);
        folder.filePath = filePath.string();

        for (const auto& item : root["snippets"]) {
            if (!item.is_object()) {
                continue;
            }

            ShaderSnippet snippet;
            snippet.name = item.value("name", std::string{});
            snippet.code = item.value("code", std::string{});

            if (snippet.name.empty() || snippet.code.empty()) {
                continue;
            }

            folder.snippets.push_back(std::move(snippet));
            m_nextSnippetId = (std::max)(m_nextSnippetId, static_cast<int>(folder.snippets.size()) + 1);
        }

        m_snippetFolders.push_back(std::move(folder));
    };

    if (fs::exists(snippetsDir)) {
        std::vector<fs::path> jsonFiles;
        for (const auto& entry : fs::directory_iterator(snippetsDir, ec)) {
            if (ec) {
                break;
            }

            if (!entry.is_regular_file()) {
                continue;
            }

            if (entry.path().extension() == ".json") {
                jsonFiles.push_back(entry.path());
            }
        }

        std::sort(jsonFiles.begin(), jsonFiles.end());
        for (const auto& path : jsonFiles) {
            loadFromFile(path, path.stem().string());
        }
    }

    const fs::path legacyPath = GetGlobalSnippetBaseDir(m_appRoot) / "snippets.json";
    if (m_snippetFolders.empty() && fs::exists(legacyPath)) {
        loadFromFile(legacyPath, "General");
    }

    if (m_snippetFolders.empty()) {
        ShaderSnippetFolder folder;
        folder.name = "General";
        folder.filePath = (snippetsDir / "General.json").string();
        m_snippetFolders.push_back(std::move(folder));
    }

    m_selectedSnippetFolderIndex = 0;
    if (!m_snippetFolders[0].snippets.empty()) {
        m_selectedSnippetIndex = 0;
    }
}

void UISystem::SaveGlobalSnippets() const {
    if (m_snippetsDirectoryPath.empty()) {
        return;
    }

    const fs::path snippetsDir(m_snippetsDirectoryPath);
    std::error_code ec;
    fs::create_directories(snippetsDir, ec);

    for (const auto& folder : m_snippetFolders) {
        fs::path filePath = folder.filePath.empty()
            ? (snippetsDir / (SanitizeSnippetFileStem(folder.name) + ".json"))
            : fs::path(folder.filePath);

        json root;
        root["version"] = 1;
        root["folder"] = folder.name;
        root["snippets"] = json::array();

        for (const auto& snippet : folder.snippets) {
            if (snippet.name.empty() || snippet.code.empty()) {
                continue;
            }

            root["snippets"].push_back({
                {"name", snippet.name},
                {"code", snippet.code}
            });
        }

        std::ofstream out(filePath);
        if (!out.is_open()) {
            continue;
        }
        out << root.dump(2);
    }
}

void UISystem::InsertSnippetIntoEditor(const std::string& snippetCode) {
    if (snippetCode.empty()) {
        return;
    }

    std::string insertText = snippetCode;
    if (!insertText.empty() && insertText.back() != '\n') {
        insertText.push_back('\n');
    }

    m_textEditor.InsertText(insertText);
    m_shaderState.text = m_textEditor.GetText();

    if (m_currentMode == UIMode::PostFX) {
        if (m_postFxSelectedIndex >= 0 && m_postFxSelectedIndex < (int)m_postFxDraftChain.size()) {
            auto& effect = m_postFxDraftChain[m_postFxSelectedIndex];
            effect.shaderCode = m_shaderState.text;
            effect.isDirty = true;
        }
    } else {
        if (m_activeSceneIndex >= 0 && m_activeSceneIndex < (int)m_scenes.size()) {
            m_scenes[m_activeSceneIndex].shaderCode = m_shaderState.text;
            m_scenes[m_activeSceneIndex].isDirty = true;
        }
    }

    m_shaderState.status = CompileStatus::Dirty;
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

void UISystem::ShowBuildSettingsWindow() {
    if (m_buildSettingsRefreshRequested) {
        m_buildSettingsPrereq = BuildPipeline::CheckPrereqs(m_appRoot, m_buildSettingsMode);
        m_buildSettingsRefreshRequested = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 windowSize(760.0f, 620.0f);
    ImGui::SetNextWindowPos(ImVec2(
        viewport->Pos.x + (viewport->Size.x - windowSize.x) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - windowSize.y) * 0.5f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);

    if (!ImGui::Begin("Build Settings", &m_showBuildSettings, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool buildModeChanged = false;
    int modeIndex = (m_buildSettingsMode == BuildMode::ReleaseCrinkled) ? 1 : 0;
    const char* modeLabels[] = { "Release (standard)", "Release Crinkled (smallest, requires Crinkler + Ninja)" };
    if (ImGui::Combo("Build Mode", &modeIndex, modeLabels, 2)) {
        m_buildSettingsMode = modeIndex == 1 ? BuildMode::ReleaseCrinkled : BuildMode::Release;
        m_buildSettingsRefreshRequested = true;
        buildModeChanged = true;
        m_buildSettingsAutoSwitchedToCrinkled = false;
    }

    ImGui::SeparatorText("Budget Target");
    static const SizeTargetPreset presets[] = {
        SizeTargetPreset::None,
        SizeTargetPreset::K1,
        SizeTargetPreset::K2,
        SizeTargetPreset::K4,
        SizeTargetPreset::K16,
        SizeTargetPreset::K32,
        SizeTargetPreset::K64
    };
    bool sizePresetChanged = false;
    for (SizeTargetPreset preset : presets) {
        ImGui::PushID((int)preset);
        const bool selected = (m_buildSettingsSizeTarget == preset);
        const std::string label = std::string(SizePresetLabel(preset)) +
            (preset == SizeTargetPreset::None ? " (No target)" : " (" + std::to_string(SizePresetBytes(preset)) + " bytes)");
        if (ImGui::RadioButton(label.c_str(), selected)) {
            m_buildSettingsSizeTarget = preset;
            sizePresetChanged = true;
            m_buildSettingsAutoSwitchedToCrinkled = false;
        }
        ImGui::PopID();
        if (preset == SizeTargetPreset::K4) {
            ImGui::SameLine();
        }
    }

    const bool tinyTargetSelected = (m_buildSettingsSizeTarget != SizeTargetPreset::None);
    bool autoSwitchedToCrinkled = false;
    const bool canAutoSwitchToCrinkled =
        tinyTargetSelected &&
        m_buildSettingsMode == BuildMode::Release &&
        m_buildSettingsPrereq.hasCrinkler &&
        m_buildSettingsPrereq.hasNinja;
    if (canAutoSwitchToCrinkled && (sizePresetChanged || buildModeChanged)) {
        m_buildSettingsMode = BuildMode::ReleaseCrinkled;
        m_buildSettingsRefreshRequested = true;
        autoSwitchedToCrinkled = true;
        m_buildSettingsAutoSwitchedToCrinkled = true;
    }

    if (autoSwitchedToCrinkled || m_buildSettingsAutoSwitchedToCrinkled) {
        ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.4f, 1.0f), "Auto-switched to Release Crinkled for tiny target.");
    }

    if (m_buildSettingsMode == BuildMode::Release && m_buildSettingsSizeTarget != SizeTargetPreset::None) {
        ImGui::TextDisabled("Tiny targets (1K-64K) use MicroPlayer on x86 by default. Open demos (no size target) use full runtime on x64.");
    }

    ImGui::Checkbox("Restricted compact track mode", &m_buildSettingsRestrictedCompactTrack);
    ImGui::TextDisabled("Stores track data as assets/track.bin and strips verbose JSON fields before packing.");
    ImGui::Checkbox("Include runtime debug logs", &m_buildSettingsRuntimeDebugLog);
    ImGui::TextDisabled("Compiles runtime log strings and debug output paths (adds bytes to final executable).");
    ImGui::Checkbox("Include compact-track debug logs", &m_buildSettingsCompactTrackDebugLog);
    ImGui::TextDisabled("Compiles compact timeline decode diagnostics (adds bytes to final executable).");

    if (m_buildSettingsRuntimeDebugLog && m_buildSettingsSizeTarget != SizeTargetPreset::None) {
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.25f, 1.0f), "Warning: Runtime debug logs increase binary size and can hurt tiny targets.");
    }
    if (m_buildSettingsCompactTrackDebugLog && m_buildSettingsSizeTarget != SizeTargetPreset::None) {
        ImGui::TextColored(ImVec4(0.95f, 0.70f, 0.25f, 1.0f), "Warning: Compact-track debug logs increase binary size and can hurt tiny targets.");
    }

    ImGui::SeparatorText("Dependencies");
    if (IconButton("RefreshDeps", OpenFontIcons::kRefresh, "Refresh dependency detection", ImVec2(120, 0))) {
        m_buildSettingsRefreshRequested = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Mode: %s", BuildModeLabel(m_buildSettingsMode));

    const bool crinklerPossible =
        (m_buildSettingsMode == BuildMode::ReleaseCrinkled) &&
        m_buildSettingsPrereq.hasCrinkler &&
        m_buildSettingsPrereq.hasNinja;
    const char* activeLinker = (m_buildSettingsMode == BuildMode::Release)
        ? "MSVC link.exe"
        : (crinklerPossible ? "Crinkler" : "Unavailable (missing Crinkler or Ninja)");
    ImGui::Text("Active linker: %s", activeLinker);

    struct DepRow {
        const char* name;
        bool present;
        bool required;
        const char* configureLabel;
        std::string configureTarget;
    };

    const bool crinklerRequired = (m_buildSettingsMode == BuildMode::ReleaseCrinkled);
    const bool ninjaRequired = (m_buildSettingsMode == BuildMode::ReleaseCrinkled);
    const std::vector<DepRow> deps = {
        { "Visual Studio C++ Build Tools", m_buildSettingsPrereq.hasVisualStudioTools, true, "Install", "https://visualstudio.microsoft.com/downloads/" },
        { "Windows SDK", m_buildSettingsPrereq.hasWindowsSdk, true, "Install", "https://developer.microsoft.com/windows/downloads/windows-sdk/" },
        { "CMake", m_buildSettingsPrereq.hasCMake, true, "Install", "https://cmake.org/download/" },
        { "DXC Runtime (dxcompiler.dll)", m_buildSettingsPrereq.hasDxcRuntime, true, "Download", "https://github.com/microsoft/DirectXShaderCompiler/releases" },
        { "Crinkler", m_buildSettingsPrereq.hasCrinkler, crinklerRequired, "Setup Guide", m_appRoot + "\\README-CRINKLER.txt" },
        { "Ninja", m_buildSettingsPrereq.hasNinja, ninjaRequired, "Install", "https://github.com/ninja-build/ninja/releases" }
    };

    if (ImGui::BeginTable("BuildDeps", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("Dependency");
        ImGui::TableSetupColumn("Required");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("Configure");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < deps.size(); ++i) {
            const auto& dep = deps[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(dep.name);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(dep.required ? "Yes" : "Optional");

            ImGui::TableSetColumnIndex(2);
            if (dep.present) {
                ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.4f, 1.0f), "Detected");
            } else if (dep.required) {
                ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Missing");
            } else {
                ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.25f, 1.0f), "Missing (Optional)");
            }

            ImGui::TableSetColumnIndex(3);
            ImGui::PushID((int)i + 1000);
            if (!dep.present && IconButton("Cfg", OpenFontIcons::kFolder, dep.configureLabel, ImVec2(96.0f, 0.0f))) {
                OpenExternal(dep.configureTarget);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!m_buildSettingsPrereq.hasCrinkler) {
        if (IconButton("PickCrinklerExe", OpenFontIcons::kFolder, "Select crinkler.exe and set SHADERLAB_CRINKLER", ImVec2(260, 0))) {
            char pathBuf[512] = { 0 };
            OPENFILENAMEA ofn = { 0 };
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = (HWND)ImGui::GetMainViewport()->PlatformHandleRaw;
            ofn.lpstrFile = pathBuf;
            ofn.nMaxFile = sizeof(pathBuf);
            ofn.lpstrFilter = "Crinkler (crinkler.exe)\0crinkler.exe\0Executables (*.exe)\0*.exe\0All Files\0*.*\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) {
                SetEnvironmentVariableA("SHADERLAB_CRINKLER", pathBuf);
                m_buildSettingsRefreshRequested = true;
            }
        }
    }

    if (!m_buildSettingsPrereq.hasNinja) {
        ImGui::SameLine();
        if (IconButton("CopyNinjaWinget", OpenFontIcons::kCopy, "Copy Ninja install command", ImVec2(230, 0))) {
            ImGui::SetClipboardText("winget install Ninja-build.Ninja");
        }
    }

    if (!m_buildSettingsPrereq.message.empty()) {
        ImGui::Separator();
        ImGui::BeginChild("PrereqMsg", ImVec2(0, 120), true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(m_buildSettingsPrereq.message.c_str());
        ImGui::EndChild();
    }

    ImGui::Separator();
    if (IconButton("CloseBuildSettings", OpenFontIcons::kXCircle, "Close", ImVec2(120, 0))) {
        m_showBuildSettings = false;
    }
    ImGui::SameLine();
    const bool canBuild = m_buildSettingsPrereq.ok;
    if (!canBuild) {
        ImGui::BeginDisabled();
    }
    if (IconButton("BuildFromSettings", OpenFontIcons::kPlay, "Choose output path and build", ImVec2(220, 0))) {
        if (m_currentMode == UIMode::PostFX && m_postFxSourceSceneIndex >= 0 && m_postFxSourceSceneIndex < (int)m_scenes.size()) {
            m_scenes[m_postFxSourceSceneIndex].postFxChain = m_postFxDraftChain;
        }

        if (m_currentProjectPath.empty()) {
            SaveProjectAs();
            if (m_currentProjectPath.empty()) {
                if (!canBuild) {
                    ImGui::EndDisabled();
                }
                ImGui::End();
                return;
            }
        } else {
            SaveProject();
        }

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
            m_isBuilding = true;
            m_buildComplete = false;
            m_buildSuccess = false;
            m_buildLog = "Initializing Build Process...\n";
            m_buildLog += std::string("Build Mode: ") + BuildModeLabel(m_buildSettingsMode) + "\n";
            m_buildLog += std::string("Size Target: ") + SizePresetLabel(m_buildSettingsSizeTarget) + "\n";
            m_buildLog += std::string("Restricted Compact Track: ") + (m_buildSettingsRestrictedCompactTrack ? "Enabled" : "Disabled") + "\n";
            m_buildLog += std::string("Runtime Debug Logs: ") + (m_buildSettingsRuntimeDebugLog ? "Enabled" : "Disabled") + "\n";
            m_buildLog += std::string("Compact Track Debug Logs: ") + (m_buildSettingsCompactTrackDebugLog ? "Enabled" : "Disabled") + "\n";
            if (m_buildSettingsAutoSwitchedToCrinkled) {
                m_buildLog += "Build Mode Auto-Switch: Release -> Release Crinkled (tiny target with Crinkler+Ninja detected)\n";
                m_buildSettingsAutoSwitchedToCrinkled = false;
            }

            const std::string targetExePath = szFile;
            const std::string projectPath = m_currentProjectPath;
            const std::string appRoot = m_appRoot;
            const BuildMode selectedMode = m_buildSettingsMode;
            const SizeTargetPreset selectedSizeTarget = m_buildSettingsSizeTarget;
            const bool selectedRestrictedCompactTrack = m_buildSettingsRestrictedCompactTrack;
            const bool selectedRuntimeDebugLog = m_buildSettingsRuntimeDebugLog;
            const bool selectedCompactTrackDebugLog = m_buildSettingsCompactTrackDebugLog;

            m_buildFuture = std::async(std::launch::async, [this, targetExePath, projectPath, appRoot, selectedMode, selectedSizeTarget, selectedRestrictedCompactTrack, selectedRuntimeDebugLog, selectedCompactTrackDebugLog]() {
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
                request.mode = selectedMode;
                request.sizeTarget = selectedSizeTarget;
                request.restrictedCompactTrack = selectedRestrictedCompactTrack;
                request.runtimeDebugLog = selectedRuntimeDebugLog;
                request.compactTrackDebugLog = selectedCompactTrackDebugLog;

                BuildResult result = BuildPipeline::BuildSelfContained(request, Log);
                m_buildSuccess = result.success;
                m_buildComplete = true;
            });
        }
    }
    if (!canBuild) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Resolve required dependencies to enable build.");
    }

    ImGui::End();
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

            if (IconButton("CopyBuildLog", OpenFontIcons::kCopy, "Copy log", ImVec2(120, 0))) {
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
                if (IconButton("CloseBuildStatus", OpenFontIcons::kXCircle, "Close", ImVec2(120, 0))) {
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

} // namespace ShaderLab
