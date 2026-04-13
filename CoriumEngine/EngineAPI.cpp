#define CORIUMENGINE_EXPORTS

#include "stdafx.h"
#include "public/EngineAPI.h"

#include <mutex>

namespace
{
    std::mutex g_rendererMutex;
    IRenderer* g_renderer = nullptr;
}

extern "C"
{
    bool Engine_Init(void* hwnd, int width, int height)
    {
        std::lock_guard<std::mutex> lock(g_rendererMutex);

        if (g_renderer)
            return true;

        if (!hwnd || width <= 0 || height <= 0)
            return false;

        IRenderer* renderer = CreateRenderer();
        if (!renderer)
            return false;

        if (!renderer->Init(static_cast<HWND>(hwnd), static_cast<uint32_t>(width), static_cast<uint32_t>(height)))
        {
            DestroyRenderer(renderer);
            return false;
        }

        g_renderer = renderer;
        return true;
    }

    void Engine_Resize(int width, int height)
    {
        std::lock_guard<std::mutex> lock(g_rendererMutex);

        if (!g_renderer || width <= 0 || height <= 0)
            return;

        g_renderer->OnResize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }

    void Engine_RenderFrame()
    {
        std::lock_guard<std::mutex> lock(g_rendererMutex);

        if (!g_renderer)
            return;

        g_renderer->Render();
    }

    void Engine_Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_rendererMutex);

        if (!g_renderer)
            return;

        g_renderer->Shutdown();
        DestroyRenderer(g_renderer);
        g_renderer = nullptr;
    }
}
