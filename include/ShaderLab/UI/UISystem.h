#pragma once

#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <functional>
#include <future>
#include <atomic>
#include <mutex>
#include <fstream>
#include <cstddef>
#include <unordered_map>
#include "TextEditor.h"
#include "ShaderLab/DevKit/BuildPipeline.h"
#include "ShaderLab/Core/ShaderLabData.h"

using Microsoft::WRL::ComPtr;

struct ImGuiContext;
struct ImFont;

namespace ShaderLab {

class Device;
class Swapchain;
class PreviewRenderer;
class AudioSystem;

enum class UIMode { Demo, Scene, PostFX };

enum class AspectRatio { Ratio_16_9, Ratio_16_10, Ratio_4_3 };

using PreviewTransport = Transport;

enum class CompileStatus { Clean, Dirty, Compiling, Success, Error };

struct Diagnostic {
    int line = 0;
    int column = 0;
    std::string message;
};

enum class EditorTheme { Dark, DarkOriginal, Light, RetroBlue };
enum class CodeFontSize { XS, S, M, L, XL };

struct ShaderEditState {
    CompileStatus status = CompileStatus::Clean;
    std::string activeFilePath;
    std::string text;
    std::string lastCompiledText;
    std::vector<Diagnostic> diagnostics;
    
    // Search/Replace
    char searchBuffer[256] = "";
    char replaceBuffer[256] = "";
    bool showSearchReplace = false;
    int currentSearchIndex = -1;
    int totalSearchMatches = 0;
    
    // Theme
    EditorTheme theme = EditorTheme::Dark;
    CodeFontSize codeFontSize = CodeFontSize::M;
    
    // Performance
    bool showPerformanceOverlay = false;
    float frameTimeMs = 0.0f;
    float fps = 0.0f;
};

struct ProjectState {
    std::vector<Scene> scenes;
    std::vector<AudioClip> audioLibrary;
    DemoTrack track;
    PreviewTransport transport;
    std::string demoTitle;
    std::string demoAuthor;
    std::string demoDescription;
    UIMode currentMode;
    ShaderEditState shaderState;
    int activeSceneIndex;
};

struct ShaderSnippet {
    std::string name;
    std::string code;
};

struct ShaderSnippetFolder {
    std::string name;
    std::string filePath;
    std::vector<ShaderSnippet> snippets;
};

struct UIThemeColors {
    ImVec4 LinesAccentColorDim = ImVec4(0.00f, 0.40f, 0.45f, 0.60f);
    ImVec4 ControlBackground = ImVec4(0.05f, 0.08f, 0.10f, 1.00f);
    ImVec4 ControlFontColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 IconColor = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    ImVec4 ButtonIconColor = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
    ImVec4 ButtonLabelColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 ButtonBackgroundColor = ImVec4(0.08f, 0.12f, 0.15f, 1.00f);
    ImVec4 PanelBackground = ImVec4(0.04f, 0.06f, 0.08f, 0.95f);
    ImVec4 WindowBackground = ImVec4(0.02f, 0.03f, 0.04f, 1.00f);
    ImVec4 TrackerHeadingBackground = ImVec4(0.08f, 0.12f, 0.15f, 1.00f);
    ImVec4 TrackerHeadingFontColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 TrackerAccentBeatBackground = ImVec4(0.00f, 0.59f, 1.00f, 0.47f);
    ImVec4 TrackerAccentBeatFontColor = ImVec4(1.00f, 0.80f, 0.20f, 1.00f);
    ImVec4 TrackerBeatBackground = ImVec4(0.00f, 0.00f, 0.00f, 0.31f);
    ImVec4 TrackerBeatFontColor = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    ImVec4 LabelFontColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 PanelTitleFontColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 PanelTitleBackground = ImVec4(0.02f, 0.03f, 0.04f, 1.00f);
    ImVec4 ActiveTabFontColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 ActiveTabBackground = ImVec4(0.00f, 0.60f, 0.65f, 1.00f);
    ImVec4 PassiveTabFontColor = ImVec4(0.55f, 0.65f, 0.65f, 1.00f);
    ImVec4 PassiveTabBackground = ImVec4(0.06f, 0.08f, 0.10f, 1.00f);
    ImVec4 ActivePanelTitleColor = ImVec4(0.00f, 0.50f, 0.55f, 1.00f);
    ImVec4 ActivePanelBackground = ImVec4(0.04f, 0.06f, 0.08f, 0.95f);
    ImVec4 PassivePanelTitleColor = ImVec4(0.02f, 0.03f, 0.04f, 1.00f);
    ImVec4 PassivePanelBackground = ImVec4(0.04f, 0.06f, 0.08f, 0.95f);
    ImVec4 LogoFontColor = ImVec4(0.00f, 0.90f, 0.90f, 1.00f);
    ImVec4 ConsoleFontColor = ImVec4(0.85f, 0.95f, 0.95f, 1.00f);
    ImVec4 ConsoleBackground = ImVec4(0.02f, 0.03f, 0.04f, 1.00f);
    ImVec4 StatusFontColor = ImVec4(0.40f, 0.45f, 0.45f, 1.00f);
    float ControlOpacity = 1.0f;
    float PanelOpacity = 0.92f;
    float PanelHeadingOpacity = 0.96f;
    std::string BackgroundImage;
};

struct NamedUITheme {
    std::string name;
    UIThemeColors colors;
};

class UISystem {
public:
    UISystem();
    ~UISystem();

