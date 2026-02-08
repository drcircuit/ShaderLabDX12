#include "ShaderLab/App/DemoPlayer.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Graphics/Swapchain.h"
#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Shader/ShaderCompiler.h"
#include "ShaderLab/Audio/AudioSystem.h"
#include "ShaderLab/Core/Serializer.h"
#include "ShaderLab/Core/PackageManager.h" 
#include "stb_image.h"
#include <windows.h>
#include <cmath>
#include <climits>
#include <iostream>
#include <fstream>
#include <filesystem>

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

namespace ShaderLab {

namespace fs = std::filesystem;

static const int kPostFxHistoryCount = 4;
static const int kMaxPostFxChain = 32;
static const char* kPackedVertexShaderPath = "assets/shaders/vertex.cso";

static bool DebugConsoleEnabled() {
    static int cached = -1;
    if (cached == -1) {
        char value[2] = {};
        cached = (GetEnvironmentVariableA("SHADERLAB_DEBUG_CONSOLE", value, sizeof(value)) > 0) ? 1 : 0;
    }
    return cached == 1 && GetConsoleWindow() != nullptr;
}

static void DebugLog(const std::string& message) {
    if (!DebugConsoleEnabled()) return;
    std::cout << message << std::endl;
}

static void DebugLogError(const std::string& message) {
    if (!DebugConsoleEnabled()) return;
    std::cerr << message << std::endl;
}

static const char* TransitionToString(TransitionType type) {
    switch (type) {
        case TransitionType::None: return "None";
        case TransitionType::Crossfade: return "Crossfade";
        case TransitionType::DipToBlack: return "DipToBlack";
        case TransitionType::FadeOut: return "FadeOut";
        case TransitionType::FadeIn: return "FadeIn";
        case TransitionType::Glitch: return "Glitch";
        case TransitionType::Pixelate: return "Pixelate";
        default: return "Unknown";
    }
}

static const char* TextureTypeToString(TextureType type) {
    switch (type) {
        case TextureType::Texture2D: return "Texture2D";
        case TextureType::TextureCube: return "TextureCube";
        case TextureType::Texture3D: return "Texture3D";
        default: return "Unknown";
    }
}

static void DebugLogProjectSummary(const ProjectData& project) {
    if (!DebugConsoleEnabled()) return;

    DebugLog("----- Project Summary -----");
    DebugLog("Scenes: " + std::to_string(project.scenes.size()));
    for (size_t i = 0; i < project.scenes.size(); ++i) {
        const auto& scene = project.scenes[i];
        DebugLog("  [" + std::to_string(i) + "] " + scene.name + " (" + TextureTypeToString(scene.outputType) + ")");
        DebugLog("    Bindings: " + std::to_string(scene.bindings.size()));
        DebugLog("    PostFX: " + std::to_string(scene.postFxChain.size()));
        for (size_t fxIndex = 0; fxIndex < scene.postFxChain.size(); ++fxIndex) {
            const auto& fx = scene.postFxChain[fxIndex];
            std::string line = "      [" + std::to_string(fxIndex) + "] " + fx.name + (fx.enabled ? " (enabled)" : " (disabled)");
            if (!fx.precompiledPath.empty()) {
                line += " precompiled=" + fx.precompiledPath;
            }
            DebugLog(line);
        }
    }

    DebugLog("Track: bpm=" + std::to_string(project.track.bpm) + " len=" + std::to_string(project.track.lengthBeats) + " rows=" + std::to_string(project.track.rows.size()));
    for (const auto& row : project.track.rows) {
        DebugLog("  row=" + std::to_string(row.rowId)
            + " scene=" + std::to_string(row.sceneIndex)
            + " trans=" + TransitionToString(row.transition)
            + " dur=" + std::to_string(row.transitionDuration)
            + " offset=" + std::to_string(row.timeOffset)
            + " music=" + std::to_string(row.musicIndex)
            + " stop=" + std::string(row.stop ? "true" : "false"));
    }
    DebugLog("--------------------------");
}

static int TransitionIndex(TransitionType type) {
    switch (type) {
        case TransitionType::Crossfade: return 0;
        case TransitionType::DipToBlack: return 1;
        case TransitionType::FadeOut: return 2;
        case TransitionType::FadeIn: return 3;
        case TransitionType::Glitch: return 4;
        case TransitionType::Pixelate: return 5;
        default: return 6;
    }
}

static const char* GetTransitionPackedPath(TransitionType type) {
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

static const TrackerRow* FindNextSceneRow(const DemoTrack& track, int afterBeat) {
    const TrackerRow* bestRow = nullptr;
    int bestBeat = INT_MAX;
    for (const auto& row : track.rows) {
        if (row.sceneIndex >= 0 && row.rowId > afterBeat && row.rowId < bestBeat) {
            bestBeat = row.rowId;
            bestRow = &row;
        }
    }
    return bestRow;
}

static int FindNextSceneIndex(const DemoTrack& track, int afterBeat) {
    const TrackerRow* row = FindNextSceneRow(track, afterBeat);
    return row ? row->sceneIndex : -1;
}

static double BeatSeconds(float bpm) {
    if (bpm <= 0.0f) return 0.0;
    return 60.0 / static_cast<double>(bpm);
}

static double SceneTimeSeconds(double exactBeat, double startBeat, float offsetBeats, float bpm) {
    const double beatSeconds = BeatSeconds(bpm);
    if (beatSeconds <= 0.0) return 0.0;
    const double sceneBeats = exactBeat - startBeat + static_cast<double>(offsetBeats);
    return sceneBeats * beatSeconds;
}

static std::string GetTransitionShaderSource(TransitionType type) {
    std::string common = R"(
float4 main(float2 fragCoord, float2 iResolution, float iTime) {
float2 uv = fragCoord / iResolution;
uv.y = 1.0 - uv.y;
float t = saturate(iTime);
float4 colA = iChannel0.Sample(iSampler0, uv);
float4 colB = iChannel1.Sample(iSampler1, uv); 
)";
    switch(type) {
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
          return common + " return colB; }";
    }
}

DemoPlayer::DemoPlayer() {}
DemoPlayer::~DemoPlayer() { Shutdown(); }

void DemoPlayer::Shutdown() {
    // Shutdown ImGui
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    if (m_audio) { m_audio->Shutdown(); delete m_audio; m_audio = nullptr; }
    if (m_renderer) { m_renderer->Shutdown(); delete m_renderer; m_renderer = nullptr; }
    if (m_compiler) { m_compiler->Shutdown(); delete m_compiler; m_compiler = nullptr; }
    // Device/Swapchain/Renderer are owned externally (except Renderer/Compiler now)
}

bool DemoPlayer::Initialize(HWND hwnd, Device* device, Swapchain* swapchain, int width, int height) {
    m_device = device;
    m_swapchain = swapchain;

    PackageManager::Get().Initialize();
    if (PackageManager::Get().HasFile(kPackedVertexShaderPath)) {
        m_precompiledVertexShader = PackageManager::Get().GetFile(kPackedVertexShaderPath);
    }
    
    // Init Compiler (Graceful failure)
    m_compiler = new ShaderCompiler();
    m_compilerReady = m_compiler->Initialize();
    if (!m_compilerReady) {
        std::cerr << "Warning: Shader Compiler init failed (missing dxcompiler.dll?). Runtime compilation may fail.\n";
    }

    // Init Renderer
    m_renderer = new PreviewRenderer();
    ShaderCompiler* compiler = m_compilerReady ? m_compiler : nullptr;
    const std::vector<uint8_t>* precompiledVs = m_precompiledVertexShader.empty() ? nullptr : &m_precompiledVertexShader;
    if (!m_renderer->Initialize(m_device, compiler, DXGI_FORMAT_R8G8B8A8_UNORM, precompiledVs)) {
        // Only fatal if initialization logic in Renderer is strict.
        // We modified Renderer to handle null m_compiler if needed, or if generic init fails it returns false.
        // PreviewRenderer::Initialize fails if compiler is invalid. 
        // We need to fix PreviewRenderer::Initialize to accept null compiler if we want this to work without DLL.
        // For now, let's assume if it fails, we log it.
        std::cerr << "Renderer Init failed\n";
    }
    
    // Create local Audio System
    m_audio = new AudioSystem();
    if (!m_audio->Initialize()) {
        std::cerr << "Failed to init audio\n";
    }

    m_width = width;
    m_height = height;

    // Dummy Texture (Black)
    {
        D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = 1; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        
        m_device->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &desc, 
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, 
            IID_PPV_ARGS(&m_dummyTexture));
            
        // Upload black pixel... (omitted for brevity, assume black/garbage is fine for dummy or fix later)
        // Creating SRV Heap
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = 1; 
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_dummySrvHeap));
        
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        
        m_device->GetDevice()->CreateShaderResourceView(m_dummyTexture.Get(), &srvDesc, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Initialize ImGui
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        
        ImGui::StyleColorsDark();
        
        // Build font atlas manually to ensure ImGui doesn't assert before backend is ready
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        // Descriptor Heap for ImGui
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_imguiSrvHeap));

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX12_Init(m_device->GetDevice(), 2,
            DXGI_FORMAT_R8G8B8A8_UNORM, m_imguiSrvHeap.Get(),
            m_imguiSrvHeap->GetCPUDescriptorHandleForHeapStart(),
            m_imguiSrvHeap->GetGPUDescriptorHandleForHeapStart());
    }

    return true;
}

