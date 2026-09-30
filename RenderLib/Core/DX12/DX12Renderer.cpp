#include "pch.h"
#include "DX12Renderer.h"
#include "DX12ImGui.h"
#include <imgui.h>
#include "../Geometry/Types.h"
#include "../Geometry/BufferGeometry.h"
#include "../Geometry/BoxGeometry.h"
#include <chrono>
#include <cstring>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

static void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::runtime_error("HRESULT failure");
	}
}

extern "C"
{
	IRenderer* CreateRenderer() { return new DX12Renderer(); }
	void DestroyRenderer(IRenderer* r) { delete r; }
}

DX12Renderer::DX12Renderer() = default;

DX12Renderer::~DX12Renderer()
{
	Shutdown();
}

bool DX12Renderer::Init(HWND hWnd, uint32_t width, uint32_t height)
{
	try
	{
		m_width = width;
		m_height = height;

#if defined(_DEBUG)
		m_debug = std::make_unique<DX12Debug>();
#endif
		m_factory = std::make_unique<DXGIFactory>();
		m_adapter = std::make_unique<DXGIAdapter>(m_factory->GetBestAdapter());
		m_device = std::make_unique<DX12Device>(*m_adapter);

#if defined(_DEBUG)
		m_debug->SetupInfoQueue(m_device->Get());
#endif
		m_commandQueue = std::make_unique<DX12CommandQueue>(m_device->Get());

		m_descriptorHeaps = std::make_unique<DX12DescriptorHeaps>();
		m_descriptorHeaps->Initialize(m_device->Get(), m_frameCount, 1024);

		m_memoryManager = std::make_unique<DX12MemoryManager>();
		m_memoryManager->Initialize(m_device->Get(), m_adapter->Get());

		m_swapChain = std::make_unique<DX12SwapChain>(
			m_factory->Get(), m_commandQueue->Get(), m_device->Get(),
			hWnd, width, height, m_frameCount, m_descriptorHeaps.get());
		m_commandObjects = std::make_unique<DX12CommandObjects>(m_device->Get(), m_frameCount);
		m_fence = std::make_unique<DX12Fence>(m_device->Get(), m_frameCount);
		m_depthStencil = std::make_unique<DX12DepthStencil>(m_device->Get(), width, height, m_descriptorHeaps.get(), m_memoryManager.get());

		if (!CreateSrvHeap())          return false;
		if (!CreatePipeline())         return false;
		if (!CreateConstantBuffers())  return false;

		// Init des systèmes
		m_renderSystem.Init(m_device->Get(), m_frameCount);

		// Les uploads de meshes sont différés à la première frame
		LoadDefaultScene();


		// ImGui — slot 0 du heap SRV réservé pour la font
		auto cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
		auto gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
		m_srvHeapUsed = 1; // slot 0 consommé par ImGui

		m_imgui = std::make_unique<DX12ImGui>();
		m_imgui->Init(hWnd, m_device->Get(), m_frameCount,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			m_srvHeap.Get(), cpuHandle, gpuHandle);

		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
		m_viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
		m_scissorRect = { 0, 0, (LONG)width, (LONG)height };

		return m_device->IsValid();
	}
	catch (const DX12Exception& e)
	{
		OutputDebugStringA(e.what());
		Shutdown();
		return false;
	}
}

bool DX12Renderer::CreateSrvHeap()
{
	// 64 slots : slot 0 = ImGui font, reste = textures futures
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = 64;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	EVAL_HR(m_device->Get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap)),
		"SRV Heap creation failed");

	m_srvDescriptorSize = m_device->Get()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return true;
}