    bool Initialize(HWND hwnd, Device* device, Swapchain* swapchain);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Render(ID3D12GraphicsCommandList* commandList);
    
    void UpdateTransport(double wallNowSeconds, float dtSeconds);
    
    void SetPreviewRenderer(PreviewRenderer* renderer) { m_previewRenderer = renderer; }
    
    PreviewTransport& GetTransport() { return m_transport; }
    const PreviewTransport& GetTransport() const { return m_transport; }
    
    ShaderEditState& GetShaderState() { return m_shaderState; }
    const ShaderEditState& GetShaderState() const { return m_shaderState; }

    void SetRestartCallback(std::function<void(int)> callback) { m_restartCallback = callback; }
    void SetAudioSystem(AudioSystem* audio) { m_audioSystem = audio; }
    
    ProjectState CaptureState();
    void RestoreState(const ProjectState& state);
    
    DemoTrack& GetActiveDemoTrack() { 
        return m_track; 
    }

    std::string GetProjectName() const;
    float GetTitlebarHeight() const { return m_titlebarHeight; }
    bool IsPreviewVsyncEnabled() const { return m_previewVsyncEnabled; }

private:
    void SetActiveScene(int index);

    void CreateDescriptorHeap(Device* device);
    void CreatePreviewTexture(uint32_t width, uint32_t height);
    void CreateTitlebarIconTexture();
    void CreateDummyTexture();
    void EnsureSceneTexture(int sceneIndex, uint32_t width, uint32_t height);
    void RenderScene(ID3D12GraphicsCommandList* commandList, int sceneIndex, uint32_t width, uint32_t height, double time);
    bool RenderPreviewTexture(ID3D12GraphicsCommandList* commandList);
    void EnsurePostFxResources(Scene& scene, uint32_t width, uint32_t height);
    void EnsurePostFxPreviewResources(uint32_t width, uint32_t height);
    void EnsurePostFxHistory(Scene::PostFXEffect& effect, uint32_t width, uint32_t height);
    bool CompilePostFxEffect(Scene::PostFXEffect& effect, std::vector<std::string>& outErrors);
    ID3D12Resource* ApplyPostFxChain(ID3D12GraphicsCommandList* commandList,
                                    Scene& scene,
                                    std::vector<Scene::PostFXEffect>& chain,
                                    ID3D12Resource* inputTexture,
                                    uint32_t width, uint32_t height,
                                    double timeSeconds,
                                    bool usePreviewResources);
    ID3D12Resource* GetSceneFinalTexture(ID3D12GraphicsCommandList* commandList,
                                        int sceneIndex,
                                        uint32_t width, uint32_t height,
                                        double timeSeconds);
    void SetupImGuiStyle();
    void BuildLayout(UIMode mode);
    void ShowTransportControls();
    void ShowMainMenuBar();
    void ShowModeWindows();
    void ShowAboutWindow();
    void ShowFullscreenPreview();
    void ShowShaderEditor();
    void ShowSnippetBin();
    void ShowDiagnostics();
    void ShowSceneList();
    void ShowDemoMetadata();
    void ShowDemoPlaylist(); // Tracker View
    void ShowAudioLibrary();
    void CreateDefaultScene();
    void CreateDefaultTrack();
    bool LoadTextureFromFile(const std::string& path, ComPtr<ID3D12Resource>& outResource);
    void CreateTextureFromData(const void* data, int width, int height, int channels, ComPtr<ID3D12Resource>& outResource);
    bool CompileScene(int sceneIndex);
    void SyncPostFxEditorToSelection();
    void AppendDemoLog(const std::string& message);
    void PushNumericFont();
    void PopNumericFont();
    float GetNumericFieldMinWidth() const;
    void SetNextNumericFieldWidth(float requestedWidth);
    void ShowThemeEditorPopup();
    void ApplyUiTheme();
    void ApplyCodeEditorControlOpacity();
    void LoadUiThemeSettings();
    void SaveUiThemeSettings() const;
    bool AddOrReplaceCustomTheme(const std::string& name, const UIThemeColors& colors);
    void EnsureThemeBackgroundTexture();
    void DrawThemeBackgroundTiled();

    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ImGuiContext* m_context = nullptr;
    bool m_initialized = false;
    
