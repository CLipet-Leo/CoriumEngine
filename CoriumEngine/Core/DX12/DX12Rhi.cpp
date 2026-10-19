#include "pch.h"
#include "../include/DirectXHeaders.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>

// Backend DX12 de la RHI. Seul fichier à voir les types D3D12/DXGI.

using Microsoft::WRL::ComPtr;

namespace
{

constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

DXGI_FORMAT ToDxgi(RhiFormat format)
{
	switch (format)
	{
	case RhiFormat::RGBA8_UNorm:  return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RhiFormat::BGRA8_UNorm:  return DXGI_FORMAT_B8G8R8A8_UNORM;
	case RhiFormat::D32_Float:    return DXGI_FORMAT_D32_FLOAT;
	case RhiFormat::RG32_Float:   return DXGI_FORMAT_R32G32_FLOAT;
	case RhiFormat::RGB32_Float:  return DXGI_FORMAT_R32G32B32_FLOAT;
	case RhiFormat::RGBA32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	default:                      return DXGI_FORMAT_UNKNOWN;
	}
}

D3D12_RESOURCE_STATES ToD3D(RhiState state, bool buffer)
{
	switch (state)
	{
	case RhiState::CopyDest:     return D3D12_RESOURCE_STATE_COPY_DEST;
	case RhiState::ShaderRead:   return buffer
		? D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER
		: D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case RhiState::RenderTarget: return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case RhiState::Depth:        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	default:                     return D3D12_RESOURCE_STATE_COMMON; // Undefined, Present
	}
}

// Heap de descripteurs, allocation first-fit de plages contiguës
class DescriptorHeap
{
public:
	void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.NumDescriptors = capacity;
		desc.Type = type;
		desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		EVAL_HR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)), "Descriptor heap creation failed");
		m_increment = device->GetDescriptorHandleIncrementSize(type);
		m_used.assign(capacity, false);
	}

	uint32_t Allocate(uint32_t count)
	{
		for (uint32_t start = 0; start + count <= m_used.size(); ++start)
		{
			uint32_t n = 0;
			while (n < count && !m_used[start + n])
				++n;
			if (n == count)
			{
				std::fill_n(m_used.begin() + start, count, true);
				return start;
			}
			start += n; // m_used[start + n] est pris
		}
		throw std::runtime_error("DX12 : heap de descripteurs plein");
	}

	void Free(uint32_t start, uint32_t count) { std::fill_n(m_used.begin() + start, count, false); }

	D3D12_CPU_DESCRIPTOR_HANDLE Cpu(uint32_t index) const
	{
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), (INT)index, m_increment);
	}
	D3D12_GPU_DESCRIPTOR_HANDLE Gpu(uint32_t index) const
	{
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->GetGPUDescriptorHandleForHeapStart(), (INT)index, m_increment);
	}
	uint32_t IndexOf(D3D12_CPU_DESCRIPTOR_HANDLE handle) const
	{
		return (uint32_t)((handle.ptr - m_heap->GetCPUDescriptorHandleForHeapStart().ptr) / m_increment);
	}
	ID3D12DescriptorHeap* Get() const { return m_heap.Get(); }

private:
	ComPtr<ID3D12DescriptorHeap> m_heap;
	UINT                         m_increment = 0;
	std::vector<bool>            m_used;
};

class DX12Buffer final : public RhiBuffer
{
public:
	uint8_t* GetMapped() const override { return mapped; }
	uint64_t GetSize() const override { return size; }

	ComPtr<ID3D12Resource> resource;
	uint8_t*               mapped = nullptr;
	uint64_t               size = 0;
};

class DX12Texture final : public RhiTexture
{
public:
	~DX12Texture() override
	{
		if (rtv != UINT32_MAX) rtvHeap->Free(rtv, 1);
		if (dsv != UINT32_MAX) dsvHeap->Free(dsv, 1);
	}
	uint32_t  GetWidth() const override { return desc.width; }
	uint32_t  GetHeight() const override { return desc.height; }
	RhiFormat GetFormat() const override { return desc.format; }

	ComPtr<ID3D12Resource> resource;
	RhiTextureDesc         desc;
	DescriptorHeap*        rtvHeap = nullptr;
	DescriptorHeap*        dsvHeap = nullptr;
	uint32_t               rtv = UINT32_MAX;
	uint32_t               dsv = UINT32_MAX;
};

class DX12Sampler final : public RhiSampler
{
public:
	D3D12_SAMPLER_DESC desc = {};
};

// Root signature : CBV en root descriptors (adresse + offset dynamique), textures et samplers
// d'un espace dans une descriptor table chacun
class DX12PipelineLayout final : public RhiPipelineLayout
{
public:
	struct Space
	{
		std::vector<RhiBindingSlot> slots;
		std::vector<uint32_t>       rootParams; // par slot, UINT32_MAX hors constant buffer
		uint32_t                    srvTable = UINT32_MAX;
		uint32_t                    samplerTable = UINT32_MAX;
		uint32_t                    srvCount = 0;
		uint32_t                    samplerCount = 0;
	};

	ComPtr<ID3D12RootSignature> rootSignature;
	std::vector<Space>          spaces;
};

class DX12BindingSet final : public RhiBindingSet
{
public:
	~DX12BindingSet() override
	{
		if (srvStart != UINT32_MAX) srvHeap->Free(srvStart, layout->spaces[space].srvCount);
		if (samplerStart != UINT32_MAX) samplerHeap->Free(samplerStart, layout->spaces[space].samplerCount);
	}

	const DX12PipelineLayout*              layout = nullptr;
	uint32_t                               space = 0;
	std::vector<D3D12_GPU_VIRTUAL_ADDRESS> addresses; // par slot (constant buffers)
	DescriptorHeap*                        srvHeap = nullptr;
	DescriptorHeap*                        samplerHeap = nullptr;
	uint32_t                               srvStart = UINT32_MAX;
	uint32_t                               samplerStart = UINT32_MAX;
};

class DX12Pipeline final : public RhiPipeline
{
public:
	ComPtr<ID3D12PipelineState> pso;
	ID3D12RootSignature*        rootSignature = nullptr;
};

class DX12CommandList final : public RhiCommandList
{
public:
	void Barrier(RhiTexture* texture, RhiState before, RhiState after) override
	{
		Transition(static_cast<DX12Texture*>(texture)->resource.Get(), ToD3D(before, false), ToD3D(after, false));
	}

	void Barrier(RhiBuffer* buffer, RhiState before, RhiState after) override
	{
		Transition(static_cast<DX12Buffer*>(buffer)->resource.Get(), ToD3D(before, true), ToD3D(after, true));
	}

	void CopyBuffer(RhiBuffer* dst, RhiBuffer* src, uint64_t size) override
	{
		list->CopyBufferRegion(static_cast<DX12Buffer*>(dst)->resource.Get(), 0,
			static_cast<DX12Buffer*>(src)->resource.Get(), 0, size);
	}

	void CopyBufferToTexture(RhiTexture* dst, RhiBuffer* src) override
	{
		auto* texture = static_cast<DX12Texture*>(dst);
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		footprint.Footprint.Format = ToDxgi(texture->desc.format);
		footprint.Footprint.Width = texture->desc.width;
		footprint.Footprint.Height = texture->desc.height;
		footprint.Footprint.Depth = 1;
		footprint.Footprint.RowPitch = (UINT)RhiAlign(texture->desc.width * 4, kRhiTextureRowAlign);

		const CD3DX12_TEXTURE_COPY_LOCATION dstLoc(texture->resource.Get(), 0);
		const CD3DX12_TEXTURE_COPY_LOCATION srcLoc(static_cast<DX12Buffer*>(src)->resource.Get(), footprint);
		list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
	}

	void BeginRenderPass(RhiTexture* color, RhiTexture* depth, const float clearColor[4]) override
	{
		auto* target = static_cast<DX12Texture*>(color);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtv = target->rtvHeap->Cpu(target->rtv);
		D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
		if (depth)
		{
			auto* depthTexture = static_cast<DX12Texture*>(depth);
			dsv = depthTexture->dsvHeap->Cpu(depthTexture->dsv);
		}

		list->OMSetRenderTargets(1, &rtv, FALSE, depth ? &dsv : nullptr);
		list->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		if (depth)
			list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

		const D3D12_VIEWPORT viewport = { 0.f, 0.f, (float)target->desc.width, (float)target->desc.height, 0.f, 1.f };
		const D3D12_RECT     scissor = { 0, 0, (LONG)target->desc.width, (LONG)target->desc.height };
		list->RSSetViewports(1, &viewport);
		list->RSSetScissorRects(1, &scissor);
		list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void EndRenderPass() override {}

	void SetPipeline(RhiPipeline* pipeline) override
	{
		auto* p = static_cast<DX12Pipeline*>(pipeline);
		BindRootSignature(p->rootSignature);
		list->SetPipelineState(p->pso.Get());
	}

	void SetBindingSet(RhiBindingSet* set, uint32_t dynamicOffset) override
	{
		auto* s = static_cast<DX12BindingSet*>(set);
		BindRootSignature(s->layout->rootSignature.Get());

		const DX12PipelineLayout::Space& space = s->layout->spaces[s->space];
		for (size_t i = 0; i < space.slots.size(); ++i)
		{
			if (space.rootParams[i] == UINT32_MAX)
				continue;
			const bool dynamic = space.slots[i].type == RhiBindingType::DynamicConstantBuffer;
			list->SetGraphicsRootConstantBufferView(space.rootParams[i], s->addresses[i] + (dynamic ? dynamicOffset : 0));
		}
		if (space.srvTable != UINT32_MAX)
			list->SetGraphicsRootDescriptorTable(space.srvTable, s->srvHeap->Gpu(s->srvStart));
		if (space.samplerTable != UINT32_MAX)
			list->SetGraphicsRootDescriptorTable(space.samplerTable, s->samplerHeap->Gpu(s->samplerStart));
	}

	void SetVertexBuffer(RhiBuffer* buffer, uint64_t offset, uint32_t stride) override
	{
		auto* b = static_cast<DX12Buffer*>(buffer);
		const D3D12_VERTEX_BUFFER_VIEW view = { b->resource->GetGPUVirtualAddress() + offset, (UINT)(b->size - offset), stride };
		list->IASetVertexBuffers(0, 1, &view);
	}

	void SetIndexBuffer(RhiBuffer* buffer, uint64_t offset) override
	{
		auto* b = static_cast<DX12Buffer*>(buffer);
		const D3D12_INDEX_BUFFER_VIEW view = { b->resource->GetGPUVirtualAddress() + offset, (UINT)(b->size - offset), DXGI_FORMAT_R32_UINT };
		list->IASetIndexBuffer(&view);
	}

	void Draw(uint32_t vertexCount) override { list->DrawInstanced(vertexCount, 1, 0, 0); }
	void DrawIndexed(uint32_t indexCount) override { list->DrawIndexedInstanced(indexCount, 1, 0, 0, 0); }

	ComPtr<ID3D12GraphicsCommandList> list;
	ID3D12RootSignature*              boundRootSignature = nullptr; // nullptr = à relier

private:
	void Transition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
	{
		// Même état (ex. profondeur d'une frame à l'autre) : rien à faire en DX12
		if (before == after)
			return;
		const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
		list->ResourceBarrier(1, &barrier);
	}

	void BindRootSignature(ID3D12RootSignature* rootSignature)
	{
		if (rootSignature == boundRootSignature)
			return;
		list->SetGraphicsRootSignature(rootSignature);
		boundRootSignature = rootSignature;
	}
};

class DX12RhiDevice final : public RhiDevice
{
public:
	DX12RhiDevice(HWND hWnd, uint32_t width, uint32_t height);
	~DX12RhiDevice() override;

	const wchar_t* GetAdapterName() const override { return m_adapterName.c_str(); }
	RhiFormat      GetBackBufferFormat() const override { return RhiFormat::RGBA8_UNorm; }
	const wchar_t* GetShaderExtension() const override { return L".cso"; }

