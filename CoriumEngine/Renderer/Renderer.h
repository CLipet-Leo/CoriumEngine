#pragma once

// Implémentation d'IRenderer, indépendante de l'API graphique : ne connaît que la RHI.
class Renderer : public IRenderer
{
public:
    explicit Renderer(RendererBackend backend);
    ~Renderer() override;

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
    // Pipeline partagé entre matériaux de même état (clé MaterialDesc::GetPipelineKey)
    RhiPipeline* GetPipelineFor(const MaterialDesc& mat);
    void CreateDepth(uint32_t width, uint32_t height);
    // Texture de validation du binding (damier), copiée au début de la première frame
    void CreateCheckerTexture();
    // Applique la taille demandée par SetSceneSize (WaitIdle si elle change)
    void ApplySceneSize();
    // Scène dans target (+ profondeur), à l'état RenderTarget
    void DrawScene(Scene& scene, RhiCommandList* cmd, RhiTexture* target, const Mat4& viewProj);

private:
    RendererBackend            m_backend;
    std::unique_ptr<RhiDevice> m_device;

    // --- Cibles ---
    std::unique_ptr<RhiTexture> m_depth;  // taille de la cible de la scène
    RhiState                    m_depthState = RhiState::Undefined;

    // --- UI éditeur (nullptr sans InitUI) ---
    std::unique_ptr<RhiUI>      m_ui;
    std::unique_ptr<RhiTexture> m_sceneTarget; // scène offscreen, affichée par le panneau Viewport
    RhiState                    m_sceneTargetState = RhiState::Undefined;
    uint64_t                    m_sceneTextureId = 0;
    uint32_t                    m_requestedSceneWidth = 0;
    uint32_t                    m_requestedSceneHeight = 0;

    // --- Pipeline ---
    std::unique_ptr<RhiPipelineLayout> m_layout;
    std::vector<uint8_t>               m_vertexShader;
    std::vector<uint8_t>               m_pixelShader;
    std::unordered_map<std::string, std::unique_ptr<RhiPipeline>> m_pipelines;

    // --- Ressources du matériau ---
    std::unique_ptr<RhiTexture> m_checker;
    std::unique_ptr<RhiBuffer>  m_checkerStaging; // non nul tant que la copie n'est pas enregistrée
    std::unique_ptr<RhiSampler> m_sampler;

    // --- Ressources par frame en vol (space0) ---
    struct FrameData
    {
        std::unique_ptr<RhiBuffer>     sceneCB;
        std::unique_ptr<RhiBuffer>     lightCB;
        std::unique_ptr<RhiBindingSet> bindings;
        // Staging libérés quand la frame est terminée sur le GPU (au retour de BeginFrame)
        std::vector<std::unique_ptr<RhiBuffer>> release;
    };
    std::array<FrameData, kRhiFramesInFlight> m_frames;

    // --- Ressources et systèmes ---
    MeshRegistry     m_meshRegistry;
    MaterialRegistry m_materialRegistry;
    CameraSystem     m_cameraSystem;
    RenderSystem     m_renderSystem;
    LightSystem      m_lightSystem;
    SceneConstants   m_sceneConstants = {};

    uint32_t m_width = 0;   // swapchain
    uint32_t m_height = 0;
};
