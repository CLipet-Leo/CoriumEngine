#pragma once

class CORIUM_API DX12CommandQueue
{
public:
    DX12CommandQueue() = default;
    explicit DX12CommandQueue(ID3D12Device* device);

    ID3D12CommandQueue* Get() const { return m_commandQueue.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
};
