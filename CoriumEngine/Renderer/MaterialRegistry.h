#pragma once

// -------------------------------------------------------
// Material — apparence + état de rendu (clé du pipeline)
// -------------------------------------------------------
struct Material
{
	MaterialDesc                   desc;
	RhiPipeline*                   pipeline = nullptr; // partagé, possédé par le Renderer
	std::unique_ptr<RhiBuffer>     constants;          // MaterialConstants
	std::unique_ptr<RhiBindingSet> bindings;           // space1
};

class MaterialRegistry
{
public:
	uint32_t Add(Material&& material);

	Material*       Get(uint32_t index);
	const Material* Get(uint32_t index) const;
	uint32_t        Find(const std::string& name) const;
	uint32_t        Count() const { return static_cast<uint32_t>(m_materials.size()); }
	void            Clear() { m_materials.clear(); }

private:
	std::vector<Material> m_materials;
};
