#include <manifest/config/SimulationConfig.h>

#include <exception>
#include <format>
#include <yaml-cpp/yaml.h>

#include <string>

#include <manifest/config/YamlFieldReaders.h>


namespace YAML
{
template <> struct convert<Manifest::Core::NodeType>
{
	static Node encode(const Manifest::Core::NodeType &nodeType)
	{
		switch (nodeType)
		{
			case Manifest::Core::NodeType::Tech: return Node("Tech");
			case Manifest::Core::NodeType::Skill: return Node("Skill");
		}

		return {};
	}

	static bool decode(const Node &node, Manifest::Core::NodeType &nodeType)
	{
		if (!node.IsScalar())
		{
			return false;
		}

		const auto value = node.as<std::string>();
		if (value == "Tech")
		{
			nodeType = Manifest::Core::NodeType::Tech;
			return true;
		}
		if (value == "Skill")
		{
			nodeType = Manifest::Core::NodeType::Skill;
			return true;
		}

		return false;
	}
};
} // namespace YAML


namespace Manifest::Core
{
namespace
{
using namespace YamlRead;

bool LoadFile(const std::string &filePath, YAML::Node &root, std::string &errorMessage)
{
	errorMessage.clear();

	try
	{
		root = YAML::LoadFile(filePath);

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		errorMessage =
		    std::format("{}{}': {}", std::string("Could not read '"), filePath, exception.what());
		return false;
	}
	catch (const std::exception &exception)
	{
		errorMessage =
		    std::format("{}{}': {}", std::string("Could not read '"), filePath, exception.what());
		return false;
	}
}


bool MarshallAges(
    const YAML::Node &root, const std::string &filePath, Manifest::Core::Ages &ages,
    std::string &errorMessage)
{
	const YAML::Node agesNode = ReadSequence(root, "ages", errorMessage);
	if (!agesNode.IsSequence())
	{
		errorMessage = std::format("{}: {}", filePath, errorMessage);
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : agesNode)
	{
		const std::string context = Context("ages", index++);

		const YAML::Node age = FindValue(entry, "age");
		if (!age.IsMap())
		{
			errorMessage = std::format(
			    "{}: {}: missing 'age' entry: expected a mapping keyed 'age'.", filePath, context);
			return false;
		}

		AgeNode ageConfig;
		if (!ReadString(age, "ageId", ageConfig.ageId, context, errorMessage) ||
		    !ReadString(age, "displayName", ageConfig.displayName, context, errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		ReadOptionalString(age, "description", ageConfig.description, context);

		ages.push_back(ageConfig);
	}

	return true;
}


bool MarshallCategories(
    const YAML::Node &root, const std::string &filePath, Manifest::Core::Categories &categories,
    std::string &errorMessage)
{
	const YAML::Node categoriesNode = ReadSequence(root, "categories", errorMessage);
	if (!categoriesNode.IsSequence())
	{
		errorMessage = std::format("{}: {}", filePath, errorMessage);
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : categoriesNode)
	{
		const std::string context = Context("categories", index++);

		const YAML::Node category = FindValue(entry, "category");
		if (!category.IsMap())
		{
			errorMessage = std::format(
			    "{}: {}: missing 'category' entry: expected a mapping keyed 'category'.", filePath,
			    context);
			return false;
		}

		CategoryNode categoryConfig;
		if (!ReadString(category, "categoryId", categoryConfig.categoryId, context, errorMessage) ||
		    !ReadString(category, "displayName", categoryConfig.displayName, context, errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		// The type and the description fall back to the category identity.
		ReadOptionalString(category, "type", categoryConfig.type, context);
		if (categoryConfig.type.empty())
		{
			categoryConfig.type = categoryConfig.categoryId;
		}

		ReadOptionalString(category, "description", categoryConfig.description, context);
		if (categoryConfig.description.empty())
		{
			categoryConfig.description = categoryConfig.displayName;
		}

		categories.push_back(categoryConfig);
	}

	return true;
}


bool MarshallItems(
    const YAML::Node &root, const std::string &filePath, Manifest::Core::Items &items,
    std::string &errorMessage)
{
	const YAML::Node itemsNode = ReadSequence(root, "items", errorMessage);
	if (!itemsNode.IsSequence())
	{
		errorMessage = std::format("{}: {}", filePath, errorMessage);
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : itemsNode)
	{
		const std::string context = Context("items", index++);

		const YAML::Node item = FindValue(entry, "item");
		if (!item.IsMap())
		{
			errorMessage = std::format(
			    "{}: {}: missing 'item' entry: expected a mapping keyed 'item'.", filePath, context);
			return false;
		}

		ItemNode itemConfig;
		if (!ReadString(item, "itemId", itemConfig.itemId, context, errorMessage) ||
		    !ReadString(item, "displayName", itemConfig.displayName, context, errorMessage) ||
		    !ReadString(item, "basePrice", itemConfig.basePrice, context, errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		ReadOptionalString(item, "categoryId", itemConfig.categoryId, context);
		ReadOptionalString(item, "ageId", itemConfig.ageId, context);
		ReadOptionalString(item, "description", itemConfig.description, context);
		ReadOptionalString(item, "unitsPerCubicMetre", itemConfig.unitsPerCubicMetre, context);
		ReadOptionalString(item, "unitType", itemConfig.unitType, context);
		ReadOptionalString(item, "density", itemConfig.density, context);
		ReadOptionalString(item, "packFactor", itemConfig.packFactor, context);

		items.push_back(itemConfig);
	}

	return true;
}


bool MarshallSkills(
    const YAML::Node &root, const std::string &filePath, Manifest::Core::Skills &skills,
    std::string &errorMessage)
{
	const YAML::Node skillsNode = ReadSequence(root, "skills", errorMessage);
	if (!skillsNode.IsSequence())
	{
		errorMessage = std::format("{}: {}", filePath, errorMessage);
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : skillsNode)
	{
		const std::string context = Context("skills", index++);

		const YAML::Node skill = FindValue(entry, "skill");
		if (!skill.IsMap())
		{
			errorMessage = std::format(
			    "{}: {}: missing 'skill' entry: expected a mapping keyed 'skill'.", filePath, context);
			return false;
		}

		SkillNode skillConfig;
		if (!ReadString(skill, "skillId", skillConfig.skillId, context, errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		ReadOptionalString(skill, "displayName", skillConfig.displayName, context);
		if (skillConfig.displayName.empty())
		{
			skillConfig.displayName = skillConfig.skillId;
		}

		ReadOptionalString(skill, "description", skillConfig.description, context);

		const YAML::Node nodeTypeNode = FindValue(skill, "nodeType");
		const std::string nodeType = nodeTypeNode.IsDefined() && nodeTypeNode.IsScalar()
		                                 ? nodeTypeNode.as<std::string>()
		                                 : std::string();
		if (nodeType == "Tech")
		{
			skillConfig.nodeType = NodeType::Tech;
		}
		else
		{
			skillConfig.nodeType = NodeType::Skill;
			if (!nodeType.empty() && nodeType != "Skill")
			{
				errorMessage = std::format(
				    "{}: {}: 'nodeType' must be 'Tech' or 'Skill', got '{}'.", filePath, context,
				    nodeType);
				return false;
			}
		}

		if (!ReadInt(skill, "maxTiers", skillConfig.maxTiers, false, 1, context, errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		if (!ReadInt(
		        skill, "resourceCostPerTier", skillConfig.resourceCostPerTier, false, 1, context,
		        errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		const YAML::Node requiresNode = FindValue(skill, "requires");
		if (requiresNode.IsDefined() && !requiresNode.IsNull() && !requiresNode.IsSequence())
		{
			errorMessage =
			    std::format("{}: {}: 'requires' must be a YAML sequence.", filePath, context);
			return false;
		}

		if (requiresNode)
		{
			std::size_t prereqIndex = 0;
			for (const auto &req : requiresNode)
			{
				const std::string prereqContext =
				    std::format("{}.requires[{}]", context, std::to_string(prereqIndex++));

				Prerequisite prerequisite;
				if (!ReadString(req, "nodeId", prerequisite.nodeId, prereqContext, errorMessage))
				{
					errorMessage = std::format("{}: {}", filePath, errorMessage);
					return false;
				}

				if (!ReadInt(
				        req, "requiredTier", prerequisite.requiredTier, false, 1, prereqContext,
				        errorMessage))
				{
					errorMessage = std::format("{}: {}", filePath, errorMessage);
					return false;
				}

				skillConfig.requiredSkills.push_back(prerequisite);
			}
		}

		// targetEffectID and exclusiveGroupId are optional: a null in the data means
		// "not set", not the string "null".
		ReadOptionalString(skill, "targetEffectID", skillConfig.targetEffectID, context);
		ReadOptionalString(skill, "exclusiveGroupId", skillConfig.exclusiveGroupId, context);

		if (!ReadFloat(skill, "gridX", skillConfig.gridX, false, 0.0f, context, errorMessage) ||
		    !ReadFloat(skill, "gridY", skillConfig.gridY, false, 0.0f, context, errorMessage))
		{
			errorMessage = std::format("{}: {}", filePath, errorMessage);
			return false;
		}

		skills.push_back(skillConfig);
	}

	return true;
}

// A yaml-cpp surprise that no reader predicted must be a load error naming the file,
// never an exception escaping into the game.
template <typename Function, typename Output>
bool MarshallGuarded(
    const std::string &filePath, Function functionToRun, const YAML::Node &root, Output &out,
    std::string &errorMessage)
{
	try
	{
		errorMessage.clear();

		if (!functionToRun(root, filePath, out, errorMessage))
		{
			out.clear();
			return false;
		}

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		out.clear();
		errorMessage = std::format("{}: {}", filePath, exception.what());
		return false;
	}
	catch (const std::exception &exception)
	{
		out.clear();
		errorMessage = std::format("{}: {}", filePath, exception.what());
		return false;
	}
}
} // namespace


bool LoadConfigAges(const std::string &filePath, Ages &ages, std::string &errorMessage)
{
	YAML::Node rootNode;
	if (!LoadFile(filePath, rootNode, errorMessage))
	{
		return false;
	}

	ages.clear();

	return MarshallGuarded(filePath, MarshallAges, rootNode, ages, errorMessage);
}


bool LoadConfigCategories(
    const std::string &filePath, Categories &categories, std::string &errorMessage)
{
	YAML::Node rootNode;
	if (!LoadFile(filePath, rootNode, errorMessage))
	{
		return false;
	}

	categories.clear();

	return MarshallGuarded(filePath, MarshallCategories, rootNode, categories, errorMessage);
}


bool LoadConfigGameMode(
    const std::string &filePath, GameModeNode &gameModeConfig, std::string &errorMessage)
{
	YAML::Node rootNode;
	if (!LoadFile(filePath, rootNode, errorMessage))
	{
		return false;
	}

	gameModeConfig.items.clear();
	gameModeConfig.skills.clear();

	try
	{
		errorMessage.clear();

		if (!MarshallItems(rootNode, filePath, gameModeConfig.items, errorMessage) ||
		    !MarshallSkills(rootNode, filePath, gameModeConfig.skills, errorMessage))
		{
			gameModeConfig.items.clear();
			gameModeConfig.skills.clear();
			return false;
		}

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		gameModeConfig.items.clear();
		gameModeConfig.skills.clear();
		errorMessage = std::format("{}: {}", filePath, exception.what());
		return false;
	}
	catch (const std::exception &exception)
	{
		gameModeConfig.items.clear();
		gameModeConfig.skills.clear();
		errorMessage = std::format("{}: {}", filePath, exception.what());
		return false;
	}
}
} // namespace Manifest::Core
