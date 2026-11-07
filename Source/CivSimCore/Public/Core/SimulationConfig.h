#pragma once

#include <string>
#include <vector>


namespace CivSimCore
{
enum class NodeType
{
	Tech,
	Skill
};

// Defines a prerequisite with a specific tier requirement.
struct Prerequisite
{
	std::string nodeId;
	int requiredTier{1};
};

struct AgeNode
{
	// Unique identifier for the age.
	std::string uniqueId;

	// Display name of the age.
	std::string displayName;

	// Description of the age.
	std::string description;
};

using Ages = std::vector<AgeNode>;

struct CategoryNode
{
	// Unique identifier for the category.
	std::string uniqueId;

	// Display name of the category.
	std::string displayName;

	// Type of the category.
	std::string type;

	// Description of the category.
	std::string description;
};

using Categories = std::vector<CategoryNode>;

struct ItemNode
{
	// Unique identifier for the item.
	std::string uniqueId;

	// Display name of the item.
	std::string displayName;

	// Description of the item.
	std::string description;

	// Category and age links. Optional: the merged documents in data/core-config
	// resolve the category and age anchors through '<<', and the item's own uniqueId
	// overwrites the inherited one, so these fields arrive empty. They are read when
	// present so a data set that carries the links works unchanged.
	std::string categoryId;
	std::string ageId;

	// The number of units that can fit into one cubic metre of storage.
	std::string unitsPerCubicMetre;

	// Universal base price of the item.
	std::string basePrice;

	// The type of units of measurement for this type of item.
	std::string unitType;

	// Specific gravity of the item's material against water at sea level, so 1.0
	// is 1000 kilograms per cubic metre.
	std::string density;

	// Packing factor of the item. The fraction of a storage cube the product
	// fills, counting packaging, dunnage and the voids between the pieces, so the
	// mass in a cubic metre is density x 1000 x packFactor kilograms.
	std::string packFactor;
};

using Items = std::vector<ItemNode>;

struct SkillNode
{
	// Unique identifier for the skill.
	std::string uniqueId;

	// Display name of the skill.
	std::string displayName;

	// Description of the skill.
	std::string description;

	// Type of the node (Tech or Skill).
	NodeType nodeType;

	// Maximum number of tiers this skill can have. Must be at least 1.
	int maxTiers{1};

	// List of skills required to unlock this skill.
	std::vector<Prerequisite> requiredSkills;

	// Amount of resources required to unlock this skill.
	int resourceCostPerTier;

	// ID of the effect that this skill targets.
	std::string targetEffectID;

	// Mutal exclusion group ID.
	// If empty, this node has no rival choices.
	// If populated, only ONE node sharing this group ID can be unlocked.
	std::string exclusiveGroupId;

	// Layout metadata for UI Rendering.
	float gridX{0.0f};
	float gridY{0.0f};
};

using Skills = std::vector<SkillNode>;

struct GameModeNode
{
	Items items;
	Skills skills;
};

// Configuration loaders. Each loader replaces the contents of its output
// container: calling a loader twice with the same file gives the same entries,
// never duplicated ones.
//
// Every loader returns true when the file parsed cleanly, false and a message in
// ErrorMessage when it did not. Missing, null and mistyped required fields are
// reported with the index and key of the offending entry.

// Loads the ages configuration from a YAML file into a vector of AgeNode.
bool LoadConfigAges(const std::string &FilePath, Ages &Ages, std::string &ErrorMessage);

// Loads the categories configuration from a YAML file into a vector of CategoryNode.
bool LoadConfigCategories(
    const std::string &FilePath, Categories &Categories, std::string &ErrorMessage);

// Loads the items and skills of a game mode from a YAML file into a GameModeNode.
bool LoadConfigGameMode(
    const std::string &FilePath, GameModeNode &GameModeConfig, std::string &ErrorMessage);
} // namespace CivSimCore
