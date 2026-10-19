#pragma once

// Fenêtre Win32 + boucle, partagées par les exe. Chaque exe dérive et fournit sa logique
// par Update (avant Input::EndFrame), sa caméra par GetView et peut intercepter les messages
// fenêtre par OnWindowMessage.
class CORIUM_API Win32Application
{
public:
    Win32Application();
    virtual ~Win32Application();

    // Initialise la fenêtre et le renderer ; iconId : ressource de l'exe (0 = icône par défaut)
    bool Initialize(HINSTANCE hInstance, int nCmdShow, const wchar_t* title, WORD iconId, uint32_t width, uint32_t height);

    // Boucle principale
    void Run();

    // Libération des ressources
    void Shutdown();

    // Gestion des messages de la fenêtre
    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Accesseurs
    HWND GetHwnd() const { return m_hWnd; }
    uint32_t GetClientWidth() const { return m_width; }
    uint32_t GetClientHeight() const { return m_height; }
    IRenderer& GetRenderer() { return *m_renderer; }
    Scene&     GetScene() { return m_scene; }
    const Input& GetInput() const { return m_input; }

protected:
    // Logique applicative de la frame, avant la mise à jour des transforms
    virtual void Update(float /*dt*/) {}
    // Vue hors scène ; nullptr = caméra isMain de la scène
    virtual const CameraView* GetView() { return nullptr; }
    // Avant le traitement par défaut ; true = message consommé (ex. backend Win32 d'ImGui)
    virtual bool OnWindowMessage(HWND /*hWnd*/, UINT /*message*/, WPARAM /*wParam*/, LPARAM /*lParam*/) { return false; }
    // Backend sans -dx12 / -vulkan en ligne de commande (lu dans Initialize)
    virtual RendererBackend GetDefaultBackend() { return RendererBackend::DX12; }

private:
    // Fonction statique pour rediriger vers l’instance (via userdata)
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Création / enregistrement de la classe de fenêtre
    bool RegisterWindowClass(HINSTANCE hInstance, WORD iconId);

    HWND        m_hWnd = nullptr;
    uint32_t    m_width = 0;
    uint32_t    m_height = 0;
    HINSTANCE   m_hInstance = nullptr;

    IRenderer* m_renderer = nullptr;
    Scene      m_scene;
    TransformSystem m_transformSystem;

    Clock        m_clock;
    Input        m_input;

    // Nom de la classe de fenêtre
    static constexpr const wchar_t* s_windowClassName = L"Win32AppWindowClass";
};