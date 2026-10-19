#include "stdafx.h"
// Memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

// Déclaré sous #if 0 dans imgui_impl_win32.h
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// L'éditeur possède le contexte ImGui et le backend Win32 ; le renderer y ajoute le backend graphique
// et rend la scène dans le panneau Viewport, vue par la caméra éditeur (hors scène).
class EditorApplication : public Win32Application
{
public:
    // Après Initialize
    void InitUI()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // layout persistant
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // fenêtres flottantes

        // Layout sauvegardé à côté de l'exe, quel que soit le dossier de lancement.
        // ImGui attend de l'UTF-8 et garde le pointeur : m_iniPath doit survivre au contexte.
        const std::wstring iniPath = GetExecutableDir() + L"imgui.ini";
        const int len = WideCharToMultiByte(CP_UTF8, 0, iniPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
        m_iniPath.resize(len - 1);
        WideCharToMultiByte(CP_UTF8, 0, iniPath.c_str(), -1, m_iniPath.data(), len, nullptr, nullptr);
        io.IniFilename = m_iniPath.c_str();

        // Police et tailles à l'échelle de l'affichage (125 %, 150 %...), taille entière pour des glyphes nets.
        // ponytail: échelle figée au lancement ; changer d'écran à autre DPI ne la met pas à jour
        const float dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(GetHwnd());
        io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", roundf(18.f * dpiScale));

        EditorUI::ApplyStyle();
        ImGui::GetStyle().ScaleAllSizes(dpiScale);

        ImGui_ImplWin32_Init(GetHwnd());

        ImGuiMemAllocFunc allocFunc = nullptr;
        ImGuiMemFreeFunc  freeFunc = nullptr;
        void*             allocUserData = nullptr;
        ImGui::GetAllocatorFunctions(&allocFunc, &freeFunc, &allocUserData);
        GetRenderer().InitUI(ImGui::GetCurrentContext(), allocFunc, freeFunc, allocUserData);
    }

    // Avant Shutdown (le renderer doit encore exister) ; ordre inverse de InitUI
    void ShutdownUI()
    {
        if (!ImGui::GetCurrentContext())
            return;
        GetRenderer().ShutdownUI();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

protected:
    void Update(float dt) override
    {
        IRenderer& renderer = GetRenderer();
        renderer.BeginUIFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        m_ui.Draw(GetScene(), renderer.GetStats(), renderer.GetSceneTexture());
        renderer.SetSceneSize(m_ui.GetViewportWidth(), m_ui.GetViewportHeight());

        // io.WantCaptureMouse est toujours vrai au-dessus du Viewport (fenêtre ImGui) :
        // c'est le survol de l'image qui autorise la caméra
        m_editorCamera.Update(dt, GetInput(), !m_ui.IsViewportHovered());
        m_view = m_editorCamera.GetView();
    }

    const CameraView* GetView() override { return &m_view; }

    RendererBackend GetDefaultBackend() override { return EditorUI::LoadBackendSetting(); }

    bool OnWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override
    {
        return ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam) != 0;
    }

private:
    EditorUI     m_ui;
    EditorCamera m_editorCamera;
    CameraView   m_view;
    std::string  m_iniPath; // référencé par ImGuiIO::IniFilename
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    // Avant la création de la fenêtre : sinon Windows étire en bitmap une fenêtre rendue à 96 DPI (texte flou)
    ImGui_ImplWin32_EnableDpiAwareness();
    const float dpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));
    const uint32_t initialWidth = (uint32_t)(1280 * dpiScale);
    const uint32_t initialHeight = (uint32_t)(720 * dpiScale);

    // Scope : app (et sa Scene) doit être détruite avant le dump des fuites
    {
        EditorApplication app;
        if (!app.Initialize(hInstance, nCmdShow, L"CoriumEditor", IDI_CORIUMEDITOR, initialWidth, initialHeight))
            return -1;
        app.InitUI();

        // Assets\ est copié à côté de l'exécutable au build
        const std::wstring scenePath = GetExecutableDir() + L"Assets\\Scenes\\Demo.scene";
        std::string error;
        if (SceneSerializer::Load(scenePath, app.GetScene(), app.GetRenderer(), &error))
        {
#if defined(_DEBUG)
            SceneSerializer::SelfCheck(scenePath, app.GetRenderer());
#endif
        }
        else
        {
            MessageBoxA(app.GetHwnd(), error.c_str(), "Demo.scene", MB_ICONERROR);
        }

        app.Run();

        app.ShutdownUI();
        app.Shutdown();
    }

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtDumpMemoryLeaks();

    return 0;
}
