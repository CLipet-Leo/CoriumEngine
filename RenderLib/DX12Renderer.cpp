#include "pch.h"
#include "DX12Renderer.h"

static void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::runtime_error("HRESULT failure");
    }
}

// Fonctions export / import (pour que l’application puisse créer ce renderer)
extern "C"
{
    IRenderer* CreateRenderer()
    {
        return new DX12Renderer();
    }

    void DestroyRenderer(IRenderer* r)
    {
        delete r;
    }
}

DX12Renderer::DX12Renderer()
{
}

DX12Renderer::~DX12Renderer()
{
    // Veiller à bien tout libérer
    Shutdown();
}

bool DX12Renderer::Init(HWND hWnd, uint32_t width, uint32_t height)
{
    m_width = width;
    m_height = height;

    if (!CreateDevice())
        return false;

    if (!CreateCommandObjects())
        return false;

    if (!CreateSwapChain(hWnd))
        return false;

    if (!CreateRenderTargetViews())
        return false;

    // Créer l’event pour la fence
    m_fenceValue = 1;
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
        return false;

    return true;
}

void DX12Renderer::OnResize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    // Attendre que le GPU finisse avant de toucher les ressources
    WaitForGpu();

    m_renderTargets.clear();

    // Resize du swap chain
    ThrowIfFailed(m_swapChain->ResizeBuffers(
        m_frameCount,
        width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Refaire les RTVs
    CreateRenderTargetViews();

    m_width = width;
    m_height = height;
}

void DX12Renderer::Render()
{
    // Reset command allocator / command list
    ThrowIfFailed(m_commandAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

    // Transition d’état du buffer en mode rendu
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Obtenir le handle du RTV courant
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
        m_frameIndex,
        m_rtvDescriptorSize);

    const float clearColor[4] = { 0.2f, 0.3f, 0.3f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Pas de dessin pour l’instant — seul le clear

    // Transition en mode présentation
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Présenter
    ThrowIfFailed(m_swapChain->Present(1, 0));

    // Signal / attente pour la synchro
    const UINT64 currentFence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFence));
    m_fenceValue++;

    if (m_fence->GetCompletedValue() < currentFence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(currentFence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::Shutdown()
{
    // synchroniser pour être sûr que le GPU a terminé
    WaitForGpu();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    // ComPtr va libérer automatiquement les interfaces

    m_renderTargets.clear();
    m_swapChain.Reset();
    m_rtvHeap.Reset();
    m_device.Reset();
    m_commandQueue.Reset();
    m_commandAllocator.Reset();
    m_commandList.Reset();
    m_fence.Reset();
    m_factory.Reset();
}

bool DX12Renderer::CreateDevice()
{
    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));

    ComPtr<IDXGIAdapter1> adapter;
    for (UINT adapterIndex = 0; ; ++adapterIndex)
    {
        if (DXGI_ERROR_NOT_FOUND == m_factory->EnumAdapters1(adapterIndex, &adapter))
        {
            break;
        }
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // ignorer les adaptateurs software
            continue;
        }
        // vérifier que le device supporte D3D12
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    // Créer le device à partir de l’adapter trouvé
    ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

    return true;
}

bool DX12Renderer::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
    // On ferme la command list pour l’instant (elle doit être fermée avant Reset)
    ThrowIfFailed(m_commandList->Close());

    return true;
}

bool DX12Renderer::CreateSwapChain(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
    swapDesc.BufferCount = m_frameCount;
    swapDesc.Width = m_width;
    swapDesc.Height = m_height;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),    // queue
        hWnd,
        &swapDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // Désactiver Alt+Enter pour plein écran automatique
    ThrowIfFailed(m_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&m_swapChain));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    return true;
}

bool DX12Renderer::CreateRenderTargetViews()
{
    // Décrire le heap RTV
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = m_frameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    m_renderTargets.resize(m_frameCount);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (uint32_t i = 0; i < m_frameCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }

    return true;
}

void DX12Renderer::WaitForGpu()
{
    // Signaler la queue
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
    // Attendre que la fence atteigne cette valeur
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
    m_fenceValue++;
}