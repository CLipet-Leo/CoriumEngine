#pragma once

class CORIUM_API DX12Debug
{
public:
    DX12Debug();
    ~DX12Debug() = default;

    // Appeler APRÈS la création du device pour configurer l'InfoQueue
    void SetupInfoQueue(ID3D12Device* device);

private:
    Microsoft::WRL::ComPtr<ID3D12Debug1> m_debug;
};