#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits>

typedef int64_t i64;
typedef uint64_t u64;
typedef unsigned int uint;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef UNICODE
#include <windows.h>

#define ARRCOUNT(x) (sizeof(x)/sizeof(x[0]))

struct include_statement {
	char* start_location;
	char* end_location;
	HANDLE file_handle;
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

char* create_ro_file_view(const wchar_t* file_name)
{
	char* result = 0;

	const auto file_handle = open_ro_file(file_name);
	if (!file_handle) return result;

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

	result = file_view;

	return result;
}

void strip_file_ext(char* out, char* file_name)
{
	char* chr;
	for (chr = file_name; *chr != '.'; chr++) {};

	memcpy(out, file_name, chr - file_name);
}

int main()
{
	wchar_t main_file_name[] = L"main.js";
	const auto main_file_view = create_ro_file_view(main_file_name);

	include_statement includes[64];
	uint includes_count = 0;

	auto haystack = main_file_view;
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

		{
			//int unicode_test = IS_TEXT_UNICODE_NOT_UNICODE_MASK;
			//const auto ret3 = IsTextUnicode(main_file_view, (int)strlen(main_file_view), &unicode_test); //@TODO: Better check for (int) conversion
			//if (!ret3)
			//{
				//printf("File specified (for #include) is not unicode\n");
				//haystack = statement_arg_end;
				//continue;
			//}

			const auto file_name_size = (uint)(statement_arg_end - statement_arg_start - 1); //@TODO: Better conversion for uint
			if (!file_name_size) {
				printf("Invalid file name for statement: #include\n");
				haystack = statement_arg_end;
				continue;
			}

			wchar_t file_name2[64];
			const auto bytes_written = MultiByteToWideChar(CP_UTF8, 0, statement_arg_start+1, file_name_size, file_name2, ARRCOUNT(file_name2));

			if (!bytes_written)
			{
				printf("Failed to interpret file name on statement: #include\n");
				haystack = statement_arg_end;
				continue;
			}

			file_name2[bytes_written] = 0;
			const auto file = open_ro_file(file_name2);
			if (!file) break;

			include.file_handle = file;

			const auto file_size = get_file_size(file);
			if (!file_size) {
				printf("Couldn't get file size of file (%ls)\n", file_name2);
				break;
			}

			include.file_size = file_size;
		}

		includes_count++;
		haystack = statement_arg_end;
	}

	auto total_buffer_size = strlen(main_file_view);
	for (uint i = 0; i < includes_count; i++)
	{
		const auto& include = includes[i];
		total_buffer_size += include.file_size;
	}

	const auto buffer = (char*)VirtualAlloc(0, total_buffer_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buffer)
	{
		fprintf(stderr, "Failed to allocate memory\n");
		return 1;
	}

	char* buffer_end = buffer;
	auto main_file_view_cursor = main_file_view;
	for (uint i = 0; i < includes_count; i++)
	{
		const auto& include = includes[i];
		const auto size = include.start_location - main_file_view_cursor;
		memcpy(buffer_end, main_file_view_cursor, size);
		main_file_view_cursor = include.end_location + 1;
		buffer_end += size;

		auto file_size_to_read = include.file_size;
		while (true)
		{
			const auto max_dword_value = std::numeric_limits<DWORD>::max();
			const auto to_read = (DWORD)(file_size_to_read > max_dword_value ? max_dword_value : file_size_to_read);

			DWORD bytes_read;
			const auto ret2 = ReadFile(include.file_handle, buffer_end, to_read, &bytes_read, 0);
			if (!ret2)
			{
				fprintf(stderr, "Failed to read from file\n");
				return 1;
			}
			if (!bytes_read)
			{
				printf("Successfuly read from file\n");
				break;
			}

			buffer_end += bytes_read - 1;
			file_size_to_read -= bytes_read;
		}
	}

	memcpy(buffer_end, main_file_view_cursor, strlen(main_file_view) - (main_file_view_cursor - main_file_view));

	wchar_t* no_ext = 0;
	wchar_t* ext = 0;
	wchar_t* pt;

	no_ext = wcstok(main_file_name, L".", &pt);

	if (no_ext)
		ext = wcstok(0, L".", &pt);

	wchar_t out_file_name[64];
	wcsncpy(out_file_name, no_ext, ARRCOUNT(out_file_name));
	wcsncat(out_file_name, L".gen.", ARRCOUNT(out_file_name));
	wcsncat(out_file_name, ext, ARRCOUNT(out_file_name));

	const auto out_file_handle = create_wo_file(out_file_name);
	if (!out_file_handle) return 1;

	auto file_size_to_write = total_buffer_size;
	while (true)
	{
		const auto max_dword_value = std::numeric_limits<DWORD>::max();
		const auto to_write = (DWORD)(file_size_to_write > max_dword_value ? max_dword_value : file_size_to_write);

		DWORD bytes_written;
		const auto ret2 = WriteFile(out_file_handle, buffer, to_write, &bytes_written, 0);
		if (!ret2)
		{
			fprintf(stderr, "Failed to write to file (%ls)\n", out_file_name);
			return 1;
		}
		if (!bytes_written)
		{
			printf("Successfuly wrote to file (%ls)\n", out_file_name);
			break;
		}

		file_size_to_write -= bytes_written;
	}

#if 0
	const auto max_dword_value = std::numeric_limits<DWORD>::max();
	const auto max_longlong_value = std::numeric_limits<LONGLONG>::max();
	printf("Max DWORD value: %lu\n", max_dword_value);

	//const auto num_loops = (int)ceilf((float)file_size / (float)max_dword_value);
	//printf("Number of loops: %d\n", num_loops);

	auto file_size_to_read = file_size;
	while (true)
	{
		const auto file_size_to_buff = (DWORD)(file_size_to_read > max_dword_value ? max_dword_value : file_size_to_read);
		DWORD bytes_read;
		const auto ret2 = ReadFile(file_handle, buffer, file_size_to_buff, &bytes_read, 0);
		if (!ret2)
		{
			fprintf(stderr, "Failed to read from file\n");
			return 1;
		}
		if (!bytes_read)
		{
			printf("Successfuly read from file\n");
			break;
		}

		file_size_to_read -= bytes_read;
	}
#endif

	return 0;
}
