#include "TestHelpers.h"

#include <string>

#include <manifest/config/SimulationConfig.h>


namespace
{
const char *const agesYaml = R"YAML(
ages:
  - age:
      ageId: AgeStone
      displayName: Stone Age
      description: Stone age
  - age:
      ageId: AgeCopper
      displayName: Copper Age
      description: Copper Age
)YAML";

const char *const categoriesYaml = R"YAML(
categories:
  - category:
      categoryId: CategoryMetals
      displayName: Metals
      type: Metal
      description: Metal goods
  - category:
      categoryId: CategoryUnknown
      displayName: Unknown
)YAML";

const char *const gameModeYaml = R"YAML(
items:
  - item:
      itemId: ItemCopper
      displayName: Copper
      description: Copper ingots
      categoryId: CategoryMetals
      ageId: AgeCopper
      unitsPerCubicMetre: 1
      basePrice: 5.6312
      unitType: Kilogram
      density: 8.96
      packFactor: 1.0
skills:
  - skill:
      skillId: SkillMining
      displayName: Mining
      description: Extracts resources
      nodeType: Skill
      maxTiers: 5
      requires: []
      resourceCostPerTier: 1
      targetEffectID: null
      exclusiveGroupId: null
      gridX: 0.0
      gridY: 0.0
  - skill:
      skillId: SkillSmelting
      displayName: Smelting
      nodeType: Tech
      maxTiers: 4
      requires:
        - nodeId: SkillMining
          requiredTier: 2
      resourceCostPerTier: 2
      targetEffectID: SmelterOutput
      exclusiveGroupId: "null"
      gridX: 1.0
      gridY: 0.0
)YAML";
} // namespace


TEST_CASE("ages_load_and_replace")
{
	const Manifest::Test::TempFile file(agesYaml);

	std::string errorMessage;
	Manifest::Core::Ages ages;

	CHECK(Manifest::Core::LoadConfigAges(file.Path(), ages, errorMessage));
	CHECK_EQ(errorMessage, std::string());
	CHECK_EQ(ages.size(), std::size_t(2));
	CHECK_EQ(ages[0].ageId, std::string("AgeStone"));
	CHECK_EQ(ages[1].description, std::string("Copper Age"));

	// The loader replaces the container: loading twice must not double the entries.
	CHECK(Manifest::Core::LoadConfigAges(file.Path(), ages, errorMessage));
	CHECK_EQ(ages.size(), std::size_t(2));

	CHECK(Manifest::Core::LoadConfigAges(file.Path(), ages, errorMessage));
	CHECK_EQ(ages.size(), std::size_t(2));
}

TEST_CASE("categories_load_and_replace")
{
	const Manifest::Test::TempFile file(categoriesYaml);

	std::string errorMessage;
	Manifest::Core::Categories categories;

	CHECK(Manifest::Core::LoadConfigCategories(file.Path(), categories, errorMessage));
	CHECK_EQ(categories.size(), std::size_t(2));
	CHECK_EQ(categories[0].type, std::string("Metal"));
	CHECK_EQ(categories[0].description, std::string("Metal goods"));

	// The type and description fall back to the identity of the category: the type to
	// its categoryId, the description to its displayName.
	CHECK_EQ(categories[1].type, std::string("CategoryUnknown"));
	CHECK_EQ(categories[1].description, std::string("Unknown"));

	CHECK(Manifest::Core::LoadConfigCategories(file.Path(), categories, errorMessage));
	CHECK_EQ(categories.size(), std::size_t(2));
}

