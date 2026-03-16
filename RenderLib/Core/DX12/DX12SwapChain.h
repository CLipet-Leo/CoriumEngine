#pragma once

class CORIUM_API DX12SwapChain
{
public:
    DX12SwapChain() = default;
    DX12SwapChain(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue,
                  ID3D12Device* device, HWND hWnd,
                  uint32_t width, uint32_t height, uint32_t frameCount,
                  DX12DescriptorHeaps* descriptorHeaps);

    void     Resize(ID3D12Device* device, uint32_t width, uint32_t height);
    void     Present(UINT syncInterval = 1);

    uint32_t                    GetCurrentBackBufferIndex() const;
    ID3D12Resource*             GetBackBuffer(uint32_t index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index) const;

private:
    void CreateRenderTargetViews(ID3D12Device* device);

    Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_swapChain;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;
    uint32_t                                            m_frameCount = 2;
    DX12DescriptorHeaps*                                m_descriptorHeaps = nullptr;
};
