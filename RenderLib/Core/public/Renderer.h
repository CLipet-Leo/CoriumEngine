#pragma once

class Scene;
class BufferGeometry;
struct MaterialDesc;

class CORIUM_API IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(HWND hWnd, uint32_t width, uint32_t height) = 0;
    virtual void OnResize(uint32_t w, uint32_t h) = 0;
    virtual void Render(Scene& scene) = 0;
    virtual void Shutdown() = 0;

    // Retourne true si le message a été consommé (UI éditeur)
    virtual bool HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;

    // Enregistre un mesh ; l'upload GPU est fait au début de la prochaine frame
    virtual uint32_t RegisterMesh(const std::string& name, const BufferGeometry& geometry) = 0;
    // Crée un matériau ; le PSO est partagé entre matériaux de même état
    virtual uint32_t CreateMaterial(const MaterialDesc& desc) = 0;
    // À appeler après modification de cull/fill/blend d'un matériau
    virtual void     RefreshMaterial(uint32_t index) = 0;
};

extern "C" {
    CORIUM_API IRenderer* CreateRenderer();
    CORIUM_API void       DestroyRenderer(IRenderer*);
}
