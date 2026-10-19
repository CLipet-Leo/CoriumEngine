#include "pch.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include "DX12ImGui.h"

void DX12ImGui::Init(
	ImGuiContext* context,
	void* (*allocFunc)(size_t, void*),
	void (*freeFunc)(void*, void*),
	void* allocUserData,
	ID3D12Device* device, int frameCount,
	DXGI_FORMAT rtvFormat,
	ID3D12DescriptorHeap* srvHeap,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	// ImGui est une lib statique : la DLL a sa propre copie de GImGui et des allocateurs.
	// Même manifeste vcpkg que l'exe -> même version et même layout de structures.
	ImGui::SetAllocatorFunctions(allocFunc, freeFunc, allocUserData);
	ImGui::SetCurrentContext(context);
	IMGUI_CHECKVERSION();

	ImGui_ImplDX12_Init(
		device, frameCount, rtvFormat,
		srvHeap, cpuHandle, gpuHandle);

	m_initialized = true;
}

void DX12ImGui::Shutdown()
{
	if (!m_initialized) return;
	ImGui_ImplDX12_Shutdown();
	ImGui::SetCurrentContext(nullptr);
	m_initialized = false;
}

void DX12ImGui::BeginFrame()
{
	ImGui_ImplDX12_NewFrame();
}

void DX12ImGui::Render(ID3D12GraphicsCommandList* cmdList)
{
	ImGui::Render();
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}

bool DX12ImGui::UpdatePlatformWindows()
{
	if (!(ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
		return false;
	ImGui::UpdatePlatformWindows();
	return ImGui::GetPlatformIO().Viewports.Size > 1;
}

void DX12ImGui::RenderPlatformWindows()
{
	ImGui::RenderPlatformWindowsDefault();
}
