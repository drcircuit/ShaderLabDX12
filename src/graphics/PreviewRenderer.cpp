#include "ShaderLab/Graphics/PreviewRenderer.h"
#include "ShaderLab/Graphics/Device.h"
#include "ShaderLab/Shader/ShaderCompiler.h"

#include <d3d12.h>
#include <stdexcept>

#ifndef SHADERLAB_TINY_PLAYER
#define SHADERLAB_TINY_PLAYER 0
#endif

#ifndef SHADERLAB_TINY_RUNTIME_COMPILE
#define SHADERLAB_TINY_RUNTIME_COMPILE 0
#endif

namespace ShaderLab {

namespace {
std::string GetShaderLabLogPath() {
    char tempPath[MAX_PATH] = {};
    DWORD len = GetTempPathA(MAX_PATH, tempPath);
    if (len > 0 && len < MAX_PATH) {
        return std::string(tempPath) + "shaderlab_log.txt";
    }
    return "shaderlab_log.txt";
}
}

struct Vertex {
    float pos[3];
    float uv[2];
};

struct Constants {
    float iTime;
    float iResolution[2];
    float iBeat;
    float iBar;
    float fBarBeat;
    float padding0;
    float padding1;
};

PreviewRenderer::PreviewRenderer() = default;

PreviewRenderer::~PreviewRenderer() {
    Shutdown();
}

bool PreviewRenderer::Initialize(Device* device, ShaderCompiler* compiler, DXGI_FORMAT renderTargetFormat, const std::vector<uint8_t>* precompiledVertexShader) {
    const std::string logPath = GetShaderLabLogPath();
    FILE* f = nullptr;
    fopen_s(&f, logPath.c_str(), "a");
    
    if (!device) {
        if (f) { fprintf(f, "PreviewRenderer: Invalid device or compiler\n"); fclose(f); }
        return false;
    }

    m_device = device;
    m_compiler = compiler;
    m_renderTargetFormat = renderTargetFormat;

    if (f) { fprintf(f, "PreviewRenderer: About to create root signature\n"); fclose(f); }
    if (!CreateRootSignature()) {
        if (!f) fopen_s(&f, logPath.c_str(), "a");
        if (f) { fprintf(f, "PreviewRenderer: Failed to create root signature\n"); fclose(f); }
        return false;
    }
    if (!f) fopen_s(&f, logPath.c_str(), "a");
    if (f) { fprintf(f, "PreviewRenderer: Root signature created\n"); fclose(f); }

    CreateFullscreenQuadVertices();
    if (!f) fopen_s(&f, logPath.c_str(), "a");
    if (f) { fprintf(f, "PreviewRenderer: Fullscreen quad created\n"); fclose(f); }

    // Init Timestamps
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = 2; // Start and End
    m_device->GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_queryHeap));

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = sizeof(uint64_t) * 2;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;
    
    m_device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_queryResultBuffer)
    );
    
    // Need command queue to get frequency. We don't have it here directly, usually on ControlQueue
    // But we can get it via the ID3D12CommandQueue if we had access. D3D12GetTimestampFrequency needs queue.
    // Assuming m_device->GetCommandQueue() is available in Device, but it's not. 
    // We will query it safely later or assume standard ticks? No, frequency varies.
    // Let's modify Device.h to expose CommandQueue later? 
    // For now, let's grab it via a small workaround or passed in.
    // Wait, Device doesn't expose command queue? Swapchain has it? 
    // Let's defer frequency query to first Render call where we might not have the queue either.
    // Actually, commandList->GetTimestampFrequency is NOT a thing.
    // We need the queue. 
    // Let's just create a temporary queue to get frequency or better, modify Device to give it.

    // Better: let's modify Render to accept frequency or just move on.
    // We will wait with frequency until we render the first time? No, we need Queue to get it.
    
    if (precompiledVertexShader && !precompiledVertexShader->empty()) {
        m_vertexShaderBytecode = *precompiledVertexShader;
        if (f) { fprintf(f, "PreviewRenderer: Using precompiled vertex shader\n"); fclose(f); }
        return true;
    }

    if (!m_compiler) {
        if (f) { fprintf(f, "PreviewRenderer: Missing compiler and no precompiled vertex shader\n"); fclose(f); }
        return false;
    }

#if SHADERLAB_TINY_PLAYER
    if (f) { fprintf(f, "PreviewRenderer: Tiny build requires precompiled vertex shader\n"); fclose(f); }
    return false;
#else

    // Compile default vertex shader
    const char* vertexShaderSource = R"(
cbuffer Constants : register(b0) {
    float iTime;
    float2 iResolution;
    float iBeat;
    float iBar;
    float fBarBeat;
};

struct VSInput {
    float3 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 fragCoord : TEXCOORD0;
};

