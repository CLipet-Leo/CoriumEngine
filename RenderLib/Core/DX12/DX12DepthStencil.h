#pragma once

class CORIUM_API DX12DepthStencil
{
public:
    static constexpr DXGI_FORMAT Format = DXGI_FORMAT_D32_FLOAT;

    DX12DepthStencil() = default;
    DX12DepthStencil(ID3D12Device* device, uint32_t width, uint32_t height,
                     DX12DescriptorHeaps* descriptorHeaps,
                     DX12MemoryManager* memoryManager);
    ~DX12DepthStencil();

    void Resize(ID3D12Device* device, uint32_t width, uint32_t height);
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;

private:
    void Create(ID3D12Device* device, uint32_t width, uint32_t height);

    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;
    DX12DescriptorHeaps*                   m_descriptorHeaps = nullptr;
    DX12MemoryManager*                     m_memoryManager = nullptr;
};
