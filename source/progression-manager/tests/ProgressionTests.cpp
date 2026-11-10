#include "Core/ProgressionManager.h"
#include "Core/SimulationConfig.h"
#include "TestFramework.h"
#include <string>
#include <vector>


namespace
{
CivSimCore::SkillNode MakeSkill(
    const std::string &id, int maxTiers = 1, int cost = 1, const std::string &exclusive = {},
    std::vector<CivSimCore::Prerequisite> prerequisites = {})
{
	CivSimCore::SkillNode node;
	node.skillId = id;
	node.displayName = id;
	node.nodeType = CivSimCore::NodeType::Skill;
	node.maxTiers = maxTiers;
	node.resourceCostPerTier = cost;
	node.exclusiveGroupId = exclusive;
	node.requiredSkills = std::move(prerequisites);

	return node;
}

CivSimCore::Prerequisite Needs(const std::string &id, int tier = 1)
{
	CivSimCore::Prerequisite prerequisite;
	prerequisite.nodeId = id;
	prerequisite.requiredTier = tier;

	return prerequisite;
}
} // namespace


TEST_CASE(register_rejects_broken_nodes)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	CHECK(!progression.RegisterNode(MakeSkill(""), errorMessage));
	CHECK_CONTAINS(errorMessage, "skillId");

	CHECK(!progression.RegisterNode(MakeSkill("SkillMining", 0), errorMessage));
	CHECK_CONTAINS(errorMessage, "maxTiers");

	CHECK(!progression.RegisterNode(MakeSkill("SkillMining", 3, -1), errorMessage));
	CHECK_CONTAINS(errorMessage, "resourceCostPerTier");

	CHECK(!progression.RegisterNode(
	    MakeSkill("SkillMining", 3, 1, {}, {Needs("SkillMining")}), errorMessage));
	CHECK_CONTAINS(errorMessage, "requires itself");

	CHECK(!progression.RegisterNode(
	    MakeSkill("SkillMining", 3, 1, {}, {Needs("SkillSmelting", 0)}), errorMessage));
	CHECK_CONTAINS(errorMessage, "tier 1");

	CHECK(progression.RegisterNode(MakeSkill("SkillMining"), errorMessage));
	CHECK(!progression.RegisterNode(MakeSkill("SkillMining"), errorMessage));
	CHECK_CONTAINS(errorMessage, "registered twice");
}

TEST_CASE(build_reports_unknown_prerequisite)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	CHECK(
	    !progression.Build({MakeSkill("SkillSmelting", 3, 1, {}, {Needs("Bellowing")})}, errorMessage));
	CHECK_CONTAINS(errorMessage, "SkillSmelting");
	CHECK_CONTAINS(errorMessage, "unknown node 'Bellowing'");
}

TEST_CASE(build_detects_a_prerequisite_cycle)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillSmelting", 3, 1, {}, {Needs("SkillMetalworking")}),
	    MakeSkill("SkillMetalworking", 3, 1, {}, {Needs("SkillSmelting")}),
	};

	CHECK(!progression.Build(nodes, errorMessage));
	CHECK_CONTAINS(errorMessage, "cycle");

	// The registry is sorted, so the reported cycle path starts at the first node in
	// that order.
	CHECK_CONTAINS(errorMessage, "SkillMetalworking -> SkillSmelting -> SkillMetalworking");
}

TEST_CASE(build_rejects_a_tier_that_cannot_be_reached)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	// Smelting asks for tier 4 of Mining, which only reaches tier 2.
	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillMining", 2),
	    MakeSkill("SkillSmelting", 3, 1, {}, {Needs("SkillMining", 4)}),
	};

	CHECK(!progression.Build(nodes, errorMessage));
	CHECK_CONTAINS(errorMessage, "maxTiers 2");
}

TEST_CASE(build_rejects_a_prerequisite_that_competes_with_its_node)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	// Pharmaceuticals requires Herbalism and competes with it in MedicinePath, so the
	// branch can never be researched.
	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillHerbalism", 4, 1, "MedicinePath"),
	    MakeSkill("SkillPharmaceuticals", 3, 1, "MedicinePath", {Needs("SkillHerbalism", 2)}),
	};

	CHECK(!progression.Build(nodes, errorMessage));
	CHECK_CONTAINS(errorMessage, "MedicinePath");
	CHECK_CONTAINS(errorMessage, "can never be researched");
}

