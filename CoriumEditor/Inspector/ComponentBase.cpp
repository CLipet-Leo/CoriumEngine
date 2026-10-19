#include "stdafx.h"

// Libellés alignés, widgets sur toute la largeur restante
bool BeginProperties(const char* id)
{
	if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp))
		return false;
	ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 5.f);
	ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
	return true;
}

void PropertyLabel(const char* label)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableNextColumn();
	ImGui::SetNextItemWidth(-FLT_MIN);
}

bool Vec3Property(const char* label, Vec3& v, float speed, float resetValue)
{
	static const ImVec4 axisColors[3] = {
		{ 0.80f, 0.20f, 0.20f, 1.f }, { 0.25f, 0.62f, 0.20f, 1.f }, { 0.20f, 0.40f, 0.85f, 1.f } };
	static const char* axisNames[3] = { "X", "Y", "Z" };

	PropertyLabel(label);
	ImGui::PushID(label);
	const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float button = ImGui::GetFrameHeight();
	const float field = (ImGui::GetContentRegionAvail().x - 2.f * spacing) / 3.f - button;

	bool changed = false;
	float* f = &v.x;
	for (int i = 0; i < 3; ++i)
	{
		ImGui::PushID(i);
		if (i > 0) ImGui::SameLine(0.f, spacing);
		const ImVec4 c = axisColors[i];
		ImGui::PushStyleColor(ImGuiCol_Button, c);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(c.x * 1.25f, c.y * 1.25f, c.z * 1.25f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, c);
		if (ImGui::Button(axisNames[i], ImVec2(button, button)))
		{
			f[i] = resetValue;
			changed = true;
		}
		ImGui::PopStyleColor(3);
		ImGui::SameLine(0.f, 0.f);
		ImGui::SetNextItemWidth(field);
		changed |= ImGui::DragFloat("##v", &f[i], speed, 0.f, 0.f, "%.2f");
		ImGui::PopID();
	}
	ImGui::PopID();
	return changed;
}
