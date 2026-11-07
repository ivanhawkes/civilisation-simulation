#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Core/SimulationConfig.h"
#include "Core/Universe.h"
#include "TestFramework.h"
#include <algorithm>
#include <regex>
#include <string>
#include <vector>


namespace
{
// The generated documents, passed by CMake.
std::string agesPath;
std::string categoriesPath;
std::string fullPath;
std::string simplePath;
std::string universePath;

std::vector<std::string> Ids(const CivSimCore::Items &items)
{
	std::vector<std::string> ids;

	for (const auto &item : items)
	{
		ids.push_back(item.uniqueId);
	}

	return ids;
}

bool HasDuplicates(const std::vector<std::string> &values)
{
	std::vector<std::string> sorted = values;
	std::sort(sorted.begin(), sorted.end());

	return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
}

CivSimCore::Ages LoadAges()
{
	std::string errorMessage;
	CivSimCore::Ages ages;
	CHECK(CivSimCore::LoadConfigAges(agesPath, ages, errorMessage));

	return ages;
}

CivSimCore::Categories LoadCategories()
{
	std::string errorMessage;
	CivSimCore::Categories categories;
	CHECK(CivSimCore::LoadConfigCategories(categoriesPath, categories, errorMessage));

	return categories;
}

CivSimCore::GameModeNode LoadGameMode(const std::string &path)
{
	std::string errorMessage;
	CivSimCore::GameModeNode gameMode;
	CHECK(CivSimCore::LoadConfigGameMode(path, gameMode, errorMessage));

	return gameMode;
}
} // namespace


TEST_CASE(ages_load_from_the_generated_document)
{
	const CivSimCore::Ages ages = LoadAges();

	// A merge that quietly truncates is the failure mode of the pipeline: the external
	// tool is yq, and an upstream change to how it resolves anchors can drop whole
	// sections. A truncated game must not be allowed to pass as a small one, so the
	// scale of the shipped data is asserted here.
	CHECK(ages.size() >= 12);

	std::vector<std::string> ids;
	for (const auto &age : ages)
	{
		CHECK(!age.uniqueId.empty());
		CHECK(!age.displayName.empty());
		ids.push_back(age.uniqueId);
	}

	CHECK(!HasDuplicates(ids));
}

TEST_CASE(categories_load_from_the_generated_document)
{
	const CivSimCore::Categories categories = LoadCategories();

	CHECK(categories.size() >= 40);

	std::vector<std::string> ids;
	for (const auto &category : categories)
	{
		CHECK(!category.uniqueId.empty());
		CHECK(!category.displayName.empty());
		CHECK(!category.type.empty());
		CHECK(!category.description.empty());
		ids.push_back(category.uniqueId);
	}

	CHECK(!HasDuplicates(ids));
}

TEST_CASE(every_item_in_the_full_game_mode_has_a_price)
{
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	CHECK(full.items.size() >= 80);

	CHECK(!HasDuplicates(Ids(full.items)));

	for (const auto &item : full.items)
	{
		CHECK(!item.uniqueId.empty());
		CHECK(!item.displayName.empty());
		CHECK(!item.unitType.empty());

		// A price the trade model cannot parse is worse than a load failure, because the
		// good is silently priced at nothing. A zero price is a free good in the data, so
		// the field has to be a number: it does not have to be positive.
		static const std::regex numberPattern(R"( *[0-9]+(\.[0-9]+)? *)");
		CHECK(std::regex_match(item.basePrice, numberPattern));
		CHECK(CivSimCore::Simulation::BasePrice(item) >= 0.0);
	}
}

TEST_CASE(nulls_in_the_skill_data_never_arrive_as_the_word_null)
{
	// yaml-cpp renders a null as the scalar text "null". If that ever reaches the
	// skill tree, every skill lands in an exclusive group named "null" and buying one
	// skill locks out all the others.
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	CHECK(full.skills.size() >= 17);

	for (const auto &skill : full.skills)
	{
		CHECK(skill.exclusiveGroupId != "null");
		CHECK(skill.exclusiveGroupId != "Null");
		CHECK(skill.exclusiveGroupId != "NULL");
		CHECK(skill.exclusiveGroupId != "~");

		CHECK(skill.targetEffectID != "null");
		CHECK(skill.targetEffectID != "~");

		CHECK(!skill.uniqueId.empty());
		CHECK(!skill.displayName.empty());
		CHECK(skill.maxTiers >= 1);
		CHECK(skill.resourceCostPerTier >= 0);
	}
}

