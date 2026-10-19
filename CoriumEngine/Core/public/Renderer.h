#pragma once

class Scene;
struct MeshDesc;
struct MaterialDesc;
struct ImGuiContext;

// API graphique, choisie au lancement (-dx12 / -vulkan, cf. Win32Application)
enum class RendererBackend : uint8_t { DX12, Vulkan };

// Infos de debug affichées par l'éditeur
struct RendererStats
{
    const wchar_t* adapterName = L"";
    const wchar_t* backendName = L"";
};

// Vue fournie hors scène (caméra éditeur), en espace monde
struct CameraView
{
    Vec3  position = { 0.f, 0.f, 0.f };
    Vec3  forward = { 0.f, 0.f, 1.f };
    Vec3  up = { 0.f, 1.f, 0.f };
    float fovY = 60.f;  // degrés
    float nearPlane = 0.1f;
    float farPlane = 1000.f;
};

class CORIUM_API IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool Init(HWND hWnd, uint32_t width, uint32_t height) = 0;
    virtual void OnResize(uint32_t w, uint32_t h) = 0;
    // Les world matrices doivent être à jour (TransformSystem::Update).
    // view : caméra hors scène (éditeur) ; nullptr = caméra isMain de la scène
    virtual void Render(Scene& scene, const CameraView* view) = 0;
    virtual void Shutdown() = 0;

    virtual RendererStats GetStats() const = 0;

    // --- UI éditeur (optionnelle) ---
    // Le contexte ImGui et le backend Win32 appartiennent à l'exe ; la DLL (copie statique
    // d'ImGui distincte) s'y rattache avec les allocateurs de l'exe et ajoute le backend graphique.
    // Une fois l'UI initialisée, la scène est rendue dans une cible offscreen (GetSceneTexture)
    // et ImGui dans la swapchain.
    virtual void InitUI(ImGuiContext* context, void* (*allocFunc)(size_t, void*),
                        void (*freeFunc)(void*, void*), void* allocUserData) = 0;
    // Avant la destruction du contexte et du backend Win32
    virtual void ShutdownUI() = 0;
    // Avant ImGui_ImplWin32_NewFrame + ImGui::NewFrame ; Render termine la frame ImGui
    virtual void BeginUIFrame() = 0;
    // Taille voulue de la cible offscreen, appliquée au début du prochain Render
    virtual void     SetSceneSize(uint32_t width, uint32_t height) = 0;
    // ImTextureID de la cible offscreen (stable, même après redimensionnement)
    virtual uint64_t GetSceneTexture() const = 0;

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
    CORIUM_API IRenderer* CreateRenderer(RendererBackend backend);
    CORIUM_API void       DestroyRenderer(IRenderer*);
}
