#pragma once

class CORIUM_API DX12SwapChain
{
public:
    DX12SwapChain() = default;
    DX12SwapChain(IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue,
                  ID3D12Device* device, HWND hWnd,
                  uint32_t width, uint32_t height, uint32_t frameCount);

    void     Resize(ID3D12Device* device, uint32_t width, uint32_t height);
    void     Present(UINT syncInterval = 1);

    uint32_t                    GetCurrentBackBufferIndex() const;
    ID3D12Resource*             GetBackBuffer(uint32_t index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32_t index) const;

private:
    void CreateRenderTargetViews(ID3D12Device* device);

    Microsoft::WRL::ComPtr<IDXGISwapChain3>             m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>        m_rtvHeap;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;
    UINT                                                m_rtvDescriptorSize = 0;
    uint32_t                                            m_frameCount = 2;
};
