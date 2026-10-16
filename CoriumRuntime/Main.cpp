#include "stdafx.h"
// Memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

// Runtime : ni UI ni caméra éditeur, rendu par la caméra isMain de la scène
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    const uint32_t initialWidth = 800;
    const uint32_t initialHeight = 600;

    // Scope : app (et sa Scene) doit être détruite avant le dump des fuites
    {
        Win32Application app;

        wchar_t title[MAX_NAME_STRING] = {};
        LoadString(hInstance, IDS_APP_TITLE, title, MAX_NAME_STRING);
        if (!app.Initialize(hInstance, nCmdShow, title, IDI_CORIUMRUNTIME, initialWidth, initialHeight))
            return -1;

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

        app.Shutdown();
    }

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtDumpMemoryLeaks();

    return 0;
}
