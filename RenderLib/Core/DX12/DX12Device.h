#pragma once

class CORIUM_API DX12Device
{
public:
    DX12Device() = default;
    explicit DX12Device(const DXGIAdapter& adapter);

    ID3D12Device* Get() const { return m_device.Get(); }
    bool IsValid() const { return m_device != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};
