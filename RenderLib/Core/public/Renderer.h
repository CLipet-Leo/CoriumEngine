#pragma once

class Scene;
struct MeshDesc;
struct MaterialDesc;
struct CameraView;

class CORIUM_API IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(HWND hWnd, uint32_t width, uint32_t height) = 0;
    virtual void OnResize(uint32_t w, uint32_t h) = 0;
    // Les world matrices doivent être à jour (TransformSystem::Update).
    // view : caméra hors scène (éditeur) ; nullptr = caméra isMain de la scène
    virtual void Render(Scene& scene, const CameraView* view) = 0;
    virtual void Shutdown() = 0;

    // Retourne true si le message a été consommé (UI éditeur)
    virtual bool HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
    // true si l'UI éditeur utilise la souris (survol/drag d'un panneau) : à ne pas traiter côté jeu
    virtual bool WantCaptureMouse() const = 0;

    // Enregistre un mesh ; l'upload GPU est fait au début de la prochaine frame
    virtual uint32_t RegisterMesh(const MeshDesc& desc) = 0;
    // Crée un matériau ; le PSO est partagé entre matériaux de même état
    virtual uint32_t CreateMaterial(const MaterialDesc& desc) = 0;
    // À appeler après modification de cull/fill/blend d'un matériau
    virtual void     RefreshMaterial(uint32_t index) = 0;

    // nullptr si l'index n'existe pas
    virtual const MeshDesc*     GetMeshDesc(uint32_t index) const = 0;
    virtual const MaterialDesc* GetMaterialDesc(uint32_t index) const = 0;
};

extern "C" {
    CORIUM_API IRenderer* CreateRenderer();
    CORIUM_API void       DestroyRenderer(IRenderer*);
}
