#include "stdafx.h"
#include "public/Win32Application.h"
// Memory leak detection
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    const uint32_t initialWidth = 800;
    const uint32_t initialHeight = 600;

    Win32Application app;

    if (!app.Initialize(hInstance, nCmdShow, initialWidth, initialHeight))
        return -1;

    app.Run();

    app.Shutdown();

    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
    _CrtDumpMemoryLeaks();

    return 0;
}
