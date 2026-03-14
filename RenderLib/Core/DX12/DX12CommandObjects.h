#pragma once

class CORIUM_API DX12CommandObjects
{
public:
    DX12CommandObjects() = default;
    explicit DX12CommandObjects(ID3D12Device* device);

    void Reset();
    void Close();

    ID3D12CommandAllocator*    GetAllocator()    const { return m_allocator.Get(); }
    ID3D12GraphicsCommandList* GetCommandList()  const { return m_commandList.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
};
