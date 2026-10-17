#include "stdafx.h"

void MeshPanel::DrawProperties(EntityID, MeshComponent& m)
{
	// Noms dans le registre du renderer, non exposé : index seulement
	PropertyLabel("Mesh");         ImGui::TextDisabled("#%u", m.meshIndex);
	PropertyLabel("Material");     ImGui::TextDisabled("#%u", m.materialIndex);
	PropertyLabel("Visible");      ImGui::Checkbox("##visible", &m.visible);
	PropertyLabel("Cast shadows"); ImGui::Checkbox("##shadows", &m.castShadows);
}
