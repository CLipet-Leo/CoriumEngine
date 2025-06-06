#pragma once

#include "Core/CoreDefinitions.h"

#include "Engine/CEngine.h"

#include "Common/Logger.h"
#include "Common/Time.h"
#include "Core/PerGameSettings.h"

#ifdef WIN32
	#include "Platform/Win32/Win32Utils.h"
	#include "Platform/Win32/SubObject.h"
    #include "Platform/Win32/w32Caption.h"
	#include "Platform/Win32/Window.h"
	#include "Platform/Win32/IApplication.h"
#endif
