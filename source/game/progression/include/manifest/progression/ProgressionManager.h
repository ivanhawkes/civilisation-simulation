#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include <manifest/config/SimulationConfig.h>


namespace Manifest::Core
{
enum class UpgradeResult
{
	Success,
	UnknownNode,
	MaxTier,
	ExclusiveLock,
	Prerequisites,
	InsufficientResources
};

const char *DescribeUpgradeResult(UpgradeResult result);

// Manages the skill tree: registration, graph validation, tier upgrades and the
// resource pool that pays for them.
//
// The registry is a std::map keyed by node id so iteration order is identical on every
// platform: the UI and the tests depend on that.
//
// This is library code: it writes nothing to the console. Every failure returns a
// reason, either as an UpgradeResult or as a message.
class ProgressionManager
{
  public:
	// Registers one node. Rejects an empty id, a duplicate id, maxTiers below 1, a
	// negative cost, a self prerequisite and a prerequisite asking for tier 0.
	bool RegisterNode(const SkillNode &node, std::string &errorMessage);

	// Registers every node and then validates the whole graph. Replaces any state
	// built by an earlier call, including the resource pool.
	bool Build(const Skills &nodes, std::string &errorMessage);

	// Checks the registered graph: prerequisites pointing at unknown nodes, prerequisite
	// cycles, a required tier above the prerequisite's own maxTiers, and a skill that
	// requires a node competing with it in the same exclusive group. Those last two
	// make a branch unreachable: the game can never research it.
	bool Validate(std::string &errorMessage) const;

	bool HasNode(const std::string &nodeId) const;

	// Node ids in sorted order.
	std::vector<std::string> NodeIds() const;

	// Tier reached by a node: 0 means locked.
	int GetCurrentTier(const std::string &nodeId) const;

	// maxTiers of a node, 0 when the node is unknown.
	int GetMaxTiers(const std::string &nodeId) const;

	// Unlocked groups: every node sharing one of these ids is locked out.
	bool IsGroupLocked(const std::string &groupId) const;

	// A group is locked out once one of its members has been unlocked.
	bool IsNodeLockedOut(const std::string &nodeId) const;

	bool CanUpgrade(const std::string &nodeId) const;

	// Why CanUpgrade is false, empty when the upgrade is allowed.
	std::string UpgradeBlockReason(const std::string &nodeId) const;

	// Upgrades one tier, spending resourceCostPerTier from the pool. Leaves the state
	// untouched when the upgrade is not allowed or not affordable.
	UpgradeResult TryUpgrade(const std::string &nodeId);
	bool TryUpgrade(const std::string &nodeId, std::string &errorMessage);

	// Resource pool: never negative.
	int GetResources() const;
	void SetResources(int amount);
	void AddResources(int amount);

	// One node as a line for the UI, for example "SkillMining tier 2/5 (cost 1/tier)".
	std::string Describe(const std::string &nodeId) const;

	// One line describing the whole tree for the UI.
	std::string Summary() const;

	// Nodes available to the UI, keyed by id, in sorted order.
	const std::map<std::string, SkillNode> &GetRegistry() const;

  private:
	std::map<std::string, SkillNode> m_registry;
	std::map<std::string, int> m_unlockedTiers;
	std::set<std::string> m_lockedExclusiveGroups;
	int m_resources{0};
};
} // namespace Manifest::Core
