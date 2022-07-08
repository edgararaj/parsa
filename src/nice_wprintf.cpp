#include <stdio.h>
#include <assert.h>
#include "windows_framework.h"

extern HANDLE g_conout;

int nice_wprintf(const wchar_t* fmt, ...)
{
	wchar_t buffer[256];
	va_list args;
	va_start(args, fmt);
	const auto chars_to_write = vswprintf(buffer, COUNTOF(buffer), fmt, args);
	if (chars_to_write < 0)
	{
		assert(false && "Buffer not sufficient!");
		return 0;
	}
	va_end(args);

	DWORD chars_written;
	if (!WriteConsoleW(g_conout, buffer, (DWORD)chars_to_write, &chars_written, 0))
	{
		wprintf(buffer);
		return 0;
	}

	return chars_written;
}
