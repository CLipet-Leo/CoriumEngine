#include "stdafx.h"

void EditorUI::Draw(Scene& scene, const RendererStats& stats, ImTextureID sceneTexture)
{
	DrawDockSpace();
	DrawViewport(sceneTexture);
	DrawStats(stats);
	DrawScenePanel(scene);
}

void EditorUI::DrawDockSpace()
{
	ImGuiWindowFlags dockFlags =
		ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->Pos);
	ImGui::SetNextWindowSize(viewport->Size);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::Begin("##DockSpace", nullptr, dockFlags);
	ImGui::PopStyleVar();

	// Layout par défaut au premier lancement (sinon restauré depuis imgui.ini)
	const ImGuiID dockId = ImGui::GetID("MainDock");
	if (!ImGui::DockBuilderGetNode(dockId))
	{
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockId, viewport->Size);
		ImGuiID center = dockId;
		const ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.2f, nullptr, &center);
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.25f, nullptr, &center);
		const ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.3f, nullptr, &right);
		ImGui::DockBuilderDockWindow("Viewport", center);
		ImGui::DockBuilderDockWindow("Scene", left);
		ImGui::DockBuilderDockWindow("Inspector", right);
		ImGui::DockBuilderDockWindow("GPU Stats", rightBottom);
		ImGui::DockBuilderFinish(dockId);
	}
	ImGui::DockSpace(dockId);
	ImGui::End();
}

void EditorUI::DrawViewport(ImTextureID sceneTexture)
{
	m_viewportHovered = false;

	// Taille si absent d'imgui.ini (sinon l'auto-ajustement au contenu, qui suit le panneau, l'écrase)
	ImGui::SetNextWindowSize(ImVec2(640.f, 360.f), ImGuiCond_FirstUseEver);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	const bool visible = ImGui::Begin("Viewport");
	ImGui::PopStyleVar();
	if (visible)
	{
		// La cible offscreen prend la taille du panneau au prochain Render (même frame)
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x >= 1.f && avail.y >= 1.f)
		{
			m_viewportWidth = (uint32_t)avail.x;
			m_viewportHeight = (uint32_t)avail.y;
			ImGui::Image(sceneTexture, ImVec2((float)m_viewportWidth, (float)m_viewportHeight));
			m_viewportHovered = ImGui::IsItemHovered();
		}
	}
	ImGui::End();
}

void EditorUI::DrawStats(const RendererStats& stats)
{
	if (ImGui::Begin("GPU Stats"))
	{
		ImGui::Text("FPS       : %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame     : %.3f ms", 1000.f / ImGui::GetIO().Framerate);
		ImGui::Text("Frame idx : %u", stats.frameIndex);
		ImGui::Separator();
		ImGui::Text("Adapter   : %ls", stats.adapterName);
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
			if (m_eulerEntity != id || memcmp(&m_eulerSource, &t->rotation, sizeof(Quat)) != 0)
			{
				m_eulerEntity = id;
				m_eulerSource = t->rotation;
				m_euler = ToEulerDeg(t->rotation);
			}
			if (ImGui::DragFloat3("Rotation", &m_euler.x, 0.5f))
			{
				t->SetRotationDeg(m_euler);
				m_eulerSource = t->rotation;
			}
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
