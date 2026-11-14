#include <manifest/progression/ProgressionManager.h>

#include <algorithm>
#include <format>
#include <sstream>
#include <string>
#include <utility>


namespace Manifest::Core
{
namespace
{
enum class Visit
{
	Unvisited,
	Visiting,
	Visited
};

std::string Join(const std::vector<std::string> &parts)
{
	std::string joined;

	for (std::size_t index = 0; index < parts.size(); ++index)
	{
		if (index != 0)
		{
			joined += " -> ";
		}

		joined += parts[index];
	}

	return joined;
}

// Depth first search with three colours: a node that is still on the stack is part of
// a cycle, so the path from that node to the current one is reported.
bool FindCycle(
    const std::map<std::string, SkillNode> &registry, const std::string &nodeId,
    std::vector<std::string> &stack, std::map<std::string, Visit> &states, std::string &cycle)
{
	states[nodeId] = Visit::Visiting;
	stack.push_back(nodeId);

	const auto self = registry.find(nodeId);
	for (const auto &prerequisite : self->second.requiredSkills)
	{
		const auto target = registry.find(prerequisite.nodeId);
		if (target == registry.end())
		{
			// Reported as a missing prerequisite by Validate.
			continue;
		}

		if (states[target->first] == Visit::Visiting)
		{
			const auto start = std::find(stack.begin(), stack.end(), target->first);
			std::vector<std::string> path(start, stack.end());
			path.push_back(prerequisite.nodeId);

			cycle = Join(path);

			return true;
		}

		if (states[target->first] == Visit::Unvisited &&
		    FindCycle(registry, target->first, stack, states, cycle))
		{
			return true;
		}
	}

	stack.pop_back();
	states[nodeId] = Visit::Visited;

	return false;
}
} // namespace


const char *DescribeUpgradeResult(UpgradeResult result)
{
	switch (result)
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


bool ProgressionManager::RegisterNode(const SkillNode &node, std::string &errorMessage)
{
	errorMessage.clear();

	if (node.skillId.empty())
	{
		errorMessage = "a skill node needs a skillId.";
		return false;
	}

	if (m_registry.find(node.skillId) != m_registry.end())
	{
		errorMessage = std::format("skill '{}' is registered twice.", node.skillId);
		return false;
	}

	if (node.maxTiers < 1)
	{
		errorMessage = std::format(
		    "skill '{}' has maxTiers {}: at least 1 is required.", node.skillId,
		    std::to_string(node.maxTiers));
		return false;
	}

	if (node.resourceCostPerTier < 0)
	{
		errorMessage = std::format(
		    "skill '{}' has a negative resourceCostPerTier ({}).", node.skillId,
		    std::to_string(node.resourceCostPerTier));
		return false;
	}

	for (const auto &prerequisite : node.requiredSkills)
	{
		if (prerequisite.nodeId == node.skillId)
		{
			errorMessage = std::format("skill '{}' requires itself.", node.skillId);
			return false;
		}

		if (prerequisite.requiredTier < 1)
		{
			errorMessage = std::format(
			    "skill '{}' requires tier {} of '{}': a prerequisite needs at least tier 1.",
			    node.skillId, std::to_string(prerequisite.requiredTier), prerequisite.nodeId);
			return false;
		}
	}

	m_registry[node.skillId] = node;

	return true;
}


bool ProgressionManager::Build(const Skills &nodes, std::string &errorMessage)
{
	errorMessage.clear();

	m_registry.clear();
	m_unlockedTiers.clear();
	m_lockedExclusiveGroups.clear();
	m_resources = 0;

	for (const auto &node : nodes)
	{
		if (!RegisterNode(node, errorMessage))
		{
			m_registry.clear();
			return false;
		}
	}

	return Validate(errorMessage);
}


bool ProgressionManager::Validate(std::string &errorMessage) const
{
	errorMessage.clear();

	for (const auto &entry : m_registry)
	{
		const SkillNode &node = entry.second;

		for (const auto &prerequisite : node.requiredSkills)
		{
			const auto target = m_registry.find(prerequisite.nodeId);
			if (target == m_registry.end())
			{
				errorMessage = std::format(
				    "skill '{}' requires unknown node '{}'.", entry.first, prerequisite.nodeId);
				return false;
			}

			if (prerequisite.requiredTier > target->second.maxTiers)
			{
				errorMessage = std::format(
				    "skill '{}' requires tier {} of '{}', which has maxTiers {}.", entry.first,
				    std::to_string(prerequisite.requiredTier), prerequisite.nodeId,
				    std::to_string(target->second.maxTiers));
				return false;
			}

			if (!node.exclusiveGroupId.empty() &&
			    node.exclusiveGroupId == target->second.exclusiveGroupId)
			{
				errorMessage = std::format(
				    "skill '{}' requires '{}', which competes with it in exclusive group '{}': that "
				    "branch can never be researched.",
				    entry.first, prerequisite.nodeId, node.exclusiveGroupId);
				return false;
			}
		}
	}

	std::map<std::string, Visit> states;
	for (const auto &entry : m_registry)
	{
		states[entry.first] = Visit::Unvisited;
	}

	for (const auto &entry : m_registry)
	{
		if (states[entry.first] != Visit::Unvisited)
		{
			continue;
		}

		std::vector<std::string> stack;
		std::string cycle;
		if (FindCycle(m_registry, entry.first, stack, states, cycle))
		{
			errorMessage = std::format("prerequisite cycle: {}.", cycle);
			return false;
		}
	}

	return true;
}


bool ProgressionManager::HasNode(const std::string &nodeId) const
{
	return m_registry.find(nodeId) != m_registry.end();
}


std::vector<std::string> ProgressionManager::NodeIds() const
{
	std::vector<std::string> ids;
	ids.reserve(m_registry.size());

	for (const auto &entry : m_registry)
	{
		ids.push_back(entry.first);
	}

	return ids;
}


int ProgressionManager::GetCurrentTier(const std::string &nodeId) const
{
	const auto entry = m_unlockedTiers.find(nodeId);

	return entry != m_unlockedTiers.end() ? entry->second : 0;
}


int ProgressionManager::GetMaxTiers(const std::string &nodeId) const
{
	const auto entry = m_registry.find(nodeId);

	return entry != m_registry.end() ? entry->second.maxTiers : 0;
}


bool ProgressionManager::IsGroupLocked(const std::string &groupId) const
{
	return !groupId.empty() && m_lockedExclusiveGroups.find(groupId) != m_lockedExclusiveGroups.end();
}


bool ProgressionManager::IsNodeLockedOut(const std::string &nodeId) const
{
	const auto entry = m_registry.find(nodeId);
	if (entry == m_registry.end())
	{
		return false;
	}

	// Once a node of a group is unlocked the rivals are locked out. A node already
	// unlocked stays upgradeable: only the first tier of a rival is blocked.
	if (GetCurrentTier(nodeId) > 0)
	{
		return false;
	}

	return IsGroupLocked(entry->second.exclusiveGroupId);
}


bool ProgressionManager::CanUpgrade(const std::string &nodeId) const
{
	return UpgradeBlockReason(nodeId).empty();
}


std::string ProgressionManager::UpgradeBlockReason(const std::string &nodeId) const
{
	const auto entry = m_registry.find(nodeId);
	if (entry == m_registry.end())
	{
		return "no skill '" + nodeId + "' in the tree.";
	}

	const SkillNode &node = entry->second;
	const int currentTier = GetCurrentTier(nodeId);

	if (currentTier >= node.maxTiers)
	{
		return "already at tier " + std::to_string(node.maxTiers) + " of " + node.skillId + ".";
	}

	if (IsNodeLockedOut(nodeId))
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

	if (node.resourceCostPerTier > m_resources)
	{
		return "costs " + std::to_string(node.resourceCostPerTier) + " resources, only " +
		       std::to_string(m_resources) + " available.";
	}

	return {};
}


UpgradeResult ProgressionManager::TryUpgrade(const std::string &nodeId)
{
	const auto entry = m_registry.find(nodeId);
	if (entry == m_registry.end())
	{
		return UpgradeResult::UnknownNode;
	}

	const SkillNode &node = entry->second;
	const int currentTier = GetCurrentTier(nodeId);

	if (currentTier >= node.maxTiers)
	{
		return UpgradeResult::MaxTier;
	}

	if (IsNodeLockedOut(nodeId))
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

	if (node.resourceCostPerTier > m_resources)
	{
		return UpgradeResult::InsufficientResources;
	}

	m_resources -= node.resourceCostPerTier;

	const int tier = currentTier + 1;
	m_unlockedTiers[nodeId] = tier;

	// The first tier of a node locks out every rival in its exclusive group.
	if (tier == 1 && !node.exclusiveGroupId.empty())
	{
		m_lockedExclusiveGroups.insert(node.exclusiveGroupId);
	}

	return UpgradeResult::Success;
}


bool ProgressionManager::TryUpgrade(const std::string &nodeId, std::string &errorMessage)
{
	// The block reason spells out which prerequisite is missing, which tier was reached
	// and what the tier costs, so the player gets one useful sentence.
	const std::string reason = UpgradeBlockReason(nodeId);
	if (!reason.empty())
	{
		errorMessage = std::format("Cannot upgrade '{}': {}", nodeId, reason);
		return false;
	}

	const UpgradeResult result = TryUpgrade(nodeId);
	if (result == UpgradeResult::Success)
	{
		errorMessage.clear();
		return true;
	}

	errorMessage = std::format("Cannot upgrade '{}': {}.", nodeId, DescribeUpgradeResult(result));

	return false;
}


int ProgressionManager::GetResources() const
{
	return m_resources;
}


void ProgressionManager::SetResources(int amount)
{
	m_resources = std::max(0, amount);
}


void ProgressionManager::AddResources(int amount)
{
	m_resources = std::max(0, m_resources + amount);
}


std::string ProgressionManager::Describe(const std::string &nodeId) const
{
	const auto entry = m_registry.find(nodeId);
	if (entry == m_registry.end())
	{
		return "no skill '" + nodeId + "' in the tree.";
	}

	const SkillNode &node = entry->second;

	std::ostringstream line;
	line << node.skillId << " tier " << GetCurrentTier(nodeId) << "/" << node.maxTiers << " cost "
	     << node.resourceCostPerTier << "/tier";

	if (!node.requiredSkills.empty())
	{
		line << " requires";
		for (const auto &prerequisite : node.requiredSkills)
		{
			line << " " << prerequisite.nodeId << " " << prerequisite.requiredTier;
		}
	}

	if (IsNodeLockedOut(nodeId))
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

	for (const auto &entry : m_unlockedTiers)
	{
		if (entry.second > 0)
		{
			++unlockedNodes;
		}

		tiers += entry.second;
	}

	std::ostringstream line;
	line << m_registry.size() << " nodes, " << unlockedNodes << " unlocked, " << tiers
	     << " tiers, resources " << m_resources << ", locked groups";

	if (m_lockedExclusiveGroups.empty())
	{
		line << " none";
	}
	else
	{
		for (const auto &group : m_lockedExclusiveGroups)
		{
			line << " " << group;
		}
	}

	return line.str();
}


const std::map<std::string, SkillNode> &ProgressionManager::GetRegistry() const
{
	return m_registry;
}
} // namespace Manifest::Core