PSInput main(VSInput input) {
    PSInput output;
    output.pos = float4(input.pos, 1.0);
    // Convert UV (0-1) to pixel coordinates like ShaderToy
    // Flip Y axis: ShaderToy has origin at bottom-left
    output.fragCoord = float2(input.uv.x, 1.0 - input.uv.y) * iResolution;
    return output;
}
)";

    if (!f) fopen_s(&f, logPath.c_str(), "a");
    if (f) { fprintf(f, "PreviewRenderer: About to compile vertex shader\n"); fclose(f); }
    auto vsResult = m_compiler->CompileFromSource(vertexShaderSource, "main", "vs_6_0", L"vertex.hlsl");
    if (!vsResult.success) {
        if (!f) fopen_s(&f, logPath.c_str(), "a");
        if (f) { 
            fprintf(f, "PreviewRenderer: Failed to compile vertex shader\n");
            for (const auto& diag : vsResult.diagnostics) {
                fprintf(f, "  %s\n", diag.message.c_str());
            }
            fclose(f);
        }
        return false;
    }
    m_vertexShaderBytecode = vsResult.bytecode;

    if (!f) fopen_s(&f, logPath.c_str(), "a");
    if (f) { fprintf(f, "PreviewRenderer: Initialized successfully\n"); fclose(f); }
    return true;
#endif
}

void PreviewRenderer::Shutdown() {
    m_rootSignature.Reset();
    m_vertexBuffer.Reset();
    m_device = nullptr;
    m_compiler = nullptr;
}

ComPtr<ID3D12PipelineState> PreviewRenderer::CompileShader(const std::string& shaderSource, const std::vector<TextureDecl>& textureDecls, std::vector<std::string>& outErrors) {
    return CompileShader(shaderSource, textureDecls, outErrors, false, "main");
}

ComPtr<ID3D12PipelineState> PreviewRenderer::CompileShader(const std::string& shaderSource, const std::vector<TextureDecl>& textureDecls, std::vector<std::string>& outErrors, bool flipFragCoord) {
    return CompileShader(shaderSource, textureDecls, outErrors, flipFragCoord, "main");
}

ComPtr<ID3D12PipelineState> PreviewRenderer::CompileShader(const std::string& shaderSource,
                                                           const std::vector<TextureDecl>& textureDecls,
                                                           std::vector<std::string>& outErrors,
                                                           bool flipFragCoord,
                                                           const std::string& shaderEntryPoint) {
    outErrors.clear();
    m_lastCompiledPixelShaderSize = 0;

#if SHADERLAB_TINY_PLAYER && !SHADERLAB_TINY_RUNTIME_COMPILE
    (void)shaderSource;
    (void)textureDecls;
    (void)flipFragCoord;
    (void)shaderEntryPoint;
    outErrors.push_back("Runtime shader compilation is disabled in tiny player builds.");
    return nullptr;
#else

    // Wrap user shader code with proper entry point
    std::string wrappedSource = R"(
cbuffer Constants : register(b0) {
    float iTime;
    float2 iResolution;
    float iBeat;
    float iBar;
    float fBarBeat;
};
)";

    // Generate texture declarations
    // Default to Texture2D for all slots if not specified, or override based on declares
    // Actually, we should just declare what is used. But for simplicity and compatibility,
    // we iterate 0-7 and use the type provided, or Texture2D as default.
    for(int i=0; i<8; ++i) {
        std::string type = "Texture2D";
        for(const auto& decl : textureDecls) {
            if (decl.slot == i) {
                type = decl.type;
                break;
            }
        }
        wrappedSource += type + " iChannel" + std::to_string(i) + " : register(t" + std::to_string(i) + ");\n";
    }

    wrappedSource += "\n";

    // Generate samplers
    for(int i=0; i<8; ++i) {
        wrappedSource += "SamplerState iSampler" + std::to_string(i) + " : register(s" + std::to_string(i) + ");\n";
    }

    wrappedSource += R"(

struct PSInput {
    float4 pos : SV_POSITION;
    float2 fragCoord : TEXCOORD0;
};

)";
    wrappedSource += shaderSource;
    wrappedSource += R"(

float4 PSMain(PSInput input) : SV_TARGET {
)";
    if (flipFragCoord) {
        wrappedSource += "    float2 fragCoord = float2(input.fragCoord.x, iResolution.y - input.fragCoord.y);\n";
    } else {
        wrappedSource += "    float2 fragCoord = input.fragCoord;\n";
    }
    wrappedSource += "    return " + shaderEntryPoint + "(fragCoord, iResolution, iTime);\n";
    wrappedSource += R"(
}
)";

    auto psResult = m_compiler->CompileFromSource(wrappedSource, "PSMain", "ps_6_0", L"pixel.hlsl");
    
    if (!psResult.success) {
        for (const auto& diag : psResult.diagnostics) {
            outErrors.push_back(diag.message);
        }
        return nullptr;
    }

    ComPtr<ID3D12PipelineState> pso;
    if (CreatePipelineState(psResult.bytecode, pso)) {
        m_lastCompiledPixelShaderSize = psResult.bytecode.size();
        return pso;
    }
    return nullptr;
#endif
}

