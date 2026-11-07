#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Core/SimulationConfig.h"
#include "Core/Universe.h"
#include "TestFramework.h"
#include <algorithm>
#include <map>
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

TEST_CASE(every_item_carries_its_age_and_category_link)
{
	// Generation reads item.ageId to bound what a world can make and item.categoryId to
	// weight what it wants. The links are anchors in the item files and the merge writes
	// them out as ordinary fields: a merge that lost them leaves every world a producer of
	// every good, and a universe with no shortages is a universe with no trade.
	const CivSimCore::Ages ages = LoadAges();
	const CivSimCore::Categories categories = LoadCategories();
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	std::vector<std::string> ageIds;
	for (const auto &age : ages)
	{
		ageIds.push_back(age.uniqueId);
	}

	std::vector<std::string> categoryIds;
	for (const auto &category : categories)
	{
		categoryIds.push_back(category.uniqueId);
	}

	CHECK(full.items.size() >= 80);

	for (const auto &item : full.items)
	{
		CHECK(!item.ageId.empty());
		CHECK(!item.categoryId.empty());

		CHECK(std::find(ageIds.begin(), ageIds.end(), item.ageId) != ageIds.end());
		CHECK(std::find(categoryIds.begin(), categoryIds.end(), item.categoryId) != categoryIds.end());
	}
}

TEST_CASE(nulls_in_the_skill_data_never_arrive_as_the_word_null)
{
	// yaml-cpp renders a null as the scalar text "null". If that ever reaches the
	// skill tree, every skill lands in an exclusive group named "null" and buying one
	// skill locks out all the others.
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	CHECK(full.skills.size() >= 85);

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

		// The depth is a property of the design, not of the loader: a skill with no
		// maxTiers defaults to one tier, which flattens every branch into a single
		// click. A merge that dropped the field would pass the graph checks and leave a
		// tree with nothing to research, so the depth is asserted here. The one-tier
		// nodes are the Tech gates: an age gate has one tier by design, so the check on
		// depth is a check on the Skill nodes, the crafts and the ship systems.
		if (skill.nodeType == CivSimCore::NodeType::Tech)
		{
			CHECK_EQ(skill.maxTiers, 1);
		}
		else
		{
			CHECK(skill.maxTiers >= 3);
		}

		CHECK(skill.resourceCostPerTier >= 0);
	}
}

TEST_CASE(the_age_gates_are_the_ages)
{
	// Every age has one gate, the gate carries the age's own uniqueId, and the gates
	// form the age ladder: each gate requires the gate of the age before it, and the
	// first gate in age order is the only one with nothing under it.
	const CivSimCore::Ages ages = LoadAges();
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	std::map<std::string, CivSimCore::SkillNode> gates;
	for (const auto &skill : full.skills)
	{
		if (skill.nodeType == CivSimCore::NodeType::Tech)
		{
			gates[skill.uniqueId] = skill;
		}
	}

	CHECK_EQ(gates.size(), ages.size());

	int gatesWithNoPrerequisite = 0;
	for (const CivSimCore::AgeNode &age : ages)
	{
		const auto gate = gates.find(age.uniqueId);
		CHECK(gate != gates.end());

		if (gate == gates.end())
		{
			continue;
		}

		CHECK_EQ(gate->second.maxTiers, 1);

		if (gate->second.requiredSkills.empty())
		{
			++gatesWithNoPrerequisite;
			continue;
		}

		// The gate sits on the gate of the age before it, one tier deep, so the ladder
		// of gates is the ladder of ages in the same order.
		CHECK_EQ(gate->second.requiredSkills.size(), std::size_t(1));
		CHECK_EQ(gate->second.requiredSkills[0].requiredTier, 1);
		CHECK(gates.find(gate->second.requiredSkills[0].nodeId) != gates.end());
	}

	CHECK_EQ(gatesWithNoPrerequisite, 1);
}

