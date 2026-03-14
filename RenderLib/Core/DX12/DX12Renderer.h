#pragma once

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
    bool CreateCubeResources();
    bool CreatePipeline();
    void UpdateSceneConstants();

private:
    struct CubeVertex
    {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT4 Color;
    };

    struct SceneConstants
    {
        DirectX::XMFLOAT4X4 WorldViewProj;
    };

    std::unique_ptr<DX12Debug>          m_debug;
    std::unique_ptr<DXGIFactory>        m_factory;
    std::unique_ptr<DXGIAdapter>        m_adapter;
    std::unique_ptr<DX12Device>         m_device;
    std::unique_ptr<DX12CommandQueue>   m_commandQueue;
    std::unique_ptr<DX12SwapChain>      m_swapChain;
    std::unique_ptr<DX12CommandObjects> m_commandObjects;
    std::unique_ptr<DX12Fence>          m_fence;
    std::unique_ptr<DX12DepthStencil>   m_depthStencil;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_cubeVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW               m_cubeVertexBufferView = {};
    UINT                                   m_cubeVertexCount = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_sceneConstantBuffer;
    SceneConstants                         m_sceneConstants = {};
    UINT8*                                 m_mappedConstantBuffer = nullptr;

    uint32_t       m_frameCount = 2;
    uint32_t       m_frameIndex = 0;
    uint32_t       m_width      = 0;
    uint32_t       m_height     = 0;

    D3D12_VIEWPORT m_viewport    = {};
    D3D12_RECT     m_scissorRect = {};
};