ComPtr<ID3D12PipelineState> PreviewRenderer::CreatePSOFromBytecode(const std::vector<uint8_t>& psBytecode) {
    ComPtr<ID3D12PipelineState> pso;
    if (CreatePipelineState(psBytecode, pso)) {
        return pso;
    }
    return nullptr;
}

void PreviewRenderer::Render(ID3D12GraphicsCommandList* commandList,
                              ID3D12PipelineState* pipelineState,
                              ID3D12Resource* renderTarget,
                              D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle,
                              D3D12_GPU_DESCRIPTOR_HANDLE srvHandle,
                              uint32_t width, uint32_t height,
                              float timeSeconds,
                              float beat,
                              float bar,
                              float barBeat16) {
    if (!pipelineState || !renderTarget) {
        return;
    }

    // Readback previous frame timestamps if permitted
    // NOTE: This causes CPU stall if we do it for the CURRENT frame which we don't here.
    // If we have a ring buffer of queries, no stall. With single buffer, we might read old data or stall?
    // Doing simple blocking read for now for simplicity as this is a tool.
    // Ideally we'd buffer N frames.
    if (m_queryResultBuffer && m_gpuFrequency > 0) {
        uint64_t* times = nullptr;
        D3D12_RANGE readRange = { 0, sizeof(uint64_t) * 2 };
        if (SUCCEEDED(m_queryResultBuffer->Map(0, &readRange, (void**)&times))) {
            uint64_t start = times[0];
            uint64_t end = times[1];
            if (end > start) {
                m_lastGPUTimeMs = (float)(end - start) / (float)m_gpuFrequency * 1000.0f;
            }
            m_queryResultBuffer->Unmap(0, nullptr);
        }
    }

    if (m_queryHeap) {
        commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    }

    // Set render target (assume caller has already transitioned)
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Clear
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Set viewport and scissor
    D3D12_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    commandList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{};
    scissor.right = static_cast<LONG>(width);
    scissor.bottom = static_cast<LONG>(height);
    commandList->RSSetScissorRects(1, &scissor);

    // Set pipeline state
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(pipelineState);

    // Set constants
    Constants constants{};
    constants.iTime = timeSeconds;
    constants.iResolution[0] = static_cast<float>(width);
    constants.iResolution[1] = static_cast<float>(height);
    constants.iBeat = beat;
    constants.iBar = bar;
    constants.fBarBeat = barBeat16;
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(Constants) / 4, &constants, 0);

    // Set textures (SRV table)
    if (srvHandle.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(1, srvHandle);
    }

    // Set vertex buffer and draw
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->DrawInstanced(3, 1, 0, 0);
    if (m_queryHeap && m_queryResultBuffer) {
        commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
        commandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_queryResultBuffer.Get(), 0);
    }
}


bool PreviewRenderer::CreateRootSignature() {
    // Root parameter 0: Constants
    D3D12_ROOT_PARAMETER rootParams[2]{};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.ShaderRegister = 0;
    rootParams[0].Constants.RegisterSpace = 0;
    rootParams[0].Constants.Num32BitValues = sizeof(Constants) / 4;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root parameter 1: SRV Descriptor Table (t0-t7)
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 8;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &range;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static samplers (s0-s7)
    D3D12_STATIC_SAMPLER_DESC samplers[8]{};
    for(int i=0; i<8; ++i) {
        samplers[i].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samplers[i].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[i].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[i].MipLODBias = 0;
        samplers[i].MaxAnisotropy = 1;
        samplers[i].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplers[i].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        samplers[i].MinLOD = 0.0f;
        samplers[i].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[i].ShaderRegister = i;
        samplers[i].RegisterSpace = 0;
        samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = rootParams;
    desc.NumStaticSamplers = 8;
    desc.pStaticSamplers = samplers;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->GetDevice()->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );

    return SUCCEEDED(hr);
}

bool PreviewRenderer::CreatePipelineState(const std::vector<uint8_t>& pixelShaderBytecode, ComPtr<ID3D12PipelineState>& outPso) {
    // Input layout
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { m_vertexShaderBytecode.data(), m_vertexShaderBytecode.size() };
    psoDesc.PS = { pixelShaderBytecode.data(), pixelShaderBytecode.size() };
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_renderTargetFormat;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = m_device->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(outPso.GetAddressOf()));
    return SUCCEEDED(hr);
}

void PreviewRenderer::CreateFullscreenQuadVertices() {
    Vertex vertices[] = {
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { { -1.0f,  3.0f, 0.0f }, { 0.0f,-1.0f } },
        { {  3.0f, -1.0f, 0.0f }, { 2.0f, 1.0f } }
    };

    const UINT vertexBufferSize = sizeof(vertices);

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc{};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = vertexBufferSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    m_device->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    );

    // Copy vertex data
    void* pData;
    m_vertexBuffer->Map(0, nullptr, &pData);
    memcpy(pData, vertices, vertexBufferSize);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;
}

} // namespace ShaderLab
