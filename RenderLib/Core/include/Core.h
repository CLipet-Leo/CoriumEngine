#pragma once

#include "Helper.h"
#include "ShaderTypes.h"
#include "../public/Renderer.h"

// Géométrie
#include "../Geometry/Types.h"
#include "../Geometry/BufferGeometry.h"
#include "../Geometry/BoxGeometry.h"

// Scène (ordre de dépendance)
#include "../../Scene/Entity.h"
#include "../../Scene/Components.h"
#include "../../Scene/Scene.h"
#include "../../Scene/MeshRegistry.h"
#include "../../Scene/MaterialRegistry.h"

// Systèmes
#include "../../Scene/Systems/TransformSystem.h"
#include "../../Scene/Systems/CameraSystem.h"
#include "../../Scene/Systems/LightSystem.h"
#include "../../Scene/Systems/RenderSystem.h"

// Éditeur
#include "../../Editor/EditorUI.h"

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
#include "../DX12/DX12Renderer.h"
#endif
