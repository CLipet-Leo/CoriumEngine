#pragma once

using Microsoft::WRL::ComPtr;

extern "C" {
    CORIUM_API IRenderer* CreateRenderer();
    CORIUM_API void DestroyRenderer(IRenderer*);
}

class CORIUM_API DX12Renderer : public IRenderer
{
public:
    DX12Renderer();
    virtual ~DX12Renderer();

    virtual bool Init(HWND hWnd, uint32_t width, uint32_t height) override;
    virtual void OnResize(uint32_t width, uint32_t height) override;
    virtual void Render() override;
    virtual void Shutdown() override;

private:
    bool CreateDevice();
    bool CreateCommandObjects();
    bool CreateSwapChain(HWND hWnd);
    bool CreateRenderTargetViews();
    void WaitForGpu();

private:
    ComPtr<IDXGIFactory4>       m_factory;
    ComPtr<ID3D12Device>        m_device;
    ComPtr<ID3D12CommandQueue>  m_commandQueue;
    ComPtr<IDXGISwapChain3>     m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT                        m_rtvDescriptorSize = 0;

    std::vector<ComPtr<ID3D12Resource>> m_renderTargets;

    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Synchronisation GPU-CPU
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    uint32_t m_frameCount = 2;
    uint32_t m_frameIndex = 0;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};