#pragma once

// RHI : interface entre le Renderer et les backends (Core/DX12, Core/Vulkan).
// Interne à la DLL, jamais exportée ; aucun type d'API graphique ici.
// Pensée pour les besoins actuels, pas plus : pas de compute, ni de multi-queue, ni de suivi d'états.

constexpr uint32_t kRhiFramesInFlight = 2;
constexpr uint32_t kRhiConstantAlign = 256;    // offset et taille d'un constant buffer
constexpr uint32_t kRhiTextureRowAlign = 256;  // pas des lignes d'une source de CopyBufferToTexture

constexpr uint64_t RhiAlign(uint64_t value, uint64_t align) { return (value + align - 1) & ~(align - 1); }

enum class RhiFormat : uint8_t
{
	Unknown,
	RGBA8_UNorm,
	BGRA8_UNorm,
	D32_Float,
	RG32_Float,
	RGB32_Float,
	RGBA32_Float,
};

// États explicites, traduits par chaque backend (layout Vulkan, état DX12).
// Undefined : ressource neuve, contenu indéfini. Present → X : contenu jeté (image de swapchain).
// ShaderRead : buffer = vertex/index/constantes, texture = échantillonnée par le pixel shader.
enum class RhiState : uint8_t { Undefined, CopyDest, ShaderRead, RenderTarget, Depth, Present };

enum class RhiMemory : uint8_t
{
	GpuOnly, // rempli par copie (CopyBuffer/CopyBufferToTexture)
	Upload,  // mappé en permanence, écrit par le CPU
};

enum RhiBufferUsage : uint32_t
{
	RhiBufferUsage_Vertex   = 1 << 0,
	RhiBufferUsage_Index    = 1 << 1,
	RhiBufferUsage_Constant = 1 << 2,
	RhiBufferUsage_CopySrc  = 1 << 3,
	RhiBufferUsage_CopyDst  = 1 << 4,
};

enum RhiTextureUsage : uint32_t
{
	RhiTextureUsage_Sampled      = 1 << 0,
	RhiTextureUsage_RenderTarget = 1 << 1,
	RhiTextureUsage_Depth        = 1 << 2,
	RhiTextureUsage_CopyDst      = 1 << 3,
};

struct RhiBufferDesc
{
	uint64_t  size = 0;
	RhiMemory memory = RhiMemory::GpuOnly;
	uint32_t  usage = 0; // RhiBufferUsage
};

struct RhiTextureDesc
{
	uint32_t  width = 1;
	uint32_t  height = 1;
	RhiFormat format = RhiFormat::RGBA8_UNorm;
	uint32_t  usage = 0; // RhiTextureUsage
};

class RhiBuffer
{
public:
	virtual ~RhiBuffer() = default;
	// RhiMemory::Upload uniquement (mapping persistant)
	virtual uint8_t* GetMapped() const = 0;
	virtual uint64_t GetSize() const = 0;
};

class RhiTexture
{
public:
	virtual ~RhiTexture() = default;
	virtual uint32_t  GetWidth() const = 0;
	virtual uint32_t  GetHeight() const = 0;
	virtual RhiFormat GetFormat() const = 0;
};

class RhiSampler
{
public:
	virtual ~RhiSampler() = default;
};

// --- Binding ---
// Un espace de registres HLSL (spaceN) = un descriptor set Vulkan (set N) = une fréquence
// de mise à jour. Mapping des registres en SPIR-V : cf. Conventions.md (décalages DXC).
enum class RhiBindingType : uint8_t
{
	ConstantBuffer,        // bN
	DynamicConstantBuffer, // bN, offset fourni à SetBindingSet (un seul par espace)
	Texture,               // tN
	Sampler,               // sN
};

struct RhiBindingSlot
{
	RhiBindingType type;
	uint32_t       reg; // N du registre HLSL dans l'espace
};

struct RhiPipelineLayoutDesc
{
	std::vector<std::vector<RhiBindingSlot>> spaces; // index = espace HLSL
};

class RhiPipelineLayout
{
public:
	virtual ~RhiPipelineLayout() = default;
};

// Une ressource par slot de l'espace, dans le même ordre
struct RhiBinding
{
	RhiBuffer*  buffer = nullptr;  // (Dynamic)ConstantBuffer
	uint64_t    offset = 0;
	uint64_t    size = 0;          // fenêtre vue par le shader
	RhiTexture* texture = nullptr; // Texture, à l'état ShaderRead au draw
	RhiSampler* sampler = nullptr; // Sampler
};

struct RhiBindingSetDesc
{
	RhiPipelineLayout*      layout = nullptr;
	uint32_t                space = 0;
	std::vector<RhiBinding> bindings;
};

class RhiBindingSet
{
public:
	virtual ~RhiBindingSet() = default;
};

// --- Pipeline ---
// Les attributs se suivent dans l'ordre de la structure d'entrée du vertex shader
// (location Vulkan = index de l'attribut)
struct RhiVertexAttribute
{
	const char* semantic;
	RhiFormat   format;
	uint32_t    offset;
};

