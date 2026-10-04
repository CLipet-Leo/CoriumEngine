#include "pch.h"
#include "EditorUI.h"
#include <imgui.h>

void EditorUI::Draw(Scene& scene, uint32_t frameIndex, const wchar_t* adapterName)
{
	ImGuiWindowFlags dockFlags =
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::Begin("##DockSpace", nullptr, dockFlags);
	ImGui::PopStyleVar();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGui::DockSpace(ImGui::GetID("MainDock"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
	}
	ImGui::End();

	DrawStats(frameIndex, adapterName);
	DrawScenePanel(scene);
}

void EditorUI::DrawStats(uint32_t frameIndex, const wchar_t* adapterName)
{
	if (ImGui::Begin("GPU Stats"))
	{
		ImGui::Text("FPS       : %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame     : %.3f ms", 1000.f / ImGui::GetIO().Framerate);
		ImGui::Text("Frame idx : %u", frameIndex);
		ImGui::Separator();
		ImGui::Text("Adapter   : %ls", adapterName);
	}
	ImGui::End();
}

void EditorUI::DrawScenePanel(Scene& scene)
{
	if (ImGui::Begin("Scene"))
	{
		for (EntityID root : scene.GetRootEntities())
			DrawEntityNode(scene, root);
	}
	ImGui::End();

	// Inspecteur de l'entité sélectionnée
	if (ImGui::Begin("Inspector") && m_selectedEntity != NULL_ENTITY)
		DrawInspector(scene, m_selectedEntity);
	ImGui::End();
}

void EditorUI::DrawEntityNode(Scene& scene, EntityID id)
{
	Entity* e = scene.GetEntity(id);
	if (!e || !e->active) return;

	auto* h = scene.GetComponent<HierarchyComponent>(id);
	bool  hasChildren = h && !h->children.empty();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanAvailWidth;
	if (!hasChildren)  flags |= ImGuiTreeNodeFlags_Leaf;
	if (id == m_selectedEntity) flags |= ImGuiTreeNodeFlags_Selected;

	bool open = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", e->name.c_str());

	if (ImGui::IsItemClicked())
		m_selectedEntity = id;

	if (open)
	{
		if (hasChildren)
			for (EntityID child : h->children)
				DrawEntityNode(scene, child);
		ImGui::TreePop();
	}
}

void EditorUI::DrawInspector(Scene& scene, EntityID id)
{
	Entity* e = scene.GetEntity(id);
	if (!e) return;

	// Nom éditable
	char buf[128];
	strncpy_s(buf, e->name.c_str(), sizeof(buf));
	if (ImGui::InputText("##name", buf, sizeof(buf)))
		e->name = buf;

	ImGui::SameLine();
	ImGui::Checkbox("Active", &e->active);
	ImGui::Separator();

	// Transform
	if (auto* t = scene.GetComponent<TransformComponent>(id))
	{
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::DragFloat3("Position", &t->position.x, 0.01f)) t->dirty = true;
			if (ImGui::DragFloat3("Rotation", &t->rotation.x, 0.5f))  t->dirty = true;
			if (ImGui::DragFloat3("Scale", &t->scale.x, 0.01f)) t->dirty = true;
		}
	}

	// Light
	if (auto* l = scene.GetComponent<LightComponent>(id))
	{
		if (ImGui::CollapsingHeader("Light"))
		{
			const char* types[] = { "Directional", "Point", "Spot" };
			int type = (int)l->type;
			if (ImGui::Combo("Type", &type, types, 3))
				l->type = (LightType)type;
			ImGui::ColorEdit3("Color", &l->color.x);
			ImGui::DragFloat("Intensity", &l->intensity, 0.01f, 0.f, 10.f);
			if (l->type != LightType::Directional)
				ImGui::DragFloat("Range", &l->range, 0.1f, 0.f, 500.f);
		}
	}
}
