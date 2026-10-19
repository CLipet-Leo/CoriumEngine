#include "pch.h"
#include <imgui.h>
#include "Renderer.h"

extern "C"
{
	IRenderer* CreateRenderer(RendererBackend backend) { return new Renderer(backend); }
	void DestroyRenderer(IRenderer* r) { delete r; }
}

static std::vector<uint8_t> ReadBinaryFile(const std::wstring& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		std::string narrow;
		for (wchar_t c : path)
			narrow += static_cast<char>(c);
		throw std::runtime_error("Shader introuvable : " + narrow);
	}
	return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}

Renderer::Renderer(RendererBackend backend)
	: m_backend(backend)
{
}

Renderer::~Renderer()
{
	Shutdown();
}

bool Renderer::Init(HWND hWnd, uint32_t width, uint32_t height)
{
	try
	{
#if defined(_DEBUG)
		MathSelfCheck();
#endif
		m_width = width;
		m_height = height;
		m_device = m_backend == RendererBackend::Vulkan
			? CreateVulkanDevice(hWnd, width, height)
			: CreateDX12Device(hWnd, width, height);

		CreateDepth(width, height);

		// Modèle de binding : un espace par fréquence de mise à jour (cf. ShaderTypes.h)
		RhiPipelineLayoutDesc layoutDesc;
		layoutDesc.spaces = {
			{ { RhiBindingType::ConstantBuffer, 0 }, { RhiBindingType::ConstantBuffer, 1 } },
			{ { RhiBindingType::ConstantBuffer, 0 }, { RhiBindingType::Texture, 0 }, { RhiBindingType::Sampler, 0 } },
			{ { RhiBindingType::DynamicConstantBuffer, 0 } },
		};
		m_layout = m_device->CreatePipelineLayout(layoutDesc);

		// Shaders compilés au build par DXC (cible CompileShaders), écrits à côté de l'exécutable
		const std::wstring shaderDir = GetExecutableDir() + L"Shaders\\";
		const std::wstring ext = m_device->GetShaderExtension();
		m_vertexShader = ReadBinaryFile(shaderDir + L"Basic_vs" + ext);
		m_pixelShader = ReadBinaryFile(shaderDir + L"Basic_ps" + ext);

		for (FrameData& frame : m_frames)
		{
			frame.sceneCB = m_device->CreateBuffer({ RhiAlign(sizeof(SceneConstants), kRhiConstantAlign),
				RhiMemory::Upload, RhiBufferUsage_Constant });
			frame.lightCB = m_device->CreateBuffer({ RhiAlign(sizeof(LightConstants), kRhiConstantAlign),
				RhiMemory::Upload, RhiBufferUsage_Constant });
			frame.bindings = m_device->CreateBindingSet({ m_layout.get(), 0, {
				{ frame.sceneCB.get(), 0, frame.sceneCB->GetSize() },
				{ frame.lightCB.get(), 0, frame.lightCB->GetSize() } } });
		}

		CreateCheckerTexture();
		m_sampler = m_device->CreateSampler();
		m_renderSystem.Init(*m_device, m_layout.get());

		// Matériau par défaut (index 0)
		MaterialDesc defaultMat;
		defaultMat.name = "Default";
		CreateMaterial(defaultMat);

		return true;
	}
	catch (const std::exception& e)
	{
		OutputDebugStringA(e.what());
		OutputDebugStringA("\n");
		Shutdown();
		return false;
	}
}

void Renderer::CreateDepth(uint32_t width, uint32_t height)
{
	m_depth = m_device->CreateTexture({ width, height, RhiFormat::D32_Float, RhiTextureUsage_Depth });
	m_depthState = RhiState::Undefined;
}

void Renderer::CreateCheckerTexture()
{
	// Damier 64x64 en cases de 8 : blanc / gris clair, la couleur des vertices reste lisible
	constexpr uint32_t size = 64;
	constexpr uint32_t cell = 8;
	const uint64_t rowPitch = RhiAlign(size * 4, kRhiTextureRowAlign);

	m_checker = m_device->CreateTexture({ size, size, RhiFormat::RGBA8_UNorm,
		RhiTextureUsage_Sampled | RhiTextureUsage_CopyDst });
	m_checkerStaging = m_device->CreateBuffer({ rowPitch * size, RhiMemory::Upload, RhiBufferUsage_CopySrc });

	uint8_t* pixels = m_checkerStaging->GetMapped();
	for (uint32_t y = 0; y < size; ++y)
	{
		uint32_t* row = reinterpret_cast<uint32_t*>(pixels + y * rowPitch);
		for (uint32_t x = 0; x < size; ++x)
			row[x] = ((x / cell + y / cell) % 2) ? 0xFFFFFFFFu : 0xFFA0A0A0u;
	}
}

RhiPipeline* Renderer::GetPipelineFor(const MaterialDesc& mat)
{
	const std::string key = mat.GetPipelineKey();
	auto it = m_pipelines.find(key);
	if (it != m_pipelines.end())
		return it->second.get();

	RhiPipelineDesc desc;
	desc.layout = m_layout.get();
	desc.vertexShader = &m_vertexShader;
	desc.pixelShader = &m_pixelShader;
	desc.attributes = {
		{ "POSITION", RhiFormat::RGB32_Float, offsetof(MeshVertex, position) },
		{ "COLOR",    RhiFormat::RGBA32_Float, offsetof(MeshVertex, color) },
		{ "TEXCOORD", RhiFormat::RG32_Float, offsetof(MeshVertex, uv) },
	};
	desc.vertexStride = sizeof(MeshVertex);
	desc.cullMode = mat.cullMode;
	desc.fillMode = mat.fillMode;
	desc.blendMode = mat.blendMode;
	desc.colorFormat = m_device->GetBackBufferFormat();
	desc.depthFormat = RhiFormat::D32_Float;

	RhiPipeline* pipeline = (m_pipelines[key] = m_device->CreatePipeline(desc)).get();
	return pipeline;
}

uint32_t Renderer::CreateMaterial(const MaterialDesc& desc)
{
	Material material;
	material.desc = desc;
	material.pipeline = GetPipelineFor(desc);

	material.constants = m_device->CreateBuffer({ kRhiConstantAlign, RhiMemory::Upload, RhiBufferUsage_Constant });
	const MaterialConstants constants = { desc.baseColor };
	memcpy(material.constants->GetMapped(), &constants, sizeof(constants));

	material.bindings = m_device->CreateBindingSet({ m_layout.get(), 1, {
		{ material.constants.get(), 0, kRhiConstantAlign },
		{ nullptr, 0, 0, m_checker.get() },
		{ nullptr, 0, 0, nullptr, m_sampler.get() } } });

	return m_materialRegistry.Add(std::move(material));
}

void Renderer::RefreshMaterial(uint32_t index)
{
	if (Material* mat = m_materialRegistry.Get(index))
		mat->pipeline = GetPipelineFor(mat->desc);
}

RendererStats Renderer::GetStats() const
{
	return { m_device ? m_device->GetAdapterName() : L"", m_backend };
}

void Renderer::InitUI(ImGuiContext* context, void* (*allocFunc)(size_t, void*),
	void (*freeFunc)(void*, void*), void* allocUserData)
{
	// ImGui est une lib statique : la DLL a sa propre copie de GImGui et des allocateurs.
	// Même manifeste vcpkg que l'exe -> même version et même layout de structures.
	ImGui::SetAllocatorFunctions(allocFunc, freeFunc, allocUserData);
	ImGui::SetCurrentContext(context);
	IMGUI_CHECKVERSION();

	m_ui = m_device->CreateUI();

	// Taille de la fenêtre en attendant celle du panneau
	m_requestedSceneWidth = m_width;
	m_requestedSceneHeight = m_height;
	ApplySceneSize();
}

void Renderer::ShutdownUI()
{
	if (!m_ui)
		return;

	// Le GPU peut encore lire les buffers ImGui et la cible offscreen
	m_device->WaitIdle();
	m_ui.reset();
	m_sceneTarget.reset();
	m_sceneTextureId = 0;
	CreateDepth(m_width, m_height);
	ImGui::SetCurrentContext(nullptr);
}

void Renderer::BeginUIFrame()
{
	if (m_ui)
		m_ui->BeginFrame();
}

void Renderer::SetSceneSize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;
	m_requestedSceneWidth = width;
	m_requestedSceneHeight = height;
}

uint64_t Renderer::GetSceneTexture() const
{
	return m_sceneTextureId;
}

void Renderer::ApplySceneSize()
{
	if (m_sceneTarget && m_requestedSceneWidth == m_sceneTarget->GetWidth()
		&& m_requestedSceneHeight == m_sceneTarget->GetHeight())
		return;

	// Les frames en vol lisent encore la texture et son descripteur
	m_device->WaitIdle();
	m_sceneTarget = m_device->CreateTexture({ m_requestedSceneWidth, m_requestedSceneHeight,
		m_device->GetBackBufferFormat(), RhiTextureUsage_RenderTarget | RhiTextureUsage_Sampled });
	m_sceneTargetState = RhiState::Undefined;
	m_sceneTextureId = m_ui->SetSceneTexture(m_sceneTarget.get());
	CreateDepth(m_requestedSceneWidth, m_requestedSceneHeight);
}

uint32_t Renderer::RegisterMesh(const MeshDesc& desc)
{
	return m_meshRegistry.Register(desc, *m_device);
}

const MeshDesc* Renderer::GetMeshDesc(uint32_t index) const
{
	const GpuMesh* mesh = m_meshRegistry.Get(index);
	return mesh ? &mesh->desc : nullptr;
}

const MaterialDesc* Renderer::GetMaterialDesc(uint32_t index) const
{
	const Material* material = m_materialRegistry.Get(index);
	return material ? &material->desc : nullptr;
}

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;

	m_device->ResizeSwapChain(width, height);
	// Avec l'UI, la profondeur suit la cible offscreen (ApplySceneSize)
	if (!m_sceneTarget)
		CreateDepth(width, height);

	m_width = width;
	m_height = height;
}

