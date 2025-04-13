#pragma once

#include <string>

namespace Time {

	/* Get current time in string format */
	WSTRING CORIUM_API GetTime(BOOL stripped = FALSE);

	/* Get current date in string format */
	WSTRING CORIUM_API GetDate(BOOL stripped = FALSE);

	/* Get current date and time in string format */
	WSTRING CORIUM_API GetDateTimeString(BOOL stripped = FALSE);
}