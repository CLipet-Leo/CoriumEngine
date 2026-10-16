#include "pch.h"
#include "DX12Renderer.h"
#include "DX12ImGui.h"

#pragma comment(lib, "d3dcompiler.lib")

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
		MathSelfCheck();
		m_debug = std::make_unique<DX12Debug>();
#endif
		m_factory = std::make_unique<DXGIFactory>();
		m_adapter = std::make_unique<DXGIAdapter>(m_factory->GetBestAdapter());
		m_device = std::make_unique<DX12Device>(*m_adapter);

		DXGI_ADAPTER_DESC1 adapterDesc;
		if (SUCCEEDED(m_adapter->Get()->GetDesc1(&adapterDesc)))
			m_adapterName = adapterDesc.Description;

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

		m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

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
	// 64 slots : slot 0 = police ImGui, slot 1 = scène offscreen, reste = textures futures
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = 64;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	EVAL_HR(m_device->Get()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap)),
		"SRV Heap creation failed");
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
	// Shaders\ est copié à côté de l'exécutable au build
	const std::wstring shaderPath = GetExecutableDir() + L"Shaders\\Basic.hlsl";

	HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &m_vertexShader, &errorBlob);
	if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	EVAL_HR(hr, "Basic.hlsl VS compilation failed");

	hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &m_pixelShader, &errorBlob);
	if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	EVAL_HR(hr, "Basic.hlsl PS compilation failed");

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
	psoDesc.RTVFormats[0] = DX12SwapChain::Format;
	psoDesc.DSVFormat = DX12DepthStencil::Format;
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

RendererStats DX12Renderer::GetStats() const
{
	return { m_frameIndex, m_adapterName.c_str() };
}

void DX12Renderer::InitUI(ImGuiContext* context, void* (*allocFunc)(size_t, void*),
	void (*freeFunc)(void*, void*), void* allocUserData)
{
	const UINT srvSize = m_device->Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const CD3DX12_CPU_DESCRIPTOR_HANDLE cpuStart(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
	const CD3DX12_GPU_DESCRIPTOR_HANDLE gpuStart(m_srvHeap->GetGPUDescriptorHandleForHeapStart());

	// Slot 0 : police ImGui
	m_imgui = std::make_unique<DX12ImGui>();
	m_imgui->Init(context, allocFunc, freeFunc, allocUserData,
		m_device->Get(), m_frameCount, DX12SwapChain::Format,
		m_srvHeap.Get(), cpuStart, gpuStart);

	// Slot 1 : scène offscreen ; taille de la fenêtre en attendant celle du panneau
	m_sceneTarget = std::make_unique<DX12RenderTarget>(m_device->Get(), CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuStart, 1, srvSize));
	m_sceneTarget->Resize(m_device->Get(), m_width, m_height);
	m_requestedSceneWidth = m_width;
	m_requestedSceneHeight = m_height;
}

void DX12Renderer::ShutdownUI()
{
	if (!m_imgui)
		return;

	// Le GPU peut encore lire les buffers ImGui et la cible offscreen
	m_fence->FlushGpu(m_commandQueue->Get());
	m_imgui.reset();
	m_sceneTarget.reset();
	m_depthStencil->Resize(m_device->Get(), m_width, m_height);
}

void DX12Renderer::BeginUIFrame()
{
	if (m_imgui)
		m_imgui->BeginFrame();
}

void DX12Renderer::SetSceneSize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;
	m_requestedSceneWidth = width;
	m_requestedSceneHeight = height;
}

uint64_t DX12Renderer::GetSceneTexture() const
{
	if (!m_sceneTarget)
		return 0;
	const UINT srvSize = m_device->Get()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart(), 1, srvSize).ptr;
}

void DX12Renderer::ApplySceneSize()
{
	if (m_requestedSceneWidth == m_sceneTarget->GetWidth() && m_requestedSceneHeight == m_sceneTarget->GetHeight())
		return;

	// Les frames en vol lisent encore la texture et son slot SRV
	m_fence->FlushGpu(m_commandQueue->Get());
	m_sceneTarget->Resize(m_device->Get(), m_requestedSceneWidth, m_requestedSceneHeight);
	m_depthStencil->Resize(m_device->Get(), m_requestedSceneWidth, m_requestedSceneHeight);
}

uint32_t DX12Renderer::RegisterMesh(const MeshDesc& desc)
{
	return m_meshRegistry.Register(desc, m_device->Get());
}

const MeshDesc* DX12Renderer::GetMeshDesc(uint32_t index) const
{
	const GpuMesh* mesh = m_meshRegistry.Get(index);
	return mesh ? &mesh->desc : nullptr;
}

const MaterialDesc* DX12Renderer::GetMaterialDesc(uint32_t index) const
{
	const Material* material = m_materialRegistry.Get(index);
	return material ? &material->desc : nullptr;
}

void DX12Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (width == 0 || height == 0)
		return;

	m_fence->FlushGpu(m_commandQueue->Get());

	m_swapChain->Resize(m_device->Get(), width, height);
	// Avec l'UI, la profondeur suit la cible offscreen (ApplySceneSize)
	if (!m_sceneTarget)
		m_depthStencil->Resize(m_device->Get(), width, height);

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	m_width = width;
	m_height = height;
}

