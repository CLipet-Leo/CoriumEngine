#pragma once

class CORIUM_API EditorUI
{
public:
    // À appeler entre DX12ImGui::BeginFrame et DX12ImGui::Render
    void Draw(Scene& scene, uint32_t frameIndex, const wchar_t* adapterName);

private:
    void DrawStats(uint32_t frameIndex, const wchar_t* adapterName);
    void DrawScenePanel(Scene& scene);
    void DrawEntityNode(Scene& scene, EntityID id);
    void DrawInspector(Scene& scene, EntityID id);

    EntityID m_selectedEntity = NULL_ENTITY;

    // Euler affichés dans l'inspecteur, recalculés seulement si la rotation change ailleurs
    // (évite l'instabilité d'une reconversion quat -> Euler à chaque frame)
    EntityID m_eulerEntity = NULL_ENTITY;
    Quat     m_eulerSource = {};
    Vec3     m_euler = {};
};