void DemoPlayer::LoadProject(const std::string& manifestPath) {
    m_manifestPath = manifestPath;
    m_loadingStage = LoadingStage::LoadingManifest;
}

// Minimal helpers
static ComPtr<ID3D12Resource> LoadTexture(Device* dev, const std::string& path) {
    // ... Implementation omitted for brevity
    return nullptr;
}


void DemoPlayer::Update(double wallTime, float dt) {
    if (m_loadingStage != LoadingStage::Ready) {
        if (m_loadingStage == LoadingStage::LoadingManifest) {
            // 1. Initialize Package System
            bool packed = PackageManager::Get().Initialize();

            bool loaded = false;
            if (packed) {
                std::cout << "Packed build detected. Loading project.json from executable." << std::endl;
                if (PackageManager::Get().HasFile("project.json")) {
                    auto data = PackageManager::Get().GetFile("project.json");
                    std::string jsonStr(data.begin(), data.end());
                    loaded = Serializer::LoadProjectFromJson(jsonStr, m_project);
                } else {
                    std::cerr << "Packed build is missing project.json in the executable.\n";
                }
            } else {
                std::cout << "No pack detected. Loading project from disk: " << m_manifestPath << std::endl;
                loaded = Serializer::LoadProject(m_manifestPath, m_project);
            }

            if (loaded) {
                DebugLogProjectSummary(m_project);
                m_loadingStage = LoadingStage::LoadingAssets;
            } else {
                // Nothing to load
            }
            return;
        }

        if (m_loadingStage == LoadingStage::LoadingAssets) {
            bool packed = PackageManager::Get().IsPacked();
            for(auto& clip : m_project.audioLibrary) {
                if (packed && PackageManager::Get().HasFile(clip.path)) {
                        auto d = PackageManager::Get().GetFile(clip.path);
                        m_audio->LoadAudioFromMemory(d.data(), d.size());
                } else {
                        m_audio->LoadAudio(clip.path);
                }
            }
            m_compilationIndex = 0;
            // Force Playing state for standalone demo
            m_transport = m_project.transport; 
            m_transport.state = TransportState::Playing; 
            m_transport.timeSeconds = 0.0;
            m_project.track.currentBeat = 0;
            m_project.track.lastTriggeredBeat = -1;
            m_lastFrameTime = wallTime;

            m_loadingStage = LoadingStage::CompilingShaders;
            return;
        }

        if (m_loadingStage == LoadingStage::CompilingShaders) {
            if (m_compilationIndex < (int)m_project.scenes.size()) {
                CompileScene(m_compilationIndex);
                m_compilationIndex++;
            } else {
                m_loadingStage = LoadingStage::Ready;
            }
            return;
        }
        return;
    }

    if (m_transport.state == TransportState::Playing) {
        m_transport.timeSeconds += dt;
        
        // Input Handling for Debug Overlay (Alt+D)
        bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
        bool d = (GetAsyncKeyState('D') & 0x8000) != 0;
        if (alt && d && !m_altPressed) {
            m_showDebug = !m_showDebug;
            m_altPressed = true;
        } else if (!d) {
            m_altPressed = false;
        }
        
        // Sync Audio
        // if (m_audio) m_audio->Update();

        // Track Logic
        float beatsPerSec = m_transport.bpm / 60.0f;
        int prevBeat = m_project.track.currentBeat;
        float exactBeat = (float)(m_transport.timeSeconds * beatsPerSec);
        m_project.track.currentBeat = (int)std::floor(exactBeat);

        if (m_transitionActive) {
            double transitionEndBeat = m_transitionStartBeat + m_transitionDurationBeats;
            if (exactBeat >= transitionEndBeat) {
                m_transitionActive = false;
                if (m_pendingActiveScene != -2) {
                    SetActiveScene(m_pendingActiveScene);
                    m_activeSceneStartBeat = m_transitionToStartBeat;
                    m_activeSceneOffset = (m_pendingActiveScene >= 0) ? m_transitionToOffset : 0.0f;
                    m_pendingActiveScene = -2;
                    m_transitionJustCompletedBeat = m_project.track.currentBeat;
                }
            }
        }
        
        auto& track = m_project.track;
        if (track.lengthBeats > 0 && track.currentBeat >= track.lengthBeats) {
            // Loop or Stop? For demo we usually stop or loop
            m_transport.timeSeconds = 0;
            m_project.track.currentBeat = 0;
            m_project.track.lastTriggeredBeat = -1;
            // Or m_transport.state = TransportState::Stopped; Use Loop for now
        }

        if (track.currentBeat > track.lastTriggeredBeat) {
             for (int b = track.lastTriggeredBeat + 1; b <= track.currentBeat; ++b) {
                 for(auto& row : track.rows) {
                     if (row.rowId == b) {
                         // Scene
                         if (row.transition != TransitionType::None && row.transitionDuration > 0) {
                            m_transitionActive = true;
                            m_transitionFromIndex = m_activeSceneIndex;
                            m_transitionFromOffset = m_activeSceneOffset;
                            m_transitionFromStartBeat = m_activeSceneStartBeat;
                            m_transitionToStartBeat = static_cast<double>(b);
                            int target = row.sceneIndex;
                            float targetOffset = row.timeOffset;
                            if (target == -1 && row.transition == TransitionType::Crossfade) {
                                const TrackerRow* nextRow = FindNextSceneRow(track, b);
                                if (nextRow) {
                                    target = nextRow->sceneIndex;
                                    targetOffset = nextRow->timeOffset;
                                }
                            }
                            if (target == -1 && row.transition != TransitionType::FadeOut) {
                                target = m_activeSceneIndex;
                                targetOffset = m_activeSceneOffset;
                                m_transitionToStartBeat = m_activeSceneStartBeat;
                            } else if (target == m_activeSceneIndex) {
                                targetOffset = m_activeSceneOffset;
                                m_transitionToStartBeat = m_activeSceneStartBeat;
                            }
                            m_transitionToIndex = target;
                            m_transitionToOffset = targetOffset;
                            m_transitionStartBeat = (double)b;
                            m_transitionDurationBeats = (double)row.transitionDuration;
                            m_currentTransitionType = row.transition;
                            m_pendingActiveScene = target;
                         } else if (row.sceneIndex >= 0) {
                             if (m_transitionJustCompletedBeat == row.rowId &&
                                 row.sceneIndex == m_activeSceneIndex) {
                                 continue;
                             }
                             if (m_transitionActive && row.sceneIndex == m_pendingActiveScene) {
                                 continue;
                             }
                             m_transitionActive = false;
                             SetActiveScene(row.sceneIndex);
                             m_activeSceneStartBeat = static_cast<double>(b);
                             m_activeSceneOffset = row.timeOffset;
                         }
                         
                         // Audio
                         if (row.musicIndex >= 0 && row.musicIndex < (int)m_project.audioLibrary.size() && m_audio) {
                             auto& clip = m_project.audioLibrary[row.musicIndex];
                             
                             if (PackageManager::Get().IsPacked() && PackageManager::Get().HasFile(clip.path)) {
                                 auto d = PackageManager::Get().GetFile(clip.path);
                                 m_audio->LoadAudioFromMemory(d.data(), d.size());
                             } else {
                                 m_audio->LoadAudio(clip.path);
                             }
                             
                             m_audio->Play();
                             if(clip.bpm > 0) m_transport.bpm = clip.bpm;
                         }
                         // Stop
                         if (row.stop) {
                             m_transport.state = TransportState::Stopped;
                             m_audio->Stop();
                         }
                     }
                 }
             }
             track.lastTriggeredBeat = track.currentBeat;
        }
        m_transitionJustCompletedBeat = -1;
    }
}