	std::unique_ptr<RhiBuffer>         CreateBuffer(const RhiBufferDesc& desc) override;
	std::unique_ptr<RhiTexture>        CreateTexture(const RhiTextureDesc& desc) override;
	std::unique_ptr<RhiSampler>        CreateSampler() override;
	std::unique_ptr<RhiPipelineLayout> CreatePipelineLayout(const RhiPipelineLayoutDesc& desc) override;
	std::unique_ptr<RhiBindingSet>     CreateBindingSet(const RhiBindingSetDesc& desc) override;
	std::unique_ptr<RhiPipeline>       CreatePipeline(const RhiPipelineDesc& desc) override;
	std::unique_ptr<RhiUI>             CreateUI() override;

	RhiCommandList* BeginFrame() override;
	void            EndFrame() override;
	void            Present() override;
	uint32_t        GetFrameIndex() const override { return m_frameIndex; }
	RhiTexture*     GetBackBuffer() override { return m_backBuffers[m_swapChain->GetCurrentBackBufferIndex()].get(); }

	void ResizeSwapChain(uint32_t width, uint32_t height) override;
	void WaitIdle() override;

	// Attend que le GPU ait atteint value (0 = rien à attendre)
	void WaitFence(uint64_t value);
	// Attend la fin de la frame courante, soumise par EndFrame
	void WaitCurrentFrame() { WaitFence(m_frameFenceValues[m_frameIndex]); }

	ComPtr<ID3D12Device>       device;
	ComPtr<ID3D12CommandQueue> queue;
	DescriptorHeap             srvHeap;     // shader-visible : binding sets + ImGui
	DescriptorHeap             samplerHeap; // shader-visible
	DescriptorHeap             rtvHeap;
	DescriptorHeap             dsvHeap;

private:
	void CreateBackBuffers();

	ComPtr<IDXGIFactory4>   m_factory;
	ComPtr<IDXGIAdapter1>   m_adapter;
	ComPtr<IDXGISwapChain3> m_swapChain;
	std::vector<std::unique_ptr<DX12Texture>> m_backBuffers;

	ComPtr<ID3D12Fence> m_fence;
	HANDLE              m_fenceEvent = nullptr;
	uint64_t            m_nextFenceValue = 1;
	std::array<uint64_t, kRhiFramesInFlight>                       m_frameFenceValues = {};
	std::array<ComPtr<ID3D12CommandAllocator>, kRhiFramesInFlight> m_allocators;
	DX12CommandList     m_commandList;
	uint32_t            m_frameIndex = 0;

	std::wstring m_adapterName;
};

DX12RhiDevice::DX12RhiDevice(HWND hWnd, uint32_t width, uint32_t height)
{
	UINT factoryFlags = 0;
#if defined(_DEBUG)
	// Couche de debug + GPU-Based Validation
	ComPtr<ID3D12Debug1> debug;
	EVAL_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "D3D12 Debug Interface creation failed");
	debug->EnableDebugLayer();
	debug->SetEnableGPUBasedValidation(TRUE);
	debug->SetEnableSynchronizedCommandQueueValidation(TRUE);
	factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	EVAL_HR(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)), "DXGI Factory creation failed");

	// Premier adaptateur matériel compatible D3D12, sinon WARP
	for (UINT i = 0; m_factory->EnumAdapters1(i, &m_adapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		DXGI_ADAPTER_DESC1 desc;
		m_adapter->GetDesc1(&desc);
		if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			&& SUCCEEDED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			break;
		m_adapter.Reset();
	}
	if (!m_adapter)
		EVAL_HR(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter)), "DXGI WARP adapter enumeration failed");

	DXGI_ADAPTER_DESC1 adapterDesc;
	if (SUCCEEDED(m_adapter->GetDesc1(&adapterDesc)))
		m_adapterName = adapterDesc.Description;

	EVAL_HR(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "D3D12 Device creation failed");

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> infoQueue;
	if (SUCCEEDED(device.As(&infoQueue)))
	{
		// Arrêt sur corruption et erreur
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

		// Messages verbeux mais inoffensifs
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
			D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
			D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
		};
		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		infoQueue->PushStorageFilter(&filter);
	}
