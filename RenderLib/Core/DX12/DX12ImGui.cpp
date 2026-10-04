#include "pch.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include "DX12ImGui.h"

void DX12ImGui::Init(
	HWND hwnd, ID3D12Device* device, int frameCount,
	DXGI_FORMAT rtvFormat,
	ID3D12DescriptorHeap* srvHeap,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // layout persistant
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // fenetres flottantes

	// Layout sauvegardé à côté de l'exe, quel que soit le dossier de lancement.
	// ImGui attend de l'UTF-8 et garde le pointeur : m_iniPath doit survivre au contexte.
	const std::wstring iniPath = GetExecutableDir() + L"imgui.ini";
	const int len = WideCharToMultiByte(CP_UTF8, 0, iniPath.c_str(), -1, nullptr, 0, nullptr, nullptr);
	m_iniPath.resize(len - 1);
	WideCharToMultiByte(CP_UTF8, 0, iniPath.c_str(), -1, m_iniPath.data(), len, nullptr, nullptr);
	io.IniFilename = m_iniPath.c_str();

	// Charge une police système en plus haute résolution pour éviter l'aspect flou/pixellisé
	float fontSize = 18.0f; // Augmente cette valeur pour avoir des textes encore plus grands
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", fontSize);

	ImGui::StyleColorsDark();

	// Ajuste le style pour que les viewports detaches matchent le theme
	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.f;
		style.Colors[ImGuiCol_WindowBg].w = 1.f;
	}

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
		device, frameCount, rtvFormat,
		srvHeap, cpuHandle, gpuHandle);

	m_initialized = true;
}

void DX12ImGui::Shutdown()
{
	if (!m_initialized) return;
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_initialized = false;
}

void DX12ImGui::BeginFrame()
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void DX12ImGui::Render(ID3D12GraphicsCommandList* cmdList)
{
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

	// Necessaire si ViewportsEnable est actif
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, cmdList);
	}
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT DX12ImGui::WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
}