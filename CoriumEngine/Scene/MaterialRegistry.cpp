#include "pch.h"
#include "MaterialRegistry.h"

uint32_t MaterialRegistry::Add(const MaterialDesc& desc, Microsoft::WRL::ComPtr<ID3D12PipelineState> pso)
{
	m_materials.push_back({ desc, std::move(pso) });
	return static_cast<uint32_t>(m_materials.size() - 1);
}

void MaterialRegistry::SetPipeline(uint32_t index, Microsoft::WRL::ComPtr<ID3D12PipelineState> pso)
{
	if (index < m_materials.size())
		m_materials[index].pso = std::move(pso);
}

Material* MaterialRegistry::Get(uint32_t index)
{
	return index < m_materials.size() ? &m_materials[index] : nullptr;
}

const Material* MaterialRegistry::Get(uint32_t index) const
{
	return index < m_materials.size() ? &m_materials[index] : nullptr;
}

uint32_t MaterialRegistry::Find(const std::string& name) const
{
	for (uint32_t i = 0; i < m_materials.size(); ++i)
		if (m_materials[i].desc.name == name)
			return i;
	return UINT32_MAX;
}
