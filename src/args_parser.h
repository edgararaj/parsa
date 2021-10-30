#pragma once

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

