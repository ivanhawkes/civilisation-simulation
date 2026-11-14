#pragma once

#include <compare>
#include <concepts>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace Manifest::Core
{
// A price in millionths of a credit per authored item unit. Quantities, physics and effect
// stats retain floating-point representations; conversions at that boundary are explicit.
class Decimal
{
  public:
	static constexpr std::int64_t kScale = 1'000'000;

	constexpr Decimal() = default;
	template <std::integral T> constexpr Decimal(T whole)
	{
		const __int128_t raw = static_cast<__int128_t>(whole) * kScale;
		Require(Fits(raw));
		m_raw = static_cast<std::int64_t>(raw);
	}

	static constexpr Decimal FromRaw(std::int64_t raw)
	{
		Decimal result;
		result.m_raw = raw;
		return result;
	}
	static constexpr Decimal Max()
	{
		return FromRaw(std::numeric_limits<std::int64_t>::max());
	}
	static constexpr Decimal Min()
	{
		return FromRaw(std::numeric_limits<std::int64_t>::min());
	}
	constexpr std::int64_t Raw() const
	{
		return m_raw;
	}
	constexpr std::int64_t Whole() const
	{
		return m_raw / kScale;
	}
	double ToDouble() const
	{
		return static_cast<double>(m_raw) / static_cast<double>(kScale);
	}
	std::string ToString() const;
	// Computed prices cross from the model's double coefficients exactly once. Rejects
	// nonfinite/out-of-range values and rounds to the nearest microcredit, ties away from zero.
	static std::optional<Decimal> TryFromDouble(double value);

	// Exact decimal text only. Excess trailing zeros are harmless; excess nonzero precision
	// and exponents are refused rather than silently changing authored balance.
	static constexpr std::optional<Decimal> TryParse(std::string_view text)
	{
		if (text.empty())
			return std::nullopt;
		bool negative = false;
		std::size_t index = 0;
		if (text[index] == '-' || text[index] == '+')
		{
			negative = text[index] == '-';
			++index;
		}
		__int128_t raw = 0;
		std::int64_t place = kScale;
		bool fraction = false;
		bool digits = false;
		for (; index < text.size(); ++index)
		{
			const char character = text[index];
			if (character == '.' && !fraction)
			{
				fraction = true;
				continue;
			}
			if (character < '0' || character > '9')
				return std::nullopt;
			digits = true;
			const int digit = character - '0';
			if (!fraction)
			{
				raw = raw * 10 + static_cast<__int128_t>(digit) * kScale;
			}
			else
			{
				place /= 10;
				if (place == 0 && digit != 0)
					return std::nullopt;
				raw += digit * place;
			}
			if (raw >
			    static_cast<__int128_t>(std::numeric_limits<std::int64_t>::max()) + (negative ? 1 : 0))
				return std::nullopt;
		}
		if (!digits)
			return std::nullopt;
		return FromRaw(static_cast<std::int64_t>(negative ? -raw : raw));
	}

	static constexpr std::optional<Decimal> TryAdd(Decimal a, Decimal b)
	{
		return Narrow(static_cast<__int128_t>(a.m_raw) + b.m_raw);
	}
	static constexpr std::optional<Decimal> TrySubtract(Decimal a, Decimal b)
	{
		return Narrow(static_cast<__int128_t>(a.m_raw) - b.m_raw);
	}
	static constexpr std::optional<Decimal> TryDivide(Decimal value, std::int64_t divisor)
	{
		if (divisor == 0)
			return std::nullopt;
		return Narrow(static_cast<__int128_t>(value.m_raw) / divisor);
	}

	constexpr auto operator<=>(const Decimal &) const = default;
	friend constexpr Decimal operator+(Decimal a, Decimal b)
	{
		return Value(TryAdd(a, b));
	}
	friend constexpr Decimal operator-(Decimal a, Decimal b)
	{
		return Value(TrySubtract(a, b));
	}
	friend constexpr Decimal operator/(Decimal value, std::int64_t divisor)
	{
		return Value(TryDivide(value, divisor));
	}

	constexpr Decimal operator-() const
	{
		return Value(TrySubtract({}, *this));
	}
	constexpr Decimal &operator+=(Decimal other)
	{
		return *this = *this + other;
	}
	constexpr Decimal &operator-=(Decimal other)
	{
		return *this = *this - other;
	}

  private:
	static constexpr bool Fits(__int128_t raw)
	{
		return raw >= std::numeric_limits<std::int64_t>::min() &&
		       raw <= std::numeric_limits<std::int64_t>::max();
	}
	static constexpr std::optional<Decimal> Narrow(__int128_t raw)
	{
		if (!Fits(raw))
			return std::nullopt;
		return FromRaw(static_cast<std::int64_t>(raw));
	}
	static constexpr void Require(bool condition)
	{
		if (!condition)
			InvalidArithmetic();
	}
	static constexpr Decimal Value(std::optional<Decimal> result)
	{
		Require(result.has_value());
		return *result;
	}
	[[noreturn]] static void InvalidArithmetic();
	std::int64_t m_raw{0};
};

// The spelling is parsed as text, avoiding a compiler's binary floating-point literal.
template <char... Characters> consteval Decimal operator""_dec()
{
	constexpr char text[] = {Characters...};
	constexpr auto result = Decimal::TryParse(std::string_view{text, sizeof...(Characters)});
	static_assert(result.has_value(), "decimal literal does not fit the millionth-unit contract");
	return *result;
}

inline std::ostream &operator<<(std::ostream &stream, Decimal value)
{
	return stream << value.ToString();
}
} // namespace Manifest::Core

template <> struct std::formatter<Manifest::Core::Decimal> : std::formatter<std::string_view>
{
	auto format(Manifest::Core::Decimal value, std::format_context &context) const
	{
		const std::string text = value.ToString();
		return std::formatter<std::string_view>::format(text, context);
	}
};
