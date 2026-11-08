#pragma once

// A very small test harness: the core has no external test dependency, and every
// suite is one executable that ctest runs. Each TEST_CASE runs in turn, checks are
// counted, and a non-zero exit code means at least one check failed.

#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace civtest
{
inline int &FailureCount()
{
	static int count = 0;

	return count;
}

inline int &CheckCount()
{
	static int count = 0;

	return count;
}

struct Case
{
	std::string name;
	std::function<void()> body;
};

inline std::vector<Case> &Suite()
{
	static std::vector<Case> suite;

	return suite;
}

struct Registrar
{
	Registrar(std::string name, std::function<void()> body)
	{
		Suite().push_back({std::move(name), std::move(body)});
	}
};

template <typename T, typename = void> struct IsStreamable : std::false_type
{
};

template <typename T>
struct IsStreamable<
    T, std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
    : std::true_type
{
};

template <typename T> std::string ToString(const T &value)
{
	if constexpr (IsStreamable<T>::value)
	{
		std::ostringstream out;
		out << value;

		return out.str();
	}
	else
	{
		return "<value>";
	}
}

inline void Fail(const char *file, int line, const std::string &message)
{
	++FailureCount();

	std::cout << "    FAIL " << file << ":" << line << ": " << message << "\n";
}

// One ordered comparison, reported the way CHECK_EQ reports a value: the caller reads
// both operands once, and the message carries both. The four order checks are the
// checks where the order is the point - the order id that counts up, the book that
// sorts price then time, the budget a fill may not spend past.
template <typename T, typename U>
void Compare(
    const char *file, int line, const std::string &expression, const T &actual, const U &limit, bool ok)
{
	++CheckCount();

	if (!ok)
	{
		Fail(file, line, expression + " (" + ToString(actual) + " vs " + ToString(limit) + ")");
	}
}

// Runs every registered case and reports. Returns the process exit code.
inline int Run(const char *suiteName)
{
	std::cout << suiteName << ": " << Suite().size() << " cases\n";

	for (const auto &test : Suite())
	{
		const int before = FailureCount();

		test.body();

		std::cout << (FailureCount() == before ? "  ok   " : "  FAIL ") << test.name << "\n";
	}

	std::cout << CheckCount() << " checks, " << FailureCount() << " failures\n";

	return FailureCount() == 0 ? 0 : 1;
}

// A file that writes its contents on construction and removes itself afterwards, so a
// suite can feed the loaders real documents without the data build step.
class TempFile
{
  public:
	explicit TempFile(const std::string &contents, const std::string &suffix = ".yaml")
	{
		static std::random_device random;
		const unsigned identity = random();

		path = std::filesystem::temp_directory_path() /
		       ("civsim-test-" + std::to_string(identity) + suffix);

		std::ofstream out(path);
		out << contents;
		out.close();
	}

	TempFile(const TempFile &) = delete;
	TempFile &operator=(const TempFile &) = delete;

	~TempFile()
	{
		std::error_code ignored;
		std::filesystem::remove(path, ignored);
	}

	std::string Path() const
	{
		return path.string();
	}

  private:
	std::filesystem::path path;
};
} // namespace civtest


#define TEST_CASE(NAME)                                                                                \
	static void NAME();                                                                                \
	static ::civtest::Registrar registrar_##NAME(#NAME, NAME);                                         \
	static void NAME()

#define CHECK(CONDITION)                                                                               \
	do                                                                                                 \
	{                                                                                                  \
		++::civtest::CheckCount();                                                                     \
		if (!(CONDITION))                                                                              \
		{                                                                                              \
			::civtest::Fail(__FILE__, __LINE__, #CONDITION);                                           \
		}                                                                                              \
	} while (false)

#define CHECK_EQ(ACTUAL, EXPECTED)                                                                     \
	do                                                                                                 \
	{                                                                                                  \
		++::civtest::CheckCount();                                                                     \
		const auto civtest_actual = (ACTUAL);                                                          \
		const auto civtest_expected = (EXPECTED);                                                      \
		if (!(civtest_actual == civtest_expected))                                                     \
		{                                                                                              \
			::civtest::Fail(                                                                           \
			    __FILE__, __LINE__,                                                                    \
			    std::string(#ACTUAL) + " == " + #EXPECTED + " (" +                                     \
			        ::civtest::ToString(civtest_actual) + " vs " +                                     \
			        ::civtest::ToString(civtest_expected) + ")");                                      \
		}                                                                                              \
	} while (false)

// An ordered comparison: each operand is read once into a local, as CHECK_EQ reads
// them, so a call inside a check runs once and both values reach the message.
#define CHECK_ORDER(ACTUAL, LIMIT, OP, NAME)                                                           \
	do                                                                                                 \
	{                                                                                                  \
		const auto civtest_actual = (ACTUAL);                                                          \
		const auto civtest_limit = (LIMIT);                                                            \
		::civtest::Compare(                                                                            \
		    __FILE__, __LINE__, std::string(#ACTUAL) + " " NAME " " + #LIMIT, civtest_actual,          \
		    civtest_limit, civtest_actual OP civtest_limit);                                           \
	} while (false)

#define CHECK_LT(ACTUAL, LIMIT) CHECK_ORDER(ACTUAL, LIMIT, <, "<")
#define CHECK_LE(ACTUAL, LIMIT) CHECK_ORDER(ACTUAL, LIMIT, <=, "<=")
#define CHECK_GT(ACTUAL, LIMIT) CHECK_ORDER(ACTUAL, LIMIT, >, ">")
#define CHECK_GE(ACTUAL, LIMIT) CHECK_ORDER(ACTUAL, LIMIT, >=, ">=")

#define CHECK_NEAR(ACTUAL, EXPECTED, TOLERANCE)                                                        \
	do                                                                                                 \
	{                                                                                                  \
		++::civtest::CheckCount();                                                                     \
		const double civtest_actual = (ACTUAL);                                                        \
		const double civtest_expected = (EXPECTED);                                                    \
		if (std::fabs(civtest_actual - civtest_expected) > (TOLERANCE))                                \
		{                                                                                              \
			::civtest::Fail(                                                                           \
			    __FILE__, __LINE__,                                                                    \
			    std::string(#ACTUAL) + " ~= " + #EXPECTED + " (" +                                     \
			        ::civtest::ToString(civtest_actual) + " vs " +                                     \
			        ::civtest::ToString(civtest_expected) + ")");                                      \
		}                                                                                              \
	} while (false)

#define CHECK_CONTAINS(HAYSTACK, NEEDLE)                                                               \
	do                                                                                                 \
	{                                                                                                  \
		++::civtest::CheckCount();                                                                     \
		const std::string civtest_haystack(HAYSTACK);                                                  \
		const std::string civtest_needle(NEEDLE);                                                      \
		if (civtest_haystack.find(civtest_needle) == std::string::npos)                                \
		{                                                                                              \
			::civtest::Fail(                                                                           \
			    __FILE__, __LINE__,                                                                    \
			    "expected to find '" + civtest_needle + "' in: " + civtest_haystack);                  \
		}                                                                                              \
	} while (false)
