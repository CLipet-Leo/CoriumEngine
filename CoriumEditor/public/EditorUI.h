#pragma once

class EditorUI
{
public:
    EditorUI();

    // Thème de l'éditeur, après ImGui::CreateContext
    static void ApplyStyle();

    // Backend du prochain lancement, dans editor.ini à côté de l'exe
    static RendererBackend LoadBackendSetting();

    // Entre ImGui::NewFrame et IRenderer::Render (qui termine la frame ImGui)
    void Draw(Scene& scene, const RendererStats& stats, ImTextureID sceneTexture);

    // Panneau Viewport : la caméra éditeur ne démarre qu'au survol de l'image
    bool     IsViewportHovered() const { return m_viewportHovered; }
    // Taille de l'image en pixels (0 tant que le panneau n'a jamais été visible)
    uint32_t GetViewportWidth() const { return m_viewportWidth; }
    uint32_t GetViewportHeight() const { return m_viewportHeight; }

private:
    void DrawDockSpace();
    void DrawViewport(ImTextureID sceneTexture);
    void DrawStats(const RendererStats& stats);
    void DrawScenePanel(Scene& scene);
    void DrawEntityNode(Scene& scene, EntityID id);
    void DrawInspector(Scene& scene, EntityID id);

    EntityID m_selectedEntity = NULL_ENTITY;

    bool     m_viewportHovered = false;
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;

    // FPS affichés, rafraîchis périodiquement (cf. DrawStats)
    double   m_statsTime = -1.0;
    float    m_statsFps = 0.f;

    RendererBackend m_nextBackend; // choisi dans GPU Stats, appliqué au redémarrage

    // Sections de l'inspecteur, une par type de composant
    std::vector<std::unique_ptr<ComponentBase>> m_components;
};
