#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <limits>
#include <assert.h>

typedef int64_t i64;
typedef uint64_t u64;
typedef unsigned int uint;

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#undef UNICODE
#include <windows.h>

#define ARRCOUNT(x) (sizeof(x)/sizeof(x[0]))

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

struct FileView {
	char* content;
	HANDLE handle;
};

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

struct IncludeStatement {
	char* file;
	u64 file_size;
};

struct IfStatement {
	char* condition;
	u64 condition_size;
};

struct Statement {
	enum {
		If,
		Include
	};

	int type;

	u64 offset;
	u64 size;
	union {
		IncludeStatement include_statement;
		IfStatement if_statement;
	};
};

struct Scope {
	Statement statements[64];
	uint statements_count;

	u64 last_statement_end()
	{
		return statements[statements_count].offset + statements[statements_count].size;
	}
};

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

int main()
{
	const wchar_t main_file_name[] = L"main.js";

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

	Scope scopes[2] = {};
	uint scope_index = 0;

	const char* haystack = main_file_buffer;
	while (true)
	{
		const auto include_statement_start = strstr(haystack, "#include ");
		const auto if_statement_start = strstr(haystack, "#if ");

		const char* statement_start;
		const char* statement_end;
		int statement_type;
		if (include_statement_start > if_statement_start)
		{
			statement_start = if_statement_start;
			statement_type = Statement::If;

			auto statement_arg_start = strchr(statement_start, ' ');
			while (*(statement_arg_start+1) == ' ') statement_arg_start++;
			const auto next_char = *(statement_arg_start+1);
			if (next_char != 0 && next_char != '\n') statement_arg_start++;

			const auto statement_arg_end_space = strchr(statement_arg_start, ' ');
			const auto statement_arg_end_newline = strchr(statement_arg_start, '\n');

			const char* statement_arg_end;
			if (statement_arg_end_space < statement_arg_end_newline)
			{
				statement_arg_end = statement_arg_end_space;
			}
			else if (statement_arg_end_space > statement_arg_end_newline)
			{
				statement_arg_end = statement_arg_end_newline;
			}
			else
			{
				printf("Couldn't find closing <space> for arg: #if\n");
				break;
			}

			statement_end = statement_arg_end - 1;

			const auto arg_size = statement_end - statement_arg_start + 1;
			if (arg_size <= 0)
				break;

			if (strncmp(statement_arg_start, "0", arg_size) == 0)
			{
				const auto endif = "#endif";
				const auto endif_statement_start = strstr(statement_end, endif);
				const auto endif_statement_end = endif_statement_start + strlen(endif) - 1;
				if (!endif_statement_start | ( (*(endif_statement_end + 1) != ' ') && (*(endif_statement_end + 1) != '\n') && (*(endif_statement_end + 1) != 0))) {
					printf("Couldn't find closing #endif statement for conditional\n");
					break;
				}
				statement_end = endif_statement_end;
			}
		}
		else if (include_statement_start < if_statement_start)
		{
			statement_start = include_statement_start;
			statement_type = Statement::Include;

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

			statement_end = statement_arg_end;
		}
		else break;

		auto& scope = scopes[scope_index];
		auto& statement = scope.statements[scope.statements_count];
		statement.type = statement_type;
		statement.offset = (statement_start - main_file_buffer) - scope.last_statement_end();
		statement.size = (statement_end - main_file_buffer) - statement.offset;
		scope.statements_count++;

		//if (statement_type == Statement::If) scope_index++;

		haystack = statement_end;
	}
}
