#pragma once

// -------------------------------------------------------
// Material — apparence + état de rendu (clé du PSO)
// -------------------------------------------------------
enum class CullMode : uint8_t { None, Front, Back };
enum class FillMode : uint8_t { Solid, Wireframe };
enum class BlendMode : uint8_t { Opaque, AlphaBlend };

struct MaterialDesc
{
	std::string name;
	XMFLOAT4    baseColor = { 1.f, 1.f, 1.f, 1.f }; // multiplié par la couleur du vertex
	CullMode    cullMode = CullMode::Back;
	FillMode    fillMode = FillMode::Solid;
	BlendMode   blendMode = BlendMode::Opaque;

	// Clé unique de l'état pipeline (sans la couleur)
	std::string GetPipelineKey() const
	{
		return "cull" + std::to_string((int)cullMode) +
			"_fill" + std::to_string((int)fillMode) +
			"_blend" + std::to_string((int)blendMode);
	}
};

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
