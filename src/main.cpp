#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits>
#include <io.h>
#include <fcntl.h>
#include <assert.h>

typedef int64_t i64;
typedef uint64_t u64;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define ARR_COUNT(x) (sizeof(x)/sizeof(x[0]))

#include <shlwapi.h>

#include "optional.cpp"
#include "nice_wprintf.cpp"
#include "wcslcpy.cpp"
#include "wcslcat.cpp"

HANDLE g_conout;

#include "args_parser.cpp"
#include "file_ults.cpp"

struct IncludeStatement {
	char* start_location;
	char* end_location;
	FileBuffer file_buffer;
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

struct ProcessResult {
	u64 out_buffer_size;
	int includes_count;
};

const ProcessResult process_include_statements(IncludeStatement* includes, const FileBuffer& in_file_buffer)
{
	ProcessResult result = {};

	auto prev_statement_arg_end = in_file_buffer.content;
	auto haystack = in_file_buffer.content;
	while (true)
	{
		const auto include_statement = "#include ";
		const auto statement_start = strstr(haystack, include_statement);
		if (!statement_start) break;

		const auto statement_end = statement_start + ARR_COUNT(include_statement) - 1;

		auto statement_arg_start = statement_end + 1;
		for (; *statement_arg_start == ' '; statement_arg_start++);
		if (*statement_arg_start != '"')
		{
			printf("Can't find opening \" for statement: #include\n");
			break;
		}

		auto statement_arg_end = statement_arg_start + 1;
		for (; *statement_arg_end != '"' && *statement_arg_end != '\n' && *statement_arg_end; statement_arg_end++);
		if (*statement_arg_end != '"')
		{
			printf("Can't find closing \" for statement: #include\n");
			break;
		}

		auto& include = includes[result.includes_count];
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

		result.out_buffer_size += (statement_start - prev_statement_arg_end) + include_file_buffer.size;
		prev_statement_arg_end = statement_arg_end + 1;

		result.includes_count++;
		haystack = statement_arg_end;
	}

	result.out_buffer_size += in_file_buffer.size - (prev_statement_arg_end - in_file_buffer.content) + 1;

	return result;
}

Optional<bool> get_canonical_selector(wchar_t* dest, const size_t dest_count, const wchar_t* src, const bool check_for_star)
{
	wchar_t* abs_path_file_part;
	if (dest_count > std::numeric_limits<DWORD>::max()) return {};
	const auto abs_path_written = GetFullPathNameW(src, (DWORD)dest_count, dest, &abs_path_file_part);
	if (!abs_path_written || abs_path_written > dest_count) return {};
	auto selecting_many = PathIsDirectoryW(dest);

	if (selecting_many)
	{
		if (abs_path_file_part)
		{
			if (wcslcat(dest, L"\\*", dest_count) >= dest_count)
			{
				printf("File path is too large!\n");
				return {};
			}
		}
		else
		{
			if (wcslcat(dest, L"*", dest_count) >= dest_count)
			{
				printf("File path is too large!\n");
				return {};
			}
		}
	}
	else
	{
		if (abs_path_file_part && check_for_star)
		{
			if (wcschr(abs_path_file_part, L'*'))
				selecting_many = 1;
		}
	}


	return selecting_many;
}

bool get_path_dir(wchar_t* dest, size_t dest_count, const wchar_t* src)
{
	const auto src_last_slash = get_last_slash(src);
	if (src_last_slash)
	{
		const size_t size = src_last_slash - src + 1;
		if (size >= dest_count)
		{
			printf("File path is too large!\n");
			return 0;
		}
		wcslcpy(dest, src, size+1);
	}
	else
	{
		dest[0] = 0;
	}

	return 1;
}

const wchar_t* get_rel_path(const wchar_t* abs_path, const wchar_t* curr_dir)
{
	const wchar_t* result = abs_path;

	bool match = 1;
	for (auto c = curr_dir; *c && match; c++)
	{
		if (*c == *result)
		{
			result++;
			match = 1;
		}
		else
			match = 0;
	}

	return match ? result : abs_path;
}

int wmain(int argc, const wchar_t** argv)
{
	// Enable conhost ascii escape sequences
	g_conout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (!g_conout || g_conout == INVALID_HANDLE_VALUE) return 0;
	DWORD mode;
	GetConsoleMode(g_conout, &mode);
	SetConsoleMode(g_conout, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	ArgEntry arg_entries[] = {
		{L"h", L"help", "Display this message", 0},
		{L"o", L"out", "Output directory/file", L"gen/", 1},
		{0, L"path", "Directory or file(s) to preprocess", L"*.js", -1},
	};

	if (!parse_args(arg_entries, ARR_COUNT(arg_entries), argc, argv))
		return 1;

#ifdef PARSA_DEBUG
	printf("--------ARGS--------\n");
	for (int i = 0; i < ARR_COUNT(arg_entries); i++)
	{
		nice_wprintf(g_conout, L"--%ls: %ls\n", arg_entries[i].long_name, arg_entries[i].value);
	}
	printf("--------------------\n\n");
#endif

	const auto in_path_arg = get_arg_entry_value(arg_entries, ARR_COUNT(arg_entries), L"path");
	wchar_t in_abs_path[64];
	auto canonical_selector_result = get_canonical_selector(in_abs_path, ARR_COUNT(in_abs_path), in_path_arg, 1);
	if (!canonical_selector_result.has_value()) return 1;
	const auto in_path_selecting_many = canonical_selector_result.value();

	const auto out_path_arg = get_arg_entry_value(arg_entries, ARR_COUNT(arg_entries), L"out");
	wchar_t out_abs_path[64];
	canonical_selector_result = get_canonical_selector(out_abs_path, ARR_COUNT(out_abs_path), out_path_arg, 0);
	if (!canonical_selector_result.has_value()) return 1;
	const auto out_path_selecting_many = canonical_selector_result.value();

	if (in_path_selecting_many && !out_path_selecting_many)
	{
		printf("Please specify a valid directory for output!\n");
		return 1;
	}

	wchar_t current_dir[64];
	const auto current_dir_result = GetCurrentDirectoryW(ARR_COUNT(current_dir), current_dir);
	if (!current_dir_result || current_dir_result > ARR_COUNT(current_dir))
		return 1;

	if (wcslcat(current_dir, L"\\", ARR_COUNT(current_dir)) >= ARR_COUNT(current_dir))
	{
		printf("File path is too large!\n");
		return 1;
	}
	const auto in_path = get_rel_path(in_abs_path, current_dir);
	const auto out_path = get_rel_path(out_abs_path, current_dir);

	wchar_t in_path_dir[64];
	if (!get_path_dir(in_path_dir, ARR_COUNT(in_path_dir), in_path))
		return 1;

	wchar_t out_path_dir[64];
	if (!get_path_dir(out_path_dir, ARR_COUNT(out_path_dir), out_path))
		return 1;

	{
		WIN32_FIND_DATAW ffd;
		auto search_handle = FindFirstFileW(in_path, &ffd);
		if (INVALID_HANDLE_VALUE == search_handle) return 1;
		do {
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			const auto in_file_name = ffd.cFileName;
			wchar_t in_file_path[64];
			if (wcslcpy(in_file_path, in_path_dir, ARR_COUNT(in_file_path)) >= ARR_COUNT(in_file_path))
			{
				printf("File path is too large!\n");
				continue;
			}
			if (wcslcat(in_file_path, in_file_name, ARR_COUNT(in_file_path)) >= ARR_COUNT(in_file_path))
			{
				printf("File path is too large!\n");
				continue;
			}

			nice_wprintf(g_conout, L"Processing file \"%ls\"...\n", in_file_path);

			wchar_t out_file_path[64];
			if (out_path_selecting_many)
			{
				if (wcslcpy(out_file_path, out_path_dir, ARR_COUNT(out_file_path)) >= ARR_COUNT(out_file_path))
				{
					printf("File path is too large!\n");
					continue;
				}
				if (wcslcat(out_file_path, in_file_name, ARR_COUNT(out_file_path)) >= ARR_COUNT(out_file_path))
				{
					printf("File path is too large!\n");
					continue;
				}
			}
			else
			{
				if (wcslcpy(out_file_path, out_abs_path, ARR_COUNT(out_file_path)) >= ARR_COUNT(out_file_path))
				{
					printf("File path is too large!\n");
					continue;
				}
			}

			const auto in_file_buffer = read_file_to_unix_buffer(in_file_path);
			if (!in_file_buffer.content)
				continue;

			IncludeStatement includes[64];

			const auto process_result = process_include_statements(includes, in_file_buffer);

			const auto out_buffer = (char*)VirtualAlloc(0, process_result.out_buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (!out_buffer)
			{
				printf("Failed to allocate memory!\n");
				continue;
			}

			auto out_buffer_end = out_buffer;
			auto in_file_buffer_cursor = in_file_buffer.content;
			for (int i = 0; i < process_result.includes_count; i++)
			{
				const auto& include = includes[i];
				const auto size = include.start_location - in_file_buffer_cursor;
				memcpy(out_buffer_end, in_file_buffer_cursor, size);
				in_file_buffer_cursor = include.end_location + 1;
				out_buffer_end += size;

				memcpy(out_buffer_end, include.file_buffer.content, include.file_buffer.size);
				VirtualFree(include.file_buffer.content, include.file_buffer.size, MEM_RELEASE);
				out_buffer_end += include.file_buffer.size;
			}

			{
				const auto size = in_file_buffer.size - (in_file_buffer_cursor - in_file_buffer.content);
				memcpy(out_buffer_end, in_file_buffer_cursor, size);
				out_buffer_end += size;
				*out_buffer_end = '\n';
			}

			const auto out_file_handle = create_wo_file(out_file_path);
			if (!out_file_handle) continue;

			if (!write_file(out_file_handle, out_file_path, out_buffer, process_result.out_buffer_size))
				continue;

		}
		while (FindNextFileW(search_handle, &ffd));

		const auto error = GetLastError();
		if (ERROR_NO_MORE_FILES != error)
			return 1;

		FindClose(search_handle);
	}

	return 0;
}

