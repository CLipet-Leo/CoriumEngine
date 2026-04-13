#pragma once

using namespace System;

namespace CoriumEngine
{
    namespace Bridge
    {
        public ref class EngineWrapper sealed
        {
        public:
            EngineWrapper();
            ~EngineWrapper();
            !EngineWrapper();

            bool Initialize(IntPtr hwnd, int width, int height);
            void Resize(int width, int height);
            void RenderFrame();
            void Shutdown();

        private:
            void ThrowIfDisposed();

            bool m_disposed;
            bool m_initialized;
        };
    }
}
