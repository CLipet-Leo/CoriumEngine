#pragma once

class DX12ImGui;

class CORIUM_API DX12Renderer : public IRenderer
{
public:
    DX12Renderer();
    virtual ~DX12Renderer();

    bool Init(HWND hWnd, uint32_t width, uint32_t height) override;
    void OnResize(uint32_t width, uint32_t height)         override;
    void Render(Scene& scene, const CameraView* view)      override;
    void Shutdown()                                        override;
    RendererStats GetStats() const override;

    void     InitUI(ImGuiContext* context, void* (*allocFunc)(size_t, void*),
                    void (*freeFunc)(void*, void*), void* allocUserData) override;
    void     ShutdownUI() override;
    void     BeginUIFrame() override;
    void     SetSceneSize(uint32_t width, uint32_t height) override;
    uint64_t GetSceneTexture() const override;

    uint32_t RegisterMesh(const MeshDesc& desc) override;
    uint32_t CreateMaterial(const MaterialDesc& desc) override;
    void     RefreshMaterial(uint32_t index) override;
    const MeshDesc*     GetMeshDesc(uint32_t index) const override;
    const MaterialDesc* GetMaterialDesc(uint32_t index) const override;

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineFor(const MaterialDesc& mat);
    bool CreatePipeline();
    bool CreateConstantBuffers();
    bool CreateSrvHeap();
    // Applique la taille demandée par SetSceneSize (FlushGpu si elle change)
    void ApplySceneSize();
    // Scène dans la cible liée (RTV + DSV), avec viewport/scissor à sa taille
    void DrawScene(Scene& scene, ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                   uint32_t width, uint32_t height, const Mat4& viewProj);

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

    // --- UI éditeur (nullptr sans InitUI) ---
    std::unique_ptr<DX12ImGui>          m_imgui;
    std::unique_ptr<DX12RenderTarget>   m_sceneTarget;  // scène offscreen, SRV au slot 1
    uint32_t                            m_requestedSceneWidth = 0;
    uint32_t                            m_requestedSceneHeight = 0;

    // --- Pipeline ---
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob>            m_vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob>            m_pixelShader;
    DX12PSOCache                                m_psoCache;

    // --- Heap SRV (slot 0 : police ImGui, slot 1 : scène offscreen) ---
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
    CameraSystem    m_cameraSystem;
    RenderSystem    m_renderSystem;
    LightSystem     m_lightSystem;
    SceneConstants  m_sceneConstants = {};

    std::wstring    m_adapterName;

    uint32_t       m_frameCount = 2;
    uint32_t       m_frameIndex = 0;
    uint32_t       m_width = 0;   // swapchain
    uint32_t       m_height = 0;
};