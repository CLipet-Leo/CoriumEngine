#include "pch.h"
#include "Win32Application.h"

Win32Application::Win32Application()
{
}

Win32Application::~Win32Application()
{
    // s’assurer du shutdown propre
    Shutdown();
}

bool Win32Application::RegisterWindowClass(HINSTANCE hInstance, WORD iconId)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = s_windowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    // Charger les icônes depuis les ressources de l'application.
    // Utiliser LR_SHARED évite de devoir appeler DestroyIcon.
    // hInstance est le module de l'exe : c'est lui qui porte les ressources.
    if (iconId)
    {
        wc.hIcon = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(iconId), IMAGE_ICON, 32, 32, LR_SHARED | LR_DEFAULTCOLOR);
        wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(iconId), IMAGE_ICON, 16, 16, LR_SHARED | LR_DEFAULTCOLOR);
    }

    if (!RegisterClassEx(&wc))
    {
        return false;
    }
    return true;
}

bool Win32Application::Initialize(HINSTANCE hInstance, int nCmdShow, const wchar_t* title, WORD iconId, uint32_t width, uint32_t height)
{
    m_hInstance = hInstance;
    m_width = width;
    m_height = height;

    if (!RegisterWindowClass(hInstance, iconId))
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

    m_input.Init(m_hWnd);

    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    // Backend choisi en ligne de commande (-vulkan / -dx12), DX12 par défaut
    RendererBackend backend = RendererBackend::DX12;
    int argc = 0;
    if (LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc))
    {
        for (int i = 1; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"-vulkan") == 0) backend = RendererBackend::Vulkan;
            else if (_wcsicmp(argv[i], L"-dx12") == 0) backend = RendererBackend::DX12;
        }
        LocalFree(argv);
    }

    m_renderer = CreateRenderer(backend);
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
            m_clock.Tick();
            Update(m_clock.DeltaTime());
            m_input.EndFrame();

            // Logique de scène, puis rendu
            m_transformSystem.Update(m_scene);
            m_renderer->Render(m_scene, GetView());
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
    // L'input voit tous les messages, même ceux de l'UI : le filtrage se fait à la lecture
    m_input.OnMessage(message, wParam, lParam);

    if (OnWindowMessage(hWnd, message, wParam, lParam))
        return true;

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
