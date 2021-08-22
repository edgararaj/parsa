#include <stdio.h>
#include <stdint.h>
#include <limits>

typedef int64_t i64;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef UNICODE
#include <windows.h>

int main()
{
	const auto file_handle = CreateFileW(L"test/test.iso", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == file_handle)
	{
		fprintf(stderr, "Failed to open file\n");
		const auto error = GetLastError();
		if (ERROR_FILE_NOT_FOUND == error)
			fprintf(stderr, "Reason: File doesn't exist\n");
		else
			fprintf(stderr, "Reason: File already open\n");

		return 1;
	}

	LARGE_INTEGER large_int;
	const auto ret = GetFileSizeEx(file_handle, &large_int);
	if (!ret)
	{
		fprintf(stderr, "Failed to get file size\n");
		return 1;
	}

	const auto file_size = large_int.QuadPart;

	printf("File size is: %lld\n\n", file_size);

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

	return 0;
}
