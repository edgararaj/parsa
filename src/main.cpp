#include <stdio.h>
#include <stdint.h>
#include <limits>
#include <io.h>
#include <fcntl.h>
#include <assert.h>

#include "wcslcpy.cpp"
#include "wcslcat.cpp"

typedef int64_t i64;
typedef uint64_t u64;

#include "utils.h"

#include "windows_framework.h"
#include <Shlwapi.h>

#include "nice_wprintf.cpp"

HANDLE g_conout;

struct Buffer {
	char* content;
	u64 size;
};

#include "args_parser.cpp"
#include "file_utils.cpp"

struct IncludeStatement {
	char* start_location;
	char* end_location;
	wchar_t file_path[64];
	Buffer file_buffer;
};

struct DefineStatement {
	char* start_location;
	char* end_location;
	Buffer token;
	Buffer replace;
};

const wchar_t* get_last_slash(const wchar_t* path)
{
	const wchar_t* last_slash = 0;
	for (auto c = path; *c; c++)
	{
		if (*c == L'\\' || *c == L'/')
			last_slash = c;
	}

	return last_slash;
}

const u64 process_define(DefineStatement& define, const Buffer& in_file_buffer)
{
	const auto statement = "#define ";
	const auto statement_start = strstr(in_file_buffer.content, statement);
	if (!statement_start) return 0;

	const auto statement_end = statement_start + COUNTOF(statement) - 1;

	auto statement_arg1_start = statement_end + 1;
	for (; *statement_arg1_start == ' '; statement_arg1_start++);
	if (!isalnum(*statement_arg1_start))
	{
		wprintf(L"First argument of #define isn't alphanumeric\n");
		return 0;
	}

	auto statement_arg1_end = statement_arg1_start + 1;
	for (; isalnum(*statement_arg1_end); statement_arg1_end++);
	if (*statement_arg1_end != ' ')
	{
		wprintf(L"Couldn't find second argument of #include statement\n");
		return 0;
	}

	auto statement_arg2_start = statement_arg1_end;
	for (; *statement_arg2_start == ' '; statement_arg2_start++);
	if (!isalnum(*statement_arg2_start))
	{
		wprintf(L"Second argument of #define isn't alphanumeric\n");
		return 0;
	}

	auto statement_arg2_end = statement_arg2_start + 1;
	for (; isalnum(*statement_arg2_end); statement_arg2_end++);

	define.start_location = statement_start;
	define.end_location = statement_arg2_end - 1;

	define.token = {statement_arg1_start, (u64)(statement_arg1_end - statement_arg1_start)};
	define.replace = {statement_arg2_start, (u64)(statement_arg2_end - statement_arg2_start)};

	return in_file_buffer.size - (statement_arg2_end - in_file_buffer.content) + (statement_start - in_file_buffer.content);
}

const u64 process_include(IncludeStatement& include, const Buffer& in_file_buffer)
{
	const auto statement = "#include ";
	const auto statement_start = strstr(in_file_buffer.content, statement);
	if (!statement_start) return 0;

	const auto statement_end = statement_start + COUNTOF(statement) - 1;

	auto statement_arg_start = statement_end + 1;
	for (; *statement_arg_start == ' '; statement_arg_start++);
	if (*statement_arg_start != '"')
	{
		wprintf(L"Can't find opening \" of #include statement\n");
		return 0;
	}

	auto statement_arg_end = statement_arg_start + 1;
	for (; *statement_arg_end != '"' && *statement_arg_end != '\n' && *statement_arg_end; statement_arg_end++);
	if (*statement_arg_end != '"')
	{
		wprintf(L"Can't find closing \" of #include statement\n");
		return 0;
	}

	include.start_location = statement_start;
	include.end_location = statement_arg_end;

	const auto include_file_path_size = statement_arg_end - statement_arg_start - 1;
	if (!include_file_path_size || include_file_path_size > std::numeric_limits<int>::max()) {
		wprintf(L"Invalid file path of #include statement\n");
		return 0;
	}

	const auto include_file_path_size_trunc = (int)include_file_path_size;

	const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, statement_arg_start+1, include_file_path_size_trunc, include.file_path, COUNTOF(include.file_path));
	include.file_path[bytes_written] = 0;

	return in_file_buffer.size - (statement_arg_end - in_file_buffer.content) + (statement_start - in_file_buffer.content) - 1;
}

