#include <manifest/types/Decimal.h>

#include <cmath>
#include <cstdlib>

#include <jettison/core/Assert.h>

namespace Manifest::Core
{
std::optional<Decimal> Decimal::TryFromDouble(double value)
{
	if (!std::isfinite(value))
		return std::nullopt;
	const long double raw = std::round(static_cast<long double>(value) * kScale);
	if (raw < static_cast<long double>(std::numeric_limits<std::int64_t>::min()) ||
	    raw > static_cast<long double>(std::numeric_limits<std::int64_t>::max()))
	{
		return std::nullopt;
	}
	return FromRaw(static_cast<std::int64_t>(raw));
}

void Decimal::InvalidArithmetic()
{
	// Recoverable boundary errors use Try* before mutating state. Reaching this is an
	// invariant failure; neither signed overflow nor a release-only wrap is permitted.
	JETTISON_VERIFY(false);
	std::abort();
}

std::string Decimal::ToString() const
{
	const auto magnitude =
	    m_raw < 0 ? static_cast<std::uint64_t>(-(m_raw + 1)) + 1 : static_cast<std::uint64_t>(m_raw);
	std::string result = (m_raw < 0 ? "-" : "") + std::to_string(magnitude / kScale);
	const auto fraction = magnitude % kScale;
	if (fraction != 0)
	{
		std::string digits = std::to_string(fraction + kScale).substr(1);
		while (digits.back() == '0')
			digits.pop_back();
		result += "." + digits;
	}
	return result;
}
} // namespace Manifest::Core
