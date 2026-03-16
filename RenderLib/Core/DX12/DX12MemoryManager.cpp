#include "pch.h"
#include "DX12MemoryManager.h"

DX12MemoryManager::~DX12MemoryManager()
{
    Shutdown();
}

HRESULT DX12MemoryManager::Initialize(ID3D12Device* device, IDXGIAdapter1* adapter)
{
    if (!device)
        return E_POINTER;

    m_device = device;

#if CORIUM_HAS_D3D12MA
    D3D12MA::ALLOCATOR_DESC desc = {};
    desc.pDevice = device;
    desc.pAdapter = adapter;

    const HRESULT hr = D3D12MA::CreateAllocator(&desc, &m_allocator);
    if (SUCCEEDED(hr) && m_allocator)
    {
        m_usesD3D12MA = true;
        return S_OK;
    }
#endif

    m_usesD3D12MA = false;
    return S_OK;
}

HRESULT DX12MemoryManager::CreateBuffer(
    DX12MemoryType memoryType,
    UINT64 size,
    D3D12_RESOURCE_STATES initialState,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outResource)
{
    const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    return CreateResource(memoryType, desc, initialState, nullptr, outResource);
}

HRESULT DX12MemoryManager::CreateResource(
    DX12MemoryType memoryType,
    const D3D12_RESOURCE_DESC& desc,
    D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE* clearValue,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outResource)
{
    if (!m_device)
        return E_FAIL;

    outResource.Reset();

#if CORIUM_HAS_D3D12MA
    if (m_allocator)
    {
        D3D12MA::ALLOCATION_DESC allocationDesc = {};
        allocationDesc.HeapType = ToHeapType(memoryType);

        D3D12MA::Allocation* allocation = nullptr;
        const HRESULT hr = m_allocator->CreateResource(
            &allocationDesc,
            &desc,
            initialState,
            clearValue,
            &allocation,
            IID_PPV_ARGS(&outResource));

        if (SUCCEEDED(hr) && allocation)
        {
            m_allocations.push_back(allocation);
        }
        return hr;
    }
#endif

    const auto heapProps = CD3DX12_HEAP_PROPERTIES(ToHeapType(memoryType));
    return m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialState,
        clearValue,
        IID_PPV_ARGS(&outResource));
}

bool DX12MemoryManager::IsUsingD3D12MA() const
{
    return m_usesD3D12MA;
}

void DX12MemoryManager::Shutdown()
{
#if CORIUM_HAS_D3D12MA
    for (auto* allocation : m_allocations)
    {
        if (allocation)
            allocation->Release();
    }
    m_allocations.clear();

    if (m_allocator)
    {
        m_allocator->Release();
        m_allocator = nullptr;
    }
#endif

    m_device = nullptr;
}

D3D12_HEAP_TYPE DX12MemoryManager::ToHeapType(DX12MemoryType memoryType)
{
    switch (memoryType)
    {
    case DX12MemoryType::Upload: return D3D12_HEAP_TYPE_UPLOAD;
    case DX12MemoryType::Readback: return D3D12_HEAP_TYPE_READBACK;
    default: return D3D12_HEAP_TYPE_DEFAULT;
    }
}
