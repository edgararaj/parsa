int nice_wprintf(const wchar_t* fmt, ...)
{
	const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!handle || handle == INVALID_HANDLE_VALUE) return 0;

	wchar_t buffer[256];
	va_list args;
	va_start(args, fmt);
	const auto chars_to_write = vswprintf(buffer, ARR_COUNT(buffer), fmt, args);
	if (chars_to_write < 0)
	{
		assert(!"Buffer not sufficient!");
		return 0;
	}
	va_end(args);

	DWORD chars_written;
	if (!WriteConsoleW(handle, buffer, (DWORD)chars_to_write, &chars_written, 0))
		return 0;
	CloseHandle(handle);

	return chars_written;
}