void DemoPlayer::SetActiveScene(int index) {
    if (index >= 0 && index < (int)m_project.scenes.size()) {
        m_activeSceneIndex = index;
    } else {
        m_activeSceneIndex = -1; // Black
    }
}

bool DemoPlayer::CompileScene(int sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_project.scenes.size()) return false;
    auto& scene = m_project.scenes[sceneIndex];
    if (!m_renderer) return false;

    bool sceneReady = false;

    auto loadBytecodeFromFile = [](const std::string& path, std::vector<uint8_t>& out) -> bool {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return !out.empty();
    };

    if (!scene.precompiledPath.empty()) {
        std::vector<uint8_t> data;
        if (PackageManager::Get().IsPacked() && PackageManager::Get().HasFile(scene.precompiledPath)) {
            data = PackageManager::Get().GetFile(scene.precompiledPath);
            DebugLog("Loaded packed scene shader: " + scene.precompiledPath + " (" + std::to_string(data.size()) + " bytes)");
        } else {
            loadBytecodeFromFile(scene.precompiledPath, data);
            if (!data.empty()) {
                DebugLog("Loaded disk scene shader: " + scene.precompiledPath + " (" + std::to_string(data.size()) + " bytes)");
            }
        }

        if (!data.empty()) {
            scene.pipelineState = m_renderer->CreatePSOFromBytecode(data);
            if (scene.pipelineState) {
                DebugLog("Scene PSO created from precompiled shader: " + scene.name);
                sceneReady = true;
            } else {
                std::cerr << "Failed to create PSO from precompiled shader for scene " << scene.name << std::endl;
            }
        }
        if (data.empty()) {
            DebugLogError("No precompiled scene shader data for: " + scene.name);
        }
    }

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
    if (!sceneReady && m_compilerReady) {
        std::vector<std::string> errors;
        scene.pipelineState = m_renderer->CompileShader(scene.shaderCode, decls, errors);
        if (!scene.pipelineState) {
            std::cerr << "Failed to compile scene " << scene.name << std::endl;
        }
        sceneReady = scene.pipelineState != nullptr;
    } else if (!sceneReady) {
        std::cerr << "No compiler available for scene " << scene.name << std::endl;
    }

    for (auto& fx : scene.postFxChain) {
        if (!CompilePostFxEffect(fx)) {
            std::cerr << "Failed to compile post fx for scene " << scene.name << " (" << fx.name << ")" << std::endl;
        }
    }
    return sceneReady;
}

