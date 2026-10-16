#include "pch.h"
#include "DX12Device.h"

DX12Device::DX12Device(const DXGIAdapter& adapter)
{
    EVAL_HR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)), "D3D12 Device creation failed");
}