#endif

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	EVAL_HR(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "Command Queue creation failed");

	EVAL_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "Fence creation failed");
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	CE_ASSERT(m_fenceEvent != nullptr);

	srvHeap.Init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);
	samplerHeap.Init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 64, true);
	rtvHeap.Init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 64, false);
	dsvHeap.Init(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, false);

	for (auto& allocator : m_allocators)
		EVAL_HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
			"Command Allocator creation failed");
	EVAL_HR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].Get(), nullptr,
		IID_PPV_ARGS(&m_commandList.list)), "Command List creation failed");
	EVAL_HR(m_commandList.list->Close(), "Command List initial close failed");

	DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
	swapDesc.BufferCount = kRhiFramesInFlight;
	swapDesc.Width = width;
	swapDesc.Height = height;
	swapDesc.Format = kBackBufferFormat;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain1;
	EVAL_HR(m_factory->CreateSwapChainForHwnd(queue.Get(), hWnd, &swapDesc, nullptr, nullptr, &swapChain1),
		"SwapChain creation failed");
	EVAL_HR(m_factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation failed");
	EVAL_HR(swapChain1.As(&m_swapChain), "SwapChain3 query failed");
	CreateBackBuffers();
}

DX12RhiDevice::~DX12RhiDevice()
{
	WaitIdle();
	m_backBuffers.clear();
	if (m_fenceEvent)
		CloseHandle(m_fenceEvent);
}

void DX12RhiDevice::CreateBackBuffers()
{
	DXGI_SWAP_CHAIN_DESC1 desc;
	EVAL_HR(m_swapChain->GetDesc1(&desc), "SwapChain GetDesc1 failed");

	m_backBuffers.resize(kRhiFramesInFlight);
	for (uint32_t i = 0; i < kRhiFramesInFlight; ++i)
	{
		auto texture = std::make_unique<DX12Texture>();
		EVAL_HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&texture->resource)), "SwapChain GetBuffer failed");
		texture->desc = { desc.Width, desc.Height, GetBackBufferFormat(), RhiTextureUsage_RenderTarget };
		texture->rtvHeap = &rtvHeap;
		texture->rtv = rtvHeap.Allocate(1);
		device->CreateRenderTargetView(texture->resource.Get(), nullptr, rtvHeap.Cpu(texture->rtv));
		m_backBuffers[i] = std::move(texture);
	}
}

std::unique_ptr<RhiBuffer> DX12RhiDevice::CreateBuffer(const RhiBufferDesc& desc)
{
	auto buffer = std::make_unique<DX12Buffer>();
	buffer->size = desc.size;

	const bool upload = desc.memory == RhiMemory::Upload;
	const CD3DX12_HEAP_PROPERTIES heap(upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(desc.size);
	EVAL_HR(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		upload ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON, nullptr,
		IID_PPV_ARGS(&buffer->resource)), "Buffer creation failed");

	if (upload)
	{
		const CD3DX12_RANGE noRead(0, 0);
		EVAL_HR(buffer->resource->Map(0, &noRead, reinterpret_cast<void**>(&buffer->mapped)), "Buffer map failed");
	}
	return buffer;
}

