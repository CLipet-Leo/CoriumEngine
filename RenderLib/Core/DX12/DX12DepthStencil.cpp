#include "pch.h"
#include "DX12DepthStencil.h"

DX12DepthStencil::DX12DepthStencil(ID3D12Device* device, uint32_t width, uint32_t height)
{
    Create(device, width, height);
}

void DX12DepthStencil::Create(ID3D12Device* device, uint32_t width, uint32_t height)
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    EVAL_HR(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)), "DSV Heap creation failed");

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format               = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth   = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC   resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D32_FLOAT, width, height,
        1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    EVAL_HR(device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue, IID_PPV_ARGS(&m_depthStencil)),
        "Depth Stencil resource creation failed");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format        = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;
    device->CreateDepthStencilView(m_depthStencil.Get(), &dsvDesc,
        m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void DX12DepthStencil::Resize(ID3D12Device* device, uint32_t width, uint32_t height)
{
    m_depthStencil.Reset();
    m_dsvHeap.Reset();
    Create(device, width, height);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DepthStencil::GetDSVHandle() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}
