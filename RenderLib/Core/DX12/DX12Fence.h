#pragma once

class CORIUM_API DX12Fence
{
public:
    DX12Fence() = default;
    explicit DX12Fence(ID3D12Device* device);
    ~DX12Fence();

    void WaitForGpu(ID3D12CommandQueue* commandQueue);

private:
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64                              m_fenceValue = 0;
    HANDLE                              m_fenceEvent = nullptr;
};
