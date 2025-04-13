#pragma once

#include "Platform/Win32/Window.h"

namespace SplashScreen
{

	VOID CORIUM_API Open();
	VOID CORIUM_API Close();
	VOID CORIUM_API AddMessage(CONST WCHAR* message);

}

class CORIUM_API SplashWindow : public Win32::Window
{
public:
	SplashWindow();
	~SplashWindow();

	virtual LRESULT MessageHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
	WCHAR m_outputMessage[MAX_NAME_STRING];

};