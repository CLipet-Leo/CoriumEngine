#pragma once

// Cible couleur offscreen lisible en SRV (Viewport de l'éditeur).
// Même format que la swapchain : les PSO sont partagés. État hors rendu : PIXEL_SHADER_RESOURCE.
class CORIUM_API DX12RenderTarget
{
public:
    static constexpr DXGI_FORMAT Format = DX12SwapChain::Format;

    // srvHandle : slot du heap SRV où écrire la vue (réécrit à chaque Resize)
    DX12RenderTarget(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle);

    // Le GPU ne doit plus utiliser la texture (FlushGpu)
    void Resize(ID3D12Device* device, uint32_t width, uint32_t height);

    ID3D12Resource*             Get() const { return m_texture.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const;
    uint32_t                    GetWidth() const { return m_width; }
    uint32_t                    GetHeight() const { return m_height; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource>       m_texture;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE                  m_srvHandle = {};
    uint32_t                                     m_width = 0;
    uint32_t                                     m_height = 0;
};
