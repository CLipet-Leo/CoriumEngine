using CoriumEngine.Bridge;
using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows;
using System.Windows.Interop;

namespace CoriumEngine.Frontend
{
    public sealed class DirectXHost : HwndHost
    {
        private readonly object _sync = new object();

        private EngineWrapper _engine;
        private IntPtr _childHwnd;

        private Thread _renderThread;
        private volatile bool _running;

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            int width = Math.Max(1, (int)Math.Ceiling(ActualWidth > 0 ? ActualWidth : Width > 0 ? Width : 1280));
            int height = Math.Max(1, (int)Math.Ceiling(ActualHeight > 0 ? ActualHeight : Height > 0 ? Height : 720));

            _childHwnd = CreateWindowEx(
                0,
                "static",
                string.Empty,
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0,
                0,
                width,
                height,
                hwndParent.Handle,
                IntPtr.Zero,
                IntPtr.Zero,
                IntPtr.Zero);

            if (_childHwnd == IntPtr.Zero)
            {
                throw new InvalidOperationException("Unable to create child host window for DX12.");
            }

            _engine = new EngineWrapper();
            if (!_engine.Initialize(_childHwnd, width, height))
            {
                _engine.Dispose();
                _engine = null;
                throw new InvalidOperationException("Engine initialization failed.");
            }

            StartRenderLoop();
            SizeChanged += OnSizeChanged;

            return new HandleRef(this, _childHwnd);
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            SizeChanged -= OnSizeChanged;
            StopRenderLoop();

            lock (_sync)
            {
                _engine?.Dispose();
                _engine = null;
            }

            if (hwnd.Handle != IntPtr.Zero)
            {
                DestroyWindow(hwnd.Handle);
            }

            _childHwnd = IntPtr.Zero;
        }

        private void StartRenderLoop()
        {
            if (_running)
            {
                return;
            }

            _running = true;
            _renderThread = new Thread(() =>
            {
                while (_running)
                {
                    lock (_sync)
                    {
                        _engine?.RenderFrame();
                    }
                }
            })
            {
                IsBackground = true,
                Name = "CoriumEngine.RenderThread"
            };

            _renderThread.Start();
        }

        private void StopRenderLoop()
        {
            _running = false;

            if (_renderThread != null)
            {
                _renderThread.Join();
                _renderThread = null;
            }
        }

        private void OnSizeChanged(object sender, SizeChangedEventArgs e)
        {
            int width = Math.Max(1, (int)Math.Ceiling(e.NewSize.Width));
            int height = Math.Max(1, (int)Math.Ceiling(e.NewSize.Height));

            if (_childHwnd != IntPtr.Zero)
            {
                MoveWindow(_childHwnd, 0, 0, width, height, true);
            }

            lock (_sync)
            {
                _engine?.Resize(width, height);
            }
        }

        private const int WS_CHILD = 0x40000000;
        private const int WS_VISIBLE = 0x10000000;
        private const int WS_CLIPSIBLINGS = 0x04000000;
        private const int WS_CLIPCHILDREN = 0x02000000;

        [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateWindowEx(
            int dwExStyle,
            string lpClassName,
            string lpWindowName,
            int dwStyle,
            int x,
            int y,
            int nWidth,
            int nHeight,
            IntPtr hWndParent,
            IntPtr hMenu,
            IntPtr hInstance,
            IntPtr lpParam);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool DestroyWindow(IntPtr hWnd);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool MoveWindow(IntPtr hWnd, int x, int y, int nWidth, int nHeight, bool bRepaint);
    }
}
