#include "TestHelpers.h"

#include <limits>

#include <manifest/types/Decimal.h>

using namespace Manifest::Core;

TEST_CASE("Decimal parses authored prices exactly and refuses lossy input")
{
	CHECK(Decimal::TryParse("5.6312") == 5.6312_dec);
	CHECK(Decimal::TryParse("0.05")->Raw() == 50'000);
	CHECK(Decimal::TryParse("150000")->Raw() == 150'000'000'000);
	CHECK(Decimal::TryParse("0.0000010") == Decimal::FromRaw(1));
	for (const auto text :
	     {"", ".", "-", "nan", "inf", "1e3", "1junk", "0.0000001", "9223372036854.775808",
	      "999999999999999999999999999999"})
	{
		CAPTURE(text);
		CHECK_FALSE(Decimal::TryParse(text).has_value());
	}
	CHECK(Decimal::TryParse("9223372036854.775807") == Decimal::Max());
	CHECK(Decimal::TryParse("-9223372036854.775808") == Decimal::Min());
	CHECK(Decimal::Min().ToString() == "-9223372036854.775808");
}

TEST_CASE("Decimal price arithmetic checks overflow and truncates scalar division")
{
	CHECK((5.631201_dec / 2).Raw() == 2'815'600);
	CHECK_FALSE(Decimal::TryAdd(Decimal::Max(), Decimal::FromRaw(1)));
	CHECK_FALSE(Decimal::TrySubtract(Decimal::Min(), Decimal::FromRaw(1)));
	CHECK_FALSE(Decimal::TryDivide(1, 0));
	CHECK_FALSE(Decimal::TryDivide(Decimal::Min(), -1));
	CHECK(Decimal::TryDivide(Decimal::Max(), 1) == Decimal::Max());
}

TEST_CASE("Computed prices round at an explicit floating point boundary")
{
	CHECK(Decimal::TryFromDouble(0.05) == 0.05_dec);
	CHECK(Decimal::TryFromDouble(0.00000049) == 0);
	CHECK(Decimal::TryFromDouble(0.00000051) == Decimal::FromRaw(1));
	CHECK(Decimal::TryFromDouble(0.0078125) == 0.007813_dec);
	CHECK(Decimal::TryFromDouble(-0.0078125) == -0.007813_dec);
	CHECK_FALSE(Decimal::TryFromDouble(std::numeric_limits<double>::quiet_NaN()));
	CHECK_FALSE(Decimal::TryFromDouble(std::numeric_limits<double>::infinity()));
	CHECK_FALSE(Decimal::TryFromDouble(1e20));
	CHECK_FALSE(Decimal::TryFromDouble(-1e20));
}
