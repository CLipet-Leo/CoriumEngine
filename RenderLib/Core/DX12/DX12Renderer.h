#pragma once

class DX12ImGui;

class CORIUM_API DX12Renderer : public IRenderer
{
public:
    DX12Renderer();
    virtual ~DX12Renderer();

    bool Init(HWND hWnd, uint32_t width, uint32_t height) override;
    void OnResize(uint32_t width, uint32_t height)         override;
    void Render(Scene& scene)                              override;
    void Shutdown()                                        override;
    bool HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    uint32_t RegisterMesh(const std::string& name, const BufferGeometry& geometry) override;
    uint32_t CreateMaterial(const MaterialDesc& desc) override;
    void     RefreshMaterial(uint32_t index) override;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineFor(const MaterialDesc& mat);
    bool CreatePipeline();
    bool CreateConstantBuffers();
    bool CreateSrvHeap();

private:
    // --- Core DX12 ---
    std::unique_ptr<DX12Debug>          m_debug;
    std::unique_ptr<DXGIFactory>        m_factory;
    std::unique_ptr<DXGIAdapter>        m_adapter;
    std::unique_ptr<DX12Device>         m_device;
    std::unique_ptr<DX12CommandQueue>   m_commandQueue;
    std::unique_ptr<DX12DescriptorHeaps> m_descriptorHeaps;
    std::unique_ptr<DX12MemoryManager>  m_memoryManager;
    std::unique_ptr<DX12SwapChain>      m_swapChain;
    std::unique_ptr<DX12CommandObjects> m_commandObjects;
    std::unique_ptr<DX12Fence>          m_fence;
    std::unique_ptr<DX12DepthStencil>   m_depthStencil;
    std::unique_ptr<DX12ImGui>          m_imgui;

    // --- Pipeline ---
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob>            m_vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob>            m_pixelShader;
    DX12PSOCache                                m_psoCache;

    // --- Heap SRV (ImGui + futures textures) ---
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;

    // --- Constant buffers partagés (scene + lights) ---
    struct PerFrameCB
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB;
        Microsoft::WRL::ComPtr<ID3D12Resource> lightCB;
        UINT8* sceneMapped = nullptr;
        UINT8* lightMapped = nullptr;
    };
    std::vector<PerFrameCB> m_perFrameCBs;

    // --- Ressources et systèmes ---
    MeshRegistry    m_meshRegistry;
    MaterialRegistry m_materialRegistry;
    TransformSystem m_transformSystem;
    CameraSystem    m_cameraSystem;
    RenderSystem    m_renderSystem;
    LightSystem     m_lightSystem;
    SceneConstants  m_sceneConstants = {};

    // --- Éditeur ---
    EditorUI     m_editorUI;
    std::wstring m_adapterName;

    uint32_t       m_frameCount = 2;
    uint32_t       m_frameIndex = 0;
    uint32_t       m_width = 0;
    uint32_t       m_height = 0;
    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT     m_scissorRect = {};
};