#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace ShaderLab {

enum class TextureType { Texture2D, TextureCube, Texture3D };
enum class BindingType { Scene, File };
enum class AudioType { Music, OneShot };
enum class TransitionType { None, Crossfade, DipToBlack, FadeOut, FadeIn, Glitch, Pixelate };

struct TextureBinding {
    int channelIndex = 0;
    bool enabled = false;
    BindingType bindingType = BindingType::Scene;
    int sourceSceneIndex = -1; 
    std::string filePath;
    ComPtr<ID3D12Resource> textureResource;
    bool fileTextureValid = false;
    TextureType type = TextureType::Texture2D; 
};

struct AudioClip {
    std::string name;
    std::string path;
    AudioType type = AudioType::Music;
    float bpm = 120.0f;
};

struct TrackerRow {
    int rowId = 0; 
    int sceneIndex = -1;
    TransitionType transition = TransitionType::None;
    float transitionDuration = 1.0f; 
    float timeOffset = 0.0f; // Offset in beats to subtract from global time for this scene
    int musicIndex = -1; 
    int oneShotIndex = -1; 
    bool isBeat = true; 
    bool stop = false; 
};

struct DemoTrack {
    std::string name = "Untitled Track";
    float bpm = 120.0f;
    int lengthBeats = 128; 
    std::vector<TrackerRow> rows;
    int currentBeat = 0;
    int lastTriggeredBeat = -1;
};

enum class TransportState { Stopped, Playing, Paused };

struct Transport { // Renamed from PreviewTransport for genera use
    TransportState state = TransportState::Stopped;
    double timeSeconds = 0.0;
    double lastFrameWallSeconds = 0.0;
    bool freezeTime = false;
    bool freezeBeat = false;
    float bpm = 140.0f;
};

struct Scene {
    std::string name;
    std::string shaderCode;
    std::vector<TextureBinding> bindings;
    TextureType outputType = TextureType::Texture2D;

    struct PostFXEffect {
        std::string name;
        std::string shaderCode;
        bool enabled = true;
        bool isDirty = true;
        std::string lastCompiledCode;
        std::string precompiledPath;
        ComPtr<ID3D12PipelineState> pipelineState;
        size_t compiledShaderBytes = 0;
        int historyIndex = 0;
        bool historyInitialized = false;
        std::vector<ComPtr<ID3D12Resource>> historyTextures;

        PostFXEffect() = default;
        PostFXEffect(const std::string& inName, const std::string& code)
            : name(inName), shaderCode(code) {}
    };

    std::vector<PostFXEffect> postFxChain;
    
    // Runtime resources
    ComPtr<ID3D12Resource> texture;
    ComPtr<ID3D12DescriptorHeap> srvHeap;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12PipelineState> pipelineState;
    size_t compiledShaderBytes = 0;
    bool textureValid = false;
    bool isDirty = true;

    // Post FX runtime resources
    ComPtr<ID3D12Resource> postFxTextureA;
    ComPtr<ID3D12Resource> postFxTextureB;
    ComPtr<ID3D12DescriptorHeap> postFxSrvHeap;
    ComPtr<ID3D12DescriptorHeap> postFxRtvHeap;
    bool postFxValid = false;
    
    // Optional Precompiled Data
    std::string precompiledPath; 

    Scene() {}
    Scene(const std::string& n, const std::string& code) : name(n), shaderCode(code) {}
};

struct ProjectData {
    std::vector<Scene> scenes;
    std::vector<AudioClip> audioLibrary;
    DemoTrack track;
    Transport transport;
};

}
