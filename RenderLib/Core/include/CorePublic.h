#pragma once

// API visible des exe : aucun type DirectX ni ImGui.
// Core.h (PCH de la DLL) l'inclut puis ajoute les headers internes.
#include "Helper.h"
#include "../../Math/Math.h"
#include "../Platform/Clock.h"
#include "../Platform/Input.h"
#include "../public/Renderer.h"

// Scène (ordre de dépendance)
#include "../../Scene/Entity.h"
#include "../../Scene/Components.h"
#include "../../Scene/Scene.h"
#include "../../Scene/SceneSerializer.h"
#include "../../Scene/Systems/TransformSystem.h"
