#include "pch.h"
#include "DXGIAdapter.h"
#include "DXGIFactory.h"

DXGIFactory::DXGIFactory()
{
    UINT flags = 0;
#if defined(_DEBUG)
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    EVAL_HR(CreateDXGIFactory2(flags, IID_PPV_ARGS(&m_factory)), "DXGI Factory creation failed");
}

DXGIAdapter DXGIFactory::GetBestAdapter() const
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(i, &adapter); ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
            return DXGIAdapter(adapter.Get());
    }
    // Fallback WARP
    Microsoft::WRL::ComPtr<IDXGIAdapter1> warpAdapter;
    EVAL_HR(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), "DXGI WARP adapter enumeration failed");
    return DXGIAdapter(warpAdapter.Get());
}