bool DX12Renderer::CreateConstantBuffers()
{
	auto uploadHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

	const UINT sceneSize = (sizeof(SceneConstants) + 255) & ~255;
	const UINT lightSize = (sizeof(LightConstants) + 255) & ~255;

	m_perFrameCBs.resize(m_frameCount);
	for (auto& fr : m_perFrameCBs)
	{
		auto sceneDesc = CD3DX12_RESOURCE_DESC::Buffer(sceneSize);
		EVAL_HR(m_device->Get()->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE,
			&sceneDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&fr.sceneCB)),
			"Scene constant buffer creation failed");

		auto lightDesc = CD3DX12_RESOURCE_DESC::Buffer(lightSize);
		EVAL_HR(m_device->Get()->CreateCommittedResource(
			&uploadHeap, D3D12_HEAP_FLAG_NONE,
			&lightDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&fr.lightCB)),
			"Light constant buffer creation failed");

		EVAL_HR(fr.sceneCB->Map(0, nullptr, reinterpret_cast<void**>(&fr.sceneMapped)),
			"Scene CB map failed");
		EVAL_HR(fr.lightCB->Map(0, nullptr, reinterpret_cast<void**>(&fr.lightMapped)),
			"Light CB map failed");
	}
	return true;
}

bool DX12Renderer::CreatePipeline()
{
	// Root Signature mise à jour : 3 CBVs
	D3D12_ROOT_PARAMETER params[3] = {};

	// b0 : données par objet (world, worldViewProj)
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].Descriptor.ShaderRegister = 0;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	// b1 : données de scène (viewProj, cameraPos)
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[1].Descriptor.ShaderRegister = 1;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// b2 : lumières
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[2].Descriptor.ShaderRegister = 2;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = _countof(params);
	rsDesc.pParameters = params;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> serialized, error;
	EVAL_HR(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		&serialized, &error), "Root Signature serialization failed");
	EVAL_HR(m_device->Get()->CreateRootSignature(0,
		serialized->GetBufferPointer(), serialized->GetBufferSize(),
		IID_PPV_ARGS(&m_rootSignature)), "Root Signature creation failed");

	UINT compileFlags = 0;