TEST_CASE(tiers_stop_at_maxTiers)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	CHECK(progression.Build({MakeSkill("SkillMining", 2, 1)}, errorMessage));
	progression.SetResources(100);

	CHECK_EQ(progression.GetCurrentTier("SkillMining"), 0);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetCurrentTier("SkillMining"), 1);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetCurrentTier("SkillMining"), 2);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::MaxTier);
	CHECK_EQ(progression.GetCurrentTier("SkillMining"), 2);
	CHECK_EQ(progression.GetMaxTiers("SkillMining"), 2);
	CHECK_EQ(progression.GetMaxTiers("Nope"), 0);
	CHECK_EQ(progression.TryUpgrade("Nope"), CivSimCore::UpgradeResult::UnknownNode);
}

TEST_CASE(prerequisites_gate_the_upgrade)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillMining", 5, 1),
	    MakeSkill("SkillSmelting", 4, 1, {}, {Needs("SkillMining", 2)}),
	};

	CHECK(progression.Build(nodes, errorMessage));
	progression.SetResources(100);

	CHECK_EQ(progression.TryUpgrade("SkillSmelting"), CivSimCore::UpgradeResult::Prerequisites);
	CHECK_CONTAINS(progression.UpgradeBlockReason("SkillSmelting"), "requires tier 2 of 'SkillMining'");

	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillSmelting"), CivSimCore::UpgradeResult::Prerequisites);

	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillSmelting"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetCurrentTier("SkillSmelting"), 1);
}

TEST_CASE(exclusive_group_locks_out_the_rivals)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillAgriculture", 5, 1),
	    MakeSkill("SkillAnimalHusbandry", 4, 1, "FoodSourcePath", {Needs("SkillAgriculture")}),
	    MakeSkill("SkillFoodProcessing", 4, 1, "FoodSourcePath", {Needs("SkillAgriculture")}),
	};

	CHECK(progression.Build(nodes, errorMessage));
	progression.SetResources(100);

	CHECK_EQ(progression.TryUpgrade("SkillAgriculture"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillAnimalHusbandry"), CivSimCore::UpgradeResult::Success);

	CHECK(progression.IsGroupLocked("FoodSourcePath"));
	CHECK(progression.IsNodeLockedOut("SkillFoodProcessing"));
	CHECK_EQ(progression.TryUpgrade("SkillFoodProcessing"), CivSimCore::UpgradeResult::ExclusiveLock);
	CHECK_CONTAINS(progression.UpgradeBlockReason("SkillFoodProcessing"), "locked out");

	// A node already unlocked keeps upgrading: only the first tier of a rival is blocked.
	CHECK_EQ(progression.TryUpgrade("SkillAnimalHusbandry"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetCurrentTier("SkillAnimalHusbandry"), 2);
	CHECK_EQ(progression.GetCurrentTier("SkillFoodProcessing"), 0);
}

TEST_CASE(resources_are_spent_and_needed)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	CHECK(progression.Build({MakeSkill("SkillMining", 5, 3)}, errorMessage));

	progression.SetResources(2);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::InsufficientResources);
	CHECK_EQ(progression.GetResources(), 2);
	CHECK_CONTAINS(progression.UpgradeBlockReason("SkillMining"), "costs 3 resources");

	progression.SetResources(7);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetResources(), 4);

	progression.AddResources(1);
	CHECK_EQ(progression.GetResources(), 5);

	progression.AddResources(-99);
	CHECK_EQ(progression.GetResources(), 0);
	progression.SetResources(-5);
	CHECK_EQ(progression.GetResources(), 0);
}

TEST_CASE(a_failed_upgrade_leaves_the_state_alone)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillMining", 5, 2),
	    MakeSkill("SkillSmelting", 4, 2, {}, {Needs("SkillMining", 3)}),
	};

	CHECK(progression.Build(nodes, errorMessage));
	progression.SetResources(10);

	CHECK(!progression.TryUpgrade("SkillSmelting", errorMessage));
	CHECK_EQ(progression.GetCurrentTier("SkillSmelting"), 0);
	CHECK_EQ(progression.GetResources(), 10);

	CHECK(!progression.TryUpgrade("Nope", errorMessage));
	CHECK_CONTAINS(errorMessage, "no skill 'Nope'");
	CHECK_EQ(progression.GetResources(), 10);
}

