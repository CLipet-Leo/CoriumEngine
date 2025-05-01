#pragma once

namespace Win32
{
	class CORIUM_API SubObject
	{
	public:
		SubObject(WSTRING  className, WSTRING classTitle, HICON icon);
		~SubObject();

		virtual VOID RegisterNewClass();
		virtual VOID Initialize() = 0;

		HWND GetHandle() { return m_Handle; }
		VOID SetHandle(HWND handle) { m_Handle = handle; }

	protected:
		static		LRESULT CALLBACK	SetupMessageHandler(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);
		static		LRESULT				AssignMessageHandler(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam);
		virtual		LRESULT				MessageHandler(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam);

	protected:
		WSTRING 			m_Class;
		WSTRING 			m_Title;

		HICON				m_hIcon;

		HWND				m_Handle;
	};
}