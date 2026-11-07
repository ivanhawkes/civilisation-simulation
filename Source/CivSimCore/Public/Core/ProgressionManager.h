#pragma once

#include "Core/SimulationConfig.h"
#include <map>
#include <set>
#include <string>
#include <vector>


namespace CivSimCore
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

const char *DescribeUpgradeResult(UpgradeResult Result);

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
	bool RegisterNode(const SkillNode &Node, std::string &ErrorMessage);

	// Registers every node and then validates the whole graph. Replaces any state
	// built by an earlier call, including the resource pool.
	bool Build(const Skills &Nodes, std::string &ErrorMessage);

	// Checks the registered graph: prerequisites pointing at unknown nodes, prerequisite
	// cycles, a required tier above the prerequisite's own maxTiers, and a skill that
	// requires a node competing with it in the same exclusive group. Those last two
	// make a branch unreachable: the game can never research it.
	bool Validate(std::string &ErrorMessage) const;

	bool HasNode(const std::string &NodeId) const;

	// Node ids in sorted order.
	std::vector<std::string> NodeIds() const;

	// Tier reached by a node: 0 means locked.
	int GetCurrentTier(const std::string &NodeId) const;

	// maxTiers of a node, 0 when the node is unknown.
	int GetMaxTiers(const std::string &NodeId) const;

	// Unlocked groups: every node sharing one of these ids is locked out.
	bool IsGroupLocked(const std::string &GroupId) const;

	// A group is locked out once one of its members has been unlocked.
	bool IsNodeLockedOut(const std::string &NodeId) const;

	bool CanUpgrade(const std::string &NodeId) const;

	// Why CanUpgrade is false, empty when the upgrade is allowed.
	std::string UpgradeBlockReason(const std::string &NodeId) const;

	// Upgrades one tier, spending resourceCostPerTier from the pool. Leaves the state
	// untouched when the upgrade is not allowed or not affordable.
	UpgradeResult TryUpgrade(const std::string &NodeId);
	bool TryUpgrade(const std::string &NodeId, std::string &ErrorMessage);

	// Resource pool: never negative.
	int GetResources() const;
	void SetResources(int Amount);
	void AddResources(int Amount);

	// One node as a line for the UI, for example "SkillMining tier 2/5 (cost 1/tier)".
	std::string Describe(const std::string &NodeId) const;

	// One line describing the whole tree for the UI.
	std::string Summary() const;

	// Nodes available to the UI, keyed by id, in sorted order.
	const std::map<std::string, SkillNode> &GetRegistry() const;

  private:
	std::map<std::string, SkillNode> registry;
	std::map<std::string, int> unlockedTiers;
	std::set<std::string> lockedExclusiveGroups;
	int resources{0};
};
} // namespace CivSimCore
