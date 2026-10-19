#pragma once

#include "CorePublic.h"
#include "ShaderTypes.h"

// Géométrie
#include "../Geometry/BufferGeometry.h"
#include "../Geometry/BoxGeometry.h"

// Ressources GPU
#include "../../Scene/ResourceDescs.h"
#include "../../Renderer/MeshRegistry.h"
#include "../../Renderer/MaterialRegistry.h"
#include "../../Renderer/RenderSystem.h"

// Systèmes
#include "../../Scene/Systems/CameraSystem.h"
#include "../../Scene/Systems/LightSystem.h"

#ifdef USE_DX12
#include "../DX12/DX12Debug.h"
#include "../DX12/DXGIAdapter.h"
#include "../DX12/DXGIFactory.h"
#include "../DX12/DX12Device.h"
#include "../DX12/DX12CommandQueue.h"
#include "../DX12/DX12CommandObjects.h"
#include "../DX12/DX12Fence.h"
#include "../DX12/DX12DescriptorHeaps.h"
#include "../DX12/DX12MemoryManager.h"
#include "../DX12/DX12PSOCache.h"
#include "../DX12/DX12SwapChain.h"
#include "../DX12/DX12DepthStencil.h"
#include "../DX12/DX12RenderTarget.h"
#include "../DX12/DX12Renderer.h"
#endif