#if defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	if (FAILED(D3DCompileFromFile(L"../RenderLib/Shaders/Basic.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &m_vertexShader, &errorBlob))) {
		if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		throw std::runtime_error("Basic.hlsl VS Failed");
	}
	if (FAILED(D3DCompileFromFile(L"../RenderLib/Shaders/Basic.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &m_pixelShader, &errorBlob))) {
		if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		throw std::runtime_error("Basic.hlsl PS Failed");
	}

	// Matériau par défaut (index 0)
	MaterialDesc defaultMat;
	defaultMat.name = "Default";
	CreateMaterial(defaultMat);

	return true;
}

Microsoft::WRL::ComPtr<ID3D12PipelineState> DX12Renderer::GetPipelineFor(const MaterialDesc& mat)
{
	D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
	psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode =
		mat.cullMode == CullMode::None  ? D3D12_CULL_MODE_NONE :
		mat.cullMode == CullMode::Front ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.FillMode =
		mat.fillMode == FillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	if (mat.blendMode == BlendMode::AlphaBlend)
	{
		auto& rt = psoDesc.BlendState.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	}
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc.Count = 1;

	return m_psoCache.GetOrCreate(mat.GetPipelineKey(), m_device->Get(), psoDesc);
}

uint32_t DX12Renderer::CreateMaterial(const MaterialDesc& desc)
{
	return m_materialRegistry.Add(desc, GetPipelineFor(desc));
}

void DX12Renderer::RefreshMaterial(uint32_t index)
{
	if (Material* mat = m_materialRegistry.Get(index))
		mat->pso = GetPipelineFor(mat->desc);
}

void DX12Renderer::LoadDefaultScene()
{
	// Enregistrement des meshes (upload différé au prochain Render)
	BoxGeometry cube(1.f);
	uint32_t cubeIdx = RegisterMesh("cube", cube);

	// Caméra principale
	Entity cam = m_scene.CreateEntity("Main Camera");
	auto& camT = *m_scene.GetComponent<TransformComponent>(cam.id);
	camT.SetPosition(0.f, 1.5f, -4.f);
	auto& camC = m_scene.AddComponent<CameraComponent>(cam.id);
	camC.isMain = true;
	camC.fovY = 60.f;

	// Lumière directionnelle
	Entity sun = m_scene.CreateEntity("Sun");
	auto& sunT = *m_scene.GetComponent<TransformComponent>(sun.id);
	sunT.rotation = { 45.f, 30.f, 0.f };
	auto& sunL = m_scene.AddComponent<LightComponent>(sun.id);
	sunL.type = LightType::Directional;
	sunL.intensity = 1.2f;

	// Cube de test
	Entity cubeEnt = m_scene.CreateEntity("Cube");
	auto& cubeM = m_scene.AddComponent<MeshComponent>(cubeEnt.id);
	cubeM.meshIndex = cubeIdx;

	// Second cube avec un matériau filaire teinté
	MaterialDesc wireDesc;
	wireDesc.name = "Wireframe";
	wireDesc.baseColor = { 0.3f, 1.f, 0.4f, 1.f };
	wireDesc.fillMode = FillMode::Wireframe;
	wireDesc.cullMode = CullMode::None;
	uint32_t wireMat = CreateMaterial(wireDesc);

	Entity wireEnt = m_scene.CreateEntity("Wire Cube");
	m_scene.GetComponent<TransformComponent>(wireEnt.id)->SetPosition(2.f, 0.f, 0.f);
	auto& wireM = m_scene.AddComponent<MeshComponent>(wireEnt.id);
	wireM.meshIndex = cubeIdx;
	wireM.materialIndex = wireMat;
}

uint32_t DX12Renderer::RegisterMesh(const std::string& name, const BufferGeometry& geometry)
{
	return m_meshRegistry.Register(name, geometry, m_device->Get());
}

void DX12Renderer::UpdateSceneConstants()
{
	// Cherche la caméra principale dans la scène
	XMMATRIX view = XMMatrixIdentity();
	XMMATRIX proj = XMMatrixIdentity();
	XMFLOAT3 camPos = { 0.f, 0.f, 0.f };

	for (auto& [id, cam] : m_scene.Cameras())
	{
		if (!cam.isMain) continue;
		auto* t = m_scene.GetComponent<TransformComponent>(id);
		if (!t) continue;

		XMMATRIX R = XMMatrixRotationRollPitchYaw(
			XMConvertToRadians(t->rotation.x),
			XMConvertToRadians(t->rotation.y),
			XMConvertToRadians(t->rotation.z));

		XMVECTOR eye = XMLoadFloat3(&t->position);
		XMVECTOR forward = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
		XMVECTOR up = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), R);
		view = XMMatrixLookToLH(eye, forward, up);
		camPos = t->position;

		float aspect = m_height > 0 ? (float)m_width / (float)m_height : 1.f;
		proj = XMMatrixPerspectiveFovLH(
			XMConvertToRadians(cam.fovY), aspect, cam.nearPlane, cam.farPlane);

		XMStoreFloat4x4(&cam.viewMatrix, view);
		XMStoreFloat4x4(&cam.projectionMatrix, proj);
		break;
	}

	XMStoreFloat4x4(&m_sceneConstants.viewProj,
		XMMatrixTranspose(view * proj));
	m_sceneConstants.cameraPos = camPos;
}

