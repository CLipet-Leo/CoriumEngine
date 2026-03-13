#include "pch.h"
#include "DX12Renderer.h"

extern "C"
{
    IRenderer* CreateRenderer()  { return new DX12Renderer(); }
    void DestroyRenderer(IRenderer* r) { delete r; }
}

DX12Renderer::DX12Renderer() = default;

DX12Renderer::~DX12Renderer()
{
    Shutdown();
}

bool DX12Renderer::Init(HWND hWnd, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

#if defined(_DEBUG)
    m_debug = std::make_unique<DX12Debug>();
#endif

    m_factory       = std::make_unique<DXGIFactory>();
    m_adapter       = std::make_unique<DXGIAdapter>(m_factory->GetBestAdapter());
    m_device        = std::make_unique<DX12Device>(*m_adapter);
    m_commandQueue  = std::make_unique<DX12CommandQueue>(m_device->Get());
    m_swapChain     = std::make_unique<DX12SwapChain>(
                          m_factory->Get(), m_commandQueue->Get(), m_device->Get(),
                          hWnd, width, height, m_frameCount);
    m_commandObjects = std::make_unique<DX12CommandObjects>(m_device->Get());
    m_fence         = std::make_unique<DX12Fence>(m_device->Get());
    m_depthStencil  = std::make_unique<DX12DepthStencil>(m_device->Get(), width, height);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

    return m_device->IsValid();
}

void DX12Renderer::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_fence->WaitForGpu(m_commandQueue->Get());

    m_swapChain->Resize(m_device->Get(), width, height);
    m_depthStencil->Resize(m_device->Get(), width, height);

    m_frameIndex  = m_swapChain->GetCurrentBackBufferIndex();
    m_width       = width;
    m_height      = height;
    m_viewport    = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void DX12Renderer::Render()
{
    m_commandObjects->Reset();
    ID3D12GraphicsCommandList* cmdList = m_commandObjects->GetCommandList();

    // Transition: Present ? RenderTarget
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_swapChain->GetBackBuffer(m_frameIndex);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_swapChain->GetRTVHandle(m_frameIndex);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_depthStencil->GetDSVHandle();

    const float clearColor[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    cmdList->RSSetViewports(1, &m_viewport);
    cmdList->RSSetScissorRects(1, &m_scissorRect);

    // --- Ici on dessinera les objets 3D ---

    // Transition: RenderTarget ? Present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
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

    // Libération dans l'ordre inverse d'initialisation
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
