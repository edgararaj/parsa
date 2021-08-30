#pragma once

struct FileBuffer {
	char* content;
	u64 size;
};

struct FileView {
	const HANDLE handle;
	const FileBuffer buffer;
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

const FileView create_ro_file_view(const wchar_t* file_path)
{
	FileBuffer buffer = {};
	FileView result = {.buffer = buffer};

	const auto file_handle = open_ro_file(file_path);
	if (!file_handle) return result;

	const auto file_map = CreateFileMappingW(file_handle, 0, PAGE_READONLY, 0, 0, 0);
	if (!file_map)
	{
		printf("Failed to create file mapping\n");
		return result;
	}

	const auto file_view = (char*)MapViewOfFile(file_map, FILE_MAP_READ, 0, 0, 0);
	if (!file_view)
	{
		printf("Failed to create file view\n");
		return result;
	}

	const auto file_view_size = get_file_size(file_handle);
	if (!file_view_size) {
		wprintf(L"Couldn't get file size of file (%ls)\n", file_path);
		return result;
	}

	return {.handle = file_handle, .buffer = {.content = file_view, .size = file_view_size}};
}

u64 read_file_view_to_unix_buffer(char* out_buffer, const FileView file_view, const wchar_t* file_path)
{
	if (strcmp(&file_view.buffer.content[file_view.buffer.size-2], "\r\n") != 0)
	{
		wprintf(L"File (%ls) is unix\n", file_path);
		const auto size = file_view.buffer.size - 1;
		memcpy(out_buffer, file_view.buffer.content, size);
		return size;
	}
	else if (strcmp(&file_view.buffer.content[file_view.buffer.size-1], "\n") != 0)
	{
		wprintf(L"File (%ls) may be corrupted\n", file_path);
		return 0;
	}

	wprintf(L"File (%ls) is dos\n", file_path);

	u64 result = file_view.buffer.size;

	auto out_buffer_end = out_buffer;
	auto haystack = file_view.buffer.content;
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

const FileBuffer read_file_to_unix_buffer(const wchar_t* file_path)
{
	const auto file_view = create_ro_file_view(file_path);
	if (!file_view.buffer.content)
		return {};

	const auto file_buffer = (char*)VirtualAlloc(0, file_view.buffer.size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!file_buffer)
	{
		printf("Failed to allocate memory\n");
		return {};
	}

	const auto file_buffer_size = read_file_view_to_unix_buffer(file_buffer, file_view, file_path);

	CloseHandle(file_view.handle);
	UnmapViewOfFile(file_view.buffer.content);

	return {.content = file_buffer, .size = file_buffer_size};
}
