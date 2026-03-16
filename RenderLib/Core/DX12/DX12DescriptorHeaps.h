#pragma once

class CORIUM_API DX12DescriptorHeaps
{
public:
    struct Allocation
    {
        D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle{};
        UINT Index = 0;
    };

    bool Initialize(ID3D12Device* device, uint32_t frameCount, UINT cbvSrvUavCapacity = 1024);
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;
    bool AllocateCbvSrvUav(Allocation& outAllocation);
    ID3D12DescriptorHeap* GetCbvSrvUavHeap() const;

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;

    UINT m_rtvDescriptorSize = 0;
    UINT m_cbvSrvUavDescriptorSize = 0;
    UINT m_cbvSrvUavCapacity = 0;
    UINT m_nextCbvSrvUavIndex = 0;
};
