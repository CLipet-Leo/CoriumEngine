#pragma once

class CORIUM_API DXGIAdapter
{
public:
    DXGIAdapter() = default;
    explicit DXGIAdapter(IDXGIAdapter1* adapter);

    IDXGIAdapter1* Get() const { return m_adapter.Get(); }
    bool IsValid() const { return m_adapter != nullptr; }

private:
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
};
