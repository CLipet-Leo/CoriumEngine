#pragma once

class CORIUM_API DX12Fence
{
public:
    DX12Fence() = default;
    explicit DX12Fence(ID3D12Device* device, uint32_t frameCount = 2);
    ~DX12Fence();

    // Signale la fin de soumission de la frame courante
    void Signal(ID3D12CommandQueue* queue, uint32_t frameIndex);

    // Attend que la frame frameIndex soit terminée sur le GPU
    // À appeler AVANT de réutiliser les ressources de cette frame
    void WaitForFrame(uint32_t frameIndex);

    // Vide complètement le GPU — à utiliser uniquement pour Shutdown/Resize
    void FlushGpu(ID3D12CommandQueue* queue);

    uint64_t GetCompletedValue() const { return m_fence->GetCompletedValue(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    std::vector<uint64_t>               m_frameFenceValues; // une valeur par frame
    uint64_t                            m_nextFenceValue = 1;
    HANDLE                              m_fenceEvent = nullptr;
    uint32_t                            m_frameCount = 2;
};