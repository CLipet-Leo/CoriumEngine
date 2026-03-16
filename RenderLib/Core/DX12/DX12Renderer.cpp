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

#if defined(_DEBUG)
        m_debug->SetupInfoQueue(m_device->Get());
#endif

        m_descriptorHeaps = std::make_unique<DX12DescriptorHeaps>();
        m_descriptorHeaps->Initialize(m_device->Get(), m_frameCount, 1024);

        m_memoryManager = std::make_unique<DX12MemoryManager>();
        EVAL_HR(m_memoryManager->Initialize(m_device->Get(), m_adapter->Get()), "Memory manager initialization failed");

        m_psoCache = std::make_unique<DX12PSOCache>();

        m_commandQueue = std::make_unique<DX12CommandQueue>(m_device->Get());
        m_swapChain = std::make_unique<DX12SwapChain>(
            m_factory->Get(), m_commandQueue->Get(), m_device->Get(),
            hWnd, width, height, m_frameCount, m_descriptorHeaps.get());

        m_commandObjects.resize(m_frameCount);
        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            m_commandObjects[i] = std::make_unique<DX12CommandObjects>(m_device->Get());
        }

        m_fence = std::make_unique<DX12Fence>(m_device->Get(), m_frameCount);
        m_depthStencil = std::make_unique<DX12DepthStencil>(m_device->Get(), width, height, m_descriptorHeaps.get(), m_memoryManager.get());

        if (!CreatePipeline()) return false;
        if (!CreateCubeResources()) return false;

        m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
        m_viewport = { 0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f };
        m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

        UpdateSceneConstants();
        return m_device->IsValid();
    }
    catch (const DX12Exception& e)
    {
        OutputDebugStringA(e.what());
        Shutdown();
        return false;
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

    D3D12_DESCRIPTOR_RANGE cbvRange = {};
    cbvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvRange.NumDescriptors = 1;
    cbvRange.BaseShaderRegister = 0;
    cbvRange.RegisterSpace = 0;
    cbvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter = {};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;
    rootParameter.DescriptorTable.pDescriptorRanges = &cbvRange;
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

    m_cubePipelineState = m_psoCache->GetOrCreate("CubeSolid", m_device->Get(), psoDesc);

    return m_cubePipelineState != nullptr;
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

    EVAL_HR(m_memoryManager->CreateBuffer(
        DX12MemoryType::Upload,
        vertexBufferSize,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        m_cubeVertexBuffer),
        "Cube vertex buffer allocation failed");

    UINT8* vertexDataBegin = nullptr;
    ThrowIfFailed(m_cubeVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataBegin)));
    memcpy(vertexDataBegin, vertices.data(), vertexBufferSize);
    m_cubeVertexBuffer->Unmap(0, nullptr);

    m_cubeVertexBufferView.BufferLocation = m_cubeVertexBuffer->GetGPUVirtualAddress();
    m_cubeVertexBufferView.StrideInBytes = sizeof(CubeVertex);
    m_cubeVertexBufferView.SizeInBytes = vertexBufferSize;

    const UINT constantBufferSize = (sizeof(SceneConstants) + 255) & ~255;
    EVAL_HR(m_memoryManager->CreateBuffer(
        DX12MemoryType::Upload,
        constantBufferSize,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        m_sceneConstantBuffer),
        "Scene constant buffer allocation failed");

    ThrowIfFailed(m_sceneConstantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedConstantBuffer)));

    DX12DescriptorHeaps::Allocation cbvAllocation;
    if (!m_descriptorHeaps->AllocateCbvSrvUav(cbvAllocation))
        return false;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = m_sceneConstantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;
    m_device->Get()->CreateConstantBufferView(&cbvDesc, cbvAllocation.CpuHandle);
    m_sceneCbvGpuHandle = cbvAllocation.GpuHandle;

    return true;
}

void DX12Renderer::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_fence->FlushGpu(m_commandQueue->Get());

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
    m_fence->WaitForFrame(m_frameIndex);

    UpdateSceneConstants();

    m_commandObjects[m_frameIndex]->Reset();
    ID3D12GraphicsCommandList* cmdList = m_commandObjects[m_frameIndex]->GetCommandList();

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_swapChain->GetBackBuffer(m_frameIndex);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(m_frameIndex);
    const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencil->GetDSVHandle();

    const float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    cmdList->RSSetViewports(1, &m_viewport);
    cmdList->RSSetScissorRects(1, &m_scissorRect);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_descriptorHeaps->GetCbvSrvUavHeap() };
    cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_cubePipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_cubeVertexBufferView);
    cmdList->SetGraphicsRootDescriptorTable(0, m_sceneCbvGpuHandle);
    cmdList->DrawInstanced(m_cubeVertexCount, 1, 0, 0);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    cmdList->ResourceBarrier(1, &barrier);

    m_commandObjects[m_frameIndex]->Close();
    ID3D12CommandList* lists[] = { m_commandObjects[m_frameIndex]->GetCommandList() };
    m_commandQueue->Get()->ExecuteCommandLists(_countof(lists), lists);

    m_swapChain->Present(1);
    m_fence->Signal(m_commandQueue->Get(), m_frameIndex);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::Shutdown()
{
    if (m_fence && m_commandQueue)
        m_fence->FlushGpu(m_commandQueue->Get());

    if (m_sceneConstantBuffer && m_mappedConstantBuffer)
    {
        m_sceneConstantBuffer->Unmap(0, nullptr);
        m_mappedConstantBuffer = nullptr;
    }

    if (m_psoCache)
        m_psoCache->Clear();

    m_sceneConstantBuffer.Reset();
    m_cubeVertexBuffer.Reset();
    m_cubePipelineState.Reset();
    m_rootSignature.Reset();

    m_depthStencil.reset();
    m_fence.reset();
    m_commandObjects.clear();
    m_swapChain.reset();
    m_psoCache.reset();

    if (m_memoryManager)
        m_memoryManager->Shutdown();
    m_memoryManager.reset();

    m_descriptorHeaps.reset();
    m_commandQueue.reset();
    m_device.reset();
    m_adapter.reset();
    m_factory.reset();
    m_debug.reset();
}
