#include "stdafx.h"

namespace
{
	// Accent « métal en fusion » (Corium)
	constexpr ImVec4 kAccent = { 1.00f, 0.48f, 0.12f, 1.00f };

	ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }

	// Réglages de l'éditeur (format INI de Windows)
	std::wstring SettingsPath() { return GetExecutableDir() + L"editor.ini"; }

	// Indexé par RendererBackend
	const char* const    kBackendNames[] = { "DirectX 12", "Vulkan" };
	const wchar_t* const kBackendKeys[] = { L"DX12", L"Vulkan" };
}

RendererBackend EditorUI::LoadBackendSetting()
{
	wchar_t value[16] = {};
	GetPrivateProfileStringW(L"Renderer", L"Backend", L"DX12", value, 16, SettingsPath().c_str());
	return _wcsicmp(value, L"Vulkan") == 0 ? RendererBackend::Vulkan : RendererBackend::DX12;
}

EditorUI::EditorUI()
	: m_nextBackend(LoadBackendSetting())
{
	// Ordre d'affichage dans l'inspecteur
	m_components.push_back(std::make_unique<TransformPanel>());
	m_components.push_back(std::make_unique<MeshPanel>());
	m_components.push_back(std::make_unique<CameraPanel>());
	m_components.push_back(std::make_unique<LightPanel>());
}

void EditorUI::ApplyStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::StyleColorsDark(&style);

	style.WindowPadding = ImVec2(8.f, 8.f);
	style.FramePadding = ImVec2(6.f, 4.f);
	style.ItemSpacing = ImVec2(8.f, 5.f);
	style.ItemInnerSpacing = ImVec2(4.f, 4.f);
	style.IndentSpacing = 14.f;
	style.ScrollbarSize = 12.f;
	style.WindowBorderSize = 1.f;
	style.FrameBorderSize = 0.f;
	style.WindowRounding = 0.f; // fenêtres détachées : bords droits comme l'OS
	style.ChildRounding = 3.f;
	style.FrameRounding = 3.f;
	style.PopupRounding = 3.f;
	style.GrabRounding = 3.f;
	style.TabRounding = 3.f;
	style.ScrollbarRounding = 6.f;
	style.WindowMenuButtonPosition = ImGuiDir_None;
	style.DockingSeparatorSize = 2.f;

	const ImVec4 bg      = { 0.10f, 0.10f, 0.11f, 1.f };
	const ImVec4 bgDark  = { 0.07f, 0.07f, 0.08f, 1.f };
	const ImVec4 frame   = { 0.16f, 0.16f, 0.18f, 1.f };
	const ImVec4 hovered = { 0.22f, 0.22f, 0.25f, 1.f };
	const ImVec4 active  = { 0.27f, 0.27f, 0.31f, 1.f };
	const ImVec4 line    = { 0.18f, 0.18f, 0.20f, 1.f };

	ImVec4* c = style.Colors;
	c[ImGuiCol_Text]                      = { 0.90f, 0.90f, 0.92f, 1.f };
	c[ImGuiCol_TextDisabled]              = { 0.45f, 0.45f, 0.48f, 1.f };
	c[ImGuiCol_WindowBg]                  = bg; // opaque, y compris les fenêtres détachées
	c[ImGuiCol_ChildBg]                   = bg;
	c[ImGuiCol_PopupBg]                   = bgDark;
	c[ImGuiCol_Border]                    = { 0.04f, 0.04f, 0.05f, 1.f };
	c[ImGuiCol_FrameBg]                   = frame;
	c[ImGuiCol_FrameBgHovered]            = hovered;
	c[ImGuiCol_FrameBgActive]             = active;
	c[ImGuiCol_TitleBg]                   = bgDark;
	c[ImGuiCol_TitleBgActive]             = bgDark;
	c[ImGuiCol_TitleBgCollapsed]          = bgDark;
	c[ImGuiCol_MenuBarBg]                 = bgDark;
	c[ImGuiCol_ScrollbarBg]               = bg;
	c[ImGuiCol_ScrollbarGrab]             = frame;
	c[ImGuiCol_ScrollbarGrabHovered]      = hovered;
	c[ImGuiCol_ScrollbarGrabActive]       = active;
	c[ImGuiCol_CheckMark]                 = kAccent;
	c[ImGuiCol_SliderGrab]                = kAccent;
	c[ImGuiCol_SliderGrabActive]          = kAccent;
	c[ImGuiCol_Button]                    = frame;
	c[ImGuiCol_ButtonHovered]             = hovered;
	c[ImGuiCol_ButtonActive]              = active;
	c[ImGuiCol_Header]                    = { 0.19f, 0.19f, 0.22f, 1.f };
	c[ImGuiCol_HeaderHovered]             = hovered;
	c[ImGuiCol_HeaderActive]              = active;
	c[ImGuiCol_Separator]                 = line;
	c[ImGuiCol_SeparatorHovered]          = WithAlpha(kAccent, 0.6f);
	c[ImGuiCol_SeparatorActive]           = kAccent;
	c[ImGuiCol_ResizeGrip]                = { 0.f, 0.f, 0.f, 0.f };
	c[ImGuiCol_ResizeGripHovered]         = WithAlpha(kAccent, 0.6f);
	c[ImGuiCol_ResizeGripActive]          = kAccent;
	c[ImGuiCol_Tab]                       = bgDark;
	c[ImGuiCol_TabHovered]                = hovered;
	c[ImGuiCol_TabSelected]               = bg;
	c[ImGuiCol_TabSelectedOverline]       = kAccent;
	c[ImGuiCol_TabDimmed]                 = bgDark;
	c[ImGuiCol_TabDimmedSelected]         = bg;
	c[ImGuiCol_TabDimmedSelectedOverline] = { 0.f, 0.f, 0.f, 0.f };
	c[ImGuiCol_DockingPreview]            = WithAlpha(kAccent, 0.5f);
	c[ImGuiCol_DockingEmptyBg]            = bgDark;
	c[ImGuiCol_TableHeaderBg]             = frame;
	c[ImGuiCol_TableBorderStrong]         = line;
	c[ImGuiCol_TableBorderLight]          = line;
	c[ImGuiCol_TextSelectedBg]            = WithAlpha(kAccent, 0.35f);
	c[ImGuiCol_NavCursor]                 = kAccent;
	c[ImGuiCol_PlotHistogram]             = kAccent;
}

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
	// Échantillonné toutes les 0.5 s : lisible au lieu de clignoter à chaque frame
	const double now = ImGui::GetTime();
	if (now - m_statsTime >= 0.5)
	{
		m_statsTime = now;
		m_statsFps = ImGui::GetIO().Framerate;
	}

	if (ImGui::Begin("GPU Stats") && BeginProperties("##stats"))
	{
		PropertyLabel("FPS");     ImGui::Text("%.1f", m_statsFps);
		PropertyLabel("Frame");   ImGui::Text("%.3f ms", 1000.f / m_statsFps);
		PropertyLabel("Backend");
		int next = (int)m_nextBackend;
		if (ImGui::Combo("##backend", &next, kBackendNames, IM_ARRAYSIZE(kBackendNames)))
		{
			m_nextBackend = (RendererBackend)next;
			WritePrivateProfileStringW(L"Renderer", L"Backend", kBackendKeys[next], SettingsPath().c_str());
		}
		if (m_nextBackend != stats.backend)
		{
			PropertyLabel("");
			ImGui::TextDisabled("Actif : %s (redémarrer)", kBackendNames[(int)stats.backend]);
		}
		PropertyLabel("Adapter"); ImGui::TextWrapped("%ls", stats.adapterName);
		ImGui::EndTable();
	}
	ImGui::End();
}

