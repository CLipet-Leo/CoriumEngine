#pragma once

using Microsoft::WRL::ComPtr;

class CORIUM_API DXGIFactory
{
public:
    DXGIFactory();
    ~DXGIFactory() = default;

    DXGIAdapter GetBestAdapter() const;
    IDXGIFactory4* Get() const { return m_factory.Get(); }

private:
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_factory;
};
