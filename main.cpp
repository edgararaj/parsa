#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits>

typedef uint64_t u64;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef _UNICODE
#include <windows.h>

#define ARR_COUNT(x) (sizeof(x)/sizeof(x[0]))

#include "args_parser.h"

struct FileView {
	HANDLE handle;
	char* content;
};

struct IncludeStatement {
	char* start_location;
	char* end_location;
	char* file;
	u64 file_size;
};

HANDLE create_wo_file(const wchar_t* file_path)
{
	const auto file_handle = CreateFileW(file_path, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == file_handle)
	{
		wprintf(L"Failed to create file (%ls)\n", file_path);
		const auto error = GetLastError();
		if (ERROR_FILE_EXISTS == error)
			wprintf(L"Reason: File (%ls) already exists\n", file_path);

		return 0;
	}

	return file_handle;
}

bool write_file(const HANDLE file_handle, const wchar_t* file_path, const void* buffer, const u64 buffer_size)
{
	auto file_size_to_write = buffer_size;
	while (true)
	{
		const auto max_dword_value = std::numeric_limits<DWORD>::max();
		const auto to_write = (DWORD)(file_size_to_write > max_dword_value ? max_dword_value : file_size_to_write);

		DWORD bytes_written;
		const auto ret = WriteFile(file_handle, buffer, to_write, &bytes_written, 0);
		if (!ret)
		{
			wprintf(L"Failed to write to file (%ls)\n", file_path);
			break;
		}
		if (!bytes_written)
		{
			wprintf(L"Successfuly wrote to file (%ls)\n", file_path);
			return 1;
		}

		file_size_to_write -= bytes_written;
	}

	return 0;
}

HANDLE open_ro_file(const wchar_t* file_path)
{
	const auto file_handle = CreateFileW(file_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == file_handle)
	{
		wprintf(L"Failed to open file (%ls)\n", file_path);
		const auto error = GetLastError();
		if (ERROR_FILE_NOT_FOUND == error)
			wprintf(L"Reason: File (%ls) doesn't exist\n", file_path);
		else
			wprintf(L"Reason: File (%ls) is being used by other program\n", file_path);

		return 0;
	}

	return file_handle;
}

u64 get_file_size(const HANDLE file_handle)
{
	LARGE_INTEGER large_int;
	const auto ret = GetFileSizeEx(file_handle, &large_int);
	if (!ret) return 0;

	return large_int.QuadPart;
}

FileView create_ro_file_view(const wchar_t* file_path)
{
	const auto file_handle = open_ro_file(file_path);
	if (!file_handle) return {};

	const auto file_map = CreateFileMappingW(file_handle, 0, PAGE_READONLY, 0, 0, 0);
	if (!file_map)
	{
		printf("Failed to create file mapping\n");
		return {};
	}

	const auto file_view = (char*)MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 0);
	if (!file_view)
	{
		printf("Failed to create file view\n");
		return {};
	}

	return {.handle = file_handle, .content = file_view};
}

u64 convert_file_view_to_unix(char* out_buffer, const FileView file_view, const u64 file_view_size, const wchar_t* file_path)
{
	if (strcmp(&file_view.content[file_view_size-2], "\r\n") != 0)
	{
		wprintf(L"File (%ls) is unix\n", file_path);
		const auto size = file_view_size - 1;
		memcpy(out_buffer, file_view.content, size);
		return size;
	}
	else if (strcmp(&file_view.content[file_view_size-1], "\n") != 0)
	{
		wprintf(L"File (%ls) may be corrupted\n", file_path);
		return 0;
	}

	wprintf(L"File (%ls) is dos\n", file_path);

	u64 result = file_view_size;

	auto out_buffer_end = out_buffer;
	auto haystack = file_view.content;
	while (true)
	{
		const auto dos_le = strstr(haystack, "\r\n");
		if (!dos_le) {
			break;
		}

		const auto size = dos_le - haystack;
		memcpy(out_buffer_end, haystack, size);
		out_buffer_end += size;
		haystack = dos_le + 1;
		result--;
	}

	return result - 1;
}

