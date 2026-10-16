#pragma once

// Backend DX12 d'ImGui. Le contexte et le backend Win32 appartiennent à l'exe (éditeur).
class CORIUM_API DX12ImGui
{
public:
	DX12ImGui() = default;
	~DX12ImGui() { Shutdown(); }

	// Rattache la copie d'ImGui de la DLL au contexte de l'exe, puis init le backend DX12
	void Init(
		ImGuiContext*            context,
		void* (*allocFunc)(size_t, void*),
		void (*freeFunc)(void*, void*),
		void*                    allocUserData,
		ID3D12Device*            device,
		int                      frameCount,
		DXGI_FORMAT              rtvFormat,
		ID3D12DescriptorHeap*    srvHeap,    // heap CBV/SRV/UAV existant
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

	// Le GPU doit être inactif (FlushGpu)
	void Shutdown();

	// Avant ImGui::NewFrame (côté exe)
	void BeginFrame();

	// Termine la frame ImGui et enregistre la fenêtre principale, avant Close()
	void Render(ID3D12GraphicsCommandList* cmdList);

	// Fenêtres détachées hors de la fenêtre principale (multi-viewport), après
	// ExecuteCommandLists : le backend les rend chacune sur sa propre queue.
	// ponytail: aucune synchro entre ces queues et la nôtre ; un Viewport détaché peut lire
	// la cible offscreen pendant son écriture (déchirure visuelle). À synchroniser si gênant.
	void RenderPlatformWindows();

private:
	bool m_initialized = false;
};