void DX12Renderer::DrawScene(Scene& scene, ID3D12GraphicsCommandList* cmd, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
	uint32_t width, uint32_t height, const Mat4& viewProj)
{
	auto dsvHandle = m_depthStencil->GetDSVHandle();
	const float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.f };
	cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
	cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle);

	const D3D12_VIEWPORT viewport = { 0.f, 0.f, (float)width, (float)height, 0.f, 1.f };
	const D3D12_RECT     scissor = { 0, 0, (LONG)width, (LONG)height };
	cmd->RSSetViewports(1, &viewport);
	cmd->RSSetScissorRects(1, &scissor);

	// Pipeline + CBVs partagés
	auto& fr = m_perFrameCBs[m_frameIndex];
	cmd->SetGraphicsRootSignature(m_rootSignature.Get());
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// b1 et b2 liés une fois pour toute la frame
	cmd->SetGraphicsRootConstantBufferView(1, fr.sceneCB->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(2, fr.lightCB->GetGPUVirtualAddress());

	// Draw calls via le RenderSystem (b0 mis à jour par objet)
	m_renderSystem.Submit(scene, cmd, m_frameIndex,
		m_meshRegistry, m_materialRegistry, viewProj);
}

void DX12Renderer::Render(Scene& scene, const CameraView* view)
{
	m_fence->WaitForFrame(m_frameIndex);
	m_meshRegistry.ReleaseCompletedUploads(m_fence->GetCompletedValue());

	// Sans UI la scène va dans la swapchain, sinon dans la cible offscreen (panneau Viewport)
	if (m_sceneTarget)
		ApplySceneSize();
	const uint32_t sceneWidth = m_sceneTarget ? m_sceneTarget->GetWidth() : m_width;
	const uint32_t sceneHeight = m_sceneTarget ? m_sceneTarget->GetHeight() : m_height;

	// 1. Collecte caméra / lumières depuis la scène
	m_lightSystem.Collect(scene);
	const float aspect = sceneHeight > 0 ? (float)sceneWidth / (float)sceneHeight : 1.f;
	const Mat4 viewProj = m_cameraSystem.Update(scene, aspect, m_sceneConstants, view);

	// 2. Upload des données de frame
	auto& fr = m_perFrameCBs[m_frameIndex];
	memcpy(fr.sceneMapped, &m_sceneConstants, sizeof(SceneConstants));
	m_lightSystem.Upload(fr.lightMapped);

	// 3. Enregistrement des commandes
	m_commandObjects->Reset(m_frameIndex);
	auto* cmd = m_commandObjects->GetCommandList();

	// Copies des meshes en attente, finalisées par le Signal de fin de frame
	m_meshRegistry.RecordPendingUploads(cmd, m_fence->GetNextValue());

	ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get() };
	cmd->SetDescriptorHeaps(_countof(heaps), heaps);

	ID3D12Resource* backBuffer = m_swapChain->GetBackBuffer(m_frameIndex);
	const auto rtvHandle = m_swapChain->GetRTVHandle(m_frameIndex);

	if (m_sceneTarget)
	{
		// 4a. Scène → offscreen, puis lue par ImGui (Image du panneau Viewport)
		auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(m_sceneTarget->Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->ResourceBarrier(1, &toRT);
		DrawScene(scene, cmd, m_sceneTarget->GetRTVHandle(), sceneWidth, sceneHeight, viewProj);

		D3D12_RESOURCE_BARRIER barriers[] = {
			CD3DX12_RESOURCE_BARRIER::Transition(m_sceneTarget->Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
			CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
				D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET) };
		cmd->ResourceBarrier(_countof(barriers), barriers);

		// 5a. ImGui → swapchain (fond masqué par le dockspace)
		const float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
		cmd->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
		m_imgui->Render(cmd);
	}
	else
	{
		// 4b. Scène directement dans la swapchain
		auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer,
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		cmd->ResourceBarrier(1, &toRT);
		DrawScene(scene, cmd, rtvHandle, m_width, m_height, viewProj);
	}

	// Transition → PRESENT
	auto barrierPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		m_swapChain->GetBackBuffer(m_frameIndex),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	cmd->ResourceBarrier(1, &barrierPresent);

	m_commandObjects->Close();
	ID3D12CommandList* lists[] = { cmd };
	m_commandQueue->Get()->ExecuteCommandLists(_countof(lists), lists);

	if (m_imgui)
		m_imgui->RenderPlatformWindows();

	m_swapChain->Present(1);
	m_fence->Signal(m_commandQueue->Get(), m_frameIndex);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::Shutdown()
{
	if (m_fence && m_commandQueue)
		m_fence->FlushGpu(m_commandQueue->Get());

	// Sécurité si l'exe n'a pas appelé ShutdownUI : son contexte ImGui doit encore exister
	m_imgui.reset();
	m_sceneTarget.reset();

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
