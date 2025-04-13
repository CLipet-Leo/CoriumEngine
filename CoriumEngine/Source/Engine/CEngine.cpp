#include "Corium.h"

namespace Engine
{
	CEngine g_CoriumEngine;
	
	VOID SetMode(EngineMode mode)
	{
		g_CoriumEngine.SetEngineMode(mode);
	}

	EngineMode GetMode()
	{
		return g_CoriumEngine.GetEngineMode();
	}

	std::wstring EngineModeToString()
	{
		switch (Engine::GetMode())
		{
		case EngineMode::DEBUG:			return L"Debug";
		case EngineMode::RELEASE:		return L"Release";
		case EngineMode::EDITOR:		return L"Editor";
		case EngineMode::SERVER:		return L"Server";
		default:						return L"None";
		}
	}
}

CEngine::CEngine()
{
#ifdef _DEBUG
	m_EngineMode = EngineMode::DEBUG;
#else
	m_EngineMode = EngineMode::RELEASE;
#endif
}

CEngine::~CEngine()
{
}

Engine::EngineMode CEngine::GetEngineMode()
{
	return m_EngineMode;
}

VOID CEngine::SetEngineMode(Engine::EngineMode mode)
{
	m_EngineMode = mode;
}