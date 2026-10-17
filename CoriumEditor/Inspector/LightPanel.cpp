#include "stdafx.h"

void LightPanel::DrawProperties(EntityID, LightComponent& l)
{
	const char* types[] = { "Directional", "Point", "Spot" };
	int type = (int)l.type;
	PropertyLabel("Type");
	if (ImGui::Combo("##type", &type, types, 3))
		l.type = (LightType)type;
	PropertyLabel("Enabled");   ImGui::Checkbox("##enabled", &l.enabled);
	PropertyLabel("Color");     ImGui::ColorEdit3("##color", &l.color.x);
	PropertyLabel("Intensity"); ImGui::DragFloat("##intensity", &l.intensity, 0.01f, 0.f, 10.f);
	if (l.type != LightType::Directional)
	{
		PropertyLabel("Range"); ImGui::DragFloat("##range", &l.range, 0.1f, 0.f, 500.f);
	}
	if (l.type == LightType::Spot)
	{
		PropertyLabel("Angle"); ImGui::SliderFloat("##angle", &l.spotAngle, 1.f, 179.f, "%.0f deg");
	}
}