TEST_CASE("game_mode_loads_items_and_skills")
{
	const Manifest::Test::TempFile file(gameModeYaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	CHECK_EQ(gameMode.items.size(), std::size_t(1));
	CHECK_EQ(gameMode.skills.size(), std::size_t(2));

	CHECK_EQ(gameMode.items[0].categoryId, std::string("CategoryMetals"));
	CHECK_EQ(gameMode.items[0].ageId, std::string("AgeCopper"));
	CHECK_EQ(gameMode.items[0].basePrice, std::string("5.6312"));

	CHECK_EQ(gameMode.skills[0].maxTiers, 5);
	CHECK_EQ(gameMode.skills[0].nodeType, Manifest::Core::NodeType::Skill);
	CHECK_EQ(gameMode.skills[1].nodeType, Manifest::Core::NodeType::Tech);
	CHECK_EQ(gameMode.skills[1].requiredSkills.size(), std::size_t(1));
	CHECK_EQ(gameMode.skills[1].requiredSkills[0].nodeId, std::string("SkillMining"));
	CHECK_EQ(gameMode.skills[1].requiredSkills[0].requiredTier, 2);
	MANIFEST_CHECK_NEAR(gameMode.skills[0].gridX, 0.0f, 0.001f);
	MANIFEST_CHECK_NEAR(gameMode.skills[1].gridX, 1.0f, 0.001f);

	// Loading twice replaces, never appends.
	CHECK(Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	CHECK_EQ(gameMode.items.size(), std::size_t(1));
	CHECK_EQ(gameMode.skills.size(), std::size_t(2));
}

TEST_CASE("yaml_null_is_not_a_value")
{
	// yaml-cpp renders a null as the scalar text "null": every spelling of it means
	// "not set", and so does an empty value and a missing key.
	const char *const yaml = R"YAML(
skills:
  - skill:
      skillId: Plain
      nullTarget: null
      targetEffectID: null
      exclusiveGroupId: null
  - skill:
      skillId: Tilde
      targetEffectID: ~
      exclusiveGroupId: ~
  - skill:
      skillId: Capital
      targetEffectID: NULL
      exclusiveGroupId: Null
  - skill:
      skillId: Empty
      targetEffectID:
      exclusiveGroupId:
  - skill:
      skillId: Quoted
      targetEffectID: "null"
      exclusiveGroupId: "null"
items: []
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	CHECK_EQ(gameMode.skills.size(), std::size_t(5));

	for (const auto &skill : gameMode.skills)
	{
		CHECK_EQ(skill.targetEffectID, std::string());
		CHECK_EQ(skill.exclusiveGroupId, std::string());
		CHECK_EQ(skill.displayName, skill.skillId);
		CHECK_EQ(skill.maxTiers, 1);
		CHECK_EQ(skill.resourceCostPerTier, 1);
	}
}

TEST_CASE("missing_required_field_reports_the_entry")
{
	const char *const yaml = R"YAML(
ages:
  - age:
      ageId: AgeStone
      displayName: Stone Age
  - age:
      ageId: AgeCopper
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::Ages ages;

	CHECK(!Manifest::Core::LoadConfigAges(file.Path(), ages, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "ages[1]");
	MANIFEST_CHECK_CONTAINS(errorMessage, "displayName");

	// A failed load leaves the output empty.
	CHECK_EQ(ages.size(), std::size_t(0));
}

TEST_CASE("mistyped_field_reports_the_value")
{
	const char *const yaml = R"YAML(
skills:
  - skill:
      skillId: SkillMining
      maxTiers: three
items: []
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(!Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "skills[0]");
	MANIFEST_CHECK_CONTAINS(errorMessage, "maxTiers");
	MANIFEST_CHECK_CONTAINS(errorMessage, "three");
	CHECK_EQ(gameMode.skills.size(), std::size_t(0));
}

TEST_CASE("bad_entry_shape_reports_the_key")
{
	const char *const yaml = R"YAML(
items:
  - Copper
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(!Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "items[0]");
	MANIFEST_CHECK_CONTAINS(errorMessage, "item");
}

TEST_CASE("missing_sequence_is_reported")
{
	const Manifest::Test::TempFile file("nothing: here\n");

	std::string errorMessage;
	Manifest::Core::Ages ages;
	CHECK(!Manifest::Core::LoadConfigAges(file.Path(), ages, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "ages");

	Manifest::Core::Categories categories;
	CHECK(!Manifest::Core::LoadConfigCategories(file.Path(), categories, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "categories");

	Manifest::Core::GameModeNode gameMode;
	CHECK(!Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "items");
}

TEST_CASE("empty_sequence_loads_nothing")
{
	const Manifest::Test::TempFile file("ages: []\n");

	std::string errorMessage;
	Manifest::Core::Ages ages;

	CHECK(Manifest::Core::LoadConfigAges(file.Path(), ages, errorMessage));
	CHECK_EQ(ages.size(), std::size_t(0));
}

TEST_CASE("unreadable_file_names_the_path")
{
	std::string errorMessage;
	Manifest::Core::Ages ages;

	CHECK(!Manifest::Core::LoadConfigAges("data/does-not-exist/ages.yaml", ages, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "data/does-not-exist/ages.yaml");
}

TEST_CASE("item_links_are_optional_metadata")
{
	// The merged documents in data/core-config carry no item links: an item without
	// them loads, with them it keeps them.
	const char *const withoutLinks = R"YAML(
items:
  - item:
      itemId: ItemCopper
      displayName: Copper
      basePrice: 5.63
skills: []
)YAML";

	const Manifest::Test::TempFile file(withoutLinks);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	CHECK_EQ(gameMode.items.size(), std::size_t(1));
	CHECK_EQ(gameMode.items[0].categoryId, std::string());
	CHECK_EQ(gameMode.items[0].ageId, std::string());
	CHECK_EQ(gameMode.items[0].density, std::string());
}

TEST_CASE("null_in_a_required_field_is_a_load_error")
{
	const char *const yaml = R"YAML(
items:
  - item:
      itemId: null
      displayName: Copper
      basePrice: 5.63
skills: []
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(!Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "items[0]");
	MANIFEST_CHECK_CONTAINS(errorMessage, "itemId");
}

TEST_CASE("bad_node_type_is_reported")
{
	const char *const yaml = R"YAML(
skills:
  - skill:
      skillId: SkillMining
      nodeType: Magic
items: []
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(!Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "nodeType");
	MANIFEST_CHECK_CONTAINS(errorMessage, "Magic");
}

TEST_CASE("requires_must_be_a_sequence")
{
	const char *const yaml = R"YAML(
skills:
  - skill:
      skillId: SkillMining
      requires: Mining
items: []
)YAML";

	const Manifest::Test::TempFile file(yaml);

	std::string errorMessage;
	Manifest::Core::GameModeNode gameMode;

	CHECK(!Manifest::Core::LoadConfigGameMode(file.Path(), gameMode, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "skills[0]");
	MANIFEST_CHECK_CONTAINS(errorMessage, "requires");
}
