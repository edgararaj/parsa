template<typename T>
class Optional {
public:
	Optional() = default;

	Optional(const T& value)
		: m_value(value)
		, m_has_value(true)
	{
	}

	const T& value() const
	{
		assert(m_has_value);
		return m_value;
	}

	bool has_value() const
	{
		return m_has_value;
	}

private:
	T m_value;
	bool m_has_value = false;
};
