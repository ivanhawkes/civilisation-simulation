#include "Core/SimulationConfig.h"
#include <exception>
#include <iostream>
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
bool LoadConfig(const std::string &FilePath, YAML::Node &Node, std::string &ErrorMessage)
{
	// Assume no error initially.
	ErrorMessage.clear();

	try
	{
		// Load the complete YAML config from the generated document.
		Node = YAML::LoadFile(FilePath);

		return true;
	}
	catch (const YAML::Exception &exception)
	{
		ErrorMessage = exception.what();
		return false;
	}
	catch (const std::exception &exception)
	{
		ErrorMessage = exception.what();
		return false;
	}
}


bool MarshallAges(YAML::Node &rootNode, std::string &ErrorMessage, CivSimCore::Ages &Ages)
{
	try
	{
		YAML::Node agesNode = rootNode["ages"];

		if (!agesNode || !agesNode.IsSequence())
		{
			ErrorMessage = "'ages' must be a YAML sequence.";
			return false;
		}

		for (const auto &AgeEntry : agesNode)
		{
			YAML::Node age = AgeEntry["age"];

			if (!age)
			{
				ErrorMessage = "Missing 'age' entry.";
				return false;
			}

			// Marshal the YAML node into an AgeNode structure.
			AgeNode ageConfig;
			ageConfig.uniqueId = age["uniqueId"].as<std::string>();
			ageConfig.displayName = age["displayName"].as<std::string>();
			ageConfig.description = age["description"].as<std::string>();
			Ages.push_back(ageConfig);
		}

		return true;
	}
	catch (const YAML::Exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
	catch (const std::exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
}


bool LoadConfigAges(const std::string &FilePath, Ages &Ages, std::string &ErrorMessage)
{
	YAML::Node rootNode;
	if (!LoadConfig(FilePath, rootNode, ErrorMessage))
	{
		return false;
	}

	return MarshallAges(rootNode, ErrorMessage, Ages);
}


bool MarshallCategories(
    YAML::Node &rootNode, std::string &ErrorMessage, CivSimCore::Categories &Categories)
{
	try
	{
		YAML::Node categoriesNode = rootNode["categories"];

		if (!categoriesNode || !categoriesNode.IsSequence())
		{
			ErrorMessage = "'categories' must be a YAML sequence.";
			return false;
		}

		for (const auto &entry : categoriesNode)
		{
			YAML::Node Category = entry["category"];

			if (!Category)
			{
				ErrorMessage = "Missing 'category' entry.";
				return false;
			}

			// Marshal the YAML node into a CategoryNode structure.
			CategoryNode categoryConfig;
			categoryConfig.uniqueId = Category["uniqueId"].as<std::string>();
			categoryConfig.displayName = Category["displayName"].as<std::string>();
			categoryConfig.type = Category["type"].as<std::string>();
			categoryConfig.description = Category["description"].as<std::string>();
			Categories.push_back(categoryConfig);
		}

		return true;
	}
	catch (const YAML::Exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
	catch (const std::exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
}


bool LoadConfigCategories(
    const std::string &FilePath, Categories &Categories, std::string &ErrorMessage)
{
	YAML::Node rootNode;
	if (!LoadConfig(FilePath, rootNode, ErrorMessage))
	{
		return false;
	}

	return MarshallCategories(rootNode, ErrorMessage, Categories);
}


bool MarshallItems(
    YAML::Node &rootNode, std::string &ErrorMessage, CivSimCore::Items &Items)
{
	try
	{
		YAML::Node itemsNode = rootNode["items"];

		if (!itemsNode || !itemsNode.IsSequence())
		{
			ErrorMessage = "'items' must be a YAML sequence.";
			return false;
		}

		for (const auto &entry : itemsNode)
		{
			YAML::Node Item = entry["item"];

			if (!Item)
			{
				ErrorMessage = "Missing 'item' entry.";
				return false;
			}

			// Marshal the YAML node into an ItemNode structure.
			ItemNode itemConfig;
			itemConfig.uniqueId = Item["uniqueId"].as<std::string>();
			itemConfig.displayName = Item["displayName"].as<std::string>();
			itemConfig.description = Item["description"].as<std::string>();
			itemConfig.unitsPerCubicMetre = Item["unitsPerCubicMetre"].as<std::string>();
			itemConfig.basePrice = Item["basePrice"].as<std::string>();
			itemConfig.unitType = Item["unitType"].as<std::string>();
			itemConfig.density = Item["density"].as<std::string>();
			itemConfig.packFactor = Item["packFactor"].as<std::string>();
			Items.push_back(itemConfig);
		}

		return true;
	}
	catch (const YAML::Exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
	catch (const std::exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
}


bool MarshallSkills(
    YAML::Node &rootNode, std::string &ErrorMessage, CivSimCore::Skills &Skills)
{
	try
	{
		YAML::Node skillsNode = rootNode["skills"];

		if (!skillsNode || !skillsNode.IsSequence())
		{
			ErrorMessage = "'skills' must be a YAML sequence.";
			return false;
		}

		for (const auto &SkillEntry : skillsNode)
		{
			YAML::Node skill = SkillEntry["skill"];

			if (!skill)
			{
				ErrorMessage = "Missing 'skill' entry.";
				return false;
			}

			// Marshal the YAML node into a SkillNode structure.
			SkillNode skillConfig;
			skillConfig.uniqueId = skill["uniqueId"].as<std::string>();
			skillConfig.displayName = skill["displayName"].as<std::string>();
			skillConfig.description = skill["description"].as<std::string>();
			skillConfig.nodeType = skill["nodeType"].as<NodeType>();

			for (const auto &req : skill["requires"])
			{
				Prerequisite prerequisite;
				prerequisite.nodeId = req["nodeId"].as<std::string>();
				prerequisite.requiredTier = req["requiredTier"].as<int>();
				skillConfig.requiredSkills.push_back(prerequisite);
			}

			skillConfig.resourceCostPerTier = skill["resourceCostPerTier"].as<int>();
			skillConfig.targetEffectID = skill["targetEffectID"].as<std::string>();
			skillConfig.exclusiveGroupId = skill["exclusiveGroupId"].as<std::string>();
			skillConfig.gridX = skill["gridX"].as<float>();
			skillConfig.gridY = skill["gridY"].as<float>();
			Skills.push_back(skillConfig);
		}

		return true;
	}
	catch (const YAML::Exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
	catch (const std::exception &Exception)
	{
		ErrorMessage = Exception.what();
		return false;
	}
}

bool LoadConfigGameMode(
    const std::string &FilePath, GameModeNode &GameModeNode, std::string &ErrorMessage)
{
	YAML::Node rootNode;
	if (!LoadConfig(FilePath, rootNode, ErrorMessage))
	{
		return false;
	}

	return MarshallItems(rootNode, ErrorMessage, GameModeNode.items) &&
	       MarshallSkills(rootNode, ErrorMessage, GameModeNode.skills);
}
} // namespace CivSimCore
