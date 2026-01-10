#ifdef _WIN32

#include "dx_renderer.h"
#include "main.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <thread>
#include <atomic>

using Microsoft::WRL::ComPtr;

// DirectX 12 shader source (HLSL)
static const char* shaderSource = R"(
struct VSInput {
    uint vertexID : SV_VertexID;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PSInput VSMain(VSInput input) {
    float2 positions[6] = {
        float2(-1.0,  1.0), float2(-1.0, -1.0), float2( 1.0, -1.0),
        float2(-1.0,  1.0), float2( 1.0, -1.0), float2( 1.0,  1.0)
    };
    float2 texCoords[6] = {
        float2(0.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0),
        float2(0.0, 0.0), float2(1.0, 1.0), float2(1.0, 0.0)
    };
    
    PSInput result;
    result.position = float4(positions[input.vertexID], 0.0, 1.0);
    result.texCoord = texCoords[input.vertexID];
    return result;
}

Texture2D<float> indexTexture : register(t0);
Texture2D<float4> paletteTexture : register(t1);
SamplerState texSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET {
    float index = indexTexture.Sample(texSampler, input.texCoord).r;
    float4 color = paletteTexture.Sample(texSampler, float2(index, 0.5));
    return color;
}
)";

// Triple buffering like Metal
static const int MAX_FRAMES_IN_FLIGHT = 3;

static ComPtr<ID3D12Device> device;
static ComPtr<IDXGISwapChain3> swapChain;
static ComPtr<ID3D12CommandQueue> commandQueue;
static ComPtr<ID3D12DescriptorHeap> rtvHeap;
static ComPtr<ID3D12DescriptorHeap> srvHeap;
static ComPtr<ID3D12RootSignature> rootSignature;
static ComPtr<ID3D12PipelineState> pipelineState;

// Per-frame resources
struct FrameData {
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Resource> renderTarget;
    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValue;
    HANDLE fenceEvent;
};
static FrameData frames[MAX_FRAMES_IN_FLIGHT];
static int currentFrame = 0;

// Textures (upload heap = shared memory like Metal)
static ComPtr<ID3D12Resource> indexTexture;
static ComPtr<ID3D12Resource> paletteTexture;
static ComPtr<ID3D12Resource> offscreenTarget;

static int frameWidth = 0;
static int frameHeight = 0;
static UINT rtvDescriptorSize = 0;
static std::atomic<int> skipped_presents(0);
static std::thread presentThread;
static std::atomic<bool> running(true);

int dx_init(void* hwnd, int width, int height) {
    frameWidth = width;
    frameHeight = height;

    // Enable debug layer in debug builds
#ifdef DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    // Create device
    ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        return -1;
    }

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
            break;
        }
    }

    if (!device) {
        return -1;
    }

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))) {
        return -1;
    }

    // Create swap chain with no vsync and triple buffering
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = MAX_FRAMES_IN_FLIGHT;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // Disable vsync

    ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), (HWND)hwnd, 
                                               &swapChainDesc, nullptr, nullptr, &swapChain1))) {
        return -1;
    }
    swapChain1.As(&swapChain);

    // Disable Alt+Enter fullscreen
    factory->MakeWindowAssociation((HWND)hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Create descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = MAX_FRAMES_IN_FLIGHT + 1; // +1 for offscreen
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)))) {
        return -1;
    }
    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 2; // index + palette textures
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap)))) {
        return -1;
    }

    // Create render targets for swap chain
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&frames[i].renderTarget)))) {
            return -1;
        }
        device->CreateRenderTargetView(frames[i].renderTarget.Get(), nullptr, rtvHandle);
        rtvHandle.ptr += rtvDescriptorSize;
    }

    // Create offscreen render target
    D3D12_RESOURCE_DESC offscreenDesc = {};
    offscreenDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    offscreenDesc.Width = width;
    offscreenDesc.Height = height;
    offscreenDesc.DepthOrArraySize = 1;
    offscreenDesc.MipLevels = 1;
    offscreenDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    offscreenDesc.SampleDesc.Count = 1;
    offscreenDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                               &offscreenDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                               nullptr, IID_PPV_ARGS(&offscreenTarget)))) {
        return -1;
    }
    device->CreateRenderTargetView(offscreenTarget.Get(), nullptr, rtvHandle);

    // Create textures with UPLOAD heap (like Metal's shared storage)
    D3D12_RESOURCE_DESC indexTexDesc = {};
    indexTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    indexTexDesc.Width = width;
    indexTexDesc.Height = height;
    indexTexDesc.DepthOrArraySize = 1;
    indexTexDesc.MipLevels = 1;
    indexTexDesc.Format = DXGI_FORMAT_R8_UNORM;
    indexTexDesc.SampleDesc.Count = 1;
    indexTexDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    
    if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
                                               &indexTexDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                               nullptr, IID_PPV_ARGS(&indexTexture)))) {
        return -1;
    }

    D3D12_RESOURCE_DESC paletteTexDesc = {};
    paletteTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    paletteTexDesc.Width = 256;
    paletteTexDesc.Height = 1;
    paletteTexDesc.DepthOrArraySize = 1;
    paletteTexDesc.MipLevels = 1;
    paletteTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    paletteTexDesc.SampleDesc.Count = 1;
    paletteTexDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
                                               &paletteTexDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                               nullptr, IID_PPV_ARGS(&paletteTexture)))) {
        return -1;
    }

    // Create SRVs
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateShaderResourceView(indexTexture.Get(), &srvDesc, srvHandle);

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(paletteTexture.Get(), &srvDesc, srvHandle);

    // Compile shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> error;
    
    UINT compileFlags = 0;
#ifdef DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    if (FAILED(D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr,
                         "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &error))) {
        if (error) {
            internal_error((char*)error->GetBufferPointer());
        }
        return -1;
    }

    if (FAILED(D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr,
                         "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &error))) {
        if (error) {
            internal_error((char*)error->GetBufferPointer());
        }
        return -1;
    }

    // Create root signature
    D3D12_DESCRIPTOR_RANGE ranges[1];
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 2;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[1];
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[0].DescriptorTable.pDescriptorRanges = ranges;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &sampler;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                          &signature, &error))) {
        return -1;
    }

    if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(),
                                          signature->GetBufferSize(),
                                          IID_PPV_ARGS(&rootSignature)))) {
        return -1;
    }

    // Create PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    
    // BlendState
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    for (int i = 0; i < 8; i++) {
        psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
        psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    
    psoDesc.SampleMask = UINT_MAX;
    
    // RasterizerState
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))) {
        return -1;
    }

    // Create per-frame resources
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  IID_PPV_ARGS(&frames[i].commandAllocator)))) {
            return -1;
        }

        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             frames[i].commandAllocator.Get(), nullptr,
                                             IID_PPV_ARGS(&frames[i].commandList)))) {
            return -1;
        }
        frames[i].commandList->Close();

        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                       IID_PPV_ARGS(&frames[i].fence)))) {
            return -1;
        }

        frames[i].fenceValue = 0;
        frames[i].fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!frames[i].fenceEvent) {
            return -1;
        }
    }

    return 0;
}

void dx_upload_frame(const unsigned char* indices, int pitch) {
    // Map and write directly to upload heap (like Metal's shared storage)
    void* data;
    D3D12_RANGE readRange = { 0, 0 };
    if (SUCCEEDED(indexTexture->Map(0, &readRange, &data))) {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        UINT numRows;
        UINT64 rowSize, totalSize;
        D3D12_RESOURCE_DESC desc = indexTexture->GetDesc();
        device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, &numRows, &rowSize, &totalSize);

        BYTE* dst = (BYTE*)data;
        for (UINT y = 0; y < frameHeight; y++) {
            memcpy(dst + y * layout.Footprint.RowPitch, indices + y * frameWidth, frameWidth);
        }
        indexTexture->Unmap(0, nullptr);
    }
}

