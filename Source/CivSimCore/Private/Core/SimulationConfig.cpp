#include "Core/SimulationConfig.h"
#include "YamlFieldReaders.h"
#include <exception>
#include <string>
#include <yaml-cpp/yaml.h>


namespace YAML
{
template <> struct convert<CivSimCore::NodeType>
{
	static Node encode(const CivSimCore::NodeType &nodeType)
	{
		switch (nodeType)
		{
			case CivSimCore::NodeType::Tech: return Node("Tech");
			case CivSimCore::NodeType::Skill: return Node("Skill");
		}

		return Node();
	}

	static bool decode(const Node &node, CivSimCore::NodeType &nodeType)
	{
		if (!node.IsScalar())
		{
			return false;
		}

		const std::string value = node.as<std::string>();
		if (value == "Tech")
		{
			nodeType = CivSimCore::NodeType::Tech;
			return true;
		}
		if (value == "Skill")
		{
			nodeType = CivSimCore::NodeType::Skill;
			return true;
		}

		return false;
	}
};
} // namespace YAML


namespace CivSimCore
{
namespace
{
using namespace YamlRead;

bool LoadFile(const std::string &FilePath, YAML::Node &Root, std::string &ErrorMessage)
{
	ErrorMessage.clear();

	try
	{
		Root = YAML::LoadFile(FilePath);

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		ErrorMessage = std::string("Could not read '") + FilePath + "': " + exception.what();
		return false;
	}
	catch (const std::exception &exception)
	{
		ErrorMessage = std::string("Could not read '") + FilePath + "': " + exception.what();
		return false;
	}
}


bool MarshallAges(
    const YAML::Node &Root, const std::string &FilePath, CivSimCore::Ages &Ages,
    std::string &ErrorMessage)
{
	const YAML::Node agesNode = ReadSequence(Root, "ages", ErrorMessage);
	if (!agesNode.IsSequence())
	{
		ErrorMessage = FilePath + ": " + ErrorMessage;
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : agesNode)
	{
		const std::string context = Context("ages", index++);

		const YAML::Node age = FindValue(entry, "age");
		if (!age.IsMap())
		{
			ErrorMessage =
			    FilePath + ": " + context + ": missing 'age' entry: expected a mapping keyed 'age'.";
			return false;
		}

		AgeNode ageConfig;
		if (!ReadString(age, "uniqueId", ageConfig.uniqueId, context, ErrorMessage) ||
		    !ReadString(age, "displayName", ageConfig.displayName, context, ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		ReadOptionalString(age, "description", ageConfig.description, context);

		Ages.push_back(ageConfig);
	}

	return true;
}


bool MarshallCategories(
    const YAML::Node &Root, const std::string &FilePath, CivSimCore::Categories &Categories,
    std::string &ErrorMessage)
{
	const YAML::Node categoriesNode = ReadSequence(Root, "categories", ErrorMessage);
	if (!categoriesNode.IsSequence())
	{
		ErrorMessage = FilePath + ": " + ErrorMessage;
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : categoriesNode)
	{
		const std::string context = Context("categories", index++);

		const YAML::Node category = FindValue(entry, "category");
		if (!category.IsMap())
		{
			ErrorMessage = FilePath + ": " + context +
			               ": missing 'category' entry: expected a mapping keyed 'category'.";
			return false;
		}

		CategoryNode categoryConfig;
		if (!ReadString(category, "uniqueId", categoryConfig.uniqueId, context, ErrorMessage) ||
		    !ReadString(category, "displayName", categoryConfig.displayName, context, ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		// The type and the description fall back to the category identity.
		ReadOptionalString(category, "type", categoryConfig.type, context);
		if (categoryConfig.type.empty())
		{
			categoryConfig.type = categoryConfig.uniqueId;
		}

		ReadOptionalString(category, "description", categoryConfig.description, context);
		if (categoryConfig.description.empty())
		{
			categoryConfig.description = categoryConfig.displayName;
		}

		Categories.push_back(categoryConfig);
	}

	return true;
}


bool MarshallItems(
    const YAML::Node &Root, const std::string &FilePath, CivSimCore::Items &Items,
    std::string &ErrorMessage)
{
	const YAML::Node itemsNode = ReadSequence(Root, "items", ErrorMessage);
	if (!itemsNode.IsSequence())
	{
		ErrorMessage = FilePath + ": " + ErrorMessage;
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : itemsNode)
	{
		const std::string context = Context("items", index++);

		const YAML::Node item = FindValue(entry, "item");
		if (!item.IsMap())
		{
			ErrorMessage =
			    FilePath + ": " + context + ": missing 'item' entry: expected a mapping keyed 'item'.";
			return false;
		}

		ItemNode itemConfig;
		if (!ReadString(item, "uniqueId", itemConfig.uniqueId, context, ErrorMessage) ||
		    !ReadString(item, "displayName", itemConfig.displayName, context, ErrorMessage) ||
		    !ReadString(item, "basePrice", itemConfig.basePrice, context, ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		ReadOptionalString(item, "categoryId", itemConfig.categoryId, context);
		ReadOptionalString(item, "ageId", itemConfig.ageId, context);
		ReadOptionalString(item, "description", itemConfig.description, context);
		ReadOptionalString(item, "unitsPerCubicMetre", itemConfig.unitsPerCubicMetre, context);
		ReadOptionalString(item, "unitType", itemConfig.unitType, context);
		ReadOptionalString(item, "density", itemConfig.density, context);
		ReadOptionalString(item, "packFactor", itemConfig.packFactor, context);

		Items.push_back(itemConfig);
	}

	return true;
}


bool MarshallSkills(
    const YAML::Node &Root, const std::string &FilePath, CivSimCore::Skills &Skills,
    std::string &ErrorMessage)
{
	const YAML::Node skillsNode = ReadSequence(Root, "skills", ErrorMessage);
	if (!skillsNode.IsSequence())
	{
		ErrorMessage = FilePath + ": " + ErrorMessage;
		return false;
	}

	std::size_t index = 0;
	for (const auto &entry : skillsNode)
	{
		const std::string context = Context("skills", index++);

		const YAML::Node skill = FindValue(entry, "skill");
		if (!skill.IsMap())
		{
			ErrorMessage = FilePath + ": " + context +
			               ": missing 'skill' entry: expected a mapping keyed 'skill'.";
			return false;
		}

		SkillNode skillConfig;
		if (!ReadString(skill, "uniqueId", skillConfig.uniqueId, context, ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		ReadOptionalString(skill, "displayName", skillConfig.displayName, context);
		if (skillConfig.displayName.empty())
		{
			skillConfig.displayName = skillConfig.uniqueId;
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
				ErrorMessage = FilePath + ": " + context +
				               ": 'nodeType' must be 'Tech' or 'Skill', got '" + nodeType + "'.";
				return false;
			}
		}

		if (!ReadInt(skill, "maxTiers", skillConfig.maxTiers, false, 1, context, ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		if (!ReadInt(
		        skill, "resourceCostPerTier", skillConfig.resourceCostPerTier, false, 1, context,
		        ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		const YAML::Node requiresNode = FindValue(skill, "requires");
		if (requiresNode.IsDefined() && !requiresNode.IsNull() && !requiresNode.IsSequence())
		{
			ErrorMessage = FilePath + ": " + context + ": 'requires' must be a YAML sequence.";
			return false;
		}

		if (requiresNode)
		{
			std::size_t prereqIndex = 0;
			for (const auto &req : requiresNode)
			{
				const std::string prereqContext =
				    context + ".requires[" + std::to_string(prereqIndex++) + "]";

				Prerequisite prerequisite;
				if (!ReadString(req, "nodeId", prerequisite.nodeId, prereqContext, ErrorMessage))
				{
					ErrorMessage = FilePath + ": " + ErrorMessage;
					return false;
				}

				if (!ReadInt(
				        req, "requiredTier", prerequisite.requiredTier, false, 1, prereqContext,
				        ErrorMessage))
				{
					ErrorMessage = FilePath + ": " + ErrorMessage;
					return false;
				}

				skillConfig.requiredSkills.push_back(prerequisite);
			}
		}

		// targetEffectID and exclusiveGroupId are optional: a null in the data means
		// "not set", not the string "null".
		ReadOptionalString(skill, "targetEffectID", skillConfig.targetEffectID, context);
		ReadOptionalString(skill, "exclusiveGroupId", skillConfig.exclusiveGroupId, context);

		if (!ReadFloat(skill, "gridX", skillConfig.gridX, false, 0.0f, context, ErrorMessage) ||
		    !ReadFloat(skill, "gridY", skillConfig.gridY, false, 0.0f, context, ErrorMessage))
		{
			ErrorMessage = FilePath + ": " + ErrorMessage;
			return false;
		}

		Skills.push_back(skillConfig);
	}

	return true;
}

// A yaml-cpp surprise that no reader predicted must be a load error naming the file,
// never an exception escaping into the game.
template <typename Function, typename Output>
bool MarshallGuarded(
    const std::string &FilePath, Function FunctionToRun, const YAML::Node &Root, Output &Out,
    std::string &ErrorMessage)
{
	try
	{
		ErrorMessage.clear();

		if (!FunctionToRun(Root, FilePath, Out, ErrorMessage))
		{
			Out.clear();
			return false;
		}

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		Out.clear();
		ErrorMessage = FilePath + ": " + exception.what();
		return false;
	}
	catch (const std::exception &exception)
	{
		Out.clear();
		ErrorMessage = FilePath + ": " + exception.what();
		return false;
	}
}
} // namespace


bool LoadConfigAges(const std::string &FilePath, Ages &Ages, std::string &ErrorMessage)
{
	YAML::Node rootNode;
	if (!LoadFile(FilePath, rootNode, ErrorMessage))
	{
		return false;
	}

	Ages.clear();

	return MarshallGuarded(FilePath, MarshallAges, rootNode, Ages, ErrorMessage);
}


bool LoadConfigCategories(
    const std::string &FilePath, Categories &Categories, std::string &ErrorMessage)
{
	YAML::Node rootNode;
	if (!LoadFile(FilePath, rootNode, ErrorMessage))
	{
		return false;
	}

	Categories.clear();

	return MarshallGuarded(FilePath, MarshallCategories, rootNode, Categories, ErrorMessage);
}


bool LoadConfigGameMode(
    const std::string &FilePath, GameModeNode &GameModeConfig, std::string &ErrorMessage)
{
	YAML::Node rootNode;
	if (!LoadFile(FilePath, rootNode, ErrorMessage))
	{
		return false;
	}

	GameModeConfig.items.clear();
	GameModeConfig.skills.clear();

	try
	{
		ErrorMessage.clear();

		if (!MarshallItems(rootNode, FilePath, GameModeConfig.items, ErrorMessage) ||
		    !MarshallSkills(rootNode, FilePath, GameModeConfig.skills, ErrorMessage))
		{
			GameModeConfig.items.clear();
			GameModeConfig.skills.clear();
			return false;
		}

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		GameModeConfig.items.clear();
		GameModeConfig.skills.clear();
		ErrorMessage = FilePath + ": " + exception.what();
		return false;
	}
	catch (const std::exception &exception)
	{
		GameModeConfig.items.clear();
		GameModeConfig.skills.clear();
		ErrorMessage = FilePath + ": " + exception.what();
		return false;
	}
}
} // namespace CivSimCore