void Renderer::DrawScene(Scene& scene, RhiCommandList* cmd, RhiTexture* target, const Mat4& viewProj)
{
	const float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.f };
	cmd->BeginRenderPass(target, m_depth.get(), clearColor);
	const uint32_t frameIndex = m_device->GetFrameIndex();
	m_renderSystem.Submit(scene, cmd, frameIndex, m_frames[frameIndex].bindings.get(),
		m_meshRegistry, m_materialRegistry, viewProj);
	cmd->EndRenderPass();
}

void Renderer::Render(Scene& scene, const CameraView* view)
{
	// Sans UI la scène va dans la swapchain, sinon dans la cible offscreen (panneau Viewport)
	if (m_ui)
		ApplySceneSize();

	RhiCommandList* cmd = m_device->BeginFrame();
	if (!cmd)
	{
		// Fenêtre minimisée : la frame ImGui commencée par l'exe doit quand même se terminer
		if (m_ui)
		{
			ImGui::EndFrame();
			if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				ImGui::UpdatePlatformWindows();
		}
		return;
	}
	FrameData& frame = m_frames[m_device->GetFrameIndex()];
	frame.release.clear();

	RhiTexture* sceneTarget = m_sceneTarget ? m_sceneTarget.get() : m_device->GetBackBuffer();

	// 1. Collecte caméra / lumières depuis la scène
	m_lightSystem.Collect(scene);
	const float aspect = sceneTarget->GetHeight() > 0
		? (float)sceneTarget->GetWidth() / (float)sceneTarget->GetHeight() : 1.f;
	const Mat4 viewProj = m_cameraSystem.Update(scene, aspect, m_sceneConstants, view);

	// 2. Upload des données de frame
	memcpy(frame.sceneCB->GetMapped(), &m_sceneConstants, sizeof(SceneConstants));
	m_lightSystem.Upload(frame.lightCB->GetMapped());

	// 3. Copies en attente (meshes, damier), dans la command list de la frame
	m_meshRegistry.RecordPendingUploads(cmd, frame.release);
	if (m_checkerStaging)
	{
		cmd->Barrier(m_checker.get(), RhiState::Undefined, RhiState::CopyDest);
		cmd->CopyBufferToTexture(m_checker.get(), m_checkerStaging.get());
		cmd->Barrier(m_checker.get(), RhiState::CopyDest, RhiState::ShaderRead);
		frame.release.push_back(std::move(m_checkerStaging));
	}
	// Profondeur réécrite à chaque frame : dépendance avec les écritures de la frame précédente
	cmd->Barrier(m_depth.get(), m_depthState, RhiState::Depth);
	m_depthState = RhiState::Depth;

	RhiTexture* backBuffer = m_device->GetBackBuffer();
	cmd->Barrier(backBuffer, RhiState::Present, RhiState::RenderTarget);

	if (m_ui)
	{
		// 4a. Scène → offscreen, puis lue par ImGui (Image du panneau Viewport)
		cmd->Barrier(m_sceneTarget.get(), m_sceneTargetState, RhiState::RenderTarget);
		DrawScene(scene, cmd, m_sceneTarget.get(), viewProj);
		cmd->Barrier(m_sceneTarget.get(), RhiState::RenderTarget, RhiState::ShaderRead);
		m_sceneTargetState = RhiState::ShaderRead;

		// 5a. ImGui → swapchain (fond masqué par le dockspace)
		const float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
		cmd->BeginRenderPass(backBuffer, nullptr, clearColor);
		m_ui->Render(cmd);
		cmd->EndRenderPass();
	}
	else
	{
		// 4b. Scène directement dans la swapchain
		DrawScene(scene, cmd, backBuffer, viewProj);
	}

	cmd->Barrier(backBuffer, RhiState::RenderTarget, RhiState::Present);
	m_device->EndFrame();

	// Fenêtres détachées : peuvent afficher la cible offscreen de la frame soumise
	if (m_ui && (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable))
	{
		ImGui::UpdatePlatformWindows();
		if (ImGui::GetPlatformIO().Viewports.Size > 1)
			m_ui->RenderPlatformWindows();
	}

	m_device->Present();
}

void Renderer::Shutdown()
{
	if (!m_device)
		return;
	m_device->WaitIdle();

	// Sécurité si l'exe n'a pas appelé ShutdownUI : son contexte ImGui doit encore exister
	m_ui.reset();
	m_sceneTarget.reset();

	// Toutes les ressources RHI avant le device
	m_materialRegistry.Clear();
	m_meshRegistry.Clear();
	m_renderSystem.Shutdown();
	m_pipelines.clear();
	for (FrameData& frame : m_frames)
		frame = {};
	m_sampler.reset();
	m_checker.reset();
	m_checkerStaging.reset();
	m_depth.reset();
	m_layout.reset();
	m_device.reset();
}
