#include "TestHelpers.h"

#include <algorithm>
#include <map>
#include <regex>
#include <string>
#include <vector>

#include <manifest/config/SimulationConfig.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/sim/Simulation.h>
#include <manifest/universe/Universe.h>


namespace
{
// The generated documents, passed by CMake.
std::string agesPath;
std::string categoriesPath;
std::string fullPath;
std::string simplePath;
std::string universePath;

std::vector<std::string> Ids(const Manifest::Core::Items &items)
{
	std::vector<std::string> ids;

	for (const auto &item : items)
	{
		ids.push_back(item.itemId);
	}

	return ids;
}

bool HasDuplicates(const std::vector<std::string> &values)
{
	std::vector<std::string> sorted = values;
	std::sort(sorted.begin(), sorted.end());

	return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
}

// The tree a node belongs to. A node id runs namespace marker, tree and node: the
// yard drydock is SkillYardDrydock, the gate of the Space Colonisation Age is
// AgeSpaceColonisation, so the tree is the run after the marker, up to its own first
// capital.
std::string TreeLabel(const std::string &nodeId)
{
	const std::size_t markerEnd = nodeId.find_first_of("A-Z");

	if (markerEnd == std::string::npos)
	{
		return nodeId;
	}

	std::string marker = nodeId.substr(0, markerEnd);
	const std::string rest = nodeId.substr(markerEnd);

	if (marker != "Skill" && marker != "Age")
	{
		return marker;
	}

	const std::size_t treeEnd = rest.find_first_of("A-Z");

	return treeEnd == std::string::npos ? rest : rest.substr(0, treeEnd);
}

Manifest::Core::Ages LoadAges()
{
	std::string errorMessage;
	Manifest::Core::Ages ages;
	CHECK(Manifest::Core::LoadConfigAges(agesPath, ages, errorMessage));

	return ages;
}

Manifest::Core::Categories LoadCategories()
{
	std::string errorMessage;
	Manifest::Core::Categories categories;
	CHECK(Manifest::Core::LoadConfigCategories(categoriesPath, categories, errorMessage));

	return categories;
}

Manifest::Core::GameModeNode LoadGameMode(const std::string &path)
{
	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;
	CHECK(Manifest::Core::LoadConfigGameMode(path, gameMode, errorMessage));

	return gameMode;
}
} // namespace


TEST_CASE("ages_load_from_the_generated_document")
{
	const Manifest::Core::Ages ages = LoadAges();

	// A merge that quietly truncates is the failure mode of the pipeline: the external
	// tool is yq, and an upstream change to how it folds the stream of source files, or a
	// file dropped from the list it reads, can leave a whole section out. A truncated game
	// must not be allowed to pass as a small one, so the scale of the shipped data is
	// asserted here.
	CHECK(ages.size() >= 12);

	std::vector<std::string> ids;
	for (const auto &age : ages)
	{
		CHECK(!age.ageId.empty());
		CHECK(!age.displayName.empty());
		ids.push_back(age.ageId);
	}

	CHECK(!HasDuplicates(ids));
}

TEST_CASE("categories_load_from_the_generated_document")
{
	const Manifest::Core::Categories categories = LoadCategories();

	CHECK(categories.size() >= 40);

	std::vector<std::string> ids;
	for (const auto &category : categories)
	{
		CHECK(!category.categoryId.empty());
		CHECK(!category.displayName.empty());
		CHECK(!category.type.empty());
		CHECK(!category.description.empty());
		ids.push_back(category.categoryId);
	}

	CHECK(!HasDuplicates(ids));
}

TEST_CASE("every_item_in_the_full_game_mode_has_a_price")
{
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	CHECK(full.items.size() >= 80);

	CHECK(!HasDuplicates(Ids(full.items)));

	for (const auto &item : full.items)
	{
		CHECK(!item.itemId.empty());
		CHECK(!item.displayName.empty());
		CHECK(!item.unitType.empty());

		// A price the trade model cannot parse is worse than a load failure, because the
		// good is silently priced at nothing. A zero price is a free good in the data, so
		// the field has to be a number: it does not have to be positive.
		static const std::regex numberPattern(R"( *[0-9]+(\.[0-9]+)? *)");
		CHECK(std::regex_match(item.basePrice, numberPattern));
		CHECK(Manifest::Core::Simulation::BasePrice(item) >= 0.0);
	}
}

