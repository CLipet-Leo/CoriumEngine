#pragma once

// -------------------------------------------------------
// Mesh procédural : nom + paramètres de génération (sérialisés)
// -------------------------------------------------------
enum class MeshPrimitive : uint8_t { Box };

struct MeshDesc
{
    std::string   name;
    MeshPrimitive primitive = MeshPrimitive::Box;
    float         size = 1.f;
};

// -------------------------------------------------------
// Material — apparence + état de rendu (clé du PSO)
// -------------------------------------------------------
enum class CullMode : uint8_t { None, Front, Back };
enum class FillMode : uint8_t { Solid, Wireframe };
enum class BlendMode : uint8_t { Opaque, AlphaBlend };

struct MaterialDesc
{
	std::string name;
	Vec4   		baseColor = { 1.f, 1.f, 1.f, 1.f }; // multiplié par la couleur du vertex
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
