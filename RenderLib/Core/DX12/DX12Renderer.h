#pragma once

class DX12ImGui;

extern "C" {
    CORIUM_API IRenderer* CreateRenderer();
    CORIUM_API void       DestroyRenderer(IRenderer*);
}

class CORIUM_API DX12Renderer : public IRenderer
{
public:
    DX12Renderer();
    virtual ~DX12Renderer();

    virtual bool Init(HWND hWnd, uint32_t width, uint32_t height) override;
    virtual void OnResize(uint32_t width, uint32_t height)         override;
    virtual void Render()                                          override;
    virtual void Shutdown()                                        override;

    Scene& GetScene() { return m_scene; }

    // Enregistre un mesh ; l'upload GPU est fait au début de la prochaine frame
    uint32_t RegisterMesh(const std::string& name, const BufferGeometry& geometry);

    // Crée un matériau ; le PSO est partagé entre matériaux de même état
    uint32_t CreateMaterial(const MaterialDesc& desc);
    // À appeler après modification de cull/fill/blend d'un matériau
    void     RefreshMaterial(uint32_t index);

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineFor(const MaterialDesc& mat);
    bool CreatePipeline();
    bool CreateConstantBuffers();
    bool CreateSrvHeap();
    void LoadDefaultScene();
    void UpdateSceneConstants();
    void DrawEditorUI();
    void DrawScenePanel();
    void DrawEntityNode(EntityID id);
    void DrawInspector(EntityID id);

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
    uint32_t m_srvDescriptorSize = 0;
    uint32_t m_srvHeapUsed = 0; // prochain slot libre

    // --- Constant buffers partagés (scene + lights) ---
    struct PerFrameCB
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB;
        Microsoft::WRL::ComPtr<ID3D12Resource> lightCB;
        UINT8* sceneMapped = nullptr;
        UINT8* lightMapped = nullptr;
    };
    std::vector<PerFrameCB> m_perFrameCBs;

    // --- Scène et systèmes ---
    Scene           m_scene;
    MeshRegistry    m_meshRegistry;
    MaterialRegistry m_materialRegistry;
    TransformSystem m_transformSystem;
    RenderSystem    m_renderSystem;
    LightSystem     m_lightSystem;
    SceneConstants  m_sceneConstants = {};

    // --- État éditeur ---
    EntityID m_selectedEntity = NULL_ENTITY;

    uint32_t       m_frameCount = 2;
    uint32_t       m_frameIndex = 0;
    uint32_t       m_width = 0;
    uint32_t       m_height = 0;
    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT     m_scissorRect = {};
};