#include "pch.h"
#include "DX12SwapChain.h"

DX12SwapChain::DX12SwapChain(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue,
                              ID3D12Device* device, HWND hWnd,
                              uint32_t width, uint32_t height, uint32_t frameCount)
    : m_frameCount(frameCount)
{
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.BufferCount  = frameCount;
    desc.Width        = width;
    desc.Height       = height;
    desc.Format       = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    EVAL_HR(factory->CreateSwapChainForHwnd(commandQueue, hWnd, &desc, nullptr, nullptr, &swapChain1),
        "SwapChain creation failed");
    EVAL_HR(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER),
        "MakeWindowAssociation failed");
    EVAL_HR(swapChain1.As(&m_swapChain), "SwapChain3 query failed");

    CreateRenderTargetViews(device);
}

void DX12SwapChain::CreateRenderTargetViews(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = m_frameCount;
    heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    EVAL_HR(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)), "RTV Heap creation failed");

    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_renderTargets.resize(m_frameCount);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (uint32_t i = 0; i < m_frameCount; ++i)
    {
        EVAL_HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])), "SwapChain GetBuffer failed");
        device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void DX12SwapChain::Resize(ID3D12Device* device, uint32_t width, uint32_t height)
{
    m_renderTargets.clear();
    m_rtvHeap.Reset();
    EVAL_HR(m_swapChain->ResizeBuffers(m_frameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0),
        "SwapChain ResizeBuffers failed");
    CreateRenderTargetViews(device);
}

void DX12SwapChain::Present(UINT syncInterval)
{
    EVAL_HR(m_swapChain->Present(syncInterval, 0), "SwapChain Present failed");
}

uint32_t DX12SwapChain::GetCurrentBackBufferIndex() const
{
    return m_swapChain->GetCurrentBackBufferIndex();
}

ID3D12Resource* DX12SwapChain::GetBackBuffer(uint32_t index) const
{
    return m_renderTargets[index].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12SwapChain::GetRTVHandle(uint32_t index) const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(static_cast<INT>(index), m_rtvDescriptorSize);
    return handle;
}