TEST_CASE(the_ship_systems_are_worth_the_trip)
{
	// The long-haul trader: three shield arcs, two gun arcs, two laser arcs, the drive
	// and the thrusters that spend it, the mast that sees, the holds, the bunkers, the
	// salvage rig and the air the crew breathes, the coating and the emissions that keep
	// the ship out of the return, and the crew, their repair skill and their spares. Every
	// one of them has three tiers, because a ship that has fitted a shield bank, a bigger
	// nozzle, a hotter bell, a snappier pair of jets, a wider dish, a deeper hold or a
	// better repair party can always fit more of it, and none of them is in an exclusive
	// group: a trader who can afford a forward and an aft screen fits both, and the tree
	// has to say so.
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	const std::vector<std::string> shipSystems = {
	    "ShipShieldsFront",    "ShipShieldsSide",        "ShipShieldsRear",
	    "ShipGunsFront",       "ShipGunsRear",           "ShipLasersFront",
	    "ShipLasersRear",      "ShipNavigationComputer", "ShipTraderAI",
	    "ShipEngineThrust",    "ShipManoeuvringJets",    "ShipAttitudeControl",
	    "ShipSensorLongRange", "ShipSensorFireControl",  "ShipSensorCommercial",
	    "ShipCargoHolds",      "ShipFuelBunkers",        "ShipSalvageRig",
	    "ShipLifeSupport",     "ShipStealthCoating",     "ShipEmissionsControl",
	    "ShipCountermeasures", "ShipCrewTraining",       "ShipDamageControl",
	    "ShipSparesLockers",
	};

	std::map<std::string, CivSimCore::SkillNode> ship;
	for (const auto &skill : full.skills)
	{
		if (skill.uniqueId.rfind("Ship", 0) == 0)
		{
			ship[skill.uniqueId] = skill;
		}
	}

	CHECK_EQ(ship.size(), shipSystems.size());

	for (const std::string &systemId : shipSystems)
	{
		const auto system = ship.find(systemId);
		CHECK(system != ship.end());

		if (system == ship.end())
		{
			continue;
		}

		CHECK_EQ(system->second.maxTiers, 3);
		CHECK_EQ(system->second.nodeType, CivSimCore::NodeType::Skill);
		CHECK(system->second.exclusiveGroupId.empty());
		CHECK(!system->second.targetEffectID.empty());
		CHECK(system->second.resourceCostPerTier >= 1);
	}
}

TEST_CASE(the_standing_trees_are_in_the_game)
{
	// The trees a civilisation stands: the yards that build and mend, the lanes that
	// keep a route open, the survey and the claims it stakes, the habitats and the
	// weather, the pirate arts, the law, the trade services and the demand side. Each
	// node is a tier-deep capability with an effect to drive, every prerequisite names a
	// node the tree has, and the links that cross trees - the refinery on the Smelting
	// craft, the biosphere on Agriculture, the insurance on the tribunal - are the point
	// of the whole shape: a tree that stands alone is a list, not a tree.
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	const std::vector<std::string> standingTrees = {
	    "YardDrydock",         "YardOutfitting",        "YardModularHulls",     "YardRefitDocks",
	    "LaneConvoy",          "LaneEscortWing",        "LaneRouteWard",        "LaneWardenPatrol",
	    "SurveyDeepRange",     "ClaimMining",           "ClaimRefinery",        "ClaimExclusive",
	    "HabitatDomes",        "HabitatClosedLoop",     "TerraformAtmosphere",  "TerraformBiosphere",
	    "PirateRaiderHulls",   "PirateBoardingParties", "PirateRansomExchange", "PirateSafeHarbours",
	    "LawAdmiraltyWarrant", "LawPiratePardon",       "LawTribunal",          "LawPirateStanding",
	    "TradeDockingLicence", "TradeInsurance",        "TradeFencingNetwork",  "ServicePublicMedicine",
	    "ServiceEducation",
	};

	std::map<std::string, CivSimCore::SkillNode> nodes;
	for (const auto &skill : full.skills)
	{
		nodes[skill.uniqueId] = skill;
	}

	std::size_t crossTreeLinks = 0;

	for (const std::string &nodeId : standingTrees)
	{
		const auto node = nodes.find(nodeId);
		CHECK(node != nodes.end());

		if (node == nodes.end())
		{
			continue;
		}

		CHECK_EQ(node->second.maxTiers, 3);
		CHECK_EQ(node->second.nodeType, CivSimCore::NodeType::Skill);
		CHECK(!node->second.targetEffectID.empty());
		CHECK(node->second.resourceCostPerTier >= 1);

		for (const auto &prerequisite : node->second.requiredSkills)
		{
			CHECK(nodes.find(prerequisite.nodeId) != nodes.end());

			if (nodes.find(prerequisite.nodeId) == nodes.end())
			{
				continue;
			}

			// A prerequisite outside the node's own tree is the link that makes the
			// shape a tree: the yard that needs the age gate, the claim that needs the
			// craft, the insurance that needs the bench.
			const std::string left = nodeId.substr(0, nodeId.find_first_of("A-Z"));
			const std::string right =
			    prerequisite.nodeId.substr(0, prerequisite.nodeId.find_first_of("A-Z"));
			if (left != right)
			{
				++crossTreeLinks;
			}
		}
	}

	// The cross-tree links are the design, so the count is asserted and not admired:
	// a tree that stops linking is a set of lists, and the suite notices.
	CHECK(crossTreeLinks >= 8);
}

TEST_CASE(one_clean_choice_locks_the_rest_of_the_standing)
{
	// CleanStanding is the one exclusive group in the shipped data, and it is the
	// reason the mechanic exists: an admiralty warrant, a standing among the crews and
	// a fence are one reputation, spent three ways. Buy one and the other two are shut,
	// so the tree has to be built, bought from a pool, and read back.
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);

	const std::vector<std::string> members = {
	    "LawAdmiraltyWarrant",
	    "LawPirateStanding",
	    "TradeFencingNetwork",
	};

	CivSimCore::ProgressionManager progression;
	std::string errorMessage;
	CHECK(progression.Build(full.skills, errorMessage));

	for (const std::string &member : members)
	{
		const auto node = progression.GetRegistry().find(member);
		CHECK(node != progression.GetRegistry().end());

		if (node == progression.GetRegistry().end())
		{
			continue;
		}

		CHECK_EQ(node->second.exclusiveGroupId, std::string("CleanStanding"));

		// A member may never require a rival of its own group: the validator rejects
		// that shape, and the shipped tree has to survive the validator.
		for (const auto &prerequisite : node->second.requiredSkills)
		{
			const auto other = progression.GetRegistry().find(prerequisite.nodeId);
			CHECK(
			    other == progression.GetRegistry().end() ||
			    other->second.exclusiveGroupId != std::string("CleanStanding"));
		}
	}

	progression.SetResources(30);

	CHECK_EQ(progression.TryUpgrade("LawAdmiraltyWarrant"), CivSimCore::UpgradeResult::Success);
	CHECK(progression.IsGroupLocked("CleanStanding"));
	CHECK(progression.IsNodeLockedOut("LawPirateStanding"));
	CHECK(progression.IsNodeLockedOut("TradeFencingNetwork"));
	CHECK_EQ(progression.TryUpgrade("LawPirateStanding"), CivSimCore::UpgradeResult::ExclusiveLock);

	// The warrant is the root the tribunal hangs off, and the tribunal is not in the
	// group: the choice shuts the rivals out, it does not pull the branch under them.
	CHECK(!progression.IsNodeLockedOut("LawTribunal"));
	CHECK_EQ(progression.TryUpgrade("LawTribunal"), CivSimCore::UpgradeResult::Success);
	CHECK(progression.GetCurrentTier("LawTribunal") >= 1);
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

	// Tier capacity across the tree: every craft has to be worth researching, and the
	// total is what the player spends resources against. The age gates are one tier by
	// design, so the depth they add is not what the check is about.
	int tierCapacity = 0;
	int crafts = 0;
	int gates = 0;
	for (const auto &skill : full.skills)
	{
		const int tiers = progression.GetMaxTiers(skill.uniqueId);
		tierCapacity += tiers;

		if (skill.nodeType == CivSimCore::NodeType::Tech)
		{
			++gates;
			continue;
		}

		++crafts;
	}

	CHECK(tierCapacity - gates >= 3 * crafts);

	// The scale, not just the ratio: the 17 crafts of skills.yaml, the 25 ship systems
	// and the 29 nodes of the yard, lane, survey, habitat, pirate, law, trade and service
	// trees, three tiers deep, above the 14 age gates. A merge that dropped a whole file
	// keeps the ratio and shortens the tree.
	CHECK(crafts >= 71);
	CHECK(gates >= 12);
	CHECK(tierCapacity >= 227);
}

TEST_CASE(the_simple_game_mode_is_a_smaller_subset_of_the_full_one)
{
	const CivSimCore::GameModeNode full = LoadGameMode(fullPath);
	const CivSimCore::GameModeNode simple = LoadGameMode(simplePath);

	CHECK(simple.items.size() >= 12);
	CHECK(simple.items.size() < full.items.size());
	CHECK(simple.skills.size() >= 85);

	// The small game keeps the whole tree, so a quick match exercises the age gates and
	// the ship systems, not only the crafts.
	CHECK_EQ(simple.skills.size(), full.skills.size());

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
