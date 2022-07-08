struct TableColumn
{
	struct TableCell
	{
		int len;
		wchar_t string[256];
	};

	int max_len;
	TableCell cell[16];
};

void table_column_printf(TableColumn& column, int r, const wchar_t* string, ...)
{
	auto& cell = column.cell[r];

	va_list va;
	va_start(va, string);
	cell.len += vswprintf(cell.string + cell.len, COUNTOF(cell.string), string, va);
	va_end(va);

	if (cell.len > column.max_len)
		column.max_len = cell.len;
}

void draw_table(TableColumn* table_columns, int rows, int columns, int space)
{
	for (int r = 0; r < rows; r++)
	{
		for (int c = 0; c < columns; c++)
		{
			auto& column = table_columns[c];
			auto& cell = column.cell[r];
			nice_wprintf(cell.string);
			for (int i = 0; i < column.max_len - cell.len + space; i++) nice_wprintf(L" ");
		}
	}
}

void print_options_usage(const ArgEntry* entries, const int entries_count)
{
	TableColumn table_columns[2] = {};
	int table_row_index = 0;
	nice_wprintf(L"Options:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.value_max_count != 0 && !entry.short_name) continue;

		table_column_printf(table_columns[0], table_row_index, L"\t");
		if (entry.short_name)
		{
			table_column_printf(table_columns[0], table_row_index, L"\x1b[1m-%ls\x1b[0m", entry.short_name);
			if (entry.short_name && entry.value_max_count != 0)
			{
				if (entry.long_name)
					table_column_printf(table_columns[0], table_row_index, L" %ls", entry.long_name);
				else
					table_column_printf(table_columns[0], table_row_index, L" %ls", entry.short_name);
			}
		}

		if (entry.long_name)
		{
			if (entry.short_name)
				table_column_printf(table_columns[0], table_row_index, L", ");

			table_column_printf(table_columns[0], table_row_index, L"\x1b[1m--%ls\x1b[0m", entry.long_name);
			if (entry.short_name && entry.value_max_count != 0)
			{
				if (entry.long_name)
					table_column_printf(table_columns[0], table_row_index, L" %ls", entry.long_name);
				else
					table_column_printf(table_columns[0], table_row_index, L" %ls", entry.short_name);
			}
		}

		if (entry.description)
			table_column_printf(table_columns[1], table_row_index, L"(%ls)", entry.description);

		table_column_printf(table_columns[1], table_row_index, L"\n");
		table_row_index++;
	}
	draw_table(table_columns, table_row_index, 2, 5);
}

void print_arguments_usage(const ArgEntry* entries, const int entries_count)
{
	TableColumn table_columns[2] = {};
	int table_row_index = 0;
	nice_wprintf(L"Arguments:\n");
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.value_max_count == 0 || entry.short_name) continue;

		table_column_printf(table_columns[0], table_row_index, L"\t");

		auto multi_arg = entry.value_max_count == -1 ? L"..." : L"";
		table_column_printf(table_columns[0], table_row_index, L"\x1b[1m%ls%ls\x1b[0m", entry.long_name, multi_arg);
		if (entry.description)
			table_column_printf(table_columns[1], table_row_index, L"(%ls)", entry.description);

		table_column_printf(table_columns[1], table_row_index, L"\n");
		table_row_index++;
	}
	draw_table(table_columns, table_row_index, 2, 5);
}

void print_help(const ArgEntry* entries, const int entries_count, const wchar_t* name)
{
	nice_wprintf(L"Usage:\n");
	nice_wprintf(L"\t\x1b[1m%ls\x1b[0m", name);
	for (int i = 0; i < entries_count; i++)
	{
		const auto& entry = entries[i];
		if (entry.short_name && wcscmp(entry.short_name, L"h") == 0) continue;

		if (entry.short_name)
		{
			if (entry.short_name)
				nice_wprintf(L" [-%ls", entry.short_name);
			else
				nice_wprintf(L" [--%ls", entry.long_name);

			if (entry.value_max_count != 0)
			{
				if (entry.long_name)
					nice_wprintf(L" %ls", entry.long_name);
				else
					nice_wprintf(L" %ls", entry.short_name);
			}

			nice_wprintf(L"]");
		}
		else 
		{
			auto multi_arg = entry.value_max_count == -1 ? L"..." : L"";
			nice_wprintf(L" <%ls%ls>", entry.long_name, multi_arg);
		}
	}
	nice_wprintf(L"\n\n");

	print_options_usage(entries, entries_count);
	nice_wprintf(L"\n");

	print_arguments_usage(entries, entries_count);
}

