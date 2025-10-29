#pragma once

#ifdef RENDERER_EXPORTS
	#define CORIUM_API __declspec(dllexport)
#else
	#define CORIUM_API __declspec(dllimport)
#endif

#define MAX_NAME_STRING 256

typedef std::wstring WSTRING;
typedef std::string  STRING;
