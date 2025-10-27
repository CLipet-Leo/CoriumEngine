#pragma once

class Win32Application
{
public:
    Win32Application();
    ~Win32Application();

    // Initialise la fenêtre et le renderer
    bool Initialize(HINSTANCE hInstance, int nCmdShow, uint32_t width, uint32_t height);

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

private:
    // Fonction statique pour rediriger vers l’instance (via userdata)
    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Création / enregistrement de la classe de fenêtre
    bool RegisterWindowClass(HINSTANCE hInstance);

    HWND        m_hWnd = nullptr;
    uint32_t    m_width = 0;
    uint32_t    m_height = 0;
    HINSTANCE   m_hInstance = nullptr;

    IRenderer* m_renderer = nullptr;

    // Nom de la classe de fenêtre
    static constexpr const wchar_t* s_windowClassName = L"Win32AppWindowClass";
};