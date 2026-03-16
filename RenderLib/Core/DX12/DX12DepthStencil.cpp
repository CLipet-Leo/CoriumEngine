#include "pch.h"
#include "DX12DepthStencil.h"

DX12DepthStencil::DX12DepthStencil(ID3D12Device* device, uint32_t width, uint32_t height,
                                   DX12DescriptorHeaps* descriptorHeaps,
                                   DX12MemoryManager* memoryManager)
    : m_descriptorHeaps(descriptorHeaps)
    , m_memoryManager(memoryManager)
{
    Create(device, width, height);
}

void DX12DepthStencil::Create(ID3D12Device* device, uint32_t width, uint32_t height)
{
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, width, height,
        1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    EVAL_HR(m_memoryManager->CreateResource(
        DX12MemoryType::Default,
        resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        m_depthStencil),
        "Depth stencil resource creation failed");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc, m_descriptorHeaps->GetDSVHandle());
}

void DX12DepthStencil::Resize(ID3D12Device* device, uint32_t width, uint32_t height)
{
    m_depthStencil.Reset();
    Create(device, width, height);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DepthStencil::GetDSVHandle() const
{
    return m_descriptorHeaps->GetDSVHandle();
}
