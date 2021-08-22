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

struct include_statement {
	char* location;
	u64 file_size;
};

HANDLE open_ro_file(const wchar_t* file_name)
{
	HANDLE result = 0;

	const auto file_handle = CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == file_handle)
	{
		fprintf(stderr, "Failed to open file\n");
		const auto error = GetLastError();
		if (ERROR_FILE_NOT_FOUND == error)
			fprintf(stderr, "Reason: File doesn't exist\n");
		else
			fprintf(stderr, "Reason: File already open\n");

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
	{
		fprintf(stderr, "Failed to get file size\n");
		return result;
	}

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

int main()
{
	const auto main_file_view = create_ro_file_view(L"main.js");

	include_statement includes[64];
	uint includes_count = 0;

	auto haystack = main_file_view;
	while (true)
	{
		const auto statement_start = strstr(haystack, "#include \"");
		if (!statement_start) {
			printf("Couldn't find opening \" for statement: #include\n");
			break;
		}
		const auto statement_arg_start = strchr(statement_start, '\"');
		if (!statement_arg_start) {
			printf("Couldn't find closing \" for statement: #include\n");
			break;
		}

		uint filename_size = 0;
		auto statement_arg_end = statement_arg_start + 1;
		for (; statement_arg_end && *statement_arg_end != '\"'; statement_arg_end++) filename_size++;
		if (!filename_size) break;

		auto& include = includes[includes_count];
		include.location = statement_start;

		{
			wchar_t file_name[64];
			memcpy(file_name, statement_arg_start + 1, filename_size);

			const auto file = open_ro_file(file_name);
			if (!file) {
				printf("Couldn't find file specified: %ls\n", file_name);
				break;
			}

			const auto file_size = get_file_size(file);
			CloseHandle(file);
			if (!file_size) {
				printf("Couldn't get file size of file specified: %ls\n", file_name);
				break;
			}

			include.file_size = file_size;
		}

		includes_count++;
		haystack = statement_arg_end;
	}

	for (uint i = 0; i < includes_count; i++)
	{
		const auto& include = includes[i];
	}

#if 0
	const auto buffer = VirtualAlloc(0, file_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!buffer)
	{
		fprintf(stderr, "Failed to allocate memory\n");
		return 1;
	}

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