void dx_update_palette(const unsigned int* palette) {
    // Map and write directly to upload heap
    void* data;
    D3D12_RANGE readRange = { 0, 0 };
    if (SUCCEEDED(paletteTexture->Map(0, &readRange, &data))) {
        unsigned int* dst = (unsigned int*)data;
        // Convert ARGB to BGRA
        for (int i = 0; i < 256; i++) {
            unsigned int argb = palette[i];
            unsigned int a = (argb >> 24) & 0xFF;
            unsigned int r = (argb >> 16) & 0xFF;
            unsigned int g = (argb >> 8) & 0xFF;
            unsigned int b = argb & 0xFF;
            dst[i] = (a << 24) | (r << 0) | (g << 8) | (b << 16); // BGRA
        }
        paletteTexture->Unmap(0, nullptr);
    }
}

void dx_present() {
    // Get current frame
    FrameData& frame = frames[currentFrame];

    // Wait for this frame's GPU work to complete (only blocks if > MAX_FRAMES_IN_FLIGHT behind)
    if (frame.fence->GetCompletedValue() < frame.fenceValue) {
        frame.fence->SetEventOnCompletion(frame.fenceValue, frame.fenceEvent);
        WaitForSingleObject(frame.fenceEvent, 1); // 1ms timeout, skip if not ready
        if (frame.fence->GetCompletedValue() < frame.fenceValue) {
            skipped_presents++;
            currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
            return;
        }
    }

    // Get swap chain back buffer index
    UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

    // Record commands
    frame.commandAllocator->Reset();
    frame.commandList->Reset(frame.commandAllocator.Get(), pipelineState.Get());

    // Render to offscreen target
    frame.commandList->SetGraphicsRootSignature(rootSignature.Get());
    frame.commandList->SetDescriptorHeaps(1, srvHeap.GetAddressOf());
    frame.commandList->SetGraphicsRootDescriptorTable(0, srvHeap->GetGPUDescriptorHandleForHeapStart());

    D3D12_VIEWPORT viewport = { 0, 0, (float)frameWidth, (float)frameHeight, 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, frameWidth, frameHeight };
    frame.commandList->RSSetViewports(1, &viewport);
    frame.commandList->RSSetScissorRects(1, &scissor);

    D3D12_CPU_DESCRIPTOR_HANDLE offscreenRtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    offscreenRtv.ptr += MAX_FRAMES_IN_FLIGHT * rtvDescriptorSize;
    frame.commandList->OMSetRenderTargets(1, &offscreenRtv, FALSE, nullptr);

    const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    frame.commandList->ClearRenderTargetView(offscreenRtv, clearColor, 0, nullptr);
    frame.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    frame.commandList->DrawInstanced(6, 1, 0, 0);

    // Copy offscreen to back buffer
    D3D12_RESOURCE_BARRIER barriers[2] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[0].Transition.pResource = offscreenTarget.Get();
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barriers[1].Transition.pResource = frame.renderTarget.Get();
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    frame.commandList->ResourceBarrier(2, barriers);

    frame.commandList->CopyResource(frame.renderTarget.Get(), offscreenTarget.Get());

    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    
    frame.commandList->ResourceBarrier(2, barriers);

    frame.commandList->Close();

    // Execute
    ID3D12CommandList* cmdLists[] = { frame.commandList.Get() };
    commandQueue->ExecuteCommandLists(1, cmdLists);

    // Present with no vsync
    swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

    // Signal fence
    frame.fenceValue++;
    commandQueue->Signal(frame.fence.Get(), frame.fenceValue);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void dx_cleanup() {
    running = false;
    
    // Wait for GPU to finish
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (frames[i].fence && frames[i].fenceEvent) {
            if (frames[i].fence->GetCompletedValue() < frames[i].fenceValue) {
                frames[i].fence->SetEventOnCompletion(frames[i].fenceValue, frames[i].fenceEvent);
                WaitForSingleObject(frames[i].fenceEvent, INFINITE);
            }
            CloseHandle(frames[i].fenceEvent);
        }
    }
}

#endif // _WIN32