struct RhiPipelineDesc
{
	RhiPipelineLayout*              layout = nullptr;
	const std::vector<uint8_t>*     vertexShader = nullptr;
	const std::vector<uint8_t>*     pixelShader = nullptr;
	std::vector<RhiVertexAttribute> attributes;
	uint32_t                        vertexStride = 0;
	CullMode                        cullMode = CullMode::Back;
	FillMode                        fillMode = FillMode::Solid;
	BlendMode                       blendMode = BlendMode::Opaque;
	RhiFormat                       colorFormat = RhiFormat::Unknown;
	RhiFormat                       depthFormat = RhiFormat::Unknown;
};

class RhiPipeline
{
public:
	virtual ~RhiPipeline() = default;
};

// --- Commandes ---
class RhiCommandList
{
public:
	virtual ~RhiCommandList() = default;

	// before : état réel de la ressource (pas de suivi automatique).
	// before == after : dépendance mémoire seule (ex. profondeur réécrite d'une frame à l'autre)
	virtual void Barrier(RhiTexture* texture, RhiState before, RhiState after) = 0;
	virtual void Barrier(RhiBuffer* buffer, RhiState before, RhiState after) = 0;

	virtual void CopyBuffer(RhiBuffer* dst, RhiBuffer* src, uint64_t size) = 0;
	// src : lignes au pas RhiAlign(largeur * 4, kRhiTextureRowAlign), format 32 bits
	virtual void CopyBufferToTexture(RhiTexture* dst, RhiBuffer* src) = 0;

	// Efface et lie les cibles (depth optionnelle, effacée à 1) ; viewport et scissor = taille de color
	virtual void BeginRenderPass(RhiTexture* color, RhiTexture* depth, const float clearColor[4]) = 0;
	virtual void EndRenderPass() = 0;

	virtual void SetPipeline(RhiPipeline* pipeline) = 0;
	// Lie le set à son espace ; reste lié tant que les pipelines partagent le même layout
	virtual void SetBindingSet(RhiBindingSet* set, uint32_t dynamicOffset = 0) = 0;
	virtual void SetVertexBuffer(RhiBuffer* buffer, uint64_t offset, uint32_t stride) = 0;
	virtual void SetIndexBuffer(RhiBuffer* buffer, uint64_t offset) = 0; // indices 32 bits
	virtual void Draw(uint32_t vertexCount) = 0;
	virtual void DrawIndexed(uint32_t indexCount) = 0;
};

// --- UI (ImGui) ---
// Le contexte est déjà rattaché par le Renderer ; le backend ajoute son implémentation ImGui.
class RhiUI
{
public:
	virtual ~RhiUI() = default;
	// Avant ImGui::NewFrame (côté exe)
	virtual void BeginFrame() = 0;
	// Termine la frame ImGui et la dessine dans la passe courante (cible = back buffer)
	virtual void Render(RhiCommandList* cmd) = 0;
	// Après EndFrame et ImGui::UpdatePlatformWindows, avant Present : fenêtres détachées
	// (multi-viewport), qui peuvent lire la cible de la frame soumise
	virtual void RenderPlatformWindows() = 0;
	// ImTextureID affichant texture (état ShaderRead) ; stable d'un appel à l'autre.
	// Le GPU ne doit plus lire l'ancienne texture (WaitIdle).
	virtual uint64_t SetSceneTexture(RhiTexture* texture) = 0;
};

// --- Device + swapchain de la fenêtre principale ---
class RhiDevice
{
public:
	virtual ~RhiDevice() = default;

	virtual const wchar_t* GetAdapterName() const = 0;
	virtual RhiFormat      GetBackBufferFormat() const = 0;
	// ".cso" (DXIL) ou ".spv" (SPIR-V)
	virtual const wchar_t* GetShaderExtension() const = 0;

	virtual std::unique_ptr<RhiBuffer>         CreateBuffer(const RhiBufferDesc& desc) = 0;
	virtual std::unique_ptr<RhiTexture>        CreateTexture(const RhiTextureDesc& desc) = 0;
	virtual std::unique_ptr<RhiSampler>        CreateSampler() = 0; // point, répétition
	virtual std::unique_ptr<RhiPipelineLayout> CreatePipelineLayout(const RhiPipelineLayoutDesc& desc) = 0;
	virtual std::unique_ptr<RhiBindingSet>     CreateBindingSet(const RhiBindingSetDesc& desc) = 0;
	virtual std::unique_ptr<RhiPipeline>       CreatePipeline(const RhiPipelineDesc& desc) = 0;
	virtual std::unique_ptr<RhiUI>             CreateUI() = 0;

	// Attend la frame en vol de ce slot, acquiert le back buffer et ouvre la command list.
	// nullptr : pas de frame possible (fenêtre minimisée).
	virtual RhiCommandList* BeginFrame() = 0;
	// Ferme et soumet la command list
	virtual void            EndFrame() = 0;
	virtual void            Present() = 0;
	// Slot de la frame courante, dans [0, kRhiFramesInFlight)
	virtual uint32_t        GetFrameIndex() const = 0;
	// Back buffer de la frame courante (état Present hors de la frame)
	virtual RhiTexture*     GetBackBuffer() = 0;

	// Le GPU ne doit plus rien utiliser de la swapchain (WaitIdle fait en interne)
	virtual void ResizeSwapChain(uint32_t width, uint32_t height) = 0;
	virtual void WaitIdle() = 0;
};

std::unique_ptr<RhiDevice> CreateDX12Device(HWND hWnd, uint32_t width, uint32_t height);
std::unique_ptr<RhiDevice> CreateVulkanDevice(HWND hWnd, uint32_t width, uint32_t height);
