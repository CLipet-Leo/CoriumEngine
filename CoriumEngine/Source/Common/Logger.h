#pragma once

#include <string>

class CORIUM_API Logger {
	// Getters and Setters for Singleton static instance
private:
	static Logger* instance;

public:
	static Logger* GetInstance() { return instance; }

	/* Logger Constructor */
	Logger();
	/* Logger Deconstructor */
	~Logger();

	/* Print to Log File */
	static VOID PrintLog(const WCHAR* fmt, ...);

	static std::wstring LogDirectory();
	
	static std::wstring LogFile();

	/* Print a line of '-' char's */
	static VOID PrintDebugSaperator();

	/* Check to see if MTail is already Running */
	static BOOL IsMTailRunning();

	/* Start MTail Application */
	static VOID StartMTail();

};