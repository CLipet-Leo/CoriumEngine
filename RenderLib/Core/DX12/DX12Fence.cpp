#include "pch.h"
#include "DX12Fence.h"

DX12Fence::DX12Fence(ID3D12Device* device)
{
    m_fenceValue = 1;
    EVAL_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "Fence creation failed");
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    CE_ASSERT(m_fenceEvent != nullptr);
}

DX12Fence::~DX12Fence()
{
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

void DX12Fence::WaitForGpu(ID3D12CommandQueue* commandQueue)
{
    if (!m_fence || !commandQueue)
        return;

    m_fenceValue++;
    EVAL_HR(commandQueue->Signal(m_fence.Get(), m_fenceValue), "Fence signal failed");

    if (m_fence->GetCompletedValue() < m_fenceValue)
    {
        EVAL_HR(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent), "Fence set event failed");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}
