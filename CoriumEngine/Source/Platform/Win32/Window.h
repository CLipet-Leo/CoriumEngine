#pragma once

#include "SubObject.h"

#include <Uxtheme.h>
#pragma comment(lib, "UxTheme.lib")

namespace Win32
{
	class CORIUM_API Window : public Win32::SubObject, public Win32::Caption
	{
	public:
		Window(WSTRING title, HICON icon, WindowType type = RESIZABLE);
		~Window();

		virtual VOID Initialize() override;

		virtual			LRESULT				MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) override;

		VOID RedrawWindow();

		VOID OnNonClientCreate();

		VOID OnNonClientActivate(BOOL active);

		VOID OnNonClientPaint(HRGN region);

		VOID PaintCaption(HDC hdc);

		VOID OnNonClientLeftMouseButtonDown();

		VOID OnGetMinMaxInfo(MINMAXINFO* minmax);

		VOID OnExitSizeMove();

		VOID OnPaint();

		// Getters
		SIZE GetSize() const { return m_Size; }
		WindowType GetType() const { return m_Type; }
		BOOL IsActive() const { return m_IsActive; }

		// Setters
		VOID SetSize(SIZE size) { m_Size = size; }
		VOID SetSize(INT cx, INT cy) { m_Size.cx = cx; m_Size.cy = cy; }

		VOID IsActive(BOOL active) { m_IsActive = active; }

	protected:
		SIZE 			m_Size;
		WindowType		m_Type;

		BOOL 			m_IsActive;
	};
}