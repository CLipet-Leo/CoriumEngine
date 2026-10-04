#include "pch.h"
#include "DX12Renderer.h"
#include "DX12ImGui.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;

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

		// ImGui — slot 0 du heap SRV réservé pour la font
		auto cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
		auto gpuHandle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

		m_imgui = std::make_unique<DX12ImGui>();
		m_imgui->Init(hWnd, m_device->Get(), m_frameCount,
			DX12SwapChain::Format,
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

bool DX12Renderer::HandleWindowMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DX12ImGui::WndProcHandler(hWnd, msg, wParam, lParam) != 0;
}

uint32_t DX12Renderer::RegisterMesh(const std::string& name, const BufferGeometry& geometry)
{
	return m_meshRegistry.Register(name, geometry, m_device->Get());
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

void DX12Renderer::Render(Scene& scene)
{
	m_fence->WaitForFrame(m_frameIndex);
	m_meshRegistry.ReleaseCompletedUploads(m_fence->GetCompletedValue());

	// 1. Systèmes logiques
	m_transformSystem.Update(scene);
	m_lightSystem.Collect(scene);
	const float aspect = m_height > 0 ? (float)m_width / (float)m_height : 1.f;
	m_cameraSystem.Update(scene, aspect, m_sceneConstants);

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
	m_renderSystem.Submit(scene, cmd, m_frameIndex,
		m_meshRegistry, m_materialRegistry, m_sceneConstants);

	// 6. ImGui
	m_imgui->BeginFrame();
	m_editorUI.Draw(scene, m_frameIndex, m_adapterName.c_str());
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
