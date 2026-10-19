#include "pch.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#pragma warning(push, 0)
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>
#pragma warning(pop)
#include <imgui.h>
#include <imgui_impl_vulkan.h>

// Backend Vulkan 1.3 de la RHI (dynamic rendering, synchronization2). Seul fichier à voir les types Vk.
// vulkan-1.dll est lié en delay-load : la DLL se charge sans pilote Vulkan tant que ce backend
// n'est pas choisi. Pas de volk : imgui.lib (vcpkg) appelle les prototypes Vulkan directement.

#define VK_CHECK(expr, msg) \
	do { VkResult _r = (expr); \
		if (_r != VK_SUCCESS) throw std::runtime_error(std::string("[Vulkan Error] ") + msg + " (VkResult " + std::to_string(_r) + ")"); \
	} while (0)

namespace
{

// Décalages des registres t et s dans un set : doivent correspondre aux options DXC
// -fvk-t-shift / -fvk-s-shift du .vcxproj (cf. Conventions.md)
constexpr uint32_t kTextureBindingShift = 10;
constexpr uint32_t kSamplerBindingShift = 20;

VkFormat ToVk(RhiFormat format)
{
	switch (format)
	{
	case RhiFormat::RGBA8_UNorm:  return VK_FORMAT_R8G8B8A8_UNORM;
	case RhiFormat::BGRA8_UNorm:  return VK_FORMAT_B8G8R8A8_UNORM;
	case RhiFormat::D32_Float:    return VK_FORMAT_D32_SFLOAT;
	case RhiFormat::RG32_Float:   return VK_FORMAT_R32G32_SFLOAT;
	case RhiFormat::RGB32_Float:  return VK_FORMAT_R32G32B32_SFLOAT;
	case RhiFormat::RGBA32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
	default:                      return VK_FORMAT_UNDEFINED;
	}
}

struct VkStateInfo
{
	VkPipelineStageFlags2 stage;
	VkAccessFlags2        access;
	VkImageLayout         layout;
};

// src : état avant la barrière (sinon après)
VkStateInfo ToVk(RhiState state, bool buffer, bool src)
{
	switch (state)
	{
	case RhiState::CopyDest:
		return { VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL };
	case RhiState::ShaderRead:
		if (buffer)
			return { VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
				VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED };
		return { VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
	case RhiState::RenderTarget:
		return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	case RhiState::Depth:
		return { VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL };
	case RhiState::Present:
		// Depuis Present : contenu jeté, après l'acquisition (sémaphore attendu sur COLOR_ATTACHMENT_OUTPUT)
		if (src)
			return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED };
		return { VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
	default: // Undefined
		return { VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE, VK_IMAGE_LAYOUT_UNDEFINED };
	}
}

// Hazard connu du backend Vulkan d'ImGui 1.91.9 (vcpkg, non corrigeable ici) : la barrière de ses
// fenêtres détachées part de TOP_OF_PIPE et ne s'enchaîne pas avec l'attente du sémaphore d'acquisition.
// Filtré seulement s'il ne touche aucune image de notre swapchain (mainImages).
// ponytail: à retirer quand imgui sera mis à jour avec la barrière corrigée
bool IsImGuiViewportAcquireHazard(const VkDebugUtilsMessengerCallbackDataEXT* data, const std::vector<VkImage>& mainImages)
{
	if (!data->pMessageIdName || !strstr(data->pMessageIdName, "SYNC-HAZARD-WRITE-AFTER-READ")
		|| !strstr(data->pMessage, "vkAcquireNextImageKHR"))
		return false;
	for (uint32_t i = 0; i < data->objectCount; ++i)
		if (data->pObjects[i].objectType == VK_OBJECT_TYPE_IMAGE
			&& std::find(mainImages.begin(), mainImages.end(), (VkImage)data->pObjects[i].objectHandle) != mainImages.end())
			return false;
	return true;
}

// userData : images de la swapchain principale (std::vector<VkImage>)
VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData)
{
	if (IsImGuiViewportAcquireHazard(data, *static_cast<const std::vector<VkImage>*>(userData)))
		return VK_FALSE;
	const std::string message = std::string("[Vulkan] ") + data->pMessage + "\n";
	OutputDebugStringA(message.c_str());
	fputs(message.c_str(), stderr);
	// Arrêt sur erreur, comme la couche de debug DX12
	if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) && IsDebuggerPresent())
		__debugbreak();
	return VK_FALSE;
}

class VulkanBuffer final : public RhiBuffer
{
public:
	~VulkanBuffer() override { vmaDestroyBuffer(allocator, buffer, allocation); }
	uint8_t* GetMapped() const override { return mapped; }
	uint64_t GetSize() const override { return size; }

	VmaAllocator  allocator = VK_NULL_HANDLE;
	VkBuffer      buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	uint8_t*      mapped = nullptr;
	uint64_t      size = 0;
};

class VulkanTexture final : public RhiTexture
{
public:
	~VulkanTexture() override
	{
		vkDestroyImageView(device, view, nullptr);
		if (allocation) // sinon image de swapchain
			vmaDestroyImage(allocator, image, allocation);
	}
	uint32_t  GetWidth() const override { return desc.width; }
	uint32_t  GetHeight() const override { return desc.height; }
	RhiFormat GetFormat() const override { return desc.format; }

	VkImageAspectFlags Aspect() const
	{
		return desc.format == RhiFormat::D32_Float ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	}

	VkDevice       device = VK_NULL_HANDLE;
	VmaAllocator   allocator = VK_NULL_HANDLE;
	VkImage        image = VK_NULL_HANDLE;
	VmaAllocation  allocation = VK_NULL_HANDLE;
	VkImageView    view = VK_NULL_HANDLE;
	RhiTextureDesc desc;
};

class VulkanSampler final : public RhiSampler
{
public:
	~VulkanSampler() override { vkDestroySampler(device, sampler, nullptr); }
	VkDevice  device = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
};

// Un descriptor set layout par espace HLSL (set N = spaceN)
class VulkanPipelineLayout final : public RhiPipelineLayout
{
public:
	~VulkanPipelineLayout() override
	{
		vkDestroyPipelineLayout(device, layout, nullptr);
		for (VkDescriptorSetLayout setLayout : setLayouts)
			vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
	}

	VkDevice                                 device = VK_NULL_HANDLE;
	VkPipelineLayout                         layout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout>       setLayouts;
	std::vector<std::vector<RhiBindingSlot>> spaces;
};

class VulkanBindingSet final : public RhiBindingSet
{
public:
	~VulkanBindingSet() override { vkFreeDescriptorSets(device, pool, 1, &set); }

	VkDevice         device = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	VkDescriptorSet  set = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
	uint32_t         space = 0;
	bool             dynamic = false; // contient un UNIFORM_BUFFER_DYNAMIC
};

class VulkanPipeline final : public RhiPipeline
{
public:
	~VulkanPipeline() override { vkDestroyPipeline(device, pipeline, nullptr); }
	VkDevice   device = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
};

uint32_t BindingIndex(const RhiBindingSlot& slot)
{
	switch (slot.type)
	{
	case RhiBindingType::Texture: return slot.reg + kTextureBindingShift;
	case RhiBindingType::Sampler: return slot.reg + kSamplerBindingShift;
	default:                      return slot.reg;
	}
}

VkDescriptorType DescriptorType(RhiBindingType type)
{
	switch (type)
	{
	case RhiBindingType::ConstantBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case RhiBindingType::DynamicConstantBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	case RhiBindingType::Texture:               return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	default:                                    return VK_DESCRIPTOR_TYPE_SAMPLER;
	}
}

class VulkanCommandList final : public RhiCommandList
{
public:
	void Barrier(RhiTexture* texture, RhiState before, RhiState after) override
	{
		auto* t = static_cast<VulkanTexture*>(texture);
		const VkStateInfo src = ToVk(before, false, true);
		const VkStateInfo dst = ToVk(after, false, false);

		VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		barrier.srcStageMask = src.stage;
		barrier.srcAccessMask = src.access;
		barrier.dstStageMask = dst.stage;
		barrier.dstAccessMask = dst.access;
		barrier.oldLayout = src.layout;
		barrier.newLayout = dst.layout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = t->image;
		barrier.subresourceRange = { t->Aspect(), 0, 1, 0, 1 };

		VkDependencyInfo dependency = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.imageMemoryBarrierCount = 1;
		dependency.pImageMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmd, &dependency);
	}

	void Barrier(RhiBuffer* buffer, RhiState before, RhiState after) override
	{
		const VkStateInfo src = ToVk(before, true, true);
		const VkStateInfo dst = ToVk(after, true, false);

		VkBufferMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
		barrier.srcStageMask = src.stage;
		barrier.srcAccessMask = src.access;
		barrier.dstStageMask = dst.stage;
		barrier.dstAccessMask = dst.access;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = static_cast<VulkanBuffer*>(buffer)->buffer;
		barrier.size = VK_WHOLE_SIZE;

		VkDependencyInfo dependency = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dependency.bufferMemoryBarrierCount = 1;
		dependency.pBufferMemoryBarriers = &barrier;
		vkCmdPipelineBarrier2(cmd, &dependency);
	}

	void CopyBuffer(RhiBuffer* dst, RhiBuffer* src, uint64_t size) override
	{
		const VkBufferCopy region = { 0, 0, size };
		vkCmdCopyBuffer(cmd, static_cast<VulkanBuffer*>(src)->buffer, static_cast<VulkanBuffer*>(dst)->buffer, 1, &region);
	}

	void CopyBufferToTexture(RhiTexture* dst, RhiBuffer* src) override
	{
		auto* texture = static_cast<VulkanTexture*>(dst);
		VkBufferImageCopy region = {};
		region.bufferRowLength = (uint32_t)(RhiAlign(texture->desc.width * 4, kRhiTextureRowAlign) / 4); // en texels
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { texture->desc.width, texture->desc.height, 1 };
		vkCmdCopyBufferToImage(cmd, static_cast<VulkanBuffer*>(src)->buffer, texture->image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void BeginRenderPass(RhiTexture* color, RhiTexture* depth, const float clearColor[4]) override
	{
		auto* target = static_cast<VulkanTexture*>(color);
		const uint32_t width = target->desc.width;
		const uint32_t height = target->desc.height;

		VkRenderingAttachmentInfo colorAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		colorAttachment.imageView = target->view;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		memcpy(colorAttachment.clearValue.color.float32, clearColor, sizeof(float) * 4);

		VkRenderingAttachmentInfo depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
		if (depth)
		{
			depthAttachment.imageView = static_cast<VulkanTexture*>(depth)->view;
			depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
			depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.clearValue.depthStencil = { 1.f, 0 };
		}

		VkRenderingInfo info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
		info.renderArea = { { 0, 0 }, { width, height } };
		info.layerCount = 1;
		info.colorAttachmentCount = 1;
		info.pColorAttachments = &colorAttachment;
		info.pDepthAttachment = depth ? &depthAttachment : nullptr;
		vkCmdBeginRendering(cmd, &info);

		// Y inversé par viewport à hauteur négative : même image qu'en DX12 (cf. Conventions.md)
		const VkViewport viewport = { 0.f, (float)height, (float)width, -(float)height, 0.f, 1.f };
		const VkRect2D   scissor = { { 0, 0 }, { width, height } };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);
	}

	void EndRenderPass() override { vkCmdEndRendering(cmd); }

	void SetPipeline(RhiPipeline* pipeline) override
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VulkanPipeline*>(pipeline)->pipeline);
	}

	void SetBindingSet(RhiBindingSet* set, uint32_t dynamicOffset) override
	{
		auto* s = static_cast<VulkanBindingSet*>(set);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, s->layout, s->space, 1, &s->set,
			s->dynamic ? 1 : 0, &dynamicOffset);
	}

	void SetVertexBuffer(RhiBuffer* buffer, uint64_t offset, uint32_t /*stride : dans le pipeline*/) override
	{
		const VkDeviceSize vkOffset = offset;
		vkCmdBindVertexBuffers(cmd, 0, 1, &static_cast<VulkanBuffer*>(buffer)->buffer, &vkOffset);
	}

	void SetIndexBuffer(RhiBuffer* buffer, uint64_t offset) override
	{
		vkCmdBindIndexBuffer(cmd, static_cast<VulkanBuffer*>(buffer)->buffer, offset, VK_INDEX_TYPE_UINT32);
	}

	void Draw(uint32_t vertexCount) override { vkCmdDraw(cmd, vertexCount, 1, 0, 0); }
	void DrawIndexed(uint32_t indexCount) override { vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0); }

	VkCommandBuffer cmd = VK_NULL_HANDLE;
};

class VulkanRhiDevice final : public RhiDevice
{
public:
	VulkanRhiDevice(HWND hWnd, uint32_t width, uint32_t height);
	~VulkanRhiDevice() override;

	const wchar_t* GetAdapterName() const override { return m_adapterName.c_str(); }
	RhiFormat      GetBackBufferFormat() const override { return m_backBufferFormat; }
	const wchar_t* GetShaderExtension() const override { return L".spv"; }

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
	RhiTexture*     GetBackBuffer() override { return m_swapImages[m_imageIndex].get(); }

	void ResizeSwapChain(uint32_t width, uint32_t height) override;
	void WaitIdle() override;

	VkInstance       instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice         device = VK_NULL_HANDLE;
	uint32_t         queueFamily = 0;
	VkQueue          queue = VK_NULL_HANDLE;
	VkFormat         surfaceFormat = VK_FORMAT_UNDEFINED; // référencé par l'init d'ImGui
	uint32_t         GetImageCount() const { return (uint32_t)m_swapImages.size(); }

private:
	// false si la surface est de taille nulle (fenêtre minimisée) : l'ancienne swapchain est gardée
	bool CreateSwapChain();
	void DestroySwapImages();

	VkDebugUtilsMessengerEXT m_messenger = VK_NULL_HANDLE;
	VkSurfaceKHR             m_surface = VK_NULL_HANDLE;
	VmaAllocator             m_allocator = VK_NULL_HANDLE;
	VkDescriptorPool         m_descriptorPool = VK_NULL_HANDLE;

	VkSwapchainKHR                              m_swapchain = VK_NULL_HANDLE;
	VkColorSpaceKHR                             m_colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	RhiFormat                                   m_backBufferFormat = RhiFormat::BGRA8_UNorm;
	std::vector<std::unique_ptr<VulkanTexture>> m_swapImages;
	std::vector<VkImage>                        m_swapImageHandles; // lu par DebugCallback
	std::vector<VkSemaphore>                    m_renderSemaphores; // par image de swapchain
	uint32_t                                    m_imageIndex = 0;
	bool                                        m_swapchainDirty = false;
	uint32_t                                    m_width = 0;
	uint32_t                                    m_height = 0;

	struct Frame
	{
		VkCommandPool   pool = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkFence         fence = VK_NULL_HANDLE;   // signalée à la création
		VkSemaphore     acquire = VK_NULL_HANDLE;
	};
	std::array<Frame, kRhiFramesInFlight> m_frames;
	uint32_t          m_frameIndex = 0;
	VulkanCommandList m_commandList;

	std::wstring m_adapterName;
};

VulkanRhiDevice::VulkanRhiDevice(HWND hWnd, uint32_t width, uint32_t height)
	: m_width(width), m_height(height)
{
	// Avant tout appel Vulkan (delay-load) : sinon exception SEH au lieu d'une erreur propre
	if (!LoadLibraryW(L"vulkan-1.dll"))
		throw std::runtime_error("[Vulkan Error] vulkan-1.dll introuvable (pilote sans Vulkan)");

	std::vector<const char*> extensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
	std::vector<const char*> layers;

	VkApplicationInfo appInfo = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	appInfo.pApplicationName = "CoriumEngine";
	appInfo.pEngineName = "CoriumEngine";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceInfo.pApplicationInfo = &appInfo;

#if defined(_DEBUG)
	// Validation layers copiées dans VkLayers\ à côté de l'exe au build (vcpkg vulkan-validationlayers)
	SetEnvironmentVariableW(L"VK_ADD_LAYER_PATH", (GetExecutableDir() + L"VkLayers").c_str());

	uint32_t layerCount = 0;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> available(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, available.data());
	const bool hasValidation = std::any_of(available.begin(), available.end(),
		[](const VkLayerProperties& l) { return strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0; });
	if (hasValidation)
		layers.push_back("VK_LAYER_KHRONOS_validation");
	else
		OutputDebugStringA("[Vulkan] VK_LAYER_KHRONOS_validation introuvable : validation désactivée\n");
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	// Validation de la synchronisation en plus de la validation standard
	const VkValidationFeatureEnableEXT enables[] = { VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT };
	VkValidationFeaturesEXT validationFeatures = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
	validationFeatures.enabledValidationFeatureCount = _countof(enables);
	validationFeatures.pEnabledValidationFeatures = enables;

	VkDebugUtilsMessengerCreateInfoEXT messengerInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
	messengerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messengerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	messengerInfo.pfnUserCallback = DebugCallback;
	messengerInfo.pUserData = &m_swapImageHandles;

	// Messenger chaîné : couvre aussi vkCreateInstance / vkDestroyInstance
	validationFeatures.pNext = &messengerInfo;
	if (hasValidation)
		instanceInfo.pNext = &validationFeatures;
#endif

	instanceInfo.enabledExtensionCount = (uint32_t)extensions.size();
	instanceInfo.ppEnabledExtensionNames = extensions.data();
	instanceInfo.enabledLayerCount = (uint32_t)layers.size();
	instanceInfo.ppEnabledLayerNames = layers.data();
	VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &instance), "Instance creation failed");

#if defined(_DEBUG)
	if (auto createMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"))
		VK_CHECK(createMessenger(instance, &messengerInfo, nullptr, &m_messenger), "Debug messenger creation failed");
#endif

	VkWin32SurfaceCreateInfoKHR surfaceInfo = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	surfaceInfo.hinstance = GetModuleHandleW(nullptr);
	surfaceInfo.hwnd = hWnd;
	VK_CHECK(vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &m_surface), "Win32 surface creation failed");

	// GPU Vulkan 1.3 avec une queue graphique qui présente sur la surface ; GPU dédié en priorité
	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr);
	std::vector<VkPhysicalDevice> gpus(gpuCount);
	vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data());
	VkPhysicalDeviceProperties chosenProps = {};
	for (VkPhysicalDevice gpu : gpus)
	{
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(gpu, &props);
		if (props.apiVersion < VK_API_VERSION_1_3)
			continue;

		uint32_t familyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familyCount, nullptr);
		std::vector<VkQueueFamilyProperties> families(familyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familyCount, families.data());
		for (uint32_t i = 0; i < familyCount; ++i)
		{
			VkBool32 present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, m_surface, &present);
			if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !present)
				continue;
			const bool better = !physicalDevice
				|| (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && chosenProps.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
			if (better)
			{
				physicalDevice = gpu;
				queueFamily = i;
				chosenProps = props;
			}
			break;
		}
	}
	if (!physicalDevice)
		throw std::runtime_error("[Vulkan Error] aucun GPU Vulkan 1.3 compatible");

	const int nameLength = MultiByteToWideChar(CP_UTF8, 0, chosenProps.deviceName, -1, nullptr, 0);
	m_adapterName.resize(nameLength > 0 ? nameLength - 1 : 0);
	MultiByteToWideChar(CP_UTF8, 0, chosenProps.deviceName, -1, m_adapterName.data(), nameLength);

	const float priority = 1.f;
	VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueFamilyIndex = queueFamily;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &priority;

	VkPhysicalDeviceVulkan13Features features13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;
	VkPhysicalDeviceFeatures2 features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	features.pNext = &features13;
	features.features.fillModeNonSolid = VK_TRUE; // FillMode::Wireframe

	// VK_KHR_dynamic_rendering explicite : le backend ImGui charge les fonctions suffixées KHR
	const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME };
	VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceInfo.pNext = &features;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;
	deviceInfo.enabledExtensionCount = _countof(deviceExtensions);
	deviceInfo.ppEnabledExtensionNames = deviceExtensions;
	VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device), "Device creation failed");
	vkGetDeviceQueue(device, queueFamily, 0, &queue);

	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.instance = instance;
	allocatorInfo.physicalDevice = physicalDevice;
	allocatorInfo.device = device;
	allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_allocator), "VMA allocator creation failed");

	// Pool des binding sets (FREE : un set est libéré avec son RhiBindingSet)
	const VkDescriptorPoolSize poolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 32 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 128 },
	};
	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 256;
	poolInfo.poolSizeCount = _countof(poolSizes);
	poolInfo.pPoolSizes = poolSizes;
	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool), "Descriptor pool creation failed");

	for (Frame& frame : m_frames)
	{
		VkCommandPoolCreateInfo cmdPoolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		cmdPoolInfo.queueFamilyIndex = queueFamily;
		VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &frame.pool), "Command pool creation failed");

		VkCommandBufferAllocateInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmdInfo.commandPool = frame.pool;
		cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdInfo.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(device, &cmdInfo, &frame.cmd), "Command buffer allocation failed");

		VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frame.fence), "Fence creation failed");

		VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.acquire), "Semaphore creation failed");
	}

	// Même choix de format que le backend ImGui pour ses fenêtres détachées : elles partagent son pipeline
	const VkFormat requestFormats[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };
	const VkSurfaceFormatKHR chosen = ImGui_ImplVulkanH_SelectSurfaceFormat(physicalDevice, m_surface,
		requestFormats, _countof(requestFormats), VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
	surfaceFormat = chosen.format;
	m_colorSpace = chosen.colorSpace;
	if (surfaceFormat == VK_FORMAT_B8G8R8A8_UNORM)
		m_backBufferFormat = RhiFormat::BGRA8_UNorm;
	else if (surfaceFormat == VK_FORMAT_R8G8B8A8_UNORM)
		m_backBufferFormat = RhiFormat::RGBA8_UNorm;
	else
		throw std::runtime_error("[Vulkan Error] aucun format de swapchain RGBA8/BGRA8");

	if (!CreateSwapChain())
		m_swapchainDirty = true;
}