void EditorUI::DrawScenePanel(Scene& scene)
{
	if (ImGui::Begin("Scene"))
	{
		// Sélection en accent (le gris de Header est réservé aux sections de l'inspecteur)
		ImGui::PushStyleColor(ImGuiCol_Header, WithAlpha(kAccent, 0.45f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, WithAlpha(kAccent, 0.55f));
		for (EntityID root : scene.GetRootEntities())
			DrawEntityNode(scene, root);
		ImGui::PopStyleColor(2);

		// Clic dans le vide : désélection
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
			m_selectedEntity = NULL_ENTITY;
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
	if (!e) return;

	auto* h = scene.GetComponent<HierarchyComponent>(id);
	bool  hasChildren = h && !h->children.empty();

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
	if (!hasChildren)  flags |= ImGuiTreeNodeFlags_Leaf;
	if (id == m_selectedEntity) flags |= ImGuiTreeNodeFlags_Selected;

	// Entités inactives grisées, pas masquées : il faut pouvoir les resélectionner pour les réactiver
	if (!e->active) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	bool open = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", e->name.c_str());
	if (!e->active) ImGui::PopStyleColor();

	// Le clic sur la flèche déplie sans sélectionner
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
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

	// En-tête : actif + nom éditable
	ImGui::Checkbox("##active", &e->active);
	ImGui::SameLine();
	char buf[128];
	strncpy_s(buf, e->name.c_str(), _TRUNCATE);
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputText("##name", buf, sizeof(buf)))
		e->name = buf;
	ImGui::Spacing();

	for (auto& component : m_components)
		component->Draw(scene, id);
}
