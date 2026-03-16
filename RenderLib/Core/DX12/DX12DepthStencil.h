#pragma once

class CORIUM_API DX12DepthStencil
{
public:
    DX12DepthStencil() = default;
    DX12DepthStencil(ID3D12Device* device, uint32_t width, uint32_t height,
                     DX12DescriptorHeaps* descriptorHeaps,
                     DX12MemoryManager* memoryManager);

    void Resize(ID3D12Device* device, uint32_t width, uint32_t height);
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

private:
    void Create(ID3D12Device* device, uint32_t width, uint32_t height);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;
    DX12DescriptorHeaps*                   m_descriptorHeaps = nullptr;
    DX12MemoryManager*                     m_memoryManager = nullptr;
};
