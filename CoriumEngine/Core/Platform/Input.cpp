#include "pch.h"
#include "Input.h"

void Input::Init(HWND hWnd)
{
    RAWINPUTDEVICE mouse = {};
    mouse.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
    mouse.usUsage = 0x02;     // HID_USAGE_GENERIC_MOUSE
    mouse.hwndTarget = hWnd;
    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
        OutputDebugStringA("Input: RegisterRawInputDevices failed, no mouse delta\n");
}

void Input::OnMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        m_down[wParam & 0xFF] = true;
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        m_down[wParam & 0xFF] = false;
        break;

    case WM_LBUTTONDOWN: m_down[VK_LBUTTON] = true;  break;
    case WM_LBUTTONUP:   m_down[VK_LBUTTON] = false; break;
    case WM_RBUTTONDOWN: m_down[VK_RBUTTON] = true;  break;
    case WM_RBUTTONUP:   m_down[VK_RBUTTON] = false; break;
    case WM_MBUTTONDOWN: m_down[VK_MBUTTON] = true;  break;
    case WM_MBUTTONUP:   m_down[VK_MBUTTON] = false; break;

    case WM_INPUT:
    {
        RAWINPUT raw = {};
        UINT size = sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
                &raw, &size, sizeof(RAWINPUTHEADER)) == UINT(-1))
            break;

        // Les souris en coordonnées absolues (tablettes, bureau distant) sont ignorées
        if (raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE))
        {
            m_mouseDelta.x += static_cast<float>(raw.data.mouse.lLastX);
            m_mouseDelta.y += static_cast<float>(raw.data.mouse.lLastY);
        }
        break;
    }

    case WM_KILLFOCUS:
        Reset();
        break;
    }
}

void Input::EndFrame()
{
    m_mouseDelta = { 0.f, 0.f };
}

void Input::Reset()
{
    m_down.fill(false);
    m_mouseDelta = { 0.f, 0.f };
}
