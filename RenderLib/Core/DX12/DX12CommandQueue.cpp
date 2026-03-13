#include "pch.h"
#include "DX12CommandQueue.h"

DX12CommandQueue::DX12CommandQueue(ID3D12Device* device)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    EVAL_HR(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_commandQueue)), "Command Queue creation failed");
}