std::unique_ptr<RhiTexture> DX12RhiDevice::CreateTexture(const RhiTextureDesc& desc)
{
	auto texture = std::make_unique<DX12Texture>();
	texture->desc = desc;
	texture->rtvHeap = &rtvHeap;
	texture->dsvHeap = &dsvHeap;

	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	if (desc.usage & RhiTextureUsage_RenderTarget)
		flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (desc.usage & RhiTextureUsage_Depth)
		flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if ((desc.usage & RhiTextureUsage_Depth) && !(desc.usage & RhiTextureUsage_Sampled))
		flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

	D3D12_CLEAR_VALUE depthClear = {};
	depthClear.Format = ToDxgi(desc.format);
	depthClear.DepthStencil.Depth = 1.f;

	const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
	const auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(ToDxgi(desc.format), desc.width, desc.height, 1, 1, 1, 0, flags);
	EVAL_HR(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_COMMON, (desc.usage & RhiTextureUsage_Depth) ? &depthClear : nullptr,
		IID_PPV_ARGS(&texture->resource)), "Texture creation failed");

	if (desc.usage & RhiTextureUsage_RenderTarget)
	{
		texture->rtv = rtvHeap.Allocate(1);
		device->CreateRenderTargetView(texture->resource.Get(), nullptr, rtvHeap.Cpu(texture->rtv));
	}
	if (desc.usage & RhiTextureUsage_Depth)
	{
		texture->dsv = dsvHeap.Allocate(1);
		device->CreateDepthStencilView(texture->resource.Get(), nullptr, dsvHeap.Cpu(texture->dsv));
	}
	return texture;
}

std::unique_ptr<RhiSampler> DX12RhiDevice::CreateSampler()
{
	auto sampler = std::make_unique<DX12Sampler>();
	sampler->desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler->desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler->desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler->desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler->desc.MaxAnisotropy = 1;
	sampler->desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler->desc.MaxLOD = D3D12_FLOAT32_MAX;
	return sampler;
}

std::unique_ptr<RhiPipelineLayout> DX12RhiDevice::CreatePipelineLayout(const RhiPipelineLayoutDesc& desc)
{
	auto layout = std::make_unique<DX12PipelineLayout>();
	std::vector<D3D12_ROOT_PARAMETER> params;
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> ranges; // gardées vivantes jusqu'à la sérialisation

	auto addTable = [&](std::vector<D3D12_DESCRIPTOR_RANGE>&& tableRanges) -> uint32_t
	{
		ranges.push_back(std::move(tableRanges));
		D3D12_ROOT_PARAMETER param = {};
		param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		param.DescriptorTable = { (UINT)ranges.back().size(), ranges.back().data() };
		param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		params.push_back(param);
		return (uint32_t)params.size() - 1;
	};

	for (uint32_t s = 0; s < desc.spaces.size(); ++s)
	{
		DX12PipelineLayout::Space space;
		space.slots = desc.spaces[s];
		space.rootParams.assign(space.slots.size(), UINT32_MAX);

		std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges, samplerRanges;
		for (size_t i = 0; i < space.slots.size(); ++i)
		{
			const RhiBindingSlot& slot = space.slots[i];
			switch (slot.type)
			{
			case RhiBindingType::ConstantBuffer:
			case RhiBindingType::DynamicConstantBuffer:
			{
				D3D12_ROOT_PARAMETER param = {};
				param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				param.Descriptor = { slot.reg, s };
				param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				space.rootParams[i] = (uint32_t)params.size();
				params.push_back(param);
				break;
			}
			case RhiBindingType::Texture:
				srvRanges.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, slot.reg, s, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND });
				break;
			case RhiBindingType::Sampler:
				samplerRanges.push_back({ D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, slot.reg, s, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND });
				break;
			}
		}
		space.srvCount = (uint32_t)srvRanges.size();
		space.samplerCount = (uint32_t)samplerRanges.size();
		if (!srvRanges.empty())
			space.srvTable = addTable(std::move(srvRanges));
		if (!samplerRanges.empty())
			space.samplerTable = addTable(std::move(samplerRanges));
		layout->spaces.push_back(std::move(space));
	}

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = (UINT)params.size();
	rsDesc.pParameters = params.data();
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> serialized, error;
	EVAL_HR(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error),
		"Root Signature serialization failed");
	EVAL_HR(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
		IID_PPV_ARGS(&layout->rootSignature)), "Root Signature creation failed");
	return layout;
}