VulkanRhiDevice::~VulkanRhiDevice()
{
	if (device)
	{
		vkDeviceWaitIdle(device);
		DestroySwapImages();
		vkDestroySwapchainKHR(device, m_swapchain, nullptr);
		for (Frame& frame : m_frames)
		{
			vkDestroyCommandPool(device, frame.pool, nullptr);
			vkDestroyFence(device, frame.fence, nullptr);
			vkDestroySemaphore(device, frame.acquire, nullptr);
		}
		vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
		vmaDestroyAllocator(m_allocator);
		vkDestroyDevice(device, nullptr);
	}
	if (instance)
	{
		vkDestroySurfaceKHR(instance, m_surface, nullptr);
		if (m_messenger)
		{
			auto destroyMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
			destroyMessenger(instance, m_messenger, nullptr);
		}
		vkDestroyInstance(instance, nullptr);
	}
}

void VulkanRhiDevice::DestroySwapImages()
{
	m_swapImages.clear();
	m_swapImageHandles.clear();
	for (VkSemaphore semaphore : m_renderSemaphores)
		vkDestroySemaphore(device, semaphore, nullptr);
	m_renderSemaphores.clear();
}

bool VulkanRhiDevice::CreateSwapChain()
{
	VkSurfaceCapabilitiesKHR caps;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_surface, &caps), "Surface capabilities query failed");

	VkExtent2D extent = caps.currentExtent;
	if (extent.width == UINT32_MAX)
	{
		extent.width = std::clamp(m_width, caps.minImageExtent.width, caps.maxImageExtent.width);
		extent.height = std::clamp(m_height, caps.minImageExtent.height, caps.maxImageExtent.height);
	}
	if (extent.width == 0 || extent.height == 0)
		return false;

	uint32_t imageCount = (std::max)(caps.minImageCount, kRhiFramesInFlight);
	if (caps.maxImageCount && imageCount > caps.maxImageCount)
		imageCount = caps.maxImageCount;

	VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	info.surface = m_surface;
	info.minImageCount = imageCount;
	info.imageFormat = surfaceFormat;
	info.imageColorSpace = m_colorSpace;
	info.imageExtent = extent;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = caps.currentTransform;
	info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync, comme Present(1) en DX12
	info.clipped = VK_TRUE;
	info.oldSwapchain = m_swapchain;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain), "Swapchain creation failed");
	DestroySwapImages();
	vkDestroySwapchainKHR(device, m_swapchain, nullptr);
	m_swapchain = swapchain;

	uint32_t count = 0;
	vkGetSwapchainImagesKHR(device, m_swapchain, &count, nullptr);
	std::vector<VkImage> images(count);
	vkGetSwapchainImagesKHR(device, m_swapchain, &count, images.data());

	for (VkImage image : images)
	{
		auto texture = std::make_unique<VulkanTexture>();
		texture->device = device;
		texture->image = image;
		texture->desc = { extent.width, extent.height, m_backBufferFormat, RhiTextureUsage_RenderTarget };

		VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = surfaceFormat;
		viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &texture->view), "Swapchain image view creation failed");
		m_swapImages.push_back(std::move(texture));
		m_swapImageHandles.push_back(image);

		VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore semaphore;
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore), "Semaphore creation failed");
		m_renderSemaphores.push_back(semaphore);
	}

	m_swapchainDirty = false;
	return true;
}

std::unique_ptr<RhiBuffer> VulkanRhiDevice::CreateBuffer(const RhiBufferDesc& desc)
{
	VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	info.size = desc.size;
	if (desc.usage & RhiBufferUsage_Vertex)   info.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (desc.usage & RhiBufferUsage_Index)    info.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (desc.usage & RhiBufferUsage_Constant) info.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (desc.usage & RhiBufferUsage_CopySrc)  info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (desc.usage & RhiBufferUsage_CopyDst)  info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	if (desc.memory == RhiMemory::Upload)
	{
		// Cohérente : les écritures CPU sont visibles au submit, sans flush
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}

	auto buffer = std::make_unique<VulkanBuffer>();
	buffer->allocator = m_allocator;
	buffer->size = desc.size;
	VmaAllocationInfo result = {};
	VK_CHECK(vmaCreateBuffer(m_allocator, &info, &allocInfo, &buffer->buffer, &buffer->allocation, &result), "Buffer creation failed");
	buffer->mapped = static_cast<uint8_t*>(result.pMappedData);
	return buffer;
}

std::unique_ptr<RhiTexture> VulkanRhiDevice::CreateTexture(const RhiTextureDesc& desc)
{
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = ToVk(desc.format);
	info.extent = { desc.width, desc.height, 1 };
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (desc.usage & RhiTextureUsage_Sampled)      info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	if (desc.usage & RhiTextureUsage_RenderTarget) info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (desc.usage & RhiTextureUsage_Depth)        info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (desc.usage & RhiTextureUsage_CopyDst)      info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

	auto texture = std::make_unique<VulkanTexture>();
	texture->device = device;
	texture->allocator = m_allocator;
	texture->desc = desc;
	VK_CHECK(vmaCreateImage(m_allocator, &info, &allocInfo, &texture->image, &texture->allocation, nullptr), "Image creation failed");

	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = texture->image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = info.format;
	viewInfo.subresourceRange = { texture->Aspect(), 0, 1, 0, 1 };
	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &texture->view), "Image view creation failed");
	return texture;
}

std::unique_ptr<RhiSampler> VulkanRhiDevice::CreateSampler()
{
	VkSamplerCreateInfo info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	info.magFilter = VK_FILTER_NEAREST;
	info.minFilter = VK_FILTER_NEAREST;
	info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	info.maxLod = VK_LOD_CLAMP_NONE;

	auto sampler = std::make_unique<VulkanSampler>();
	sampler->device = device;
	VK_CHECK(vkCreateSampler(device, &info, nullptr, &sampler->sampler), "Sampler creation failed");
	return sampler;
}

std::unique_ptr<RhiPipelineLayout> VulkanRhiDevice::CreatePipelineLayout(const RhiPipelineLayoutDesc& desc)
{
	auto layout = std::make_unique<VulkanPipelineLayout>();
	layout->device = device;
	layout->spaces = desc.spaces;

	for (const auto& slots : desc.spaces)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		for (const RhiBindingSlot& slot : slots)
			bindings.push_back({ BindingIndex(slot), DescriptorType(slot.type), 1,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr });

		VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		info.bindingCount = (uint32_t)bindings.size();
		info.pBindings = bindings.data();
		VkDescriptorSetLayout setLayout;
		VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout), "Descriptor set layout creation failed");
		layout->setLayouts.push_back(setLayout);
	}

	VkPipelineLayoutCreateInfo info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	info.setLayoutCount = (uint32_t)layout->setLayouts.size();
	info.pSetLayouts = layout->setLayouts.data();
	VK_CHECK(vkCreatePipelineLayout(device, &info, nullptr, &layout->layout), "Pipeline layout creation failed");
	return layout;
}

std::unique_ptr<RhiBindingSet> VulkanRhiDevice::CreateBindingSet(const RhiBindingSetDesc& desc)
{
	auto* layout = static_cast<VulkanPipelineLayout*>(desc.layout);
	const auto& slots = layout->spaces[desc.space];
	CE_ASSERT(desc.bindings.size() == slots.size());

	auto set = std::make_unique<VulkanBindingSet>();
	set->device = device;
	set->pool = m_descriptorPool;
	set->layout = layout->layout;
	set->space = desc.space;

	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout->setLayouts[desc.space];
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &set->set), "Descriptor set allocation failed");

	std::vector<VkDescriptorBufferInfo> bufferInfos(slots.size());
	std::vector<VkDescriptorImageInfo>  imageInfos(slots.size());
	std::vector<VkWriteDescriptorSet>   writes;
	for (size_t i = 0; i < slots.size(); ++i)
	{
		const RhiBinding& binding = desc.bindings[i];
		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = set->set;
		write.dstBinding = BindingIndex(slots[i]);
		write.descriptorCount = 1;
		write.descriptorType = DescriptorType(slots[i].type);
		switch (slots[i].type)
		{
		case RhiBindingType::DynamicConstantBuffer:
			set->dynamic = true;
			[[fallthrough]];
		case RhiBindingType::ConstantBuffer:
			bufferInfos[i] = { static_cast<VulkanBuffer*>(binding.buffer)->buffer, binding.offset, binding.size };
			write.pBufferInfo = &bufferInfos[i];
			break;
		case RhiBindingType::Texture:
			imageInfos[i] = { VK_NULL_HANDLE, static_cast<VulkanTexture*>(binding.texture)->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			write.pImageInfo = &imageInfos[i];
			break;
		case RhiBindingType::Sampler:
			imageInfos[i] = { static_cast<VulkanSampler*>(binding.sampler)->sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };
			write.pImageInfo = &imageInfos[i];
			break;
		}
		writes.push_back(write);
	}
	vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
	return set;
}

std::unique_ptr<RhiPipeline> VulkanRhiDevice::CreatePipeline(const RhiPipelineDesc& desc)
{
	auto createModule = [&](const std::vector<uint8_t>& code)
	{
		VkShaderModuleCreateInfo info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		info.codeSize = code.size();
		info.pCode = reinterpret_cast<const uint32_t*>(code.data());
		VkShaderModule module;
		VK_CHECK(vkCreateShaderModule(device, &info, nullptr, &module), "Shader module creation failed");
		return module;
	};
	const VkShaderModule vs = createModule(*desc.vertexShader);
	const VkShaderModule ps = createModule(*desc.pixelShader);

	VkPipelineShaderStageCreateInfo stages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "VSMain" },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, ps, "PSMain" },
	};

	const VkVertexInputBindingDescription vertexBinding = { 0, desc.vertexStride, VK_VERTEX_INPUT_RATE_VERTEX };
	std::vector<VkVertexInputAttributeDescription> attributes;
	for (uint32_t i = 0; i < desc.attributes.size(); ++i)
		attributes.push_back({ i, 0, ToVk(desc.attributes[i].format), desc.attributes[i].offset });

	VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &vertexBinding;
	vertexInput.vertexAttributeDescriptionCount = (uint32_t)attributes.size();
	vertexInput.pVertexAttributeDescriptions = attributes.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineViewportStateCreateInfo viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// Avec le viewport à hauteur négative, l'ordre des sommets à l'écran est le même qu'en DX12 :
	// CLOCKWISE = FrontCounterClockwise FALSE (cf. Conventions.md)
	VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	raster.polygonMode = desc.fillMode == FillMode::Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	raster.cullMode = desc.cullMode == CullMode::None ? VK_CULL_MODE_NONE
		: desc.cullMode == CullMode::Front ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT;
	raster.frontFace = VK_FRONT_FACE_CLOCKWISE;
	raster.depthClampEnable = VK_FALSE;
	raster.lineWidth = 1.f;

	VkPipelineMultisampleStateCreateInfo multisample = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	const bool alphaBlend = desc.blendMode == BlendMode::AlphaBlend;
	VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = alphaBlend ? VK_FALSE : VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	// Mêmes réglages que le blend DX12 (alpha : ONE / ZERO)
	VkPipelineColorBlendAttachmentState blendAttachment = {};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	if (alphaBlend)
	{
		blendAttachment.blendEnable = VK_TRUE;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAttachment;

	const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic.dynamicStateCount = _countof(dynamicStates);
	dynamic.pDynamicStates = dynamicStates;

	const VkFormat colorFormat = ToVk(desc.colorFormat);
	VkPipelineRenderingCreateInfo rendering = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &colorFormat;
	rendering.depthAttachmentFormat = ToVk(desc.depthFormat);

	VkGraphicsPipelineCreateInfo info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	info.pNext = &rendering;
	info.stageCount = _countof(stages);
	info.pStages = stages;
	info.pVertexInputState = &vertexInput;
	info.pInputAssemblyState = &inputAssembly;
	info.pViewportState = &viewportState;
	info.pRasterizationState = &raster;
	info.pMultisampleState = &multisample;
	info.pDepthStencilState = &depthStencil;
	info.pColorBlendState = &blend;
	info.pDynamicState = &dynamic;
	info.layout = static_cast<VulkanPipelineLayout*>(desc.layout)->layout;

	auto pipeline = std::make_unique<VulkanPipeline>();
	pipeline->device = device;
	const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline->pipeline);
	vkDestroyShaderModule(device, vs, nullptr);
	vkDestroyShaderModule(device, ps, nullptr);
	VK_CHECK(result, "Graphics pipeline creation failed");
	return pipeline;
}

RhiCommandList* VulkanRhiDevice::BeginFrame()
{
	Frame& frame = m_frames[m_frameIndex];
	VK_CHECK(vkWaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX), "Frame fence wait failed");

	if (m_swapchainDirty)
	{
		vkDeviceWaitIdle(device);
		if (!CreateSwapChain())
			return nullptr; // minimisée
	}

	const VkResult acquire = vkAcquireNextImageKHR(device, m_swapchain, UINT64_MAX, frame.acquire, VK_NULL_HANDLE, &m_imageIndex);
	if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_swapchainDirty = true;
		return nullptr;
	}
	if (acquire == VK_SUBOPTIMAL_KHR)
		m_swapchainDirty = true; // image utilisable, recréée à la frame suivante
	else
		VK_CHECK(acquire, "Swapchain image acquisition failed");

	VK_CHECK(vkResetFences(device, 1, &frame.fence), "Fence reset failed");
	VK_CHECK(vkResetCommandPool(device, frame.pool, 0), "Command pool reset failed");
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(frame.cmd, &beginInfo), "Command buffer begin failed");

	m_commandList.cmd = frame.cmd;
	return &m_commandList;
}

void VulkanRhiDevice::EndFrame()
{
	Frame& frame = m_frames[m_frameIndex];
	VK_CHECK(vkEndCommandBuffer(frame.cmd), "Command buffer end failed");

	VkSemaphoreSubmitInfo wait = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	wait.semaphore = frame.acquire;
	wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSemaphoreSubmitInfo signal = { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO };
	signal.semaphore = m_renderSemaphores[m_imageIndex];
	signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	VkCommandBufferSubmitInfo cmdInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO };
	cmdInfo.commandBuffer = frame.cmd;

	VkSubmitInfo2 submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO_2 };
	submit.waitSemaphoreInfoCount = 1;
	submit.pWaitSemaphoreInfos = &wait;
	submit.commandBufferInfoCount = 1;
	submit.pCommandBufferInfos = &cmdInfo;
	submit.signalSemaphoreInfoCount = 1;
	submit.pSignalSemaphoreInfos = &signal;
	VK_CHECK(vkQueueSubmit2(queue, 1, &submit, frame.fence), "Queue submit failed");
}

void VulkanRhiDevice::Present()
{
	VkPresentInfoKHR info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = &m_renderSemaphores[m_imageIndex];
	info.swapchainCount = 1;
	info.pSwapchains = &m_swapchain;
	info.pImageIndices = &m_imageIndex;
	const VkResult result = vkQueuePresentKHR(queue, &info);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		m_swapchainDirty = true;
	else
		VK_CHECK(result, "Present failed");

	m_frameIndex = (m_frameIndex + 1) % kRhiFramesInFlight;
}

void VulkanRhiDevice::ResizeSwapChain(uint32_t width, uint32_t height)
{
	m_width = width;
	m_height = height;
	WaitIdle();
	if (!CreateSwapChain())
		m_swapchainDirty = true;
}

void VulkanRhiDevice::WaitIdle()
{
	VK_CHECK(vkDeviceWaitIdle(device), "Device wait idle failed");
}

// Le backend Win32 d'ImGui ne crée pas de surface Vulkan pour les fenêtres détachées
int CreateViewportSurface(ImGuiViewport* viewport, ImU64 instance, const void* allocator, ImU64* outSurface)
{
	VkWin32SurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	info.hinstance = GetModuleHandleW(nullptr);
	info.hwnd = static_cast<HWND>(viewport->PlatformHandleRaw);
	return (int)vkCreateWin32SurfaceKHR((VkInstance)instance, &info,
		static_cast<const VkAllocationCallbacks*>(allocator), reinterpret_cast<VkSurfaceKHR*>(outSurface));
}

void CheckImGuiResult(VkResult result)
{
	if (result != VK_SUCCESS)
		OutputDebugStringA(("[Vulkan] ImGui : VkResult " + std::to_string(result) + "\n").c_str());
}

class VulkanUI final : public RhiUI
{
public:
	explicit VulkanUI(VulkanRhiDevice& device)
		: m_device(device)
	{
		ImGui::GetPlatformIO().Platform_CreateVkSurface = CreateViewportSurface;

		ImGui_ImplVulkan_InitInfo info = {};
		info.ApiVersion = VK_API_VERSION_1_3;
		info.Instance = device.instance;
		info.PhysicalDevice = device.physicalDevice;
		info.Device = device.device;
		info.QueueFamily = device.queueFamily;
		info.Queue = device.queue; // même queue que la frame (cf. RenderPlatformWindows)
		info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 8;
		info.MinImageCount = kRhiFramesInFlight;
		info.ImageCount = (std::max)(device.GetImageCount(), kRhiFramesInFlight);
		info.UseDynamicRendering = true;
		info.PipelineRenderingCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
		info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
		info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &device.surfaceFormat;
		info.CheckVkResultFn = CheckImGuiResult;
		ImGui_ImplVulkan_Init(&info);

		// Affichage de la cible offscreen : filtrage linéaire, comme le sampler du backend DX12
		VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
		VK_CHECK(vkCreateSampler(device.device, &samplerInfo, nullptr, &m_sampler), "UI sampler creation failed");
	}

	~VulkanUI() override
	{
		if (m_sceneSet)
			ImGui_ImplVulkan_RemoveTexture(m_sceneSet);
		ImGui_ImplVulkan_Shutdown();
		ImGui::GetPlatformIO().Platform_CreateVkSurface = nullptr;
		vkDestroySampler(m_device.device, m_sampler, nullptr);
	}

	void BeginFrame() override { ImGui_ImplVulkan_NewFrame(); }

	void Render(RhiCommandList* cmd) override
	{
		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VulkanCommandList*>(cmd)->cmd);
	}

	void RenderPlatformWindows() override
	{
		// Soumises sur la même queue après la frame : la barrière RenderTarget → ShaderRead de la
		// cible offscreen couvre aussi ces soumissions, pas d'attente CPU
		ImGui::RenderPlatformWindowsDefault();
	}

	uint64_t SetSceneTexture(RhiTexture* texture) override
	{
		const VkImageView view = static_cast<VulkanTexture*>(texture)->view;
		if (!m_sceneSet)
		{
			m_sceneSet = ImGui_ImplVulkan_AddTexture(m_sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		else
		{
			// Même descriptor set réécrit : l'ImTextureID reste stable
			const VkDescriptorImageInfo imageInfo = { m_sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
			VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstSet = m_sceneSet;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.pImageInfo = &imageInfo;
			vkUpdateDescriptorSets(m_device.device, 1, &write, 0, nullptr);
		}
		return (uint64_t)m_sceneSet;
	}

private:
	VulkanRhiDevice& m_device;
	VkSampler        m_sampler = VK_NULL_HANDLE;
	VkDescriptorSet  m_sceneSet = VK_NULL_HANDLE;
};

std::unique_ptr<RhiUI> VulkanRhiDevice::CreateUI()
{
	return std::make_unique<VulkanUI>(*this);
}

} // namespace

std::unique_ptr<RhiDevice> CreateVulkanDevice(HWND hWnd, uint32_t width, uint32_t height)
{
	return std::make_unique<VulkanRhiDevice>(hWnd, width, height);
}