int process_replace_include(Buffer& in_file_buffer2, const Buffer& out_file_buffer, const wchar_t in_path_dir[64])
{
	IncludeStatement include;
	const auto size1 = process_include(include, out_file_buffer);
	if (!size1) return 0;
	in_file_buffer2.size = size1;

	wchar_t include_file_path[64];
	// generate include_file_path {{{
	if (wcslcpy(include_file_path, in_path_dir, COUNTOF(include_file_path)) >= COUNTOF(include_file_path))
	{
		wprintf(L"File path is too large!\n");
		return -1;
	}
	if (wcslcat(include_file_path, include.file_path, COUNTOF(include_file_path)) >= COUNTOF(include_file_path))
	{
		wprintf(L"File path is too large!\n");
		return -1;
	}
	// }}}

	include.file_buffer = read_file_to_unix_buffer(include_file_path);
	in_file_buffer2.size += include.file_buffer.size;

	in_file_buffer2.content = (char *)VirtualAlloc(0, in_file_buffer2.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!in_file_buffer2.content)
	{
		wprintf(L"Failed to allocate memory!\n");
		return -1;
	}

	auto out_file_buffer_end = in_file_buffer2.content;
	auto in_file_buffer_cursor = out_file_buffer.content;

	{
		const auto size = include.start_location - in_file_buffer_cursor;
		memcpy(out_file_buffer_end, in_file_buffer_cursor, size);
		in_file_buffer_cursor = include.end_location + 1;
		out_file_buffer_end += size;

		memcpy(out_file_buffer_end, include.file_buffer.content, include.file_buffer.size);
		VirtualFree(include.file_buffer.content, include.file_buffer.size, MEM_RELEASE);
		out_file_buffer_end += include.file_buffer.size;
	}

	{
		const auto size = out_file_buffer.size - (in_file_buffer_cursor - out_file_buffer.content);
		memcpy(out_file_buffer_end, in_file_buffer_cursor, size);
	}

	return 1;
}

const wchar_t* get_rel_path(const wchar_t* abs_path, const wchar_t* curr_dir)
{
	const wchar_t* result = abs_path;

	for (auto c = curr_dir; *c; c++)
		if (*c != *result++) return abs_path;

	return result;
}

enum class CanonicalSelectorResult {
	None, File, Directory, Files
};

CanonicalSelectorResult get_canonical_selector(wchar_t* dest, const size_t dest_count, const wchar_t* src, const bool check_for_star)
{
	const auto file_too_large_error = []() {
		wprintf(L"File path is too large!\n");
		return CanonicalSelectorResult::None;
	};

	wchar_t* abs_path_file_part;
	wchar_t abs_path[64];
	const auto abs_path_written = GetFullPathNameW(src, COUNTOF(abs_path), abs_path, &abs_path_file_part);
	if (!abs_path_written || abs_path_written > COUNTOF(abs_path))
		return file_too_large_error();

	//if (dest_count > std::numeric_limits<DWORD>::max()) return CanonicalSelectorResult::None;

	wchar_t current_dir[64];
	const auto current_dir_result = GetCurrentDirectoryW(COUNTOF(current_dir), current_dir);
	if (!current_dir_result || current_dir_result > COUNTOF(current_dir))
		return file_too_large_error();

	if (wcslcat(current_dir, L"\\", COUNTOF(current_dir)) >= COUNTOF(current_dir))
		return file_too_large_error();

	const auto rel_path = get_rel_path(abs_path, current_dir);
	if (wcslcpy(dest, rel_path, dest_count) >= dest_count)
		return file_too_large_error();

	if (GetFileAttributesW(dest) & FILE_ATTRIBUTE_DIRECTORY)
	{
		if (abs_path_file_part)
		{
			if (wcslcat(dest, L"\\", dest_count) >= dest_count)
				return file_too_large_error();
		}
		if (wcslcat(dest, L"*", dest_count) >= dest_count)
			return file_too_large_error();
		return CanonicalSelectorResult::Directory;
	}
	else
	{
		if (abs_path_file_part && check_for_star && wcschr(abs_path_file_part, L'*'))
			return CanonicalSelectorResult::Files;
		return CanonicalSelectorResult::File;
	}
}

