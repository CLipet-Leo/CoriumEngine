#pragma once

namespace Corium
{
	class CORIUM_API Simulation : public Win32::IApplication, public Win32::Window
	{
	public:
		Simulation();
		~Simulation();

		virtual VOID PreInitialize() override;

		virtual LRESULT MessageHandler(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

	};
}