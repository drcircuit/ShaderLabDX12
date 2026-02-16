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
#include "TextEditor.h"
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
    UIMode currentMode;
    ShaderEditState shaderState;
    int activeSceneIndex;
};

struct ShaderSnippet {
    std::string name;
    std::string code;
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
    bool IsPointInTitlebarButtons(POINT screenPt) const;
    bool IsPointInTitlebarDrag(POINT screenPt) const;

private:
    void SetActiveScene(int index);

    void CreateDescriptorHeap(Device* device);
    void CreatePreviewTexture(uint32_t width, uint32_t height);
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
    ImVec2 m_titlebarButtonsMin = ImVec2(0.0f, 0.0f);
    ImVec2 m_titlebarButtonsMax = ImVec2(0.0f, 0.0f);
    ImVec2 m_titlebarDragMin = ImVec2(0.0f, 0.0f);
    ImVec2 m_titlebarDragMax = ImVec2(0.0f, 0.0f);
    
    // Transition Resources
    ComPtr<ID3D12PipelineState> m_transitionPSO;
    ComPtr<ID3D12DescriptorHeap> m_transitionSrvHeap;
    TransitionType m_compiledTransitionType = TransitionType::None;

    // Cycle detection
    std::vector<int> m_renderStack;

    // Callbacks
    std::function<void(int)> m_restartCallback;

    std::string m_currentProjectPath;
    std::string m_appRoot; // Root directory where ShaderLab started (where CMakeLists.txt resides)
    HWND m_hwnd = nullptr;

    // Post FX editor state
    int m_postFxSourceSceneIndex = -1;
    int m_postFxSelectedIndex = -1;
    std::vector<Scene::PostFXEffect> m_postFxDraftChain;

    bool m_previewFullscreen = false;

    // Post FX preview resources (draft)
    ComPtr<ID3D12Resource> m_postFxPreviewTextureA;
    ComPtr<ID3D12Resource> m_postFxPreviewTextureB;
    ComPtr<ID3D12DescriptorHeap> m_postFxPreviewSrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_postFxPreviewRtvHeap;
    uint32_t m_postFxPreviewWidth = 0;
    uint32_t m_postFxPreviewHeight = 0;

    std::vector<std::string> m_demoLog;
    bool m_demoLogAutoScroll = true;

    void SaveProject();
    void SaveProjectAs();
    void OpenProject();
    void BuildProject();
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

    std::vector<ShaderSnippet> m_snippets;
    int m_selectedSnippetIndex = -1;
    std::string m_snippetsConfigPath;
    int m_nextSnippetId = 1;
};

} // namespace ShaderLab
