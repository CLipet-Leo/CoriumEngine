#include "pch.h"
#include "DX12CommandObjects.h"

DX12CommandObjects::DX12CommandObjects(ID3D12Device* device, uint32_t frameCount)
{
    m_allocators.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i)
    {
        EVAL_HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocators[i])),
            "Command Allocator creation failed");
    }
    EVAL_HR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)),
        "Command List creation failed");
    EVAL_HR(m_commandList->Close(), "Command List initial close failed");
}

void DX12CommandObjects::Reset(uint32_t frameIndex)
{
    EVAL_HR(m_allocators[frameIndex]->Reset(), "Command Allocator reset failed");
    EVAL_HR(m_commandList->Reset(m_allocators[frameIndex].Get(), nullptr), "Command List reset failed");
}

void DX12CommandObjects::Close()
{
    EVAL_HR(m_commandList->Close(), "Command List close failed");
}
