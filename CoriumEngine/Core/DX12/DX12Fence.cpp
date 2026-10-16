#include "pch.h"
#include "DX12Fence.h"

DX12Fence::DX12Fence(ID3D12Device* device, uint32_t frameCount)
    : m_frameCount(frameCount)
    , m_frameFenceValues(frameCount, 0)
{
    EVAL_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)),
        "Fence creation failed");

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

void DX12Fence::Signal(ID3D12CommandQueue* queue, uint32_t frameIndex)
{
    m_frameFenceValues[frameIndex] = m_nextFenceValue;
    EVAL_HR(queue->Signal(m_fence.Get(), m_nextFenceValue),
        "Fence signal failed");
    ++m_nextFenceValue;
}

void DX12Fence::WaitForFrame(uint32_t frameIndex)
{
    const uint64_t valueToWait = m_frameFenceValues[frameIndex];
    if (valueToWait == 0)
        return; // Frame jamais soumise, rien à attendre

    if (m_fence->GetCompletedValue() < valueToWait)
    {
        EVAL_HR(m_fence->SetEventOnCompletion(valueToWait, m_fenceEvent),
            "Fence SetEventOnCompletion failed");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DX12Fence::FlushGpu(ID3D12CommandQueue* queue)
{
    // Signale avec la valeur la plus haute et attend
    EVAL_HR(queue->Signal(m_fence.Get(), m_nextFenceValue),
        "Fence flush signal failed");

    if (m_fence->GetCompletedValue() < m_nextFenceValue)
    {
        EVAL_HR(m_fence->SetEventOnCompletion(m_nextFenceValue, m_fenceEvent),
            "Fence flush wait failed");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    ++m_nextFenceValue;
}