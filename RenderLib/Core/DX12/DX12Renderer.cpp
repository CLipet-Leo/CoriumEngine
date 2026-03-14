#include "pch.h"
#include "DX12Renderer.h"
#include "../Geometry/Types.h"
#include "../Geometry/BufferGeometry.h"
#include "../Geometry/BoxGeometry.h"
#include <chrono>
#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

static void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("HRESULT failure");
    }
}

extern "C"
{
    IRenderer* CreateRenderer() { return new DX12Renderer(); }
    void DestroyRenderer(IRenderer* r) { delete r; }
}

DX12Renderer::DX12Renderer() = default;

DX12Renderer::~DX12Renderer()
{
    Shutdown();
}

bool DX12Renderer::Init(HWND hWnd, uint32_t width, uint32_t height)
{
    try
    {
        m_width = width;
        m_height = height;

#if defined(_DEBUG)
        m_debug = std::make_unique<DX12Debug>();
#endif

        m_factory = std::make_unique<DXGIFactory>();
        m_adapter = std::make_unique<DXGIAdapter>(m_factory->GetBestAdapter());
        m_device = std::make_unique<DX12Device>(*m_adapter);
        m_commandQueue = std::make_unique<DX12CommandQueue>(m_device->Get());
        m_swapChain = std::make_unique<DX12SwapChain>(
            m_factory->Get(), m_commandQueue->Get(), m_device->Get(),
            hWnd, width, height, m_frameCount);
        m_commandObjects = std::make_unique<DX12CommandObjects>(m_device->Get());
        m_fence = std::make_unique<DX12Fence>(m_device->Get());
        m_depthStencil = std::make_unique<DX12DepthStencil>(m_device->Get(), width, height);

        if (!CreatePipeline())
            return false;

        if (!CreateCubeResources())
            return false;

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

        m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
        m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

        UpdateSceneConstants();

        return m_device->IsValid();
    }
    catch (...)
    {
        Shutdown();
        return false;
    }
}

bool DX12Renderer::CreatePipeline()
{
    const char* shaderSource = R"(
cbuffer SceneCB : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), gWorldViewProj);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    ThrowIfFailed(D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr,
        "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errorBlob));

    ThrowIfFailed(D3DCompile(shaderSource, strlen(shaderSource), nullptr, nullptr, nullptr,
        "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errorBlob));

    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameter.Descriptor.ShaderRegister = 0;
    rootParameter.Descriptor.RegisterSpace = 0;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSignature,
        &errorBlob));

    ThrowIfFailed(m_device->Get()->CreateRootSignature(
        0,
        serializedRootSignature->GetBufferPointer(),
        serializedRootSignature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(m_device->Get()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));

    return true;
}

bool DX12Renderer::CreateCubeResources()
{
    BoxGeometry cube(1.0f);
    const auto& positions = cube.GetVertices();

    std::vector<CubeVertex> vertices;
    vertices.reserve(positions.size());

    for (const auto& p : positions)
    {
        const float r = (p.x + 0.5f) + 0.25f;
        const float g = (p.y + 0.5f) + 0.25f;
        const float b = (p.z + 0.5f) + 0.25f;
        vertices.push_back({ XMFLOAT3(p.x, p.y, p.z), XMFLOAT4(r, g, b, 1.0f) });
    }

    m_cubeVertexCount = static_cast<UINT>(vertices.size());
    const UINT vertexBufferSize = static_cast<UINT>(vertices.size() * sizeof(CubeVertex));

    const auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    const auto vertexBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

    ThrowIfFailed(m_device->Get()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &vertexBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_cubeVertexBuffer)));

    UINT8* vertexDataBegin = nullptr;
    ThrowIfFailed(m_cubeVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataBegin)));
    memcpy(vertexDataBegin, vertices.data(), vertexBufferSize);
    m_cubeVertexBuffer->Unmap(0, nullptr);

    m_cubeVertexBufferView.BufferLocation = m_cubeVertexBuffer->GetGPUVirtualAddress();
    m_cubeVertexBufferView.StrideInBytes = sizeof(CubeVertex);
    m_cubeVertexBufferView.SizeInBytes = vertexBufferSize;

    const UINT constantBufferSize = (sizeof(SceneConstants) + 255) & ~255;
    const auto constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);

    ThrowIfFailed(m_device->Get()->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_sceneConstantBuffer)));

    ThrowIfFailed(m_sceneConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantBuffer)));

    return true;
}

void DX12Renderer::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_fence->WaitForGpu(m_commandQueue->Get());

    m_swapChain->Resize(m_device->Get(), width, height);
    m_depthStencil->Resize(m_device->Get(), width, height);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_width = width;
    m_height = height;
    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void DX12Renderer::UpdateSceneConstants()
{
    static auto startTime = std::chrono::high_resolution_clock::now();
    const auto now = std::chrono::high_resolution_clock::now();
    const float t = std::chrono::duration<float>(now - startTime).count();

    const XMMATRIX world = XMMatrixRotationY(t) * XMMatrixRotationX(0.5f * t);
    const XMVECTOR eye = XMVectorSet(0.0f, 1.5f, -3.0f, 1.0f);
    const XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

    const float aspect = m_height > 0 ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);

    const XMMATRIX wvp = XMMatrixTranspose(world * view * proj);
    XMStoreFloat4x4(&m_sceneConstants.WorldViewProj, wvp);

    memcpy(m_mappedConstantBuffer, &m_sceneConstants, sizeof(SceneConstants));
}

void DX12Renderer::Render()
{
    UpdateSceneConstants();

    m_commandObjects->Reset();
    ID3D12GraphicsCommandList* cmdList = m_commandObjects->GetCommandList();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_swapChain->GetBackBuffer(m_frameIndex);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(m_frameIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencil->GetDSVHandle();

    const float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    cmdList->RSSetViewports(1, &m_viewport);
    cmdList->RSSetScissorRects(1, &m_scissorRect);

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_cubeVertexBufferView);
    cmdList->SetGraphicsRootConstantBufferView(0, m_sceneConstantBuffer->GetGPUVirtualAddress());
    cmdList->DrawInstanced(m_cubeVertexCount, 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);

    m_commandObjects->Close();

    ID3D12CommandList* ppCommandLists[] = { cmdList };
    m_commandQueue->Get()->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_swapChain->Present(1);
    m_fence->WaitForGpu(m_commandQueue->Get());
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::Shutdown()
{
    if (m_fence && m_commandQueue)
        m_fence->WaitForGpu(m_commandQueue->Get());

    if (m_sceneConstantBuffer && m_mappedConstantBuffer)
    {
        m_sceneConstantBuffer->Unmap(0, nullptr);
        m_mappedConstantBuffer = nullptr;
    }

    m_sceneConstantBuffer.Reset();
    m_cubeVertexBuffer.Reset();
    m_pipelineState.Reset();
    m_rootSignature.Reset();

    m_depthStencil.reset();
    m_fence.reset();
    m_commandObjects.reset();
    m_swapChain.reset();
    m_commandQueue.reset();
    m_device.reset();
    m_adapter.reset();
    m_factory.reset();
    m_debug.reset();
}