bool get_path_dir(wchar_t* dest, size_t dest_count, const wchar_t* src)
{
	const auto src_last_slash = get_last_slash(src);
	if (src_last_slash)
	{
		const size_t size = src_last_slash - src + 1;
		if (size >= dest_count)
		{
			wprintf(L"File path is too large!\n");
			return 0;
		}
		wcslcpy(dest, src, size+1);
	}
	else
		dest[0] = 0;

	return 1;
}

bool get_parent_path_dir(wchar_t* dest, size_t dest_count, const wchar_t* src)
{
	const auto src_last_slash = get_last_slash(src);
	if (src_last_slash)
	{
		const size_t size = src_last_slash - src;
		if (size >= dest_count)
		{
			wprintf(L"File path is too large!\n");
			return 0;
		}
		wcslcpy(dest, src, size+1);
	}
	else
		dest[0] = 0;

	return 1;
}

#ifdef TEST
#define MAIN entry
#else
#define MAIN wmain
#endif

int MAIN(int argc, const wchar_t** argv)
{
	// Enable conhost ascii escape sequences
	g_conout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!g_conout || g_conout == INVALID_HANDLE_VALUE) return 0;
	DWORD mode;
	GetConsoleMode(g_conout, &mode);
	SetConsoleMode(g_conout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	_setmode(_fileno(stdout), _O_U16TEXT);

	ArgEntry arg_entries[] = {
		{L"h", L"help", L"Display this message"},
		{L"o", L"out", L"Output directory/file", 1, L"gen/"},
		{0, L"path", L"Directory or file(s) to preprocess", -1},
	};

	const auto parse_args_result = parse_args(arg_entries, COUNTOF(arg_entries), argc, argv, L"parsa");
	if (parse_args_result == ParseArgsResult::Error)
		return 1;
	else if (parse_args_result == ParseArgsResult::Help)
		return 0;

#ifdef DEBUG
	wprintf(L"--------ARGS--------\n");
	for (int i = 0; i < COUNTOF(arg_entries); i++)
	{
		nice_wprintf(L"--%ls: ", arg_entries[i].long_name);
		auto value = arg_entries[i].value;
		int j = 0;
		for (; j < arg_entries[i].value_count; j++)
		{
			nice_wprintf(L"%ls ", value);
			for (; *value; value++) {}
			value++;
		}
		if (!j && value)
			nice_wprintf(L"<%ls>", arg_entries[i].value);

		nice_wprintf(L"\n");
	}
	wprintf(L"--------------------\n\n");
#endif

	const auto in_path_arg = get_arg_entry_value(arg_entries, COUNTOF(arg_entries), L"path");
	wchar_t in_path[64];
	const auto in_canonical_selector_result = get_canonical_selector(in_path, COUNTOF(in_path), in_path_arg, 1);
	if (in_canonical_selector_result == CanonicalSelectorResult::None) return 1;

	const auto out_path_arg = get_arg_entry_value(arg_entries, COUNTOF(arg_entries), L"out");
	wchar_t out_path[64];
	const auto out_canonical_selector_result = get_canonical_selector(out_path, COUNTOF(out_path), out_path_arg, 0);
	if (out_canonical_selector_result == CanonicalSelectorResult::None) return 1;

	if (in_canonical_selector_result >= CanonicalSelectorResult::Directory && out_canonical_selector_result == CanonicalSelectorResult::File)
	{
		wprintf(L"Please specify a valid directory for output!\n");
		return 1;
	}

	wchar_t in_path_dir[64];
	if (!get_path_dir(in_path_dir, COUNTOF(in_path_dir), in_path))
		return 1;

	wchar_t out_path_dir[64];
	if (!get_path_dir(out_path_dir, COUNTOF(out_path_dir), out_path))
		return 1;

	if (!PathFileExistsW(out_path_dir))
	{
		nice_wprintf(L"Output directory \"%ls\" doesn't exist!\n", out_path_dir);
		wchar_t create_out_path_dir[64];
		memcpy(create_out_path_dir, out_path_dir, sizeof(out_path_dir));
		while (true)
		{
			nice_wprintf(L"Creating directory \"%ls\"...\n", create_out_path_dir);
			if (CreateDirectoryW(create_out_path_dir, 0) != ERROR_PATH_NOT_FOUND)
				break;
			if (!get_parent_path_dir(create_out_path_dir, COUNTOF(create_out_path_dir), create_out_path_dir))
				break;
		}
	}

	{
		WIN32_FIND_DATAW ffd;
		auto search_handle = FindFirstFileW(in_path, &ffd);
		if (INVALID_HANDLE_VALUE == search_handle) {
			switch (in_canonical_selector_result)
			{
				case CanonicalSelectorResult::File:
					nice_wprintf(L"File \"%ls\" not found!\n", in_path);
					break;
				case CanonicalSelectorResult::Files:
					nice_wprintf(L"No matching files found for \"%ls\"!\n", in_path);
					break;
			}
			return 1;
		}
		int items_found = 0;
		do {
			items_found++;
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			const auto in_file_name = ffd.cFileName;
			wchar_t in_file_path[64];
			// generate in_file_path {{{
			if (wcslcpy(in_file_path, in_path_dir, COUNTOF(in_file_path)) >= COUNTOF(in_file_path))
			{
				wprintf(L"File path is too large!\n");
				continue;
			}
			if (wcslcat(in_file_path, in_file_name, COUNTOF(in_file_path)) >= COUNTOF(in_file_path))
			{
				wprintf(L"File path is too large!\n");
				continue;
			}
			// }}}

			wchar_t out_file_path[64];
			// generate out_file_path {{{
			if (out_canonical_selector_result >= CanonicalSelectorResult::Directory)
			{
				if (wcslcpy(out_file_path, out_path_dir, COUNTOF(out_file_path)) >= COUNTOF(out_file_path))
				{
					wprintf(L"File path is too large!\n");
					continue;
				}
				if (wcslcat(out_file_path, in_file_name, COUNTOF(out_file_path)) >= COUNTOF(out_file_path))
				{
					wprintf(L"File path is too large!\n");
					continue;
				}
			}
			else
			{
				if (wcslcpy(out_file_path, out_path, COUNTOF(out_file_path)) >= COUNTOF(out_file_path))
				{
					wprintf(L"File path is too large!\n");
					continue;
				}
			}
			// }}}

			nice_wprintf(L"Processing file \"%ls\"...\n", in_file_path);

			const auto in_file_buffer = read_file_to_unix_buffer(in_file_path);
			if (!in_file_buffer.content)
				continue;

			int out_file_buffer_index = 0;
			Buffer out_file_buffers[] = {in_file_buffer, {}};
			while (true)
			{
				auto next_out_file_buffer_index = (out_file_buffer_index + 1) % 2;
				auto ret = process_replace_include(out_file_buffers[next_out_file_buffer_index], out_file_buffers[out_file_buffer_index], in_path_dir);
				if (ret == 0)
					break;
				else if (ret == -1)
					continue;

				out_file_buffer_index = next_out_file_buffer_index;
			}
				
/*
				{
					DefineStatement define;
					out_file_buffer = in_file_buffer2;
					const auto size1 = process_define(define, in_file_buffer2);
					if (!size1) break;
					out_file_buffer.size = size1;

					out_file_buffer.content = (char*)VirtualAlloc(0, out_file_buffer.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
					if (!out_file_buffer.content)
					{
						wprintf(L"Failed to allocate memory!\n");
						continue;
					}

					auto out_file_buffer_end = out_file_buffer.content;
					auto in_file_buffer_cursor = in_file_buffer2.content;

					while (true)
					{
						const auto size = define.start_location - in_file_buffer_cursor;
						memcpy(out_file_buffer_end, in_file_buffer_cursor, size);
						in_file_buffer_cursor = define.end_location + 1;
						out_file_buffer_end += size;
					}

					{
						const auto size = in_file_buffer2.size - (in_file_buffer_cursor - in_file_buffer2.content);
						memcpy(out_file_buffer_end, in_file_buffer_cursor, size);
					}
				}
				*/

			const auto out_file_handle = create_wo_file(out_file_path);
			if (!out_file_handle) continue;

			const auto out_file_buffer = out_file_buffers[out_file_buffer_index];
			if (!write_file(out_file_handle, out_file_path, out_file_buffer))
				continue;

		}
		while (FindNextFileW(search_handle, &ffd));

		FindClose(search_handle);

		if (in_canonical_selector_result == CanonicalSelectorResult::Directory && items_found == 2)
			nice_wprintf(L"Directory \"%ls\" is empty!\n", in_path_dir);

		const auto error = GetLastError();
		if (ERROR_NO_MORE_FILES != error)
			return 1;
	}

	return 0;
}

