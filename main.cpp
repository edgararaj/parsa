#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits>

typedef int64_t i64;
typedef uint64_t u64;
typedef unsigned int uint;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef _UNICODE
#include <windows.h>

#define ARR_COUNT(x) (sizeof(x)/sizeof(x[0]))

struct FileView {
	char* content;
	HANDLE handle;
};

struct IncludeStatement {
	char* start_location;
	char* end_location;
	char* file;
	u64 file_size;
};

HANDLE create_wo_file(const wchar_t* file_name)
{
	HANDLE result = 0;

	const auto file_handle = CreateFileW(file_name, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == file_handle)
	{
		fprintf(stderr, "Failed to create file with name (%ls)\n", file_name);
		const auto error = GetLastError();
		if (ERROR_FILE_EXISTS == error)
			fprintf(stderr, "Reason: File with name (%ls) already exists\n", file_name);

		return result;
	}

	result = file_handle;

	return result;
}

HANDLE open_ro_file(const wchar_t* file_name)
{
	HANDLE result = 0;

	const auto file_handle = CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == file_handle)
	{
		fprintf(stderr, "Failed to open file (%ls)\n", file_name);
		const auto error = GetLastError();
		if (ERROR_FILE_NOT_FOUND == error)
			fprintf(stderr, "Reason: File (%ls) doesn't exist\n", file_name);
		else
			fprintf(stderr, "Reason: File (%ls) is being used by other program\n", file_name);

		return result;
	}

	result = file_handle;
	return result;
}

u64 get_file_size(const HANDLE file_handle)
{
	u64 result = 0;

	LARGE_INTEGER large_int;
	const auto ret = GetFileSizeEx(file_handle, &large_int);
	if (!ret)
		return result;

	const auto file_size = large_int.QuadPart;

	result = file_size;

	return result;
}

FileView create_ro_file_view(const wchar_t* file_name)
{
	FileView result = {};

	const auto file_handle = open_ro_file(file_name);
	if (!file_handle) return result;

	result.handle = file_handle;

	const auto file_map = CreateFileMappingW(file_handle, 0, PAGE_READONLY, 0, 0, 0);
	if (!file_map)
	{
		fprintf(stderr, "Failed to create file mapping\n");
		return result;
	}

	const auto file_view = (char*)MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 0);
	if (!file_view)
	{
		fprintf(stderr, "Failed to create file view\n");
		return result;
	}

	result.content = file_view;

	return result;
}

u64 convert_file_view_to_unix(char* out_buffer, FileView file_view, u64 file_view_size, const wchar_t* main_file_name)
{
	if (strcmp(&file_view.content[file_view_size-2], "\r\n") != 0)
	{
		printf("File (%ls) is unix\n", main_file_name);
		const auto size = file_view_size - 1;
		memcpy(out_buffer, file_view.content, size);
		return size;
	}
	else if (strcmp(&file_view.content[file_view_size-1], "\n") != 0)
	{
		printf("File (%ls) may be corrupted\n", main_file_name);
		return 0;
	}

	printf("File (%ls) is dos\n", main_file_name);

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

struct ArgEntry
{
	const char* const short_name;
	const char* const long_name;
	const char* const description;
	const char* default_value;
	const char* value;
	bool already_filled;
};

void print_usage(const ArgEntry* const entries, const int entries_count)
{
	printf("Usage:\n");
	printf("\tparsa");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (strcmp(entry.long_name, "help") == 0) continue;

		if (entry.short_name) // Is option
		{
			if (entry.long_name)
				printf(" [--%s", entry.long_name);
			else
				printf(" [-%s", entry.short_name);

			if (entry.default_value)
				printf(" %s", entry.long_name);

			printf("]");
		}
		else // Is argument
		{
			printf(" <%s>", entry.long_name);
		}
	}
	printf("\n\n");

	printf("Options:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (!entry.short_name) continue;

		printf("\t-%s", entry.short_name);
		if (entry.default_value)
			printf(" %s", entry.long_name);

		if (entry.long_name)
		{
			printf(", --%s", entry.long_name);
			if (entry.default_value)
				printf(" %s", entry.long_name);
		}

		if (entry.description)
			printf(" (%s)", entry.description);

		printf("\n");
	}
	printf("\n");


	printf("Arguments:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.short_name) continue;

		printf("\t");

		printf("%s", entry.long_name);
		if (entry.description)
			printf(" (%s)", entry.description);

		printf("\n");
	}
}

