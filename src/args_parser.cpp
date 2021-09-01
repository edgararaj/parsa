#pragma once

struct ArgEntry
{
	const wchar_t* const short_name;
	const wchar_t* const long_name;
	const char* const description;
	const wchar_t* value;
	int num_values;
	bool already_filled;
};

void print_usage(const ArgEntry* entries, const int entries_count)
{
	printf("Usage:\n");
	printf("\t\x1b[1mparsa\x1b[0m");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (wcscmp(entry.long_name, L"help") == 0) continue;

		if (entry.short_name) // Is option
		{
			if (entry.short_name)
				wprintf(L" [-%ls", entry.short_name);
			else
				wprintf(L" [--%ls", entry.long_name);

			if (entry.num_values) wprintf(L" %ls", entry.long_name);

			printf("]");
		}
		else // Is argument
		{
			wprintf(L" <%ls", entry.long_name);
			if (entry.num_values) printf("...");
			printf(">");
		}
	}
	printf("\n\n");

	printf("Options:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (!entry.short_name) continue;

		wprintf(L"\t\x1b[1m-%ls\x1b[0m", entry.short_name);
		if (entry.num_values) wprintf(L" %ls", entry.long_name);

		if (entry.long_name)
		{
			wprintf(L", \x1b[1m--%ls\x1b[0m", entry.long_name);
			if (entry.num_values) wprintf(L" %ls", entry.long_name);
		}

		if (entry.description)
		{
			if (!entry.num_values) printf("\t");
			printf("\t(%s)", entry.description);
		}

		printf("\n");
	}
	printf("\n");


	printf("Arguments:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.short_name) continue;

		printf("\t");

		wprintf(L"\x1b[1m%ls", entry.long_name);
		if (entry.num_values) printf("...");
		printf("\x1b[0m");
		if (entry.description)
		{
			if (!entry.num_values) printf("\t");
			printf("\t(%s)", entry.description);
		}

		printf("\n");
	}
}

const wchar_t* get_arg_entry_value(const ArgEntry* entries, const int entries_count, const wchar_t* entry_name) {
	for (int i = 0; i < entries_count; i++)
	{
		auto& entry = entries[i];
		if (entry.short_name && wcscmp(entry.short_name, entry_name) == 0 ||
			entry.long_name && wcscmp(entry.long_name, entry_name) == 0)
		{
			return entry.value;
		}
	}

	return 0;
}

bool parse_args(ArgEntry* arg_entries, const int arg_entries_count, const int argc, const wchar_t** argv)
{
	ArgEntry* entry_matched = 0;
	for (int i = 1; i < argc; i++)
	{
		const auto arg_is_option = *argv[i] == L'-';

		if (!arg_is_option && entry_matched)
		{
			entry_matched->value = argv[i];
			entry_matched->already_filled = 1;
			entry_matched = 0;
			continue;
		}

		for (int j = 0; j < arg_entries_count; j++)
		{
			auto& entry = arg_entries[j];
			if (arg_is_option)
			{
				if (entry.short_name)
				{
					auto next_char = argv[i]+1;
					if (*next_char && wcscmp(next_char, entry.short_name) == 0 ||
							*(next_char++) == L'-' && *next_char && entry.long_name && wcscmp(next_char, entry.long_name) == 0)
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

		if (arg_is_option)
		{
			if (!entry_matched)
			{
				wprintf(L"No option \"%ls\" found!\n", argv[i]);
				print_usage(arg_entries, arg_entries_count);
				return 0;
			}
			else
			{
				if (!entry_matched->num_values)
				{
					if (entry_matched->short_name && wcscmp(entry_matched->short_name, L"h") == 0)
					{
						print_usage(arg_entries, arg_entries_count);
						return 0;
					}

					entry_matched->value = L"";
					entry_matched->already_filled = 1;
					entry_matched = 0;
				}
			}
		}
	}

	bool argument_missing = 0;
	for (int i = 0; i < arg_entries_count; i++)
	{
		auto& entry = arg_entries[i];
		if (!entry.short_name && !entry.value)
		{
			argument_missing = 1;
			if (entry.description)
				printf("Please specify: %s!\n", entry.description);
		}
	}

	if (argument_missing) return 0;
	return 1;
}