void DemoPlayer::EnsureSceneTexture(int sceneIndex) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_project.scenes.size()) return;
    auto& scene = m_project.scenes[sceneIndex];
    if (m_width == 0 || m_height == 0) return;

    bool needsCreate = !scene.texture;
    if (scene.texture) {
        auto desc = scene.texture->GetDesc();
        if (desc.Width != m_width || desc.Height != m_height) needsCreate = true;
    }

    if (needsCreate) {
        scene.texture.Reset();
        scene.srvHeap.Reset();
        scene.textureValid = false;

        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC texDesc = {};
        texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texDesc.Width = m_width;
        texDesc.Height = m_height;
        texDesc.DepthOrArraySize = (scene.outputType == TextureType::TextureCube) ? 6 : 1;
        texDesc.MipLevels = 1;
        texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        memcpy(clearValue.Color, clearColor, sizeof(clearColor));

        m_device->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, 
            &clearValue, IID_PPV_ARGS(&scene.texture));
            
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 8 * kMaxPostFxChain;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&scene.srvHeap));

        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 1;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        m_device->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&scene.rtvHeap));

        auto rtvHandle = scene.rtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_device->GetDevice()->CreateRenderTargetView(scene.texture.Get(), nullptr, rtvHandle);
    }
}

void DemoPlayer::EnsurePostFxResources(Scene& scene) {
    if (m_width == 0 || m_height == 0 || !m_device) return;

    bool needsCreate = !scene.postFxTextureA || !scene.postFxTextureB;
    if (scene.postFxTextureA) {
        auto desc = scene.postFxTextureA->GetDesc();
        if (desc.Width != m_width || desc.Height != m_height) needsCreate = true;
    }
    if (!needsCreate) return;

    scene.postFxTextureA.Reset();
    scene.postFxTextureB.Reset();
    scene.postFxSrvHeap.Reset();
    scene.postFxRtvHeap.Reset();
    scene.postFxValid = false;

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    memcpy(clearValue.Color, clearColor, sizeof(clearColor));

    m_device->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&scene.postFxTextureA));

    m_device->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue, IID_PPV_ARGS(&scene.postFxTextureB));

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 8 * kMaxPostFxChain;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&scene.postFxSrvHeap));

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_device->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&scene.postFxRtvHeap));
}

