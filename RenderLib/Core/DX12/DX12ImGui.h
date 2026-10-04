#pragma once

class CORIUM_API DX12ImGui
{
public:
	DX12ImGui() = default;
	~DX12ImGui() { Shutdown(); }

	void Init(
		HWND                     hwnd,
		ID3D12Device* device,
		int                      frameCount,
		DXGI_FORMAT              rtvFormat,
		ID3D12DescriptorHeap* srvHeap,    // heap CBV/SRV/UAV existant
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

	void Shutdown();

	// Appeler en début de frame, avant d'enregistrer les commandes
	void BeginFrame();

	// Appeler EN FIN d'enregistrement, juste avant Close()
	void Render(ID3D12GraphicsCommandList* cmdList);

	// Passe la fenêtre Win32 pour la gestion input
	static LRESULT WndProcHandler(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
	bool        m_initialized = false;
	std::string m_iniPath; // référencé par ImGuiIO::IniFilename
};