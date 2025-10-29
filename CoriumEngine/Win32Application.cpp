#include "stdafx.h"
#include "public/Win32Application.h"
#include <iostream>

// On suppose que ta librairie de rendu fournit ces fonctions externes
// Pour créer / détruire un renderer concret (DX12 par ex.)
extern IRenderer* CreateRenderer();
extern void DestroyRenderer(IRenderer*);

Win32Application::Win32Application()
{
}

Win32Application::~Win32Application()
{
    // s’assurer du shutdown propre
    Shutdown();
}

bool Win32Application::RegisterWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = s_windowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // Charger les icônes depuis les ressources de l'application.
    // Utiliser LR_SHARED évite de devoir appeler DestroyIcon.
    wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_CORIUMENGINE), IMAGE_ICON, 32, 32, LR_SHARED | LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_CORIUMENGINE), IMAGE_ICON, 16, 16, LR_SHARED | LR_DEFAULTCOLOR);

    if (!RegisterClassEx(&wc))
    {
        return false;
    }
    return true;
}

bool Win32Application::Initialize(HINSTANCE hInstance, int nCmdShow, uint32_t width, uint32_t height)
{
    m_hInstance = hInstance;
    m_width = width;
    m_height = height;
	wchar_t titleBuffer[256] = {0};
	LoadString(hInstance, IDS_APP_TITLE, titleBuffer, 256);
	LPCWSTR title = titleBuffer;

    if (!RegisterWindowClass(hInstance))
        return false;

    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect = { 0, 0, (LONG)width, (LONG)height };
    AdjustWindowRect(&rect, style, FALSE);

    m_hWnd = CreateWindowEx(
        0,
        s_windowClassName,
        title,         // titre de la fenêtre
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr,
        nullptr,
        hInstance,
        this    // passe l’instance cette classe via lpParam
    );

    if (!m_hWnd)
        return false;

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    // Créer le renderer via ta librairie
    m_renderer = CreateRenderer();
    if (!m_renderer)
        return false;

    if (!m_renderer->Init(m_hWnd, width, height))
        return false;

    return true;
}

void Win32Application::Run()
{
    assert(m_hWnd && m_renderer);  // s’assurer que Initialize a bien été appelé

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Ici tu peux aussi faire des mises à jour logiques (input, etc.)
            m_renderer->Render();
        }
    }
}

void Win32Application::Shutdown()
{
	std::cout << "Shutting down application..." << std::endl;
    if (m_renderer)
    {
        m_renderer->Shutdown();
        DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }

    if (m_hWnd)
    {
        DestroyWindow(m_hWnd);
        m_hWnd = nullptr;
    }
}

LRESULT CALLBACK Win32Application::WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Win32Application* app = nullptr;
    if (message == WM_NCCREATE)
    {
        // extraire le pointeur passé dans lpParam
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        app = reinterpret_cast<Win32Application*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    else
    {
        app = reinterpret_cast<Win32Application*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (app)
    {
        return app->HandleMessage(hWnd, message, wParam, lParam);
    }
    else
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

LRESULT Win32Application::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    {
        uint32_t newWidth = LOWORD(lParam);
        uint32_t newHeight = HIWORD(lParam);
        if (m_renderer && newWidth > 0 && newHeight > 0)
        {
            m_renderer->OnResize(newWidth, newHeight);
            m_width = newWidth;
            m_height = newHeight;
        }
    }
    return 0;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    // éventuellement d’autres messages : input clavier/souris, focus, etc.

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}
