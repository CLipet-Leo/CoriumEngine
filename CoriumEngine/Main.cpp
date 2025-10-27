#include "stdafx.h"
#include "Win32Application.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, PWSTR /*pCmdLine*/, int nCmdShow)
{
    const uint32_t initialWidth = 800;
    const uint32_t initialHeight = 600;

    Win32Application app;

    if (!app.Initialize(hInstance, nCmdShow, initialWidth, initialHeight))
        return -1;

    app.Run();

    app.Shutdown();

    return 0;
}
