#include "pch.h"
#include "DX12RenderTarget.h"

DX12RenderTarget::DX12RenderTarget(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle)
    : m_srvHandle(srvHandle)
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = 1;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    EVAL_HR(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap)),
        "Offscreen RTV heap creation failed");
}

void DX12RenderTarget::Resize(ID3D12Device* device, uint32_t width, uint32_t height)
{
    m_texture.Reset();

    const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
        Format, width, height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    EVAL_HR(device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr,
        IID_PPV_ARGS(&m_texture)),
        "Offscreen render target creation failed");

    device->CreateRenderTargetView(m_texture.Get(), nullptr, GetRTVHandle());
    device->CreateShaderResourceView(m_texture.Get(), nullptr, m_srvHandle);

    m_width = width;
    m_height = height;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12RenderTarget::GetRTVHandle() const
{
    return m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
}
