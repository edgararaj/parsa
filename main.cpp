#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits>

typedef int64_t i64;
typedef uint64_t u64;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef _UNICODE
#include <windows.h>

#define ARR_COUNT(x) (sizeof(x)/sizeof(x[0]))

#include "args_parser.h"
#include "file_ults.h"

struct IncludeStatement {
	char* start_location;
	char* end_location;
	FileBuffer file_buffer;
};

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

	const auto main_file_buffer = read_file_to_unix_buffer(main_file_path);
	if (!main_file_buffer.content)
		return 1;

	IncludeStatement includes[64];
	int includes_count = 0;

	u64 out_buffer_size = 0;

	auto prev_statement_arg_end = main_file_buffer.content;
	auto haystack = main_file_buffer.content;
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

		const auto include_file_path_size = statement_arg_end - statement_arg_start - 1;
		if (!include_file_path_size || include_file_path_size > std::numeric_limits<int>::max()) {
			printf("Invalid file path for statement: #include\n");
			haystack = statement_arg_end;
			continue;
		}

		const auto include_file_path_size_trunc = (int)include_file_path_size;

		wchar_t include_file_path[64];
		{
			const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, statement_arg_start+1, include_file_path_size_trunc, include_file_path, ARR_COUNT(include_file_path));
			include_file_path[bytes_written] = 0;
		}

		const auto include_file_buffer = read_file_to_unix_buffer(include_file_path);
		if (!include_file_buffer.content)
			break;

		include.file_buffer = include_file_buffer;

		out_buffer_size += (statement_start - prev_statement_arg_end) + include_file_buffer.size;
		prev_statement_arg_end = statement_arg_end + 1;

		includes_count++;
		haystack = statement_arg_end;
	}

	out_buffer_size += main_file_buffer.size - (prev_statement_arg_end - main_file_buffer.content) + 1;

	const auto out_buffer = (char*)VirtualAlloc(0, out_buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!out_buffer)
	{
		printf("Failed to allocate memory\n");
		return 1;
	}

	auto out_buffer_end = out_buffer;
	auto main_file_buffer_cursor = main_file_buffer.content;
	for (int i = 0; i < includes_count; i++)
	{
		const auto& include = includes[i];
		const auto size = include.start_location - main_file_buffer_cursor;
		memcpy(out_buffer_end, main_file_buffer_cursor, size);
		main_file_buffer_cursor = include.end_location + 1;
		out_buffer_end += size;

		memcpy(out_buffer_end, include.file_buffer.content, include.file_buffer.size);
		VirtualFree(include.file_buffer.content, include.file_buffer.size, MEM_RELEASE);
		out_buffer_end += include.file_buffer.size;
	}

	{
		const auto size = main_file_buffer.size - (main_file_buffer_cursor - main_file_buffer.content);
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
