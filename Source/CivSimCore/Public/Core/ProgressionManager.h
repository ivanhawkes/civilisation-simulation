/**
    AI suggested code for handling the progression of skills in a game. This is
    intended to manage skill unlocking, prerequisites, and exclusive groups.

    It builds, but remains untested and may require further refinement.
*/

#pragma once

#include "Core/SimulationConfig.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>


namespace CivSimCore
{
class ProgressionManager
{
  private:
	std::unordered_map<std::string, SkillNode> skillRegistry;

	// Tracking active runtime state.
	std::unordered_map<std::string, int> unlockedSkillTiers; // NodeId -> current_tier
	std::unordered_set<std::string> lockedExclusiveGroups;

  public:
	void RegisterNode(const SkillNode &node)
	{
		skillRegistry[node.uniqueId] = node;
	}


	int GetCurrentTier(const std::string &NodeId) const
	{
		auto it = unlockedSkillTiers.find(NodeId);
		return (it != unlockedSkillTiers.end()) ? it->second : 0;
	}


	bool CanUpgrade(const std::string &NodeId) const
	{
		auto it = skillRegistry.find(NodeId);
		if (it == skillRegistry.end())
			return false;

		const auto &node = it->second;
		int current_tier = GetCurrentTier(NodeId);

		// Cap check
		if (current_tier >= node.maxTiers)
			return false;

		// Exclusion check (only blocks Tier 1 purchases)
		if (current_tier == 0 && !node.exclusiveGroupId.empty())
		{
			if (lockedExclusiveGroups.count(node.exclusiveGroupId))
			{
				return false;
			}
		}

		// Multi-tier Prerequisite check
		for (const auto &prereq : node.requiredSkills)
		{
			if (GetCurrentTier(prereq.nodeId) < prereq.requiredTier)
			{
				return false;
			}
		}

		return true;
	}


	bool TryUpgrade(const std::string &NodeId)
	{
		if (!CanUpgrade(NodeId))
			return false;

		const auto &node = skillRegistry[NodeId];

		// Upgrade the tier
		unlockedSkillTiers[NodeId]++;

		// If this was the first point, activate the mutual exclusion lockout
		if (unlockedSkillTiers[NodeId] == 1 && !node.exclusiveGroupId.empty())
		{
			lockedExclusiveGroups.insert(node.exclusiveGroupId);
		}

		std::cout << "Upgraded " << node.displayName << " to Tier "
		          << unlockedSkillTiers[NodeId] << "\n";
		return true;
	}

	// Exposed for the UI layer to read state cleanly
	const std::unordered_map<std::string, SkillNode> &GetRegistry() const
	{
		return skillRegistry;
	}
};
} // namespace CivSimCore
