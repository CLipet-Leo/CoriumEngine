#pragma once

// Fichiers d'en-tête DirectX
#include "directx/d3dx12.h"
#include <d3d12.h>
//#include <D3dx12.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>

// D3D12 Memory Allocator (optionnel)
#if __has_include(<D3D12MemAlloc.h>)
#include <D3D12MemAlloc.h>
#define CORIUM_HAS_D3D12MA 1
#else
#define CORIUM_HAS_D3D12MA 0
#endif
