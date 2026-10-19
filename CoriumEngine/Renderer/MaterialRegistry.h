#pragma once

// -------------------------------------------------------
// Material — apparence + état de rendu (clé du PSO)
// -------------------------------------------------------
struct Material
{
	MaterialDesc                                desc;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso; // partagé via le PSO cache
};

class CORIUM_API MaterialRegistry
{
public:
	uint32_t Add(const MaterialDesc& desc, Microsoft::WRL::ComPtr<ID3D12PipelineState> pso);
	void     SetPipeline(uint32_t index, Microsoft::WRL::ComPtr<ID3D12PipelineState> pso);

	Material*       Get(uint32_t index);
	const Material* Get(uint32_t index) const;
	uint32_t        Find(const std::string& name) const;
	uint32_t        Count() const { return static_cast<uint32_t>(m_materials.size()); }

private:
	std::vector<Material> m_materials;
};
