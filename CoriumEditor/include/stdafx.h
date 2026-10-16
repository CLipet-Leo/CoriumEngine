#pragma once

#include "targetver.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclure les en-têtes Windows rarement utilisés
#endif

#include "Resource.h"
#include "framework.h"

#include "CoreDefinition.h"
#include "CorePublic.h"

// ImGui (lib statique vcpkg) : contexte et backend Win32 ici, backend DX12 dans la DLL CoriumEngine
#include <imgui.h>
#include <imgui_internal.h>     // DockBuilder (layout par défaut)
#include <imgui_impl_win32.h>

#include "../public/EditorCamera.h"
#include "../public/EditorUI.h"