int main(int argc, const char** argv)
{
	// Enable conhost ascii escape sequences
	const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode;
	GetConsoleMode(handle, &mode);
	SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	ArgEntry arg_entries[] = {
		{"h", "help", "Display this message"},
		{"o", "out", "Output directory/file", ""},
		{0, "path...", "Directory or file(s) to preprocess", "*"},
	};

	if (!parse_args(arg_entries, ARR_COUNT(arg_entries), argc, argv))
		return 1;

	wchar_t main_file_path[64];
	{
		const auto main_file_path_arg = get_arg_entry_value(arg_entries, ARR_COUNT(arg_entries), "path...");
		const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, main_file_path_arg, -1, main_file_path, ARR_COUNT(main_file_path));
		main_file_path[bytes_written] = 0;
	}

	wchar_t out_path[64];
	{
		const auto out_path_arg = get_arg_entry_value(arg_entries, ARR_COUNT(arg_entries), "out");
		const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, out_path_arg, -1, out_path, ARR_COUNT(out_path));
		out_path[bytes_written] = 0;
	}

	for (int i = 0; i < ARR_COUNT(arg_entries); i++)
	{
		printf("%s: %s\n", arg_entries[i].short_name, arg_entries[i].value);
	}

	const auto main_file_view = create_ro_file_view(main_file_path);
	if (!main_file_view.content)
		return 1;

	const auto main_file_view_size = get_file_size(main_file_view.handle);
	if (!main_file_view_size) {
		wprintf(L"Couldn't get file size of file (%ls)\n", main_file_path);
		return 1;
	}

	const auto main_file_buffer = (char*)VirtualAlloc(0, main_file_view_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!main_file_buffer)
	{
		printf("Failed to allocate memory\n");
		return 1;
	}
	const auto main_file_buffer_size = convert_file_view_to_unix(main_file_buffer, main_file_view, main_file_view_size, main_file_path);

	IncludeStatement includes[64];
	int includes_count = 0;

	u64 out_buffer_size = 0;

	auto prev_statement_arg_end = main_file_buffer;
	auto haystack = main_file_buffer;
	while (true)
	{
		const auto statement_start = strstr(haystack, "#include");
		if (!statement_start) break;
		const auto statement_arg_start = strchr(statement_start, '\"');
		if (!statement_arg_start) {
			printf("Couldn't find opening \" for statement: #include\n");
			break;
		}

		const auto statement_arg_end = strchr(statement_arg_start+1, '\"');
		if (!statement_arg_end)
		{
			printf("Couldn't find closing \" for statement: #include\n");
			break;
		}

		auto& include = includes[includes_count];
		include.start_location = statement_start;
		include.end_location = statement_arg_end;

		const auto include_file_path_size = (int)(statement_arg_end - statement_arg_start - 1); // @TODO
		if (!include_file_path_size) {
			printf("Invalid file path for statement: #include\n");
			haystack = statement_arg_end;
			continue;
		}

		wchar_t include_file_path[64];
		{
			const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, statement_arg_start+1, include_file_path_size, include_file_path, ARR_COUNT(include_file_path));
			include_file_path[bytes_written] = 0;
		}

		const auto include_file_view = create_ro_file_view(include_file_path);
		if (!include_file_view.content)
			break;

		const auto include_file_view_size = get_file_size(include_file_view.handle);
		if (!include_file_view_size) {
			wprintf(L"Couldn't get file size of file (%ls)\n", include_file_path);
			break;
		}

		const auto include_file_buffer = (char*)VirtualAlloc(0, include_file_view_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!include_file_buffer)
		{
			printf("Failed to allocate memory\n");
			break;
		}

		include.file = include_file_buffer;

		const auto include_file_buffer_size = convert_file_view_to_unix(include_file_buffer, include_file_view, include_file_view_size, include_file_path);
		CloseHandle(include_file_view.handle);

		include.file_size = include_file_buffer_size;

		out_buffer_size += (statement_start - prev_statement_arg_end) + include_file_buffer_size;
		prev_statement_arg_end = statement_arg_end + 1;

		includes_count++;
		haystack = statement_arg_end;
	}

	out_buffer_size += main_file_buffer_size - (prev_statement_arg_end - main_file_buffer) + 1;

	const auto out_buffer = (char*)VirtualAlloc(0, out_buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!out_buffer)
	{
		printf("Failed to allocate memory\n");
		return 1;
	}

	auto out_buffer_end = out_buffer;
	auto main_file_buffer_cursor = main_file_buffer;
	for (int i = 0; i < includes_count; i++)
	{
		const auto& include = includes[i];
		const auto size = include.start_location - main_file_buffer_cursor;
		memcpy(out_buffer_end, main_file_buffer_cursor, size);
		main_file_buffer_cursor = include.end_location + 1;
		out_buffer_end += size;

		memcpy(out_buffer_end, include.file, include.file_size);
		out_buffer_end += include.file_size;
	}

	{
		const auto size = main_file_buffer_size - (main_file_buffer_cursor - main_file_buffer);
		memcpy(out_buffer_end, main_file_buffer_cursor, size);
		out_buffer_end += size;
		*out_buffer_end = '\n';
	}

	wchar_t out_file_path[64];
	{
		wchar_t* no_ext = 0;
		wchar_t* ext = 0;
		wchar_t* pt;

		wchar_t file_path[ARR_COUNT(main_file_path)];
		wcsncpy(file_path, main_file_path, ARR_COUNT(main_file_path));
		no_ext = wcstok(file_path, L".", &pt);

		if (no_ext)
			ext = wcstok(0, L".", &pt);

		wcsncpy(out_file_path, out_path, ARR_COUNT(out_file_path));
		wcsncat(out_file_path, no_ext, ARR_COUNT(out_file_path));
		wcsncat(out_file_path, L".gen.", ARR_COUNT(out_file_path));
		wcsncat(out_file_path, ext, ARR_COUNT(out_file_path));
	}

	const auto out_file_handle = create_wo_file(out_file_path);
	if (!out_file_handle) return 1;

	if (!write_file(out_file_handle, out_file_path, out_buffer, out_buffer_size))
		return 1;

	return 0;
}
