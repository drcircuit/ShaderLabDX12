#pragma once

#include "ShaderLab/Core/ShaderLabData.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <array>

using Microsoft::WRL::ComPtr;

namespace ShaderLab {

class Device;
class Swapchain;
class PreviewRenderer;
class AudioSystem;
class ShaderCompiler;

class DemoPlayer {
public:
    DemoPlayer();
    ~DemoPlayer();

    bool Initialize(HWND hwnd, Device* device, Swapchain* swapchain, int width, int height);
    void LoadProject(const std::string& manifestPath);
    
    void Update(double wallTime, float dt);
    void Render(ID3D12GraphicsCommandList* commandList, ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle);
    void OnResize(int width, int height);
    void Shutdown();

private:
    void SetActiveScene(int index);
    bool CompileScene(int sceneIndex);
    void EnsureSceneTexture(int sceneIndex);
    void RenderScene(ID3D12GraphicsCommandList* commandList, int sceneIndex, double time);
    std::string GetTransitionShader(TransitionType type);
    void EnsurePostFxResources(Scene& scene);
    void EnsurePostFxHistory(Scene::PostFXEffect& effect);
    bool CompilePostFxEffect(Scene::PostFXEffect& effect);
    ID3D12Resource* ApplyPostFxChain(ID3D12GraphicsCommandList* commandList,
                                    Scene& scene,
                                    ID3D12Resource* inputTexture,
                                    double timeSeconds);
    ID3D12Resource* GetSceneFinalTexture(ID3D12GraphicsCommandList* commandList,
                                        int sceneIndex,
                                        double timeSeconds);
    
    // Core Refs
    Device* m_device = nullptr;
    Swapchain* m_swapchain = nullptr;
    PreviewRenderer* m_renderer = nullptr;
    AudioSystem* m_audio = nullptr;
    ShaderCompiler* m_compiler = nullptr; 
    bool m_compilerReady = false;

    // Data
    ProjectData m_project;
    
    // Loading State
    enum class LoadingStage {
        Idle,
        LoadingManifest,
        LoadingAssets,
        CompilingShaders,
        Ready
    };
    LoadingStage m_loadingStage = LoadingStage::Idle;
    std::string m_loadingStatus;
    int m_compilationIndex = 0;
    std::string m_manifestPath;

    // Runtime State
    int m_activeSceneIndex = -1;
    float m_activeSceneOffset = 0.0f; // Offset in beats relative to scene start
    double m_activeSceneStartBeat = 0.0;
    Transport m_transport; // Runtime transport state
    
    // Render Resources
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    
    ComPtr<ID3D12Resource> m_dummyTexture;
    ComPtr<ID3D12DescriptorHeap> m_dummySrvHeap;
    ComPtr<ID3D12DescriptorHeap> m_imguiSrvHeap; // Dedicated heap for ImGui
    
    // Debug State
    bool m_showDebug = false;
    bool m_altPressed = false;
    double m_lastFrameTime = 0.0;
    
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
    int m_pendingActiveScene = -2;
    int m_transitionJustCompletedBeat = -1;
    
    ComPtr<ID3D12PipelineState> m_transitionPSO;
    ComPtr<ID3D12DescriptorHeap> m_transitionSrvHeap;
    TransitionType m_compiledTransitionType = TransitionType::None;

    std::vector<int> m_renderStack;
    std::vector<uint8_t> m_precompiledVertexShader;
    std::array<std::vector<uint8_t>, 7> m_transitionBytecode;
};

}
