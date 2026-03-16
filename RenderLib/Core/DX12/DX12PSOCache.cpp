#include "pch.h"
#include "DX12PSOCache.h"

Microsoft::WRL::ComPtr<ID3D12PipelineState> DX12PSOCache::GetOrCreate(
    const std::string& key,
    ID3D12Device* device,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
{
    auto it = m_graphicsPSOs.find(key);
    if (it != m_graphicsPSOs.end())
        return it->second;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
    EVAL_HR(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso)), "Graphics PSO creation failed");

    m_graphicsPSOs.emplace(key, pso);
    return pso;
}

void DX12PSOCache::Clear()
{
    m_graphicsPSOs.clear();
}