void DemoPlayer::EnsurePostFxHistory(Scene::PostFXEffect& effect) {
    if (!m_device || m_width == 0 || m_height == 0) return;

    bool needsCreate = (int)effect.historyTextures.size() != kPostFxHistoryCount;
    if (!needsCreate) {
        auto desc = effect.historyTextures[0]->GetDesc();
        if (desc.Width != m_width || desc.Height != m_height) needsCreate = true;
    }

    if (!needsCreate) return;

    effect.historyTextures.clear();
    effect.historyTextures.resize(kPostFxHistoryCount);
    effect.historyIndex = 0;
    effect.historyInitialized = false;

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    for (int i = 0; i < kPostFxHistoryCount; ++i) {
        m_device->GetDevice()->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            nullptr, IID_PPV_ARGS(&effect.historyTextures[i]));
    }
}

bool DemoPlayer::CompilePostFxEffect(Scene::PostFXEffect& effect) {
    if (!m_renderer) return false;
    auto loadBytecodeFromFile = [](const std::string& path, std::vector<uint8_t>& out) -> bool {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return !out.empty();
    };

    if (!effect.precompiledPath.empty()) {
        std::vector<uint8_t> data;
        if (PackageManager::Get().IsPacked() && PackageManager::Get().HasFile(effect.precompiledPath)) {
            data = PackageManager::Get().GetFile(effect.precompiledPath);
            DebugLog("Loaded packed post FX shader: " + effect.precompiledPath + " (" + std::to_string(data.size()) + " bytes)");
        } else {
            loadBytecodeFromFile(effect.precompiledPath, data);
            if (!data.empty()) {
                DebugLog("Loaded disk post FX shader: " + effect.precompiledPath + " (" + std::to_string(data.size()) + " bytes)");
            }
        }

        if (!data.empty()) {
            effect.pipelineState = m_renderer->CreatePSOFromBytecode(data);
            if (effect.pipelineState) {
                effect.isDirty = false;
                effect.lastCompiledCode = effect.shaderCode;
                DebugLog("Post FX PSO created from precompiled shader: " + effect.name);
                return true;
            }
        }
        if (data.empty()) {
            DebugLogError("No precompiled post FX shader data for: " + effect.name);
        }
    }

    if (m_compilerReady) {
        std::vector<PreviewRenderer::TextureDecl> decls = { {0, "Texture2D"} };
        std::vector<std::string> errors;
        effect.pipelineState = m_renderer->CompileShader(effect.shaderCode, decls, errors, true);
        if (effect.pipelineState) {
            effect.isDirty = false;
            effect.lastCompiledCode = effect.shaderCode;
            return true;
        }
    }

    return false;
}