    // Custom fonts for editor
    ImFont* m_fontHackedLogo = nullptr;      // Large logo font
    ImFont* m_fontHackedHeading = nullptr;   // Heading font
    ImFont* m_fontOrbitronText = nullptr;    // Regular UI text
    ImFont* m_fontErbosDracoNumbers = nullptr; // Numerical fields
    ImFont* m_fontMenuSmall = nullptr;       // Menu font
    ImFont* m_fontCode = nullptr;            // Code editor font
    ImFont* m_fontCodeItalic = nullptr;      // Code editor italic font
    ImFont* m_fontCodeSizes[5] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    
    UIMode m_currentMode = UIMode::Demo;
    UIMode m_lastMode = UIMode::Demo;
    bool m_layoutBuilt = false;
    AspectRatio m_aspectRatio = AspectRatio::Ratio_16_9;
    float m_modeChangeFlashSeconds = 0.0f;
    
    PreviewTransport m_transport;
    ShaderEditState m_shaderState;

    // References
    Device* m_deviceRef = nullptr;
    Swapchain* m_swapchainRef = nullptr;
    PreviewRenderer* m_previewRenderer = nullptr;
    AudioSystem* m_audioSystem = nullptr;

    // Preview Texture (Final/Active)
    ComPtr<ID3D12Resource> m_previewTexture;
    ComPtr<ID3D12DescriptorHeap> m_previewRtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_previewRtvHandle{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_previewSrvGpuHandle{};
    ComPtr<ID3D12Resource> m_titlebarIconTexture;
    D3D12_GPU_DESCRIPTOR_HANDLE m_titlebarIconSrvGpuHandle{};
    uint32_t m_previewTextureWidth = 0;
    uint32_t m_previewTextureHeight = 0;
    
    // Dummy texture for unbound slots
    ComPtr<ID3D12Resource> m_dummyTexture;
    ComPtr<ID3D12DescriptorHeap> m_dummySrvHeap;

    ComPtr<ID3D12Resource> m_dummyTextureCube;
    ComPtr<ID3D12DescriptorHeap> m_dummySrvHeapCube;

    ComPtr<ID3D12Resource> m_dummyTexture3D;
    ComPtr<ID3D12DescriptorHeap> m_dummySrvHeap3D;

    // Scene management
    DemoTrack m_track;
    std::vector<AudioClip> m_audioLibrary;
    int m_activeMusicIndex = -1;

    std::vector<Scene> m_scenes;
    int m_activeSceneIndex = 0;
    float m_activeSceneOffset = 0.0f; // Offset in beats relative to scene start
    double m_activeSceneStartBeat = 0.0;
    
    // Editor
    TextEditor m_textEditor;
    TextEditor m_snippetTextEditor;
    
    // Transition State
    bool m_transitionActive = false;
    double m_transitionStartBeat = 0.0;
    double m_transitionDurationBeats = 1.0;
    int m_transitionFromIndex = -1;
    int m_transitionToIndex = -1;
    double m_transitionFromStartBeat = 0.0;
    double m_transitionToStartBeat = 0.0;
    float m_transitionFromOffset = 0.0f;
    float m_transitionToOffset = 0.0f;
    TransitionType m_currentTransitionType = TransitionType::None;
    int m_pendingActiveScene = -2; // -2 = None
    int m_transitionJustCompletedBeat = -1;

    float m_titlebarHeight = 0.0f;
    
    // Transition Resources
    ComPtr<ID3D12PipelineState> m_transitionPSO;
    ComPtr<ID3D12DescriptorHeap> m_transitionSrvHeap;
    TransitionType m_compiledTransitionType = TransitionType::None;

    // Cycle detection
    std::vector<int> m_renderStack;

    // Callbacks
    std::function<void(int)> m_restartCallback;

    std::string m_currentProjectPath;
    std::string m_demoTitle = "Untitled Demo";
    std::string m_demoAuthor;
    std::string m_demoDescription;
    std::string m_appRoot; // Root directory where ShaderLab started (where CMakeLists.txt resides)
    HWND m_hwnd = nullptr;

    // Post FX editor state
    int m_postFxSourceSceneIndex = -1;
    int m_postFxSelectedIndex = -1;
    std::vector<Scene::PostFXEffect> m_postFxDraftChain;

    bool m_previewFullscreen = false;
    bool m_previewVsyncEnabled = true;

    UIThemeColors m_uiThemeColors;
    std::vector<NamedUITheme> m_customThemes;
    std::string m_activeThemeName = "ShaderPunk";
    bool m_showThemeEditor = false;
    char m_themeNameBuffer[128] = {};
    ComPtr<ID3D12Resource> m_themeBackgroundTexture;
    D3D12_GPU_DESCRIPTOR_HANDLE m_themeBackgroundSrvGpuHandle{};
    int m_themeBackgroundWidth = 0;
    int m_themeBackgroundHeight = 0;
    std::string m_loadedThemeBackgroundPath;

    // Post FX preview resources (draft)
    ComPtr<ID3D12Resource> m_postFxPreviewTextureA;
    ComPtr<ID3D12Resource> m_postFxPreviewTextureB;
    ComPtr<ID3D12DescriptorHeap> m_postFxPreviewSrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_postFxPreviewRtvHeap;
    uint32_t m_postFxPreviewWidth = 0;
    uint32_t m_postFxPreviewHeight = 0;

    std::vector<std::string> m_demoLog;
    bool m_demoLogAutoScroll = true;
    bool m_playbackBlockedByCompileError = false;
    bool m_screenKeysOverlayEnabled = false;
    std::vector<std::string> m_screenKeyLog;

    void SaveProject();
    void SaveProjectAs();
    void OpenProject();
    void LoadProjectUiSettings();
    void SaveProjectUiSettings() const;
    void BuildProject();
    void BuildScreenSaverProject();
    void BuildPackagedDemoProject();
    void BuildMicroDemoProject();
    void BuildMicroDeveloperDemoProject();
    void ShowBuildSettingsWindow();
    void RefreshMicroUbershaderConflictCache();
    void ExportRuntimePackage();
    void SeekToBeat(int beat);

    void LoadGlobalSnippets();
    void SaveGlobalSnippets() const;
    void InsertSnippetIntoEditor(const std::string& snippetCode);

    // Auto-Build State
    bool m_isBuilding = false;
    std::string m_buildLog;
    std::mutex m_buildLogMutex;
    std::future<void> m_buildFuture;
    bool m_buildComplete = false;
    bool m_buildSuccess = false;
    bool m_showBuildSettings = false;
    BuildTargetKind m_buildSettingsTargetKind = BuildTargetKind::SelfContainedDemo;
    BuildMode m_buildSettingsMode = BuildMode::Release;
    SizeTargetPreset m_buildSettingsSizeTarget = SizeTargetPreset::K64;
    bool m_buildSettingsRestrictedCompactTrack = false;
    bool m_buildSettingsRuntimeDebugLog = false;
    bool m_buildSettingsCompactTrackDebugLog = false;
    bool m_buildSettingsMicroDeveloperBuild = false;
    std::string m_buildSettingsCleanSolutionRootPath;
    std::string m_buildSettingsCrinklerPath;
    BuildPrereqReport m_buildSettingsPrereq;
    bool m_buildSettingsRefreshRequested = true;
    bool m_buildSettingsAutoSwitchedToCrinkled = false;
    std::vector<MicroUbershaderConflict> m_microUbershaderConflicts;
    std::unordered_map<std::string, std::vector<std::string>> m_microUbershaderKeepEntrypointsBySignature;
    bool m_microUbershaderConflictsDirty = true;
    void UpdateBuildLogic();

    void RenderAboutLogo(ID3D12GraphicsCommandList* commandList);
    void EnsureAboutScene(uint32_t width, uint32_t height);

    bool m_showAbout = false;
    bool m_aboutInitialized = false;
    uint32_t m_aboutTargetWidth = 0;
    uint32_t m_aboutTargetHeight = 0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_aboutSrvGpuHandle{};
    double m_aboutTimeSeconds = 0.0;
    Scene m_aboutScene;

    std::vector<ShaderSnippetFolder> m_snippetFolders;
    int m_selectedSnippetFolderIndex = -1;
    int m_selectedSnippetIndex = -1;
    std::string m_snippetsDirectoryPath;
    int m_nextSnippetId = 1;
    bool m_snippetCodeLocked = true;
    bool m_snippetDraftDirty = false;
    int m_snippetDraftFolderIndex = -1;
    int m_snippetDraftIndex = -1;
    std::string m_snippetDraftCode;

    size_t m_lastDemoCompiledSizeBytes = 0;
    bool m_hasDemoCompiledSize = false;
};

} // namespace ShaderLab