TEST_CASE(build_replaces_the_previous_tree)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	CHECK(progression.Build({MakeSkill("SkillMining", 5, 1)}, errorMessage));
	progression.SetResources(10);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);

	CHECK(progression.Build({MakeSkill("SkillLumber", 5, 1)}, errorMessage));
	CHECK_EQ(progression.GetCurrentTier("SkillMining"), 0);
	CHECK(!progression.HasNode("SkillMining"));
	CHECK(progression.HasNode("SkillLumber"));
	CHECK_EQ(progression.GetResources(), 0);
	CHECK_EQ(progression.GetRegistry().size(), std::size_t(1));
}

TEST_CASE(iteration_order_is_sorted)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillResearch", 5, 1),
	    MakeSkill("SkillAgriculture", 5, 1),
	    MakeSkill("SkillMining", 5, 1),
	};

	CHECK(progression.Build(nodes, errorMessage));

	const std::vector<std::string> ids = progression.NodeIds();
	CHECK_EQ(ids.size(), std::size_t(3));
	CHECK_EQ(ids[0], std::string("SkillAgriculture"));
	CHECK_EQ(ids[1], std::string("SkillMining"));
	CHECK_EQ(ids[2], std::string("SkillResearch"));
}

TEST_CASE(text_for_the_user_interface)
{
	CivSimCore::ProgressionManager progression;
	std::string errorMessage;

	const CivSimCore::Skills nodes = {
	    MakeSkill("SkillMining", 5, 1),
	    MakeSkill("SkillSmelting", 4, 2, "CraftsPath", {Needs("SkillMining", 2)}),
	};

	CHECK(progression.Build(nodes, errorMessage));
	progression.SetResources(9);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);

	CHECK_CONTAINS(progression.Describe("SkillSmelting"), "SkillSmelting tier 0/4");
	CHECK_CONTAINS(progression.Describe("SkillSmelting"), "requires SkillMining 2");
	CHECK_CONTAINS(progression.Describe("SkillSmelting"), "rival group CraftsPath");
	CHECK_CONTAINS(progression.Describe("Nope"), "no skill 'Nope'");

	CHECK_CONTAINS(progression.Summary(), "2 nodes");
	CHECK_CONTAINS(progression.Summary(), "1 unlocked");
	CHECK_CONTAINS(progression.Summary(), "resources 8");

	CHECK_CONTAINS(
	    std::string(
	        CivSimCore::DescribeUpgradeResult(CivSimCore::UpgradeResult::InsufficientResources)),
	    "not enough resources");
}

TEST_CASE(max_tiers_come_from_the_data)
{
	const char *const yaml = R"YAML(
items: []
skills:
  - skill:
      skillId: SkillMining
      maxTiers: 3
      resourceCostPerTier: 2
  - skill:
      skillId: SkillSmelting
      requires:
        - nodeId: SkillMining
          requiredTier: 3
)YAML";

	const civtest::TempFile file(yaml);

	std::string errorMessage;
	CivSimCore::GameModeNode gameMode;
	CHECK(CivSimCore::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	CHECK_EQ(gameMode.skills[0].maxTiers, 3);

	CivSimCore::ProgressionManager progression;
	CHECK(progression.Build(gameMode.skills, errorMessage));
	progression.SetResources(100);

	CHECK_EQ(progression.GetMaxTiers("SkillMining"), 3);
	CHECK_EQ(progression.TryUpgrade("SkillSmelting"), CivSimCore::UpgradeResult::Prerequisites);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetCurrentTier("SkillMining"), 3);
	CHECK_EQ(progression.TryUpgrade("SkillSmelting"), CivSimCore::UpgradeResult::Success);
	CHECK_EQ(progression.GetCurrentTier("SkillSmelting"), 1);

	// Mining stops at the maxTiers the data gives it.
	CHECK_EQ(progression.TryUpgrade("SkillMining"), CivSimCore::UpgradeResult::MaxTier);
}

int main()
{
	return civtest::Run("ProgressionTests");
}
