#pragma once

#include <string>
#include <vector>


namespace CivSimCore
{
    enum class NodeType { Tech, Skill };

    // Defines a prerequisite with a specific tier requirement.
    struct Prerequisite {
        std::string nodeId;
        int requiredTier {1};
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

        // The number of units that can fit into one cubic metre of storage.
        std::string unitsPerCubicMetre;

        // Universal base price of the item.
        std::string basePrice;

        // The type of units of measurement for this type of item.
        std::string unitType;

        // Density of the item in kilograms per cubic metre.
        std::string density;

        // Packing factor of the item. Lost space due to packing inefficiencies.
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

        // Maximum number of tiers this skill can have.
        int maxTiers {1};

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
        float gridX {0.0f};
        float gridY {0.0f};
    };

    using Skills = std::vector<SkillNode>;

    struct GameModeNode
    {
        Items items;
        Skills skills;
    };

    // Loads the ages configuration from a YAML file and returns a vector of AgeNode structures.
    // Returns true if the configuration was loaded successfully, false otherwise.
    bool LoadConfigAges(const std::string &FilePath, Ages &Ages, std::string &ErrorMessage);

    // Loads the categories configuration from a YAML file and returns a vector of CategoryNode structures.
    // Returns true if the configuration was loaded successfully, false otherwise.
    bool LoadConfigCategories(const std::string &FilePath, Categories &Categories, std::string &ErrorMessage);

    // Loads the items configuration from a YAML file and returns a GameModeNode.
    // Returns true if the configuration was loaded successfully, false otherwise.
    bool LoadConfigGameMode(
        const std::string& FilePath,
        GameModeNode& GameModeConfig,
        std::string& ErrorMessage
    );
} // namespace CivSimCore