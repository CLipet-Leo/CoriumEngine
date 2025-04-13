#pragma once

class CORIUM_API CEngine;
namespace Engine
{
	enum EngineMode : INT {
		NONE,
		DEBUG,
		RELEASE,
		EDITOR,
		SERVER
	};


	extern CEngine g_CoriumEngine;

	VOID CORIUM_API SetMode(EngineMode mode);
	EngineMode CORIUM_API GetMode();
	
	WSTRING CORIUM_API EngineModeToString();

}

using namespace Engine;
class CORIUM_API CEngine
{
public:
	CEngine();
	~CEngine();

private:
	EngineMode m_EngineMode;

public:
	EngineMode GetEngineMode();
	VOID SetEngineMode(EngineMode mode);

};