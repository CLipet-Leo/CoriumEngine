#pragma once

#ifdef CORIUMENGINE_EXPORTS
#define CORIUM_ENGINE_API __declspec(dllexport)
#else
#define CORIUM_ENGINE_API __declspec(dllimport)
#endif

extern "C"
{
    CORIUM_ENGINE_API bool Engine_Init(void* hwnd, int width, int height);
    CORIUM_ENGINE_API void Engine_Resize(int width, int height);
    CORIUM_ENGINE_API void Engine_RenderFrame();
    CORIUM_ENGINE_API void Engine_Shutdown();
}