void DX12Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;

	m_fence->FlushGpu(m_commandQueue->Get());

	m_swapChain->Resize(m_device->Get(), width, height);
	m_depthStencil->Resize(m_device->Get(), width, height);

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	m_width = width;
	m_height = height;
	m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void DX12Renderer::Render()
{
	m_fence->WaitForFrame(m_frameIndex);
	m_meshRegistry.ReleaseCompletedUploads(m_fence->GetCompletedValue());

	// 1. Systèmes logiques
	m_transformSystem.Update(m_scene);
	m_lightSystem.Collect(m_scene);
	UpdateSceneConstants();

	// 2. Upload des données de frame
	auto& fr = m_perFrameCBs[m_frameIndex];
	memcpy(fr.sceneMapped, &m_sceneConstants, sizeof(SceneConstants));
	m_lightSystem.Upload(fr.lightMapped);

	// 3. Enregistrement des commandes
	m_commandObjects->Reset(m_frameIndex);
	auto* cmd = m_commandObjects->GetCommandList();

	// Copies des meshes en attente, finalisées par le Signal de fin de frame
	m_meshRegistry.RecordPendingUploads(cmd, m_fence->GetNextValue());

	// Transition → RENDER_TARGET
	auto barrierRT = CD3DX12_RESOURCE_BARRIER::Transition(
		m_swapChain->GetBackBuffer(m_frameIndex),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmd->ResourceBarrier(1, &barrierRT);

	auto rtvHandle = m_swapChain->GetRTVHandle(m_frameIndex);
	auto dsvHandle = m_depthStencil->GetDSVHandle();
	const float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.f };
	cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
	cmd->RSSetViewports(1, &m_viewport);
	cmd->RSSetScissorRects(1, &m_scissorRect);

	// 4. Pipeline + CBVs partagés
	cmd->SetGraphicsRootSignature(m_rootSignature.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
	cmd->SetDescriptorHeaps(_countof(heaps), heaps);

	// b1 et b2 liés une fois pour toute la frame
	cmd->SetGraphicsRootConstantBufferView(1, fr.sceneCB->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(2, fr.lightCB->GetGPUVirtualAddress());

	// 5. Soumission des draw calls via le RenderSystem (b0 mis à jour par objet)
	m_renderSystem.Submit(m_scene, cmd, m_frameIndex,
		m_meshRegistry, m_materialRegistry, m_sceneConstants);

	// 6. ImGui
	m_imgui->BeginFrame();
	DrawEditorUI();
	m_imgui->Render(cmd);

	// Transition → PRESENT
	auto barrierPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		m_swapChain->GetBackBuffer(m_frameIndex),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	cmd->ResourceBarrier(1, &barrierPresent);

	m_commandObjects->Close();
	ID3D12CommandList* lists[] = { cmd };
	m_commandQueue->Get()->ExecuteCommandLists(_countof(lists), lists);

	m_swapChain->Present(1);
	m_fence->Signal(m_commandQueue->Get(), m_frameIndex);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::Shutdown()
{
	if (m_fence && m_commandQueue)
		m_fence->FlushGpu(m_commandQueue->Get());

	if (m_imgui)
	{
		m_imgui->Shutdown();
		m_imgui.reset();
	}

	for (auto& fr : m_perFrameCBs)
	{
		if (fr.sceneCB && fr.sceneMapped)
		{
			fr.sceneCB->Unmap(0, nullptr);
			fr.sceneMapped = nullptr;
		}
		if (fr.lightCB && fr.lightMapped)
		{
			fr.lightCB->Unmap(0, nullptr);
			fr.lightMapped = nullptr;
		}
	}
}

void DX12Renderer::DrawEditorUI()
{
	if (!m_imgui) return;

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

	if (ImGui::Begin("GPU Stats"))
	{
		ImGui::Text("FPS       : %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame     : %.3f ms", 1000.f / ImGui::GetIO().Framerate);
		ImGui::Text("Frame idx : %u", m_frameIndex);
		ImGui::Separator();
		if (m_adapter)
		{
			DXGI_ADAPTER_DESC1 desc;
			if (SUCCEEDED(m_adapter->Get()->GetDesc1(&desc)))
			{
				ImGui::Text("Adapter   : %ls", desc.Description);
			}
		}
	}
	ImGui::End();

	DrawScenePanel();
}

void DX12Renderer::DrawScenePanel()
{
	if (ImGui::Begin("Scene"))
	{
		for (EntityID root : m_scene.GetRootEntities())
			DrawEntityNode(root);
	}
	ImGui::End();

	// Inspecteur de l'entité sélectionnée
	if (ImGui::Begin("Inspector") && m_selectedEntity != NULL_ENTITY)
		DrawInspector(m_selectedEntity);
	ImGui::End();
}

void DX12Renderer::DrawEntityNode(EntityID id)
{
	Entity* e = m_scene.GetEntity(id);
	if (!e || !e->active) return;

	auto* h = m_scene.GetComponent<HierarchyComponent>(id);
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
				DrawEntityNode(child);
		ImGui::TreePop();
	}
}

void DX12Renderer::DrawInspector(EntityID id)
{
	Entity* e = m_scene.GetEntity(id);
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
	if (auto* t = m_scene.GetComponent<TransformComponent>(id))
	{
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::DragFloat3("Position", &t->position.x, 0.01f)) t->dirty = true;
			if (ImGui::DragFloat3("Rotation", &t->rotation.x, 0.5f))  t->dirty = true;
			if (ImGui::DragFloat3("Scale", &t->scale.x, 0.01f)) t->dirty = true;
		}
	}

	// Light
	if (auto* l = m_scene.GetComponent<LightComponent>(id))
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