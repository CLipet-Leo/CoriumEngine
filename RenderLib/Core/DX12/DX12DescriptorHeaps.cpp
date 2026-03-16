#include "pch.h"
#include "DX12DescriptorHeaps.h"

bool DX12DescriptorHeaps::Initialize(ID3D12Device* device, uint32_t frameCount, UINT cbvSrvUavCapacity)
{
    if (!device)
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = frameCount;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    EVAL_HR(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap)), "RTV heap creation failed");

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    EVAL_HR(device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap)), "DSV heap creation failed");

    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDesc = {};
    cbvSrvUavDesc.NumDescriptors = cbvSrvUavCapacity;
    cbvSrvUavDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvSrvUavDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    EVAL_HR(device->CreateDescriptorHeap(&cbvSrvUavDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap)), "CBV/SRV/UAV heap creation failed");

    m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_cbvSrvUavCapacity = cbvSrvUavCapacity;
    m_nextCbvSrvUavIndex = 0;

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeaps::GetRTVHandle(uint32_t index) const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(static_cast<INT>(index), m_rtvDescriptorSize);
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorHeaps::GetDSVHandle() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

bool DX12DescriptorHeaps::AllocateCbvSrvUav(Allocation& outAllocation)
{
    if (m_nextCbvSrvUavIndex >= m_cbvSrvUavCapacity)
        return false;

    outAllocation.Index = m_nextCbvSrvUavIndex;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(static_cast<INT>(m_nextCbvSrvUavIndex), m_cbvSrvUavDescriptorSize);
    outAllocation.CpuHandle = cpuHandle;

    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    gpuHandle.Offset(static_cast<INT>(m_nextCbvSrvUavIndex), m_cbvSrvUavDescriptorSize);
    outAllocation.GpuHandle = gpuHandle;

    ++m_nextCbvSrvUavIndex;
    return true;
}

ID3D12DescriptorHeap* DX12DescriptorHeaps::GetCbvSrvUavHeap() const
{
    return m_cbvSrvUavHeap.Get();
}
