struct ArgEntry
{
	const wchar_t* const short_name;
	const wchar_t* const long_name;
	const wchar_t* const description;
	enum class Type {
		Flag, MultiArg, Option
	};
	const Type type;
	const wchar_t* value;

	// RESERVED
	int value_count;

	bool already_filled() const
	{
		return value_count > 1 && type == Type::Option;
	}
};

void print_usage(const ArgEntry* entries, const int entries_count)
{
	nice_wprintf(g_conout, L"Usage:\n");
	nice_wprintf(g_conout, L"\t\x1b[1mparsa\x1b[0m");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.short_name && wcscmp(entry.short_name, L"h") == 0) continue;

		if (entry.type == ArgEntry::Type::Option || entry.type == ArgEntry::Type::Flag)
		{
			if (entry.short_name)
				nice_wprintf(g_conout, L" [-%ls", entry.short_name);
			else
				nice_wprintf(g_conout, L" [--%ls", entry.long_name);

			if (entry.type == ArgEntry::Type::Option)
			{
				if (entry.long_name)
					nice_wprintf(g_conout, L" %ls", entry.long_name);
				else
					nice_wprintf(g_conout, L" %ls", entry.short_name);
			}

			nice_wprintf(g_conout, L"]");
		}
		else // Is multi argument
			nice_wprintf(g_conout, L" <%ls...>", entry.long_name);
	}
	nice_wprintf(g_conout, L"\n\n");

	wchar_t terminal_table[64][256];
	nice_wprintf(g_conout, L"Options:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.type != ArgEntry::Type::Option && entry.type != ArgEntry::Type::Flag) continue;

		nice_wprintf(g_conout, L"\t");
		//swprintf(terminal_table[0], ARR_COUNT(terminal_table[0]), L"\t");
		if (entry.short_name)
		{
			nice_wprintf(g_conout, L"\x1b[1m-%ls\x1b[0m", entry.short_name);
			if (entry.type == ArgEntry::Type::Option)
			{
				if (entry.long_name)
					nice_wprintf(g_conout, L" %ls", entry.long_name);
				else
					nice_wprintf(g_conout, L" %ls", entry.short_name);
			}
		}

		if (entry.long_name)
		{
			if (entry.short_name)
				nice_wprintf(g_conout, L", ");

			nice_wprintf(g_conout, L"\x1b[1m--%ls\x1b[0m", entry.long_name);
			if (entry.type == ArgEntry::Type::Option)
			{
				if (entry.long_name)
					nice_wprintf(g_conout, L" %ls", entry.long_name);
				else
					nice_wprintf(g_conout, L" %ls", entry.short_name);
			}
		}

		if (entry.description)
			nice_wprintf(g_conout, L"\t(%ls)", entry.description);

		nice_wprintf(g_conout, L"\n");
	}
	nice_wprintf(g_conout, L"\n");

	nice_wprintf(g_conout, L"Arguments:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.type != ArgEntry::Type::MultiArg) continue;

		nice_wprintf(g_conout, L"\t");

		nice_wprintf(g_conout, L"\x1b[1m%ls...\x1b[0m", entry.long_name);
		if (entry.description)
			nice_wprintf(g_conout, L"\t(%ls)", entry.description);

		nice_wprintf(g_conout, L"\n");
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

enum class ParseArgsResult {
	Success, Error, Help
};

ParseArgsResult parse_args(ArgEntry* arg_entries, const int arg_entries_count, const int argc, const wchar_t** argv)
{
	ArgEntry* entry_matched = 0;
	const wchar_t* prev_argv = 0;
	for (int i = 1; i < argc; i++)
	{
		const auto arg_is_option = *argv[i] == L'-';

		if (arg_is_option && entry_matched && entry_matched->type == ArgEntry::Type::Option)
			nice_wprintf(g_conout, L"Option \"%ls\" expected an argument!\n", prev_argv);

		if (arg_is_option)
			prev_argv = argv[i];

		if (arg_is_option || !entry_matched)
		{
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
					if (!entry.already_filled() && !entry.short_name)
					{
						entry_matched = &entry;
						break;
					}
				}
			}
		}

		if (!arg_is_option && entry_matched)
		{
			if (!entry_matched->value_count)
				entry_matched->value = argv[i];
			entry_matched->value_count++;

			if (entry_matched->already_filled())
				entry_matched = 0;

			continue;
		}

		if (arg_is_option)
		{
			if (!entry_matched)
			{
				nice_wprintf(g_conout, L"No option \"%ls\" found!\n", argv[i]);
				print_usage(arg_entries, arg_entries_count);
				return ParseArgsResult::Error;
			}
			else if (entry_matched->type == ArgEntry::Type::Flag)
			{
				if (entry_matched->short_name && wcscmp(entry_matched->short_name, L"h") == 0)
				{
					print_usage(arg_entries, arg_entries_count);
					return ParseArgsResult::Help;
				}

				entry_matched->value = L"";
				entry_matched = 0;
			}
		}
	}

	if (entry_matched && entry_matched->type == ArgEntry::Type::Option)
		nice_wprintf(g_conout, L"Option \"%ls\" expected an argument!\n", prev_argv);

	bool argument_missing = 0;
	for (int i = 0; i < arg_entries_count; i++)
	{
		auto& entry = arg_entries[i];
		if (!entry.short_name && !entry.value)
		{
			argument_missing = 1;
			if (entry.description)
				nice_wprintf(g_conout, L"Please specify: %ls!\n", entry.description);
		}
	}

	if (argument_missing) return ParseArgsResult::Error;
	return ParseArgsResult::Success;
}
