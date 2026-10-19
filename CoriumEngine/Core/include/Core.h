#pragma once

#include "CorePublic.h"
#include "ShaderTypes.h"

// Géométrie
#include "../Geometry/BufferGeometry.h"
#include "../Geometry/BoxGeometry.h"

// Ressources GPU (RHI : interne à la DLL ; les backends Core/DX12 et Core/Vulkan
// incluent eux-mêmes les en-têtes de leur API)
#include "../../Scene/ResourceDescs.h"
#include "../../Renderer/Rhi.h"
#include "../../Renderer/MeshRegistry.h"
#include "../../Renderer/MaterialRegistry.h"
#include "../../Renderer/RenderSystem.h"

// Systèmes
#include "../../Scene/Systems/CameraSystem.h"
#include "../../Scene/Systems/LightSystem.h"

#include "../../Renderer/Renderer.h"
