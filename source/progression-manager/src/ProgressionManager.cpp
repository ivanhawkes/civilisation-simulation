#include "Core/ProgressionManager.h"
#include <algorithm>
#include <sstream>
#include <string>
#include <utility>


namespace ManifestCore
{
namespace
{
enum class Visit
{
	Unvisited,
	Visiting,
	Visited
};

std::string Join(const std::vector<std::string> &Parts)
{
	std::string joined;

	for (std::size_t index = 0; index < Parts.size(); ++index)
	{
		if (index != 0)
		{
			joined += " -> ";
		}

		joined += Parts[index];
	}

	return joined;
}

// Depth first search with three colours: a node that is still on the stack is part of
// a cycle, so the path from that node to the current one is reported.
bool FindCycle(
    const std::map<std::string, SkillNode> &Registry, const std::string &NodeId,
    std::vector<std::string> &Stack, std::map<std::string, Visit> &States, std::string &Cycle)
{
	States[NodeId] = Visit::Visiting;
	Stack.push_back(NodeId);

	const auto self = Registry.find(NodeId);
	for (const auto &prerequisite : self->second.requiredSkills)
	{
		const auto target = Registry.find(prerequisite.nodeId);
		if (target == Registry.end())
		{
			// Reported as a missing prerequisite by Validate.
			continue;
		}

		if (States[target->first] == Visit::Visiting)
		{
			const auto start = std::find(Stack.begin(), Stack.end(), target->first);
			std::vector<std::string> path(start, Stack.end());
			path.push_back(prerequisite.nodeId);

			Cycle = Join(path);

			return true;
		}

		if (States[target->first] == Visit::Unvisited &&
		    FindCycle(Registry, target->first, Stack, States, Cycle))
		{
			return true;
		}
	}

	Stack.pop_back();
	States[NodeId] = Visit::Visited;

	return false;
}
} // namespace


const char *DescribeUpgradeResult(UpgradeResult Result)
{
	switch (Result)
	{
		case UpgradeResult::Success: return "upgraded";
		case UpgradeResult::UnknownNode: return "unknown node";
		case UpgradeResult::MaxTier: return "already at maximum tier";
		case UpgradeResult::ExclusiveLock: return "locked out by a rival choice";
		case UpgradeResult::Prerequisites: return "prerequisites are not met";
		case UpgradeResult::InsufficientResources: return "not enough resources";
	}

	return "failed";
}


bool ProgressionManager::RegisterNode(const SkillNode &Node, std::string &ErrorMessage)
{
	ErrorMessage.clear();

	if (Node.skillId.empty())
	{
		ErrorMessage = "a skill node needs a skillId.";
		return false;
	}

	if (registry.find(Node.skillId) != registry.end())
	{
		ErrorMessage = "skill '" + Node.skillId + "' is registered twice.";
		return false;
	}

	if (Node.maxTiers < 1)
	{
		ErrorMessage = "skill '" + Node.skillId + "' has maxTiers " + std::to_string(Node.maxTiers) +
		               ": at least 1 is required.";
		return false;
	}

	if (Node.resourceCostPerTier < 0)
	{
		ErrorMessage = "skill '" + Node.skillId + "' has a negative resourceCostPerTier (" +
		               std::to_string(Node.resourceCostPerTier) + ").";
		return false;
	}

	for (const auto &prerequisite : Node.requiredSkills)
	{
		if (prerequisite.nodeId == Node.skillId)
		{
			ErrorMessage = "skill '" + Node.skillId + "' requires itself.";
			return false;
		}

		if (prerequisite.requiredTier < 1)
		{
			ErrorMessage = "skill '" + Node.skillId + "' requires tier " +
			               std::to_string(prerequisite.requiredTier) + " of '" + prerequisite.nodeId +
			               "': a prerequisite needs at least tier 1.";
			return false;
		}
	}

	registry[Node.skillId] = Node;

	return true;
}


bool ProgressionManager::Build(const Skills &Nodes, std::string &ErrorMessage)
{
	ErrorMessage.clear();

	registry.clear();
	unlockedTiers.clear();
	lockedExclusiveGroups.clear();
	resources = 0;

	for (const auto &node : Nodes)
	{
		if (!RegisterNode(node, ErrorMessage))
		{
			registry.clear();
			return false;
		}
	}

	return Validate(ErrorMessage);
}


bool ProgressionManager::Validate(std::string &ErrorMessage) const
{
	ErrorMessage.clear();

	for (const auto &entry : registry)
	{
		const SkillNode &node = entry.second;

		for (const auto &prerequisite : node.requiredSkills)
		{
			const auto target = registry.find(prerequisite.nodeId);
			if (target == registry.end())
			{
				ErrorMessage =
				    "skill '" + entry.first + "' requires unknown node '" + prerequisite.nodeId + "'.";
				return false;
			}

			if (prerequisite.requiredTier > target->second.maxTiers)
			{
				ErrorMessage = "skill '" + entry.first + "' requires tier " +
				               std::to_string(prerequisite.requiredTier) + " of '" +
				               prerequisite.nodeId + "', which has maxTiers " +
				               std::to_string(target->second.maxTiers) + ".";
				return false;
			}

			if (!node.exclusiveGroupId.empty() &&
			    node.exclusiveGroupId == target->second.exclusiveGroupId)
			{
				ErrorMessage = "skill '" + entry.first + "' requires '" + prerequisite.nodeId +
				               "', which competes with it in exclusive group '" +
				               node.exclusiveGroupId + "': that branch can never be researched.";
				return false;
			}
		}
	}

	std::map<std::string, Visit> states;
	for (const auto &entry : registry)
	{
		states[entry.first] = Visit::Unvisited;
	}

	for (const auto &entry : registry)
	{
		if (states[entry.first] != Visit::Unvisited)
		{
			continue;
		}

		std::vector<std::string> stack;
		std::string cycle;
		if (FindCycle(registry, entry.first, stack, states, cycle))
		{
			ErrorMessage = "prerequisite cycle: " + cycle + ".";
			return false;
		}
	}

	return true;
}


bool ProgressionManager::HasNode(const std::string &NodeId) const
{
	return registry.find(NodeId) != registry.end();
}


std::vector<std::string> ProgressionManager::NodeIds() const
{
	std::vector<std::string> ids;
	ids.reserve(registry.size());

	for (const auto &entry : registry)
	{
		ids.push_back(entry.first);
	}

	return ids;
}


int ProgressionManager::GetCurrentTier(const std::string &NodeId) const
{
	const auto entry = unlockedTiers.find(NodeId);

	return entry != unlockedTiers.end() ? entry->second : 0;
}


int ProgressionManager::GetMaxTiers(const std::string &NodeId) const
{
	const auto entry = registry.find(NodeId);

	return entry != registry.end() ? entry->second.maxTiers : 0;
}


bool ProgressionManager::IsGroupLocked(const std::string &GroupId) const
{
	return !GroupId.empty() && lockedExclusiveGroups.find(GroupId) != lockedExclusiveGroups.end();
}


bool ProgressionManager::IsNodeLockedOut(const std::string &NodeId) const
{
	const auto entry = registry.find(NodeId);
	if (entry == registry.end())
	{
		return false;
	}

	// Once a node of a group is unlocked the rivals are locked out. A node already
	// unlocked stays upgradeable: only the first tier of a rival is blocked.
	if (GetCurrentTier(NodeId) > 0)
	{
		return false;
	}

	return IsGroupLocked(entry->second.exclusiveGroupId);
}


bool ProgressionManager::CanUpgrade(const std::string &NodeId) const
{
	return UpgradeBlockReason(NodeId).empty();
}


std::string ProgressionManager::UpgradeBlockReason(const std::string &NodeId) const
{
	const auto entry = registry.find(NodeId);
	if (entry == registry.end())
	{
		return "no skill '" + NodeId + "' in the tree.";
	}

	const SkillNode &node = entry->second;
	const int currentTier = GetCurrentTier(NodeId);

	if (currentTier >= node.maxTiers)
	{
		return "already at tier " + std::to_string(node.maxTiers) + " of " + node.skillId + ".";
	}

	if (IsNodeLockedOut(NodeId))
	{
		return "locked out: a rival of exclusive group '" + node.exclusiveGroupId + "' was chosen.";
	}

	for (const auto &prerequisite : node.requiredSkills)
	{
		if (GetCurrentTier(prerequisite.nodeId) < prerequisite.requiredTier)
		{
			return "requires tier " + std::to_string(prerequisite.requiredTier) + " of '" +
			       prerequisite.nodeId + "'.";
		}
	}

	if (node.resourceCostPerTier > resources)
	{
		return "costs " + std::to_string(node.resourceCostPerTier) + " resources, only " +
		       std::to_string(resources) + " available.";
	}

	return {};
}


UpgradeResult ProgressionManager::TryUpgrade(const std::string &NodeId)
{
	const auto entry = registry.find(NodeId);
	if (entry == registry.end())
	{
		return UpgradeResult::UnknownNode;
	}

	const SkillNode &node = entry->second;
	const int currentTier = GetCurrentTier(NodeId);

	if (currentTier >= node.maxTiers)
	{
		return UpgradeResult::MaxTier;
	}

	if (IsNodeLockedOut(NodeId))
	{
		return UpgradeResult::ExclusiveLock;
	}

	for (const auto &prerequisite : node.requiredSkills)
	{
		if (GetCurrentTier(prerequisite.nodeId) < prerequisite.requiredTier)
		{
			return UpgradeResult::Prerequisites;
		}
	}

	if (node.resourceCostPerTier > resources)
	{
		return UpgradeResult::InsufficientResources;
	}

	resources -= node.resourceCostPerTier;

	const int tier = currentTier + 1;
	unlockedTiers[NodeId] = tier;

	// The first tier of a node locks out every rival in its exclusive group.
	if (tier == 1 && !node.exclusiveGroupId.empty())
	{
		lockedExclusiveGroups.insert(node.exclusiveGroupId);
	}

	return UpgradeResult::Success;
}


bool ProgressionManager::TryUpgrade(const std::string &NodeId, std::string &ErrorMessage)
{
	// The block reason spells out which prerequisite is missing, which tier was reached
	// and what the tier costs, so the player gets one useful sentence.
	const std::string reason = UpgradeBlockReason(NodeId);
	if (!reason.empty())
	{
		ErrorMessage = "Cannot upgrade '" + NodeId + "': " + reason;
		return false;
	}

	const UpgradeResult result = TryUpgrade(NodeId);
	if (result == UpgradeResult::Success)
	{
		ErrorMessage.clear();
		return true;
	}

	ErrorMessage = "Cannot upgrade '" + NodeId + "': " + DescribeUpgradeResult(result) + ".";

	return false;
}


int ProgressionManager::GetResources() const
{
	return resources;
}


void ProgressionManager::SetResources(int Amount)
{
	resources = std::max(0, Amount);
}


void ProgressionManager::AddResources(int Amount)
{
	resources = std::max(0, resources + Amount);
}


std::string ProgressionManager::Describe(const std::string &NodeId) const
{
	const auto entry = registry.find(NodeId);
	if (entry == registry.end())
	{
		return "no skill '" + NodeId + "' in the tree.";
	}

	const SkillNode &node = entry->second;

	std::ostringstream line;
	line << node.skillId << " tier " << GetCurrentTier(NodeId) << "/" << node.maxTiers << " cost "
	     << node.resourceCostPerTier << "/tier";

	if (!node.requiredSkills.empty())
	{
		line << " requires";
		for (const auto &prerequisite : node.requiredSkills)
		{
			line << " " << prerequisite.nodeId << " " << prerequisite.requiredTier;
		}
	}

	if (IsNodeLockedOut(NodeId))
	{
		line << " [locked out by " << node.exclusiveGroupId << "]";
	}
	else if (!node.exclusiveGroupId.empty())
	{
		line << " [rival group " << node.exclusiveGroupId << "]";
	}

	return line.str();
}


std::string ProgressionManager::Summary() const
{
	int unlockedNodes = 0;
	int tiers = 0;

	for (const auto &entry : unlockedTiers)
	{
		if (entry.second > 0)
		{
			++unlockedNodes;
		}

		tiers += entry.second;
	}

	std::ostringstream line;
	line << registry.size() << " nodes, " << unlockedNodes << " unlocked, " << tiers
	     << " tiers, resources " << resources << ", locked groups";

	if (lockedExclusiveGroups.empty())
	{
		line << " none";
	}
	else
	{
		for (const auto &group : lockedExclusiveGroups)
		{
			line << " " << group;
		}
	}

	return line.str();
}


const std::map<std::string, SkillNode> &ProgressionManager::GetRegistry() const
{
	return registry;
}
} // namespace ManifestCore
