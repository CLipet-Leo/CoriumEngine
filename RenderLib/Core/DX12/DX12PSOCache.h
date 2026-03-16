#pragma once

#include <string>
#include <unordered_map>

class CORIUM_API DX12PSOCache
{
public:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreate(
        const std::string& key,
        ID3D12Device* device,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

    void Clear();

private:
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_graphicsPSOs;
};