int main(int argc, const char** argv)
{
	const wchar_t main_file_name[] = L"main.js";

	ArgEntry arg_entries[] = {
		{"h", "help", "Display this message"},
		{"o", "out", "Output directory/file", "."},
		{0, "path...", "Directory or file(s) to preprocess", "*"},
	};

	if (argc <= 1)
	{
		printf("Specify directory or file(s) to preprocess\n");
		return 1;
	}

	ArgEntry* entry_matched = 0;
	for (int i = 1; i < argc; i++)
	{
		const auto arg_is_option = *argv[i] == '-';

		if (!arg_is_option && entry_matched)
		{
			entry_matched->value = argv[i];
			entry_matched->already_filled = 1;
			entry_matched = 0;
			continue;
		}

		for (int j = 0; j < ARR_COUNT(arg_entries); j++)
		{
			auto& entry = arg_entries[j];
			if (arg_is_option)
			{
				if (entry.short_name)
				{
					auto next_char = argv[i]+1;
					if (*next_char && strcmp(next_char, entry.short_name) == 0 ||
							*(next_char++) == '-' && *next_char && entry.long_name && strcmp(next_char, entry.long_name) == 0)
					{
						entry_matched = &entry;
						break;
					}
				}
			}
			else
			{
				if (!entry.already_filled && !entry.short_name)
				{
					entry.value = argv[i];
					entry.already_filled = 1;
					break;
				}
			}
		}

		if (arg_is_option && !entry_matched)
		{
			printf("No option %s found!\n", argv[i]);
			print_usage(arg_entries, ARR_COUNT(arg_entries));
			return 0;
		}

		if (entry_matched)
		{
			if (!entry_matched->default_value)
			{
				if (entry_matched->short_name && strcmp(entry_matched->short_name, "h") == 0)
				{
					print_usage(arg_entries, ARR_COUNT(arg_entries));
					return 0;
				}

				entry_matched->value = "";
				entry_matched->already_filled = 1;
				entry_matched = 0;
			}
		}
	}

	for (int j = 0; j < ARR_COUNT(arg_entries); j++)
	{
		auto& entry = arg_entries[j];
		if (entry.short_name && strcmp(entry.short_name, "h") == 0 && entry.value)
		{
			print_usage(arg_entries, ARR_COUNT(arg_entries));
			return 0;
		}
	}

	for (int i = 0; i < ARR_COUNT(arg_entries); i++)
	{
		printf("%s: %s\n", arg_entries[i].short_name, arg_entries[i].value);
	}

	const auto main_file_view = create_ro_file_view(main_file_name);
	if (!main_file_view.content)
		return 1;

	const auto main_file_view_size = get_file_size(main_file_view.handle);
	if (!main_file_view_size) {
		printf("Couldn't get file size of file (%ls)\n", main_file_name);
		return 1;
	}

	const auto main_file_buffer = (char*)VirtualAlloc(0, main_file_view_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!main_file_buffer)
	{
		fprintf(stderr, "Failed to allocate memory\n");
		return 1;
	}
	const auto main_file_buffer_size = convert_file_view_to_unix(main_file_buffer, main_file_view, main_file_view_size, main_file_name);

	IncludeStatement includes[64];
	uint includes_count = 0;

	size_t buffer_size = 0;

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

		const auto file_name_size = (uint)(statement_arg_end - statement_arg_start - 1); //@TODO: Better conversion for uint
		if (!file_name_size) {
			printf("Invalid file name for statement: #include\n");
			haystack = statement_arg_end;
			continue;
		}

		wchar_t file_name[64];
		const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, statement_arg_start+1, file_name_size, file_name, ARR_COUNT(file_name));

		file_name[bytes_written] = 0;

		const auto file_view = create_ro_file_view(file_name);
		if (!file_view.content)
			break;

		const auto file_view_size = get_file_size(file_view.handle);
		if (!file_view_size) {
			printf("Couldn't get file size of file (%ls)\n", file_name);
			break;
		}

		const auto file_buffer = (char*)VirtualAlloc(0, file_view_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!file_buffer)
		{
			fprintf(stderr, "Failed to allocate memory\n");
			break;
		}

		include.file = file_buffer;

		const auto file_buffer_size = convert_file_view_to_unix(file_buffer, file_view, file_view_size, file_name);
		CloseHandle(file_view.handle);

		include.file_size = file_buffer_size;

		buffer_size += (statement_start - prev_statement_arg_end) + file_buffer_size;
		prev_statement_arg_end = statement_arg_end + 1;

		includes_count++;
		haystack = statement_arg_end;
	}

	buffer_size += main_file_buffer_size - (prev_statement_arg_end - main_file_buffer) + 1;

	const auto buffer = (char*)VirtualAlloc(0, buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buffer)
	{
		fprintf(stderr, "Failed to allocate memory\n");
		return 1;
	}

	auto buffer_end = buffer;
	auto main_file_buffer_cursor = main_file_buffer;
	for (uint i = 0; i < includes_count; i++)
	{
		const auto& include = includes[i];
		const auto size = include.start_location - main_file_buffer_cursor;
		memcpy(buffer_end, main_file_buffer_cursor, size);
		main_file_buffer_cursor = include.end_location + 1;
		buffer_end += size;

		memcpy(buffer_end, include.file, include.file_size);
		buffer_end += include.file_size;
	}

	const auto size = main_file_buffer_size - (main_file_buffer_cursor - main_file_buffer);
	memcpy(buffer_end, main_file_buffer_cursor, size);
	buffer_end += size;
	*buffer_end = '\n';

	wchar_t out_file_path[64];
	{
		wchar_t* no_ext = 0;
		wchar_t* ext = 0;
		wchar_t* pt;

		wchar_t file_name[ARR_COUNT(main_file_name)];
		wcsncpy(file_name, main_file_name, ARR_COUNT(main_file_name));
		no_ext = wcstok(file_name, L".", &pt);

		if (no_ext)
			ext = wcstok(0, L".", &pt);

		wcsncpy(out_file_path, L"gen/", ARR_COUNT(out_file_path));
		wcsncat(out_file_path, no_ext, ARR_COUNT(out_file_path));
		wcsncat(out_file_path, L".", ARR_COUNT(out_file_path));
		wcsncat(out_file_path, ext, ARR_COUNT(out_file_path));
	}

	const auto out_file_handle = create_wo_file(out_file_path);
	if (!out_file_handle) return 1;

	auto file_size_to_write = buffer_size;
	while (true)
	{
		const auto max_dword_value = std::numeric_limits<DWORD>::max();
		const auto to_write = (DWORD)(file_size_to_write > max_dword_value ? max_dword_value : file_size_to_write);

		DWORD bytes_written;
		const auto ret = WriteFile(out_file_handle, buffer, to_write, &bytes_written, 0);
		if (!ret)
		{
			fprintf(stderr, "Failed to write to file (%ls)\n", out_file_path);
			return 1;
		}
		if (!bytes_written)
		{
			printf("Successfuly wrote to file (%ls)\n", out_file_path);
			break;
		}

		file_size_to_write -= bytes_written;
	}

	return 0;
}