TEST_CASE(the_skill_tree_is_a_valid_graph)
{
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	// The whole tree must register and validate: known prerequisites, no cycles, no
	// tier above a prerequisite's reach, no branch locked out by its own rival.
	CHECK(progression.Build(full.skills, errorMessage));
	CHECK_EQ(progression.GetRegistry().size(), full.skills.size());
	CHECK_EQ(progression.NodeIds().size(), full.skills.size());
}

TEST_CASE(the_simple_game_mode_is_a_smaller_subset_of_the_full_one)
{
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);
	const CivSimCore::GameModeNode simple = LoadGameMode(simplePath);

	CHECK(simple.items.size() >= 12);
	CHECK(simple.items.size() < full.items.size());
	CHECK(simple.skills.size() >= 17);

	const std::vector<std::string> fullIds = Ids(full.items);
	for (const std::string &id : Ids(simple.items))
	{
		CHECK(std::find(fullIds.begin(), fullIds.end(), id) != fullIds.end());
	}

	const std::vector<std::string> fullSkills = [&]()
	{
		std::vector<std::string> ids;
		for (const auto &skill : full.skills)
		{
			ids.push_back(skill.uniqueId);
		}

		return ids;
	}();

	for (const auto &skill : simple.skills)
	{
		CHECK(std::find(fullSkills.begin(), fullSkills.end(), skill.uniqueId) != fullSkills.end());
	}
}

TEST_CASE(the_universe_names_load)
{
	std::string errorMessage;
	CivSimCore::UniverseNamePools pools;

	CHECK(CivSimCore::LoadUniverseNamePools(universePath, pools, errorMessage));
	CHECK(pools.PoolCount() > std::size_t(0));
	CHECK(pools.NameCount() > std::size_t(100));

	for (const auto &poolName : pools.PoolNames())
	{
		CHECK(!pools.Find(poolName).empty());
	}
}

TEST_CASE(the_generated_data_drives_a_universe)
{
	// The end to end path the front-end uses: generated documents in, a trading
	// universe out.
	const CivSimCore::Ages ages = LoadAges();
	const CivSimCore::Categories categories = LoadCategories();
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	std::string errorMessage;
	CivSimCore::UniverseNamePools pools;
	CHECK(CivSimCore::LoadUniverseNamePools(universePath, pools, errorMessage));

	CivSimCore::GenerateOptions options;
	options.civilisationCount = 8;
	options.seed = 1;

	CivSimCore::Simulation simulation;
	CHECK(simulation.Generate(ages, categories, full.items, pools, options, errorMessage));

	CHECK_EQ(simulation.GetCivilisations().size(), std::size_t(8));
	CHECK(simulation.AdvanceTick(errorMessage));
	CHECK_EQ(simulation.GetTick(), 1LL);

	const std::vector<CivSimCore::TradeRoute> routes = simulation.PlanTrade(5);
	for (const auto &route : routes)
	{
		CHECK(route.profit > 0.0);
	}

	CHECK_CONTAINS(simulation.StatusLine(), "8 worlds");
}

int main(int argc, char **argv)
{
	if (argc != 6)
	{
		std::cout << "Usage: " << argv[0]
		          << " <ages.yaml> <categories.yaml> <full.yaml> <simple.yaml> "
		             "<solar-system-names.yaml>\n"
		             "Build them with the CivSimData target: cmake --build <build-dir>\n";

		return 2;
	}

	agesPath = argv[1];
	categoriesPath = argv[2];
	fullPath = argv[3];
	simplePath = argv[4];
	universePath = argv[5];

	return civtest::Run("DataTests");
}
