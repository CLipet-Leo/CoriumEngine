#pragma once

class CORIUM_API DX12CommandObjects
{
public:
    DX12CommandObjects() = default;
    explicit DX12CommandObjects(ID3D12Device* device, uint32_t frameCount);

    void Reset(uint32_t frameIndex);
    void Close();

    ID3D12CommandAllocator*    GetAllocator(uint32_t frameIndex)    const { return m_allocators[frameIndex].Get(); }
    ID3D12GraphicsCommandList* GetCommandList()  const { return m_commandList.Get(); }

private:
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_allocators;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>           m_commandList;
};
