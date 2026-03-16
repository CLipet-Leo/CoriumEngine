#pragma once

#include <vector>

#if __has_include(<D3D12MemAlloc.h>)
#include <D3D12MemAlloc.h>
#define CORIUM_HAS_D3D12MA 1
#else
#define CORIUM_HAS_D3D12MA 0
#endif

enum class DX12MemoryType
{
    Default,
    Upload,
    Readback
};

class CORIUM_API DX12MemoryManager
{
public:
    DX12MemoryManager() = default;
    ~DX12MemoryManager();

    HRESULT Initialize(ID3D12Device* device, IDXGIAdapter1* adapter);

    HRESULT CreateBuffer(
        DX12MemoryType memoryType,
        UINT64 size,
        D3D12_RESOURCE_STATES initialState,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outResource);

    HRESULT CreateResource(
        DX12MemoryType memoryType,
        const D3D12_RESOURCE_DESC& desc,
        D3D12_RESOURCE_STATES initialState,
        const D3D12_CLEAR_VALUE* clearValue,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outResource);

    bool IsUsingD3D12MA() const;
    void Shutdown();

private:
    static D3D12_HEAP_TYPE ToHeapType(DX12MemoryType memoryType);

private:
    ID3D12Device* m_device = nullptr;
    bool m_usesD3D12MA = false;

#if CORIUM_HAS_D3D12MA
    D3D12MA::Allocator* m_allocator = nullptr;
    std::vector<D3D12MA::Allocation*> m_allocations;
#endif
};