TEST_CASE("every_item_carries_its_age_and_category_link")
{
	// Generation reads item.ageId to bound what a world can make and item.categoryId to
	// weight what it wants. The two links of an item are plain-text ids in the item file -
	// the ageId of an age in base/ages.yaml, the categoryId of a category in
	// trade-items/categories.yaml - and the merge carries them into the shipped document:
	// an item that lost them leaves every world a producer of every good, and a universe
	// with no shortages is a universe with no trade.
	const Manifest::Core::Ages ages = LoadAges();
	const Manifest::Core::Categories categories = LoadCategories();
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	std::vector<std::string> ageIds;
	for (const auto &age : ages)
	{
		ageIds.push_back(age.ageId);
	}

	std::vector<std::string> categoryIds;
	for (const auto &category : categories)
	{
		categoryIds.push_back(category.categoryId);
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

TEST_CASE("the_id_of_a_definition_names_the_kind_of_thing_it_is")
{
	// Every id is prefixed with the kind of thing it names: Age for an age, Category for
	// a category, Item for a good, Skill for a node of the tree. A reference in a
	// require, a game mode or the link of an item therefore says which table it points
	// into, and one word can name a craft and a shelf of goods at once without the two
	// tables colliding. The ages drop the old Age postfix for the prefix, so no id ends
	// with the word it starts with.
	const Manifest::Core::Ages ages = LoadAges();
	const Manifest::Core::Categories categories = LoadCategories();
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	std::vector<std::string> ageIds;
	for (const auto &age : ages)
	{
		CHECK(age.ageId.rfind("Age", 0) == 0);
		CHECK(age.ageId.rfind("Age") != age.ageId.size() - 3);
		ageIds.push_back(age.ageId);
	}

	std::vector<std::string> names = ageIds;

	std::vector<std::string> categoryIds;
	for (const auto &category : categories)
	{
		CHECK(category.categoryId.rfind("Category", 0) == 0);
		categoryIds.push_back(category.categoryId);
		names.push_back(category.categoryId);
	}

	const std::vector<std::string> itemIds = Ids(full.items);
	for (const std::string &itemId : itemIds)
	{
		CHECK(itemId.rfind("Item", 0) == 0);
		names.push_back(itemId);
	}

	std::vector<std::string> skillIds;
	for (const auto &skill : full.skills)
	{
		// The gate of an age carries the ageId of the age it opens, so the gate is the
		// one name the tree shares with another table, and it is shared by design.
		const bool isGate = std::find(ageIds.begin(), ageIds.end(), skill.skillId) != ageIds.end();

		CHECK((isGate ? skill.skillId.rfind("Age", 0) == 0 : skill.skillId.rfind("Skill", 0) == 0));
		skillIds.push_back(skill.skillId);

		if (!isGate)
		{
			names.push_back(skill.skillId);
		}
	}

	// One name, one kind of thing: the same word is never an age, a category, a good and
	// a node in turn, so a reference needs no lookup to say what it resolves to.
	CHECK(names.size() >= 200);
	CHECK(!HasDuplicates(names));

	// The words the data reuses across the tables are the test of the rule: one word
	// works a bench and stacks a pallet, and the two ids never meet.
	for (const std::string &word : {"Pharmaceuticals", "Polymers", "Textiles"})
	{
		CHECK(std::find(skillIds.begin(), skillIds.end(), "Skill" + word) != skillIds.end());
		CHECK(
		    std::find(categoryIds.begin(), categoryIds.end(), "Category" + word) != categoryIds.end());
	}

	for (const std::string &word : {"Electronics", "Slaves"})
	{
		CHECK(std::find(itemIds.begin(), itemIds.end(), "Item" + word) != itemIds.end());
		CHECK(
		    std::find(categoryIds.begin(), categoryIds.end(), "Category" + word) != categoryIds.end());
	}
}

TEST_CASE("nulls_in_the_skill_data_never_arrive_as_the_word_null")
{
	// yaml-cpp renders a null as the scalar text "null". If that ever reaches the
	// skill tree, every skill lands in an exclusive group named "null" and buying one
	// skill locks out all the others.
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	CHECK(full.skills.size() >= 85);

	for (const auto &skill : full.skills)
	{
		CHECK(skill.exclusiveGroupId != "null");
		CHECK(skill.exclusiveGroupId != "Null");
		CHECK(skill.exclusiveGroupId != "NULL");
		CHECK(skill.exclusiveGroupId != "~");

		CHECK(skill.targetEffectID != "null");
		CHECK(skill.targetEffectID != "~");

		CHECK(!skill.skillId.empty());
		CHECK(!skill.displayName.empty());

		// The depth is a property of the design, not of the loader: a skill with no
		// maxTiers defaults to one tier, which flattens every branch into a single
		// click. A merge that dropped the field would pass the graph checks and leave a
		// tree with nothing to research, so the depth is asserted here. The one-tier
		// nodes are the Tech gates: an age gate has one tier by design, so the check on
		// depth is a check on the Skill nodes, the crafts and the ship systems.
		if (skill.nodeType == Manifest::Core::NodeType::Tech)
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

TEST_CASE("the_age_gates_are_the_ages")
{
	// Every age has one gate, the gate carries the age's own ageId as its skillId, and the gates
	// form the age ladder: each gate requires the gate of the age before it, and the
	// first gate in age order is the only one with nothing under it.
	const Manifest::Core::Ages ages = LoadAges();
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	std::map<std::string, Manifest::Core::SkillNode> gates;
	for (const auto &skill : full.skills)
	{
		if (skill.nodeType == Manifest::Core::NodeType::Tech)
		{
			gates[skill.skillId] = skill;
		}
	}

	CHECK_EQ(gates.size(), ages.size());

	int gatesWithNoPrerequisite = 0;
	for (const Manifest::Core::AgeNode &age : ages)
	{
		const auto gate = gates.find(age.ageId);
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

TEST_CASE("the_ship_systems_are_worth_the_trip")
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
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	const std::vector<std::string> shipSystems = {
	    "SkillShipShieldsFront",    "SkillShipShieldsSide",        "SkillShipShieldsRear",
	    "SkillShipGunsFront",       "SkillShipGunsRear",           "SkillShipLasersFront",
	    "SkillShipLasersRear",      "SkillShipNavigationComputer", "SkillShipTraderAI",
	    "SkillShipEngineThrust",    "SkillShipManoeuvringJets",    "SkillShipAttitudeControl",
	    "SkillShipSensorLongRange", "SkillShipSensorFireControl",  "SkillShipSensorCommercial",
	    "SkillShipCargoHolds",      "SkillShipFuelBunkers",        "SkillShipSalvageRig",
	    "SkillShipLifeSupport",     "SkillShipStealthCoating",     "SkillShipEmissionsControl",
	    "SkillShipCountermeasures", "SkillShipCrewTraining",       "SkillShipDamageControl",
	    "SkillShipSparesLockers",
	};

	std::map<std::string, Manifest::Core::SkillNode> ship;
	for (const auto &skill : full.skills)
	{
		if (skill.skillId.rfind("SkillShip", 0) == 0)
		{
			ship[skill.skillId] = skill;
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
		CHECK_EQ(system->second.nodeType, Manifest::Core::NodeType::Skill);
		CHECK(system->second.exclusiveGroupId.empty());
		CHECK(!system->second.targetEffectID.empty());
		CHECK(system->second.resourceCostPerTier >= 1);
	}
}

TEST_CASE("the_standing_trees_are_in_the_game")
{
	// The trees a civilisation stands: the yards that build and mend, the lanes that
	// keep a route open, the survey and the claims it stakes, the habitats and the
	// weather, the pirate arts, the law, the trade services and the demand side. Each
	// node is a tier-deep capability with an effect to drive, every prerequisite names a
	// node the tree has, and the links that cross trees - the refinery on the Smelting
	// craft, the biosphere on the Agriculture craft, the insurance on the tribunal - are
	// the point of the whole shape: a tree that stands alone is a list, not a tree.
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	const std::vector<std::string> standingTrees = {
	    "SkillYardDrydock",           "SkillYardOutfitting",     "SkillYardModularHulls",
	    "SkillYardRefitDocks",        "SkillLaneConvoy",         "SkillLaneEscortWing",
	    "SkillLaneRouteWard",         "SkillLaneWardenPatrol",   "SkillSurveyDeepRange",
	    "SkillClaimMining",           "SkillClaimRefinery",      "SkillClaimExclusive",
	    "SkillHabitatDomes",          "SkillHabitatClosedLoop",  "SkillTerraformAtmosphere",
	    "SkillTerraformBiosphere",    "SkillPirateRaiderHulls",  "SkillPirateBoardingParties",
	    "SkillPirateRansomExchange",  "SkillPirateSafeHarbours", "SkillLawAdmiraltyWarrant",
	    "SkillLawPiratePardon",       "SkillLawTribunal",        "SkillLawPirateStanding",
	    "SkillTradeDockingLicence",   "SkillTradeInsurance",     "SkillTradeFencingNetwork",
	    "SkillServicePublicMedicine", "SkillServiceEducation",
	};

	std::map<std::string, Manifest::Core::SkillNode> nodes;
	for (const auto &skill : full.skills)
	{
		nodes[skill.skillId] = skill;
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
		CHECK_EQ(node->second.nodeType, Manifest::Core::NodeType::Skill);
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
			const std::string left = TreeLabel(nodeId);
			const std::string right = TreeLabel(prerequisite.nodeId);
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

TEST_CASE("one_clean_choice_locks_the_rest_of_the_standing")
{
	// CleanStanding is the one exclusive group in the shipped data, and it is the
	// reason the mechanic exists: an admiralty warrant, a standing among the crews and
	// a fence are one reputation, spent three ways. Buy one and the other two are shut,
	// so the tree has to be built, bought from a pool, and read back.
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	const std::vector<std::string> members = {
	    "SkillLawAdmiraltyWarrant",
	    "SkillLawPirateStanding",
	    "SkillTradeFencingNetwork",
	};

	Manifest::Core::ProgressionManager progression;
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
			    (other == progression.GetRegistry().end() ||
			     other->second.exclusiveGroupId != std::string("CleanStanding")));
		}
	}

	progression.SetResources(30);

	CHECK_EQ(
	    progression.TryUpgrade("SkillLawAdmiraltyWarrant"), Manifest::Core::UpgradeResult::Success);
	CHECK(progression.IsGroupLocked("CleanStanding"));
	CHECK(progression.IsNodeLockedOut("SkillLawPirateStanding"));
	CHECK(progression.IsNodeLockedOut("SkillTradeFencingNetwork"));
	CHECK_EQ(
	    progression.TryUpgrade("SkillLawPirateStanding"), Manifest::Core::UpgradeResult::ExclusiveLock);

	// The warrant is the root the tribunal hangs off, and the tribunal is not in the
	// group: the choice shuts the rivals out, it does not pull the branch under them.
	CHECK(!progression.IsNodeLockedOut("SkillLawTribunal"));
	CHECK_EQ(progression.TryUpgrade("SkillLawTribunal"), Manifest::Core::UpgradeResult::Success);
	CHECK(progression.GetCurrentTier("SkillLawTribunal") >= 1);
}

TEST_CASE("the_skill_tree_is_a_valid_graph")
{
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	Manifest::Core::ProgressionManager progression;
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
		const int tiers = progression.GetMaxTiers(skill.skillId);
		tierCapacity += tiers;

		if (skill.nodeType == Manifest::Core::NodeType::Tech)
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

TEST_CASE("the_simple_game_mode_is_a_smaller_subset_of_the_full_one")
{
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);
	const Manifest::Core::GameModeNode simple = LoadGameMode(simplePath);

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
			ids.push_back(skill.skillId);
		}

		return ids;
	}();

	for (const auto &skill : simple.skills)
	{
		CHECK(std::find(fullSkills.begin(), fullSkills.end(), skill.skillId) != fullSkills.end());
	}
}

TEST_CASE("the_universe_names_load")
{
	std::string errorMessage;
	Manifest::Core::UniverseNamePools pools;

	CHECK(Manifest::Core::LoadUniverseNamePools(universePath, pools, errorMessage));
	CHECK(pools.PoolCount() > std::size_t(0));
	CHECK(pools.NameCount() > std::size_t(100));

	for (const auto &poolName : pools.PoolNames())
	{
		CHECK(!pools.Find(poolName).empty());
	}
}

TEST_CASE("the_generated_data_drives_a_universe")
{
	// The end to end path the front-end uses: generated documents in, a trading
	// universe out.
	const Manifest::Core::Ages ages = LoadAges();
	const Manifest::Core::Categories categories = LoadCategories();
	const Manifest::Core::GameModeNode full = LoadGameMode(fullPath);

	std::string errorMessage;
	Manifest::Core::UniverseNamePools pools;
	CHECK(Manifest::Core::LoadUniverseNamePools(universePath, pools, errorMessage));

	Manifest::Core::GenerateOptions options;
	options.civilisationCount = 8;
	options.seed = 1;

	Manifest::Core::Simulation simulation;
	CHECK(simulation.Generate(ages, categories, full.items, pools, options, errorMessage));

	CHECK_EQ(simulation.GetCivilisations().size(), std::size_t(8));
	CHECK(simulation.AdvanceTick(errorMessage));
	CHECK_EQ(simulation.GetTick(), 1LL);

	const std::vector<Manifest::Core::TradeRoute> routes = simulation.PlanTrade(5);
	for (const auto &route : routes)
	{
		CHECK(route.profit > 0.0);
	}

	MANIFEST_CHECK_CONTAINS(simulation.StatusLine(), "8 worlds");
}

int main(int argc, char **argv)
{
	if (argc != 6)
	{
		std::cout << "Usage: " << argv[0]
		          << " <ages.yaml> <categories.yaml> <full.yaml> <simple.yaml> "
		             "<solar-system-names.yaml>\n"
		             "Build them with: cmake --build <build-dir> --target manifest_data\n";

		return 2;
	}

	agesPath = argv[1];
	categoriesPath = argv[2];
	fullPath = argv[3];
	simplePath = argv[4];
	universePath = argv[5];

	doctest::Context context;
	return context.run();
}
