#include "EngineWrapper.h"
#include "..\CoriumEngine\public\EngineAPI.h"

using namespace System;
using namespace CoriumEngine::Bridge;

EngineWrapper::EngineWrapper()
    : m_disposed(false), m_initialized(false)
{
}

EngineWrapper::~EngineWrapper()
{
    this->!EngineWrapper();
    GC::SuppressFinalize(this);
}

EngineWrapper::!EngineWrapper()
{
    if (m_disposed)
    {
        return;
    }

    if (m_initialized)
    {
        Engine_Shutdown();
        m_initialized = false;
    }

    m_disposed = true;
}

bool EngineWrapper::Initialize(IntPtr hwnd, int width, int height)
{
    ThrowIfDisposed();

    if (m_initialized)
    {
        return true;
    }

    const bool initialized = Engine_Init(hwnd.ToPointer(), width, height);
    m_initialized = initialized;
    return initialized;
}

void EngineWrapper::Resize(int width, int height)
{
    ThrowIfDisposed();

    if (!m_initialized)
    {
        return;
    }

    Engine_Resize(width, height);
}

void EngineWrapper::RenderFrame()
{
    ThrowIfDisposed();

    if (!m_initialized)
    {
        return;
    }

    Engine_RenderFrame();
}

void EngineWrapper::Shutdown()
{
    ThrowIfDisposed();

    if (!m_initialized)
    {
        return;
    }

    Engine_Shutdown();
    m_initialized = false;
}

void EngineWrapper::ThrowIfDisposed()
{
    if (m_disposed)
    {
        throw gcnew ObjectDisposedException("EngineWrapper");
    }
}