std::unique_ptr<RhiBindingSet> DX12RhiDevice::CreateBindingSet(const RhiBindingSetDesc& desc)
{
	auto set = std::make_unique<DX12BindingSet>();
	set->layout = static_cast<const DX12PipelineLayout*>(desc.layout);
	set->space = desc.space;
	set->srvHeap = &srvHeap;
	set->samplerHeap = &samplerHeap;

	const DX12PipelineLayout::Space& space = set->layout->spaces[desc.space];
	CE_ASSERT(desc.bindings.size() == space.slots.size());
	set->addresses.assign(space.slots.size(), 0);
	if (space.srvCount)
		set->srvStart = srvHeap.Allocate(space.srvCount);
	if (space.samplerCount)
		set->samplerStart = samplerHeap.Allocate(space.samplerCount);

	uint32_t srvIndex = 0, samplerIndex = 0;
	for (size_t i = 0; i < space.slots.size(); ++i)
	{
		const RhiBinding& binding = desc.bindings[i];
		switch (space.slots[i].type)
		{
		case RhiBindingType::ConstantBuffer:
		case RhiBindingType::DynamicConstantBuffer:
			set->addresses[i] = static_cast<DX12Buffer*>(binding.buffer)->resource->GetGPUVirtualAddress() + binding.offset;
			break;
		case RhiBindingType::Texture:
			device->CreateShaderResourceView(static_cast<DX12Texture*>(binding.texture)->resource.Get(), nullptr,
				srvHeap.Cpu(set->srvStart + srvIndex++));
			break;
		case RhiBindingType::Sampler:
			device->CreateSampler(&static_cast<DX12Sampler*>(binding.sampler)->desc,
				samplerHeap.Cpu(set->samplerStart + samplerIndex++));
			break;
		}
	}
	return set;
}

std::unique_ptr<RhiPipeline> DX12RhiDevice::CreatePipeline(const RhiPipelineDesc& desc)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
	for (const RhiVertexAttribute& attribute : desc.attributes)
		inputLayout.push_back({ attribute.semantic, 0, ToDxgi(attribute.format), 0, attribute.offset,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });

	auto* layout = static_cast<DX12PipelineLayout*>(desc.layout);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
	psoDesc.pRootSignature = layout->rootSignature.Get();
	psoDesc.VS = { desc.vertexShader->data(), desc.vertexShader->size() };
	psoDesc.PS = { desc.pixelShader->data(), desc.pixelShader->size() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // FrontCounterClockwise = FALSE
	psoDesc.RasterizerState.CullMode =
		desc.cullMode == CullMode::None  ? D3D12_CULL_MODE_NONE :
		desc.cullMode == CullMode::Front ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.FillMode =
		desc.fillMode == FillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	if (desc.blendMode == BlendMode::AlphaBlend)
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
	psoDesc.RTVFormats[0] = ToDxgi(desc.colorFormat);
	psoDesc.DSVFormat = ToDxgi(desc.depthFormat);
	psoDesc.SampleDesc.Count = 1;

	auto pipeline = std::make_unique<DX12Pipeline>();
	pipeline->rootSignature = layout->rootSignature.Get();
	EVAL_HR(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pso)), "Graphics PSO creation failed");
	return pipeline;
}

RhiCommandList* DX12RhiDevice::BeginFrame()
{
	WaitFence(m_frameFenceValues[m_frameIndex]);
	EVAL_HR(m_allocators[m_frameIndex]->Reset(), "Command Allocator reset failed");
	EVAL_HR(m_commandList.list->Reset(m_allocators[m_frameIndex].Get(), nullptr), "Command List reset failed");
	m_commandList.boundRootSignature = nullptr;

	ID3D12DescriptorHeap* heaps[] = { srvHeap.Get(), samplerHeap.Get() };
	m_commandList.list->SetDescriptorHeaps(_countof(heaps), heaps);
	return &m_commandList;
}

void DX12RhiDevice::EndFrame()
{
	EVAL_HR(m_commandList.list->Close(), "Command List close failed");
	ID3D12CommandList* lists[] = { m_commandList.list.Get() };
	queue->ExecuteCommandLists(_countof(lists), lists);

	EVAL_HR(queue->Signal(m_fence.Get(), m_nextFenceValue), "Fence signal failed");
	m_frameFenceValues[m_frameIndex] = m_nextFenceValue++;
}

void DX12RhiDevice::Present()
{
	EVAL_HR(m_swapChain->Present(1, 0), "SwapChain Present failed");
	m_frameIndex = (m_frameIndex + 1) % kRhiFramesInFlight;
}

void DX12RhiDevice::ResizeSwapChain(uint32_t width, uint32_t height)
{
	WaitIdle();
	m_backBuffers.clear();
	EVAL_HR(m_swapChain->ResizeBuffers(kRhiFramesInFlight, width, height, kBackBufferFormat, 0),
		"SwapChain ResizeBuffers failed");
	CreateBackBuffers();
}

void DX12RhiDevice::WaitIdle()
{
	EVAL_HR(queue->Signal(m_fence.Get(), m_nextFenceValue), "Fence flush signal failed");
	WaitFence(m_nextFenceValue++);
}

void DX12RhiDevice::WaitFence(uint64_t value)
{
	if (value == 0 || m_fence->GetCompletedValue() >= value)
		return;
	EVAL_HR(m_fence->SetEventOnCompletion(value, m_fenceEvent), "Fence SetEventOnCompletion failed");
	WaitForSingleObject(m_fenceEvent, INFINITE);
}

// Backend DX12 d'ImGui ; ses textures prennent leurs descripteurs dans srvHeap
class DX12UI final : public RhiUI
{
public:
	explicit DX12UI(DX12RhiDevice& device)
		: m_device(device)
	{
		ImGui_ImplDX12_InitInfo info;
		info.Device = device.device.Get();
		info.CommandQueue = device.queue.Get();
		info.NumFramesInFlight = kRhiFramesInFlight;
		info.RTVFormat = kBackBufferFormat;
		info.DSVFormat = DXGI_FORMAT_UNKNOWN;
		info.UserData = &device.srvHeap;
		info.SrvDescriptorHeap = device.srvHeap.Get();
		info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
		{
			auto* heap = static_cast<DescriptorHeap*>(info->UserData);
			const uint32_t index = heap->Allocate(1);
			*cpu = heap->Cpu(index);
			*gpu = heap->Gpu(index);
		};
		info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			auto* heap = static_cast<DescriptorHeap*>(info->UserData);
			heap->Free(heap->IndexOf(cpu), 1);
		};
		ImGui_ImplDX12_Init(&info);
	}

	~DX12UI() override
	{
		ImGui_ImplDX12_Shutdown();
		if (m_sceneSrv != UINT32_MAX)
			m_device.srvHeap.Free(m_sceneSrv, 1);
	}

	void BeginFrame() override { ImGui_ImplDX12_NewFrame(); }

	void Render(RhiCommandList* cmd) override
	{
		auto* list = static_cast<DX12CommandList*>(cmd);
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list->list.Get());
		list->boundRootSignature = nullptr; // ImGui a lié sa propre root signature
	}

	void RenderPlatformWindows() override
	{
		// Le backend rend chaque fenêtre détachée sur sa propre queue, inaccessible pour un Wait GPU.
		// ponytail: attente CPU de la frame, seulement quand une fenêtre est détachée
		m_device.WaitCurrentFrame();
		ImGui::RenderPlatformWindowsDefault();
	}

	uint64_t SetSceneTexture(RhiTexture* texture) override
	{
		if (m_sceneSrv == UINT32_MAX)
			m_sceneSrv = m_device.srvHeap.Allocate(1);
		m_device.device->CreateShaderResourceView(static_cast<DX12Texture*>(texture)->resource.Get(), nullptr,
			m_device.srvHeap.Cpu(m_sceneSrv));
		return m_device.srvHeap.Gpu(m_sceneSrv).ptr;
	}

private:
	DX12RhiDevice& m_device;
	uint32_t       m_sceneSrv = UINT32_MAX;
};

std::unique_ptr<RhiUI> DX12RhiDevice::CreateUI()
{
	return std::make_unique<DX12UI>(*this);
}

} // namespace

std::unique_ptr<RhiDevice> CreateDX12Device(HWND hWnd, uint32_t width, uint32_t height)
{
	return std::make_unique<DX12RhiDevice>(hWnd, width, height);
}
