#include "Core/Universe.h"
#include "TestFramework.h"
#include <string>


TEST_CASE(pools_load_in_sorted_order)
{
	const civtest::TempFile file(R"YAML(
greek:
  - Andreas
  - Bessas
latin:
  - Aaron
  - Bolivar
)YAML");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_EQ(pools.PoolCount(), std::size_t(2));
	CHECK_EQ(pools.NameCount(), std::size_t(4));
	CHECK(!pools.Empty());

	const std::vector<std::string> names = pools.PoolNames();
	CHECK_EQ(names.size(), std::size_t(2));
	CHECK_EQ(names[0], std::string("greek"));
	CHECK_EQ(names[1], std::string("latin"));

	CHECK_EQ(pools.Find("latin").size(), std::size_t(2));
	CHECK_EQ(pools.Find("latin")[1], std::string("Bolivar"));

	// An unknown pool is empty rather than an error.
	CHECK(pools.Find("coptic").empty());
}

TEST_CASE(loading_replaces_the_pools)
{
	const civtest::TempFile file(R"YAML(
latin:
  - Aaron
  - Abigail
)YAML");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK(ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_EQ(pools.NameCount(), std::size_t(2));
}

TEST_CASE(null_name_in_a_pool_is_rejected)
{
	const civtest::TempFile file(R"YAML(
latin:
  - Aaron
  - null
)YAML");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(!ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_CONTAINS(errorMessage, "latin");
	CHECK_CONTAINS(errorMessage, "[1]");
}

TEST_CASE(empty_pool_is_rejected)
{
	const civtest::TempFile file(R"YAML(
latin: []
)YAML");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(!ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_CONTAINS(errorMessage, "holds no names");
}

TEST_CASE(pool_must_be_a_sequence)
{
	const civtest::TempFile file(R"YAML(
latin: Aaron
)YAML");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(!ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_CONTAINS(errorMessage, "must be a YAML sequence");
}

TEST_CASE(root_must_be_a_mapping)
{
	const civtest::TempFile file(R"YAML(
- Aaron
- Abigail
)YAML");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(!ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_CONTAINS(errorMessage, "mapping of pool name");
}

TEST_CASE(no_pools_is_rejected)
{
	const civtest::TempFile file("{}\n");

	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(!ManifestCore::LoadUniverseNamePools(file.Path(), pools, errorMessage));
	CHECK_CONTAINS(errorMessage, "no name pools");
}

TEST_CASE(unreadable_file_names_the_path)
{
	std::string errorMessage;
	ManifestCore::UniverseNamePools pools;

	CHECK(!ManifestCore::LoadUniverseNamePools("data/nope.yaml", pools, errorMessage));
	CHECK_CONTAINS(errorMessage, "data/nope.yaml");
}

int main()
{
	return civtest::Run("UniverseTests");
}