ID3D12Resource* DemoPlayer::ApplyPostFxChain(ID3D12GraphicsCommandList* commandList,
                                            Scene& scene,
                                            ID3D12Resource* inputTexture,
                                            double timeSeconds) {
    if (!commandList || !inputTexture) return inputTexture;

    bool anyEnabled = false;
    for (const auto& fx : scene.postFxChain) {
        if (fx.enabled) { anyEnabled = true; break; }
    }
    if (!anyEnabled) return inputTexture;

    EnsurePostFxResources(scene);
    if (!scene.postFxTextureA || !scene.postFxTextureB || !scene.postFxSrvHeap || !scene.postFxRtvHeap) return inputTexture;

    auto device = m_device->GetDevice();
    auto handleStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto startHandle = scene.postFxSrvHeap->GetCPUDescriptorHandleForHeapStart();

    auto bindInput = [&](ID3D12Resource* src, Scene::PostFXEffect& fx, int baseSlot) {
        D3D12_CPU_DESCRIPTOR_HANDLE dest = startHandle;
        dest.ptr += baseSlot * handleStep;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        device->CreateShaderResourceView(src, &srvDesc, dest);

        for (int i = 1; i <= kPostFxHistoryCount; ++i) {
            int historySlot = i;
            int historyIndex = fx.historyIndex - (i - 1);
            while (historyIndex < 0) historyIndex += kPostFxHistoryCount;
            ID3D12Resource* historyRes = nullptr;
            if (!fx.historyTextures.empty()) {
                historyRes = fx.historyTextures[historyIndex].Get();
            }

            D3D12_CPU_DESCRIPTOR_HANDLE histDest = startHandle;
            histDest.ptr += (baseSlot + historySlot) * handleStep;
            if (historyRes) {
                device->CreateShaderResourceView(historyRes, &srvDesc, histDest);
            } else if (m_dummySrvHeap) {
                device->CopyDescriptorsSimple(1, histDest, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
        }

        for (int i = kPostFxHistoryCount + 1; i < 8; ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE dummyDest = startHandle;
            dummyDest.ptr += (baseSlot + i) * handleStep;
            if (m_dummySrvHeap) {
                device->CopyDescriptorsSimple(1, dummyDest, m_dummySrvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            }
        }
    };

    ID3D12Resource* ping = scene.postFxTextureA.Get();
    ID3D12Resource* pong = scene.postFxTextureB.Get();
    ID3D12Resource* currentInput = inputTexture;
    ID3D12Resource* currentOutput = ping;

    int passIndex = 0;
    for (auto& fx : scene.postFxChain) {
        if (!fx.enabled) continue;
        if (!fx.pipelineState) continue;
        if (passIndex >= kMaxPostFxChain) break;

        EnsurePostFxHistory(fx);
        if (fx.historyTextures.empty()) continue;

        if (!fx.historyInitialized) {
            for (int i = 0; i < kPostFxHistoryCount; ++i) {
                D3D12_RESOURCE_BARRIER initBarriers[2] = {};
                initBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                initBarriers[0].Transition.pResource = currentInput;
                initBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                initBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                initBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                initBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                initBarriers[1].Transition.pResource = fx.historyTextures[i].Get();
                initBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                initBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                initBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                commandList->ResourceBarrier(2, initBarriers);

                commandList->CopyResource(fx.historyTextures[i].Get(), currentInput);

                std::swap(initBarriers[0].Transition.StateBefore, initBarriers[0].Transition.StateAfter);
                std::swap(initBarriers[1].Transition.StateBefore, initBarriers[1].Transition.StateAfter);
                commandList->ResourceBarrier(2, initBarriers);
            }
            fx.historyInitialized = true;
            fx.historyIndex = 0;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = currentOutput;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);

        int baseSlot = passIndex * 8;
        bindInput(currentInput, fx, baseSlot);
        ID3D12DescriptorHeap* heaps[] = { scene.postFxSrvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = scene.postFxRtvHeap->GetCPUDescriptorHandleForHeapStart();
        m_device->GetDevice()->CreateRenderTargetView(currentOutput, nullptr, rtvHandle);

        D3D12_GPU_DESCRIPTOR_HANDLE srvGpu = scene.postFxSrvHeap->GetGPUDescriptorHandleForHeapStart();
        srvGpu.ptr += baseSlot * handleStep;
        m_renderer->Render(
            commandList,
            fx.pipelineState.Get(),
            currentOutput,
            rtvHandle,
            srvGpu,
            m_width, m_height,
            (float)timeSeconds
        );

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        commandList->ResourceBarrier(1, &barrier);

        int writeIndex = (fx.historyIndex + 1) % kPostFxHistoryCount;
        D3D12_RESOURCE_BARRIER historyBarriers[2] = {};
        historyBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        historyBarriers[0].Transition.pResource = currentOutput;
        historyBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        historyBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        historyBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        historyBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        historyBarriers[1].Transition.pResource = fx.historyTextures[writeIndex].Get();
        historyBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        historyBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        historyBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(2, historyBarriers);

        commandList->CopyResource(fx.historyTextures[writeIndex].Get(), currentOutput);

        std::swap(historyBarriers[0].Transition.StateBefore, historyBarriers[0].Transition.StateAfter);
        std::swap(historyBarriers[1].Transition.StateBefore, historyBarriers[1].Transition.StateAfter);
        commandList->ResourceBarrier(2, historyBarriers);
        fx.historyIndex = writeIndex;

        currentInput = currentOutput;
        currentOutput = (currentOutput == ping) ? pong : ping;
        passIndex++;
    }

    scene.postFxValid = true;
    return currentInput;
}

ID3D12Resource* DemoPlayer::GetSceneFinalTexture(ID3D12GraphicsCommandList* commandList,
                                                int sceneIndex,
                                                double timeSeconds) {
    if (sceneIndex < 0 || sceneIndex >= (int)m_project.scenes.size()) return nullptr;
    RenderScene(commandList, sceneIndex, timeSeconds);
    auto& scene = m_project.scenes[sceneIndex];
    if (!scene.texture) return nullptr;
    if (scene.postFxChain.empty()) return scene.texture.Get();
    return ApplyPostFxChain(commandList, scene, scene.texture.Get(), timeSeconds);
}

void DemoPlayer::OnResize(int width, int height) {
    m_width = width;
    m_height = height;
}

void DemoPlayer::RenderScene(ID3D12GraphicsCommandList* cmd, int sceneIndex, double time) {
     for(int s : m_renderStack) { if(s == sceneIndex) return; }
     m_renderStack.push_back(sceneIndex);

     EnsureSceneTexture(sceneIndex);
     auto& scene = m_project.scenes[sceneIndex];
     
     if (!scene.texture) { m_renderStack.pop_back(); return; }

     // 1. Inputs
     for (const auto& binding : scene.bindings) {
        if (binding.enabled && binding.sourceSceneIndex != -1 && binding.sourceSceneIndex != sceneIndex) {
             RenderScene(cmd, binding.sourceSceneIndex, time);
        }
    }

    if (!scene.pipelineState) CompileScene(sceneIndex);
    if (!scene.pipelineState) { m_renderStack.pop_back(); return; }

    // 2. Bindings
    if (scene.srvHeap) {
        auto device = m_device->GetDevice();
        auto handleStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto startHandle = scene.srvHeap->GetCPUDescriptorHandleForHeapStart();
        
        for (int i=0; i<8; ++i) {
            D3D12_CPU_DESCRIPTOR_HANDLE dest = startHandle;
            dest.ptr += i * handleStep;
            
            bool bound = false;
            for(const auto& b : scene.bindings) {
                if (b.channelIndex == i && b.enabled) {
                    ID3D12Resource* srcRes = nullptr;
                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                    if (b.bindingType == BindingType::Scene) {
                        if (b.sourceSceneIndex >= 0 && b.sourceSceneIndex < (int)m_project.scenes.size()) {
                             auto& src = m_project.scenes[b.sourceSceneIndex];
                             if (src.texture) {
                                 srcRes = src.texture.Get();
                                 if (b.type == TextureType::TextureCube) {
                                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                                    srvDesc.TextureCube.MipLevels = 1; 
                                 } else {
                                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                                    srvDesc.Texture2D.MipLevels = 1;
                                 }
                             }
                        }
                    }
                    
                    if (srcRes) {
                        device->CreateShaderResourceView(srcRes, &srvDesc, dest);
                        bound = true;
                    }
                }
            }
        }
    }

    // 3. Render
    // Barrier: Resource -> RT
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = scene.texture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    cmd->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = scene.rtvHeap->GetCPUDescriptorHandleForHeapStart();

    float clearColor[] = {0,0,0,1};
    cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    
    // Set heaps
    ID3D12DescriptorHeap* heaps[] = { scene.srvHeap.Get() };
    if (scene.srvHeap) cmd->SetDescriptorHeaps(1, heaps);

    m_renderer->Render(cmd, scene.pipelineState.Get(), scene.texture.Get(), rtvHandle, 
                       scene.srvHeap ? scene.srvHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{}, 
                       m_width, m_height, (float)time);

    // Barrier: RT -> Resource
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    cmd->ResourceBarrier(1, &barrier);
    
    scene.textureValid = true;
    m_renderStack.pop_back();
}

void DemoPlayer::Render(ID3D12GraphicsCommandList* cmd, ID3D12Resource* renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle) {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    if (m_showDebug) {
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.5f);
        if (ImGui::Begin("Debug Overlay", &m_showDebug, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Text("Time: %.2f s", m_transport.timeSeconds);
            if (m_loadingStage != LoadingStage::Ready) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1,1,0,1), "Loading Stage: %d", (int)m_loadingStage);
            } else {
                ImGui::Text("Scene: %d", m_activeSceneIndex);
                if (m_transitionActive) ImGui::TextColored(ImVec4(0.4f,1.0f,0.4f,1.0f), "Transition Active");
            }
        }
        ImGui::End();
    }

    if (m_loadingStage != LoadingStage::Ready) {
        float clearColor[] = {0.1f, 0.1f, 0.2f, 1.0f};
        cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        goto render_ui;
    }

    m_renderStack.clear(); 

    if (m_transitionActive) {
         float beatsPerSec = m_transport.bpm / 60.0f;
         double exactBeat = m_transport.timeSeconds * beatsPerSec;
         double progress = (exactBeat - m_transitionStartBeat) / m_transitionDurationBeats;
         
         if (progress >= 1.0) {
             m_transitionActive = false;
             if (m_pendingActiveScene != -2) {
                 SetActiveScene(m_pendingActiveScene);
                 m_activeSceneStartBeat = m_transitionToStartBeat;
                 m_activeSceneOffset = (m_pendingActiveScene >= 0) ? m_transitionToOffset : 0.0f;
                 m_pendingActiveScene = -2;
             }
         } else {
             ID3D12Resource* fromTex = nullptr;
             ID3D12Resource* toTex = nullptr;

             if (m_transitionFromIndex >= 0) {
                const double fromTime = SceneTimeSeconds(exactBeat, m_transitionFromStartBeat, m_transitionFromOffset, m_transport.bpm);
                fromTex = GetSceneFinalTexture(cmd, m_transitionFromIndex, fromTime);
             }
             if (m_transitionToIndex >= 0) {
                const double toTime = SceneTimeSeconds(exactBeat, m_transitionToStartBeat, m_transitionToOffset, m_transport.bpm);
                toTex = GetSceneFinalTexture(cmd, m_transitionToIndex, toTime);
             }
             
             if (!m_transitionPSO || m_compiledTransitionType != m_currentTransitionType) {
                  auto loadTransitionBytecode = [&](TransitionType type) -> const std::vector<uint8_t>* {
                      int idx = TransitionIndex(type);
                      if (idx < 0 || idx >= (int)m_transitionBytecode.size()) return nullptr;
                      if (!m_transitionBytecode[idx].empty()) return &m_transitionBytecode[idx];

                      const char* packedPath = GetTransitionPackedPath(type);
                      if (packedPath && *packedPath) {
                          if (PackageManager::Get().IsPacked() && PackageManager::Get().HasFile(packedPath)) {
                              m_transitionBytecode[idx] = PackageManager::Get().GetFile(packedPath);
                              if (!m_transitionBytecode[idx].empty()) return &m_transitionBytecode[idx];
                          }

                          fs::path diskPath = fs::path(m_manifestPath).parent_path() / packedPath;
                          std::ifstream file(diskPath, std::ios::binary);
                          if (file.is_open()) {
                              m_transitionBytecode[idx].assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                              if (!m_transitionBytecode[idx].empty()) return &m_transitionBytecode[idx];
                          }
                      }
                      return nullptr;
                  };

                  const std::vector<uint8_t>* bytecode = loadTransitionBytecode(m_currentTransitionType);
                  if (bytecode && !bytecode->empty()) {
                      m_transitionPSO = m_renderer->CreatePSOFromBytecode(*bytecode);
                      m_compiledTransitionType = m_currentTransitionType;
                  } else if (m_compilerReady) {
                      std::vector<PreviewRenderer::TextureDecl> decls = {{0, "Texture2D"}, {1, "Texture2D"}};
                      std::string code = GetTransitionShaderSource(m_currentTransitionType);
                      std::vector<std::string> errs;
                      m_transitionPSO = m_renderer->CompileShader(code, decls, errs);
                      m_compiledTransitionType = m_currentTransitionType;
                  }
             }
             if (m_transitionPSO) {
                 if (!m_transitionSrvHeap) {
                    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
                    heapDesc.NumDescriptors = 8; 
                    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                    m_device->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_transitionSrvHeap));
                 }
                 auto device = m_device->GetDevice();
                 auto handleStep = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                 auto start = m_transitionSrvHeap->GetCPUDescriptorHandleForHeapStart();
                 
                 auto Bind = [&](ID3D12Resource* res, int slot) {
                     D3D12_CPU_DESCRIPTOR_HANDLE dest = start;
                     dest.ptr += slot * handleStep;
                     D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
                     srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                     srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                     srv.Texture2D.MipLevels = 1;
                     srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                     if (!res) res = m_dummyTexture.Get();
                     device->CreateShaderResourceView(res, &srv, dest);
                 };
                 Bind(fromTex, 0);
                 Bind(toTex, 1);
                 
                 ID3D12DescriptorHeap* heaps[] = { m_transitionSrvHeap.Get() };
                 cmd->SetDescriptorHeaps(1, heaps);
                 
                 m_renderer->Render(cmd, m_transitionPSO.Get(), renderTarget, rtvHandle, 
                     m_transitionSrvHeap->GetGPUDescriptorHandleForHeapStart(), m_width, m_height, (float)progress);
             }
             goto render_ui;
         }
    }

    if (m_activeSceneIndex >= 0) {
        const double beatsPerSec = m_transport.bpm / 60.0f;
        const double exactBeat = m_transport.timeSeconds * beatsPerSec;
        const double activeTime = SceneTimeSeconds(exactBeat, m_activeSceneStartBeat, m_activeSceneOffset, m_transport.bpm);
        ID3D12Resource* finalTex = GetSceneFinalTexture(cmd, m_activeSceneIndex, activeTime);
        if (finalTex) {
            D3D12_RESOURCE_BARRIER pre = {};
            pre.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pre.Transition.pResource = renderTarget;
            pre.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            pre.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            cmd->ResourceBarrier(1, &pre);
            
            D3D12_RESOURCE_BARRIER preSrc = {};
            preSrc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            preSrc.Transition.pResource = finalTex;
            preSrc.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            preSrc.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            cmd->ResourceBarrier(1, &preSrc);
            
            cmd->CopyResource(renderTarget, finalTex);
            
            std::swap(pre.Transition.StateBefore, pre.Transition.StateAfter);
            cmd->ResourceBarrier(1, &pre);
            std::swap(preSrc.Transition.StateBefore, preSrc.Transition.StateAfter);
            cmd->ResourceBarrier(1, &preSrc);
        }
    } else {
        float clearColor[] = {0,0,0,1};
        cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    }

render_ui:
    ImGui::Render();
    if (m_imguiSrvHeap) {
        ID3D12DescriptorHeap* heaps[] = { m_imguiSrvHeap.Get() };
        cmd->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
    }
}

}
