#pragma once

#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace Manifest::Test
{
template <typename T> double Number(const T &value)
{
	if constexpr (requires { value.ToDouble(); })
	{
		return value.ToDouble();
	}
	else
	{
		return static_cast<double>(value);
	}
}

class TempFile
{
  public:
	explicit TempFile(const std::string &contents, const std::string &suffix = ".yaml")
	{
		static std::random_device random;
		const unsigned identity = random();

		m_path = std::filesystem::temp_directory_path() /
		         ("manifest-test-" + std::to_string(identity) + suffix);

		std::ofstream out(m_path);
		out << contents;
		out.close();
	}

	TempFile(const TempFile &) = delete;
	TempFile &operator=(const TempFile &) = delete;

	~TempFile()
	{
		std::error_code ignored;
		std::filesystem::remove(m_path, ignored);
	}

	std::string Path() const
	{
		return m_path.string();
	}

  private:
	std::filesystem::path m_path;
};
} // namespace Manifest::Test

// Keep the existing suites' absolute tolerances and substring diagnostics.
#define MANIFEST_CHECK_NEAR(actual, expected, tolerance)                                               \
	CHECK(                                                                                             \
	    std::abs(Manifest::Test::Number(actual) - Manifest::Test::Number(expected)) <=                 \
	    Manifest::Test::Number(tolerance))
#define MANIFEST_CHECK_CONTAINS(haystack, needle)                                                      \
	CHECK(std::string(haystack).find(needle) != std::string::npos)
