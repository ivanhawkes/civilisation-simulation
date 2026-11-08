#include "Core/ProgressionManager.h"
#include "Core/SkillLoader.h"
#include "Core/SkillLoaderInternal.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/CivilisationEffects.h"
#include "Effects/EffectCatalogue.h"
#include "Effects/EffectTypes.h"
#include "Effects/ShipEffectEngine.h"
#include "TestFramework.h"
#include <algorithm>
#include <string>
#include <vector>

namespace
{
using namespace CivSimCore;

// Every effect id the shipped skill tree names, gathered from the data by the loader.
Skills LoadAllShippedTrees()
{
	const std::string dataDirectory = []
	{
		const std::vector<std::string> candidates = {"./data", "../data", "../../data", "../../../data"};

		for (const std::string &candidate : candidates)
		{
			if (std::filesystem::exists(SkillLoader::SkillDataPaths(candidate).skillTreeFile))
			{
				return candidate;
			}
		}

		return std::string{"./data"};
	}();

	const SkillDataPaths paths = SkillLoader::SkillDataPaths(dataDirectory);

	std::string errorMessage;
	const Skills loaded = SkillLoader::LoadSkillTreeFromFiles(SkillLoader::SkillTreeFilePaths(paths), errorMessage);

	if (loaded.empty() && !errorMessage.empty())
	{
		FAIL("the shipped skill tree did not load: " + errorMessage);
	}

	return loaded;
}

// A node of a tree, the way ProgressionTests builds them: a node is its id, its age,
// its kind, its cost, and the effect it names.
SkillNode Node(
    const std::string &SkillId, const std::string &AgeId, const std::string &NodeType, int MaxTiers, int Cost,
    const std::string &EffectId, int EffectTier = 1)
{
	SkillNode node;

	node.skillId = SkillId;
	node.name = SkillId;
	node.description = "the scaffold's own node";
	node.ageId = AgeId;
	node.nodeType = NodeType;
	node.maxTiers = MaxTiers;
	node.nodeCost = Cost;
	node.targetEffectID = EffectId;
	node.effectDescription = "the scaffold's own effect";
	node.effectTier = EffectTier;

	return node;
}

ProgressionManager Tree(const std::vector<SkillNode> &Nodes, int Resources)
{
	Skills skills;

	for (const SkillNode &node : Nodes)
	{
		skills.push_back(node);
	}

	ProgressionManager progression;
	std::string errorMessage;

	if (!progression.Build(skills, errorMessage))
	{
		FAIL("the scaffold's tree did not build: " + errorMessage);
	}

	progression.SetResources(Resources);

	return progression;
}

// A civilisation, its planet and the markets it can hold: the store a body effect, a
// market effect and a lane effect all land on, built without the marketplace service so
// the effect engine's own rules are what the test reads.
void AddCivilisation(EcsWorld &world, const std::string &civilisationId, const std::string &planetName)
{
	std::string errorMessage;

	if (!world.Create(civilisationId, errorMessage))
	{
		FAIL("the civilisation did not appear: " + errorMessage);
	}

	BodyInfo body;
	body.bodyId = civilisationId;
	body.kind = BodyKind::Planet;
	body.displayName = planetName;
	body.systemName = planetName + " System";
	body.civilisationId = civilisationId;
	body.ageId = "Age02";

	world.Add<BodyInfo>(civilisationId, std::move(body));
	world.Add<Treasury>(civilisationId, Treasury{1000});
	world.Add<Stockpile>(civilisationId, Stockpile{{{"iron_ore", 100.0}}});
}

void AddVenue(EcsWorld &world, const std::string &venueBodyId, const std::string &civilisationId)
{
	MarketplaceInfo venue;
	venue.marketId = venueBodyId;
	venue.venueName = venueBodyId;
	venue.civilisationId = civilisationId;
	venue.atPlanet = true;
	venue.baseListingFeeBps = 40;
	venue.orderLifeTicks = 24;

	world.Put<MarketplaceInfo>(venueBodyId, std::move(venue));
	world.Put<MarketRecord>(venueBodyId, MarketRecord{});
}

EffectDefinition Definition(
    const std::string &effectId, EffectDomain domain, EffectStat stat, double perTier,
    EffectOperator op = EffectOperator::Add)
{
	EffectDefinition definition;

	definition.effectId = effectId;
	definition.displayName = effectId + " display";
	definition.description = effectId + " description";
	definition.domain = domain;
	definition.modifiers.push_back(EffectModifier{stat, perTier, op});

	return definition;
}
} // namespace


TEST_CASE(the_default_catalogue_covers_every_effect_id_the_shipped_tree_names)
{
	const Skills tree = LoadAllShippedTrees();
	CHECK(!tree.empty());

	EffectCatalogue catalogue = EffectCatalogue::Default();
	std::string errorMessage;

	// The catalogue is the half of the bridge the data has no word for: the tree names
	// an effect id, the catalogue says what it moves, and the two lists have to be one
	// list. The tree names 54 ids, and 4 of them twice over, so 54 definitions.
	CHECK_EQ(catalogue.EffectCount(), 54u);
	CHECK(catalogue.EffectIds().size() == 54);
	CHECK(catalogue.EffectsMissingFrom(tree).empty());
	CHECK(catalogue.EffectsUnusedBy(tree).empty());
	CHECK(catalogue.CoversTree(tree, errorMessage));
	CHECK(errorMessage.empty());

	// The domains the scaffold is about: ships, yards, lanes, claims, markets, bodies.
	CHECK(catalogue.EffectIdsInDomain(EffectDomain::Ship).size() > 25);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Yard).size(), 4u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Route).size(), 4u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Survey).size(), 4u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Market).size(), 11u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Body).size(), 6u);

	CHECK(catalogue.Has("ShieldFront"));
	CHECK(catalogue.Has("Drydock"));
	CHECK(catalogue.Has("Convoy"));
	CHECK(catalogue.Has("DeepSurvey"));
	CHECK(catalogue.Has("TradeTribunal"));
	CHECK(catalogue.Has("TerraformBiosphere"));
	CHECK(!catalogue.Has("SkillShipShieldsFront"));

	const EffectDefinition *shield = catalogue.Find("ShieldFront");
	const EffectDefinition *tribunal = catalogue.Find("TradeTribunal");
	const EffectDefinition *drydock = catalogue.Find("Drydock");

	CHECK(shield != nullptr);
	CHECK(drydock != nullptr);
	CHECK(tribunal != nullptr);

	CHECK_EQ(shield->displayName, std::string("Front shield bank"));
	CHECK_EQ(EffectDomainOf(shield->domain), EffectDomain::Ship);
	CHECK_EQ(shield->modifiers.size(), 1u);
	CHECK_EQ(shield->modifiers[0].stat, EffectStat::HullIntegrity);
	CHECK_EQ(shield->modifiers[0].perTier, 12.0);

	// A yard is a place and a set of berths, and a market that clears the deck is a fee
	// off the berth.
	CHECK_EQ(drydock->modifiers.size(), 3u);
	CHECK(tribunal->modifiers.size() >= 2u);
	CHECK(catalogue.Find("VanguardWing")->effectId == "Convoy");
	CHECK(catalogue.Find("YardCrews")->effectId == "YardOutfitting");
	CHECK(catalogue.Find("PatrolNet")->effectId == "RouteWard");
	CHECK(catalogue.Find("ProspectorWage")->effectId == "DeepSurvey");
}


TEST_CASE(a_definition_that_does_not_stand_up_is_refused, "EffectCatalogue::Register")
{
	EffectCatalogue catalogue;
	std::string errorMessage;

	CHECK(!catalogue.Register(Definition("", EffectDomain::Ship, EffectStat::HullIntegrity, 12.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");

	EffectDefinition noModifiers = Definition("NoModifiers", EffectDomain::Ship, EffectStat::HullIntegrity, 12.0);
	noModifiers.modifiers.clear();

	CHECK(!catalogue.Register(noModifiers, errorMessage));
	CHECK_CONTAINS(errorMessage, "modifier");

	// An effect of one domain cannot move a stat of another: a yard that moves a ship's
	// integrity is a game the tests cannot read, and the domains are the reason the
	// catalogue checks this.
	CHECK(!catalogue.Register(Definition("YardMovesHull", EffectDomain::Yard, EffectStat::HullIntegrity, 12.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "Yard");
	CHECK_CONTAINS(errorMessage, "HullIntegrity");

	CHECK(!catalogue.Register(Definition("ShipMovesMarket", EffectDomain::Ship, EffectStat::MarketInsurance, 4.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "Ship");

	CHECK(!catalogue.Register(Definition("MarketMovesLane", EffectDomain::Market, EffectStat::RouteRiskPremium, 4.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "Route");

	CHECK(!catalogue.Register(Definition("BodyMovesYard", EffectDomain::Body, EffectStat::DrydockThroughput, 4.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "Drydock");

	CHECK(!catalogue.Register(Definition("ZeroPerTier", EffectDomain::Ship, EffectStat::HullIntegrity, 0.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "tier");

	CHECK(catalogue.Register(Definition("ShieldFront", EffectDomain::Ship, EffectStat::HullIntegrity, 12.0), errorMessage));
	CHECK_EQ(catalogue.EffectCount(), 1u);

	// A second definition of one id is the same effect twice, and which of them moves
	// the number is the sort of question a test cannot answer.
	CHECK(!catalogue.Register(Definition("ShieldFront", EffectDomain::Ship, EffectStat::ShieldRear, 5.0), errorMessage));
	CHECK_CONTAINS(errorMessage, "ShieldFront");
	CHECK_EQ(catalogue.Find("ShieldFront")->modifiers[0].stat, EffectStat::HullIntegrity);

	CHECK(catalogue.Validate(errorMessage));
	CHECK(errorMessage.empty());
}


TEST_CASE(the_catalogue_names_an_effect_the_tree_asks_for_that_it_does_not_have)
{
	EffectCatalogue catalogue;
	std::string errorMessage;

	CHECK(catalogue.Register(Definition("ShieldFront", EffectDomain::Ship, EffectStat::HullIntegrity, 12.0), errorMessage));

	const std::vector<SkillNode> nodes = {
	    Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 3),
	    Node("SkillShipDroneDefense", "Age02", "ShipSystem", 3, 2, "DroneDefense", 2),
	    Node("Age02", "Age02", "AgeGate", 1, 1, "", 0),
	};

	const Skills tree(nodes.begin(), nodes.end());
	ProgressionManager progression = Tree(nodes, 0);

	CHECK_EQ(catalogue.EffectsMissingFrom(tree).size(), 1u);
	CHECK_EQ(catalogue.EffectsMissingFrom(tree)[0], std::string("DroneDefense"));

	// An age gate names no effect, and the age gates of the shipped tree are the
	// entries whose targetEffectID is null, which is the list the bridge skips.
	CHECK(catalogue.EffectsUnusedBy(tree).empty());
	CHECK(!catalogue.CoversTree(tree, errorMessage));
	CHECK_CONTAINS(errorMessage, "DroneDefense");
	CHECK_CONTAINS(errorMessage, "SkillShipDroneDefense");
	CHECK_CONTAINS(errorMessage, "catalogue");

	CHECK(catalogue.Register(Definition("DroneDefense", EffectDomain::Ship, EffectStat::PointDefence, 2.0), errorMessage));
	CHECK(catalogue.CoversTree(tree, errorMessage));
	CHECK(catalogue.EffectsMissingFrom(tree).empty());
}


TEST_CASE(the_modifiers_are_applied_the_way_their_operator_says, "ApplyEffectModifiers")
{
	std::map<EffectStat, double> stats{{EffectStat::HullIntegrity, 120.0}, {EffectStat::SpeedMaxKms, 22.0}};

	ApplyEffectModifiers(stats, EffectModifier{EffectStat::HullIntegrity, 60.0, EffectOperator::Add}, 3);

	CHECK_EQ(ReadEffectStat(stats, EffectStat::HullIntegrity), 300.0);

	// A Multiply is a fraction of what the ship already does: the reactor's 25 percent
	// a tier, on the output the hull and the reactor together give.
	ApplyEffectModifiers(stats, EffectModifier{EffectStat::SpeedMaxKms, 0.10, EffectOperator::Multiply}, 2);

	CHECK_EQ(ReadEffectStat(stats, EffectStat::SpeedMaxKms), 26.4);

	ApplyEffectModifiers(stats, EffectModifier{EffectStat::SpeedMaxKms, 8.0, EffectOperator::Set}, 1);

	CHECK_EQ(ReadEffectStat(stats, EffectStat::SpeedMaxKms), 8.0);
	CHECK_EQ(stats.count(EffectStat::CargoCapacity), 0u);

	// A stat the effect moves that the store never had is a stat the store has now: a
	// market that buys insurance is a market whose insurance is no longer nothing.
	std::map<EffectStat, double> market;

	ApplyEffectModifiers(market, EffectModifier{EffectStat::MarketInsurance, 4.0, EffectOperator::Add}, 3);

	CHECK_EQ(market.size(), 1u);
	CHECK_EQ(ReadEffectStat(market, EffectStat::MarketInsurance), 12.0);
	CHECK_EQ(ReadEffectStat(market, EffectStat::MarketRiskPremium), 0.0);

	// The text a UI prints: units, a signed number, and the operator's word.
	CHECK_EQ(FormatEffectStatValue(120.0), std::string("120"));
	CHECK_EQ(FormatEffectStatValue(26.4), std::string("26.4"));
	CHECK_EQ(FormatEffectStatValue(-0.12), std::string("-0.12"));
	CHECK_EQ(FormatEffectStatValue(12.0), std::string("12"));

	const std::string statsText = DescribeEffectStats(stats);

	CHECK_CONTAINS(statsText, "hull integrity");
	CHECK_CONTAINS(statsText, "300");
	CHECK_CONTAINS(statsText, "km/s");

	const std::string modifierText =
	    DescribeEffectModifier(EffectModifier{EffectStat::MarketRiskPremium, -12.0, EffectOperator::Add});

	CHECK_CONTAINS(modifierText, "lane risk premium");
	CHECK_CONTAINS(modifierText, "-12");
	CHECK_CONTAINS(modifierText, "Add");

	CHECK_CONTAINS(DescribeEffectModifier(EffectModifier{EffectStat::CargoCapacity, 0.25, EffectOperator::Multiply}), "x25%");
	CHECK_CONTAINS(DescribeEffectModifier(EffectModifier{EffectStat::PowerCapacity, 40.0, EffectOperator::Set}), "Set");

	// The domain of a stat, and the name of it, which is the text a UI prints beside a
	// number the catalogue moved.
	CHECK_EQ(EffectDomainOf(EffectStat::MarketBypass), EffectDomain::Market);
	CHECK_EQ(EffectDomainOf(EffectStat::RouteConvoySize), EffectDomain::Route);
	CHECK_EQ(EffectDomainOf(EffectStat::DrydockSlots), EffectDomain::Yard);
	CHECK_EQ(EffectDomainOf(EffectStat::SurveyAnomalyFind), EffectDomain::Survey);
	CHECK_EQ(EffectDomainOf(EffectStat::PopCapBonus), EffectDomain::Body);
	CHECK_EQ(EffectDomainOf(EffectStat::PointDefence), EffectDomain::Ship);
	CHECK_EQ(std::string(StatName(EffectStat::HullIntegrity)), std::string("hull integrity"));
	CHECK_EQ(std::string(StatUnit(EffectStat::CargoCapacity)), std::string("tonnes"));
	CHECK_EQ(std::string(DescribeEffectDomain(EffectDomain::Survey)), std::string("survey"));
	CHECK_EQ(std::string(DescribeEffectOrigin(EffectOrigin::Technology)), std::string("technology"));
	CHECK_EQ(std::string(DescribeEffectOperator(EffectOperator::Multiply)), std::string("Multiply"));
	CHECK_EQ(AllEffectStats().size(), 32u);
	CHECK(EffectDomainOf(EffectStat::None) == EffectDomain::None);
}


TEST_CASE(a_ship_of_a_class_carries_the_base_stats_the_class_describes)
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	const ShipClassSpec trader = ShipEffectEngine::LongHaulTrader();

	CHECK_EQ(trader.hullId, std::string("HullTraderLongHaul"));
	CHECK_EQ(trader.baseStats.at(EffectStat::HullIntegrity), 120.0);
	CHECK_EQ(trader.baseStats.at(EffectStat::CargoCapacity), 400.0);
	CHECK(trader.fitableEffects.size() > 25);

	CHECK(engine.BuildShip(world, "Ship-01", trader, "Civ-01", errorMessage));
	CHECK(engine.HasShip(world, "Ship-01"));
	CHECK_EQ(engine.ShipsOf(world, "Civ-01").size(), 1u);
	CHECK_EQ(engine.ShipsOf(world, "Civ-02").size(), 0u);
	CHECK_EQ(engine.ShipsOf(world, {}).size(), 1u);

	const double integrity = engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity);
	const double cargo = engine.GetStat(world, "Ship-01", EffectStat::CargoCapacity);

	CHECK_EQ(integrity, 120.0);
	CHECK_EQ(cargo, 400.0);

	// The hull's own numbers, and the numbers it flies with, before anything is fitted:
	// two maps that a test compares when a system is unbolted.
	CHECK_EQ(engine.BaseStatsOf(world, "Ship-01"), engine.StatsOf(world, "Ship-01"));
	CHECK_EQ(engine.StatsOf(world, "Ship-01").count(EffectStat::ShieldRear), 0u);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::ShieldFront), 0.0);
	CHECK(engine.FittedEffects(world, "Ship-01").empty());
	CHECK_EQ(engine.FittedTier(world, "Ship-01", "ShieldFront"), 0);

	const ShipInfo *info = world.Get<ShipInfo>("Ship-01");

	CHECK(info != nullptr);
	CHECK_EQ(info->hullId, std::string("HullTraderLongHaul"));
	CHECK_EQ(info->operatorId, std::string("Civ-01"));
	CHECK(world.HasComponent<ShipInfo, ShipBaseStats, ShipStats, ShipSlots, EntityId>("Ship-01"));

	// The ship is an entity of the store, addressed by the id the core gave it.
	CHECK(world.Has("Ship-01"));
	CHECK_EQ(world.ComponentCount<ShipStats>(), 1u);

	CHECK(!engine.BuildShip(world, "", trader, "Civ-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");
	CHECK(!engine.BuildShip(world, "Ship-02", trader, "", errorMessage));
	CHECK_CONTAINS(errorMessage, "operator");
	CHECK(!engine.BuildShip(world, "Ship-01", trader, "Civ-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "Ship-01");

	ShipClassSpec noNumbers;
	noNumbers.hullId = "HullEmpty";

	CHECK(!engine.BuildShip(world, "Ship-03", noNumbers, "Civ-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "base stats");

	CHECK(!engine.Recompute(world, "Ship-99", errorMessage));
	CHECK_CONTAINS(errorMessage, "Ship-99");
	CHECK_EQ(engine.ShipsOf(world, "Civ-01").size(), 1u);
}


TEST_CASE(fitting_a_shield_bank_moves_the_number_the_shield_moves, "ShipEffectEngine::Fit")
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	CHECK(engine.BuildShip(world, "Ship-01", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));

	// The tree of the data: the front shield bank has 5 tiers, and the data's tier-3
	// entry reads "+12 integrity per tier", so tier 3 is 36 on the hull's 120.
	CHECK(engine.Fit(world, "Ship-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 3, errorMessage));

	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 156.0);
	CHECK_EQ(engine.FittedTier(world, "Ship-01", "ShieldFront"), 3);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 1u);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01")[0], std::string("ShieldFront"));
	CHECK_EQ(engine.BaseStatsOf(world, "Ship-01").at(EffectStat::HullIntegrity), 120.0);

	const std::vector<EffectSource> sources = engine.SourcesOn(world, "Ship-01");

	CHECK_EQ(sources.size(), 1u);
	CHECK_EQ(sources[0].skillId, std::string("SkillShipShieldsFront"));
	CHECK_EQ(sources[0].effectId, std::string("ShieldFront"));
	CHECK_EQ(sources[0].tier, 3);
	CHECK(sources[0].origin == EffectOrigin::Skill);

	// A refit dock: the same system at a better tier replaces the old tier.
	CHECK(engine.Fit(world, "Ship-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 5, errorMessage));

	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 180.0);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 1u);
	CHECK_EQ(engine.SourcesOn(world, "Ship-01").size(), 1u);

	// Two systems that move the same number: the rear bank and the front bank both
	// add to the hull, and the hull reads both.
	CHECK(engine.Fit(world, "Ship-01", "SkillShipShieldsRear", "ShieldRear", EffectOrigin::Skill, 2, errorMessage));

	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 240.0);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::ShieldRear), 40.0);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 2u);

	// A system unbolted: the number goes back to what the hull came with.
	CHECK(engine.UnfitEffect(world, "Ship-01", "ShieldRear", errorMessage));

	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::ShieldRear), 0.0);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 180.0);
	CHECK(!engine.UnfitEffect(world, "Ship-01", "ShieldRear", errorMessage));
	CHECK_CONTAINS(errorMessage, "ShieldRear");
	CHECK(engine.FittedEffects(world, "Ship-01").size() == 1);
}


TEST_CASE(a_skill_effect_needs_a_slot_on_the_hull_and_a_technology_does_not)
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	// A hull of the scaffold's own making, with one slot: the hull is the class, and
	// the class is the list of systems it has room for.
	ShipClassSpec tender;
	tender.hullId = "HullTender";
	tender.className = "Forward tender";
	tender.description = "a hull of the scaffold's own making, with room for one system";
	tender.baseStats[EffectStat::HullIntegrity] = 40.0;
	tender.baseStats[EffectStat::SpeedMaxKms] = 8.0;
	tender.fitableEffects = {"CruisePlates"};

	CHECK(engine.BuildShip(world, "Ship-Tender", tender, "Civ-01", errorMessage));

	// A ship system is a thing bolted to a hull, and a hull with no slot for it is a
	// hull that cannot bolt it: the message says the effect, the hull and the fit.
	CHECK(!engine.Fit(world, "Ship-Tender", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));

	CHECK_CONTAINS(errorMessage, "ShieldFront");
	CHECK_CONTAINS(errorMessage, "HullTender");

	// A technology is a fact about the civisation's ships, bought in a tree, and it
	// needs no room on a hull: the knowledge of fusion drives is fitted to every hull
	// that has a reactor, not bolted into a slot.
	CHECK(engine.Fit(world, "Ship-Tender", "SkillShipPropulsionFusionDrive", "FusionDrive", EffectOrigin::Technology, 3, errorMessage));

	CHECK_EQ(engine.GetStat(world, "Ship-Tender", EffectStat::SpeedMaxKms), 10.4);
	CHECK_EQ(engine.FittedTier(world, "Ship-Tender", "FusionDrive"), 3);

	{
		const std::vector<EffectSource> sources = engine.SourcesOn(world, "Ship-Tender");

		CHECK_EQ(sources.size(), 1u);
		CHECK(sources[0].origin == EffectOrigin::Technology);
	}

	// A ship system that the hull has room for goes on, and the number moves.
	CHECK(engine.Fit(world, "Ship-Tender", "SkillShipPropulsionCruisePlates", "CruisePlates", EffectOrigin::Skill, 2, errorMessage));

	CHECK_EQ(engine.GetStat(world, "Ship-Tender", EffectStat::SpeedMaxKms), 12.4);
	CHECK_EQ(engine.FittedEffects(world, "Ship-Tender").size(), 2u);

	// The order the systems are applied in is the effect id, not the order they were
	// fitted in: two ships of one class, one hull fitted first with the drive and the
	// other first with the plates, fly at one speed.
	CHECK(engine.BuildShip(world, "Ship-Tender-2", tender, "Civ-01", errorMessage));
	CHECK(engine.Fit(world, "Ship-Tender-2", "SkillShipPropulsionCruisePlates", "CruisePlates", EffectOrigin::Skill, 2, errorMessage));
	CHECK(engine.Fit(world, "Ship-Tender-2", "SkillShipPropulsionFusionDrive", "FusionDrive", EffectOrigin::Technology, 3, errorMessage));

	CHECK_EQ(
	    engine.GetStat(world, "Ship-Tender", EffectStat::SpeedMaxKms),
	    engine.GetStat(world, "Ship-Tender-2", EffectStat::SpeedMaxKms));
	CHECK_EQ(engine.StatsOf(world, "Ship-Tender"), engine.StatsOf(world, "Ship-Tender-2"));
	CHECK_EQ(engine.ShipsOf(world, "Civ-01").size(), 2u);
}


TEST_CASE(a_fit_that_cannot_be_made_says_why, "ShipEffectEngine::Fit")
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	CHECK(engine.BuildShip(world, "Ship-01", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));

	CHECK(!engine.Fit(world, "Ship-01", "SkillShipShieldsFront", "NoSuchEffect", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "NoSuchEffect");
	CHECK_CONTAINS(errorMessage, "catalogue");

	CHECK(!engine.Fit(world, "Ship-01", "SkillShipShieldsFront", "", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");

	CHECK(!engine.Fit(world, "Ship-01", "", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "skillId");

	// A ship system is a thing bolted to a hull; a yard is a place, and a hull has no
	// room for a place, so the domain says where the effect goes.
	CHECK(!engine.Fit(world, "Ship-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Drydock");
	CHECK_CONTAINS(errorMessage, "Yard");
	CHECK(!engine.Fit(world, "Ship-01", "SkillLawTribunal", "TradeTribunal", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Market");
	CHECK(!engine.Fit(world, "Ship-01", "SkillHabitatTerraforming", "TerraformAtmosphere", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Body");

	CHECK(!engine.Fit(world, "Ship-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 0, errorMessage));
	CHECK_CONTAINS(errorMessage, "tier");
	CHECK(!engine.Fit(world, "Ship-99", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Ship-99");
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 0u);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 120.0);
}


TEST_CASE(a_node_of_the_tree_is_the_effect_it_names_and_the_kind_it_is, "ShipEffectEngine::FitSkill")
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	CHECK(engine.BuildShip(world, "Ship-01", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));

	bool bought = false;

	// A ShipSystem node, read from the data as it is written: the node's own
	// nodeType says it is a system that goes on a hull, and its targetEffectID is the
	// effect the catalogue moves.
	CHECK(engine.FitSkill(
	    world, "Ship-01", Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 3), 3, bought,
	    errorMessage));

	CHECK(bought);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 156.0);

	// Buying the node twice is fitting the system at a better tier, not bolting a
	// second shield bank to the hull, and the tier the ship carries is the better of
	// the two.
	CHECK(engine.FitSkill(
	    world, "Ship-01", Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 1), 1, bought,
	    errorMessage));

	CHECK(bought);
	CHECK_EQ(engine.FittedTier(world, "Ship-01", "ShieldFront"), 3);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 1u);

	// An age gate names no effect at all, and a node that names no effect buys nothing
	// on a ship: the walk of a whole tree asks this question of every node.
	CHECK(engine.FitSkill(world, "Ship-01", Node("Age02", "Age02", "AgeGate", 1, 1, "", 0), 1, bought, errorMessage));
	CHECK(!bought);

	// A node of another domain: the yard of the same tree is a place, not a system,
	// and the ship engine is not where a yard lives. The node is not an error; it is a
	// node the ship engine has no room for, and the caller writes it in a yard store.
	CHECK(engine.FitSkill(
	    world, "Ship-01", Node("SkillShipShipyardsDrydock", "Age04", "ShipYard", 3, 4, "Drydock", 1), 1, bought,
	    errorMessage));

	CHECK(!bought);
	CHECK(!world.Has("Yard-Civ-01"));
	CHECK(engine.HasShip(world, "Ship-01"));

	CHECK(engine.FitSkill(
	    world, "Ship-01", Node("SkillShipHullDrydock", "Age05", "ShipHull", 5, 12, "Drydock", 5), 1, bought,
	    errorMessage));

	CHECK(!bought);

	// An effect id the catalogue does not have is the data naming a number the
	// catalogue has no word for, and that is the failure the test wants to hear about.
	CHECK(!engine.FitSkill(
	    world, "Ship-01", Node("SkillShipBroken", "Age02", "ShipSystem", 3, 1, "NoSuchEffect", 1), 1, bought,
	    errorMessage));

	CHECK_CONTAINS(errorMessage, "NoSuchEffect");
	CHECK_CONTAINS(errorMessage, "SkillShipBroken");
	CHECK_CONTAINS(errorMessage, "catalogue");
	CHECK(!bought);

	// A node with an empty id is a node no report can name.
	CHECK(!engine.FitSkill(world, "Ship-01", SkillNode{}, 1, bought, errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");

	// The ship's own tier is the tier the node was bought at, and the effect of the
	// node is what moved the number.
	CHECK_EQ(engine.FittedTier(world, "Ship-01", "ShieldFront"), 3);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 156.0);
}


TEST_CASE(a_tree_of_bought_nodes_fits_the_ships_it_bought, "ShipEffectEngine::ApplyProgression")
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	// A tree of the scaffold: two ship systems bought, one ship system not bought, one
	// yard bought (not the ship engine's), one gate (which names no effect), and one
	// node of a domain the ship engine has no room for.
	const std::vector<SkillNode> nodes = {
	    Node("SkillShipPropulsionCruisePlates", "Age02", "ShipSystem", 3, 1, "CruisePlates", 2),
	    Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 3),
	    Node("SkillShipShieldsRear", "Age02", "ShipSystem", 5, 2, "ShieldRear", 2),
	    Node("SkillShipShipyardsDrydock", "Age04", "ShipYard", 3, 4, "Drydock", 1),
	    Node("SkillPiracyRaiderHulls", "Age03", "ShipHull", 3, 2, "RaiderHulls", 2),
	    Node("Age02", "Age02", "AgeGate", 1, 1, "", 0),
	};

	ProgressionManager progression = Tree(nodes, 20);

	CHECK(progression.TryUpgrade("SkillShipPropulsionCruisePlates", errorMessage));
	CHECK(progression.TryUpgrade("SkillShipPropulsionCruisePlates", errorMessage));
	CHECK(progression.TryUpgrade("SkillShipShieldsFront", errorMessage));
	CHECK(progression.TryUpgrade("SkillShipShieldsFront", errorMessage));
	CHECK(progression.TryUpgrade("SkillShipShieldsFront", errorMessage));
	CHECK(progression.TryUpgrade("SkillShipShipyardsDrydock", errorMessage));
	CHECK(progression.TryUpgrade("SkillPiracyRaiderHulls", errorMessage));
	CHECK_EQ(progression.GetCurrentTier("SkillShipShieldsRear"), 0);
	CHECK_EQ(progression.GetCurrentTier("SkillShipShieldsFront"), 3);

	CHECK(engine.BuildShip(world, "Ship-01", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));
	CHECK(engine.BuildShip(world, "Ship-02", ShipEffectEngine::LongHaulTrader(), "Civ-02", errorMessage));

	unsigned applied = 0;
	unsigned skipped = 0;

	// One civilisation's tree onto one ship: the plates at 2 tiers and the front bank
	// at 3, and nothing of the yard, the raider hulls' market risk, the unbought rear
	// bank, or the gate.
	CHECK(engine.ApplyProgression(world, progression, "Ship-01", applied, skipped, errorMessage));

	CHECK_EQ(applied, 2u);
	CHECK_EQ(skipped, 4u);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 156.0);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::SpeedMaxKms), 26.0);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::ShieldRear), 0.0);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 2u);
	CHECK(engine.FittedEffects(world, "Ship-01")[0] < engine.FittedEffects(world, "Ship-01")[1]);
	CHECK(!world.Has("Yard-Civ-01"));

	// A technology reaches every ship of the world that bought it: the raider hulls
	// are a fact about the fleet, and a fleet of one ship is a fleet.
	unsigned fleetApplied = 0;
	unsigned fleetSkipped = 0;

	CHECK(engine.ApplyProgressionToWorld(world, progression, "Civ-02", fleetApplied, fleetSkipped, errorMessage));

	CHECK_EQ(fleetApplied, 1u);
	CHECK_EQ(engine.GetStat(world, "Ship-02", EffectStat::CargoCapacity), 340.0);
	CHECK_EQ(engine.GetStat(world, "Ship-02", EffectStat::HullIntegrity), 120.0);
	CHECK(engine.FittedEffects(world, "Ship-02").size() == 1);

	// The respec road: the tiers go back to the pool, and the systems come off every
	// hull they were bolted to.
	CHECK_EQ(engine.UnfitSkill(world, "SkillShipShieldsFront", errorMessage), 1u);

	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 120.0);
	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::SpeedMaxKms), 26.0);
	CHECK_EQ(engine.FittedEffects(world, "Ship-01").size(), 1u);
	CHECK_EQ(engine.UnfitSkill(world, "SkillShipShieldsFront", errorMessage), 0u);

	// Every system off, and the hull is the hull it built.
	CHECK_EQ(engine.UnfitSkill(world, "SkillShipPropulsionCruisePlates", errorMessage), 1u);
	CHECK_EQ(engine.UnfitSkill(world, "SkillShipShipyardsDrydock", errorMessage), 0u);
	CHECK_EQ(engine.UnfitSkill(world, "SkillPiracyRaiderHulls", errorMessage), 1u);

	CHECK_EQ(engine.GetStat(world, "Ship-01", EffectStat::SpeedMaxKms), 22.0);
	CHECK_EQ(engine.GetStat(world, "Ship-02", EffectStat::CargoCapacity), 400.0);
	CHECK(engine.FittedEffects(world, "Ship-01").empty());
	CHECK(engine.FittedEffects(world, "Ship-02").empty());
	CHECK(!world.Has("Yard-Civ-02"));

	CHECK(!engine.ApplyProgression(world, progression, "Ship-99", applied, skipped, errorMessage));
	CHECK_CONTAINS(errorMessage, "Ship-99");
	CHECK_EQ(engine.RecomputeAll(world, errorMessage), 2u);
}


TEST_CASE(a_ship_read_out_names_the_hull_the_systems_and_the_numbers)
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	CHECK(engine.BuildShip(world, "Ship-01", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));
	CHECK(engine.Fit(world, "Ship-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 3, errorMessage));
	CHECK(engine.Fit(world, "Ship-01", "SkillShipPropulsionFusionDrive", "FusionDrive", EffectOrigin::Technology, 2, errorMessage));

	const std::string out = engine.DescribeShip(world, "Ship-01");

	CHECK_CONTAINS(out, "Ship-01");
	CHECK_CONTAINS(out, "Long-haul trader");
	CHECK_CONTAINS(out, "Civ-01");
	CHECK_CONTAINS(out, "Front shield bank");
	CHECK_CONTAINS(out, "Fusion drive");
	CHECK_CONTAINS(out, "156");
	CHECK_CONTAINS(out, "tier 3");
	CHECK_CONTAINS(out, "skill");
	CHECK_CONTAINS(out, "technology");
	CHECK_CONTAINS(out, "hull integrity");

	CHECK_CONTAINS(engine.DescribeShip(world, "Ship-99"), "no ship");
	CHECK(!engine.Fit(world, "Ship-99", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!engine.UnfitEffect(world, "Ship-99", "ShieldFront", errorMessage));
	CHECK(engine.ShipsOf(world, "Civ-99").empty());
}


TEST_CASE(a_civilisation_effect_lands_on_the_store_its_domain_owns)
{
	EcsWorld world;
	CivilisationEffectEngine engine;
	std::string errorMessage;

	AddCivilisation(world, "Civ-01", "Abigail Prime");
	AddCivilisation(world, "Civ-02", "Beacon Reach");
	AddVenue(world, "Civ-01", "Civ-01");

	// A yard: one store per civilisation, and the tiers of the tree that bought it.
	CHECK(engine.Apply(world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 2, errorMessage));

	const std::map<EffectStat, double> yard = engine.YardStatsOf(world, "Civ-01");

	CHECK_EQ(yard.size(), 3u);
	CHECK_EQ(ReadEffectStat(yard, EffectStat::DrydockThroughput), 20.0);
	CHECK_EQ(ReadEffectStat(yard, EffectStat::DrydockSlots), 2.0);
	CHECK_EQ(ReadEffectStat(yard, EffectStat::RepairBays), 2.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-01"), EffectStat::DrydockThroughput), 20.0);
	CHECK_EQ(engine.YardStatsOf(world, "Civ-02").size(), 0u);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-02"), EffectStat::DrydockSlots), 0.0);
	CHECK(world.Has("Yard-Civ-01"));
	CHECK(!world.Has("Yard-Civ-02"));
	CHECK(world.HasComponent<YardEffects, EntityId>("Yard-Civ-01"));

	// Lanes: the lane effects of the tree are one store per civilisation, and the
	// market reads the risk off it.
	CHECK(engine.Apply(world, "Civ-01", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 3, errorMessage));
	CHECK(engine.Apply(world, "Civ-02", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 1, errorMessage));

	CHECK_EQ(engine.ReadStat(world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"), EffectStat::RouteRiskPremium), -36.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"), EffectStat::RouteLaneSpeed), 1.05);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"), EffectStat::RouteConvoySize), 6.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-02"), EffectStat::RouteRiskPremium), -12.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"), EffectStat::DrydockThroughput), 0.0);
	CHECK_EQ(engine.RouteStatsOf(world, "Civ-01").size(), 3u);
	CHECK(world.HasComponent<RouteEffects, EntityId>("Lanes-Civ-01"));

	// Claims: a survey store, and the claim of a tier that pays for itself.
	CHECK(engine.Apply(world, "Civ-01", "SkillSurveyDeepSurvey", "DeepSurvey", EffectOrigin::Skill, 4, errorMessage));

	CHECK_EQ(engine.ReadStat(world, EffectDomain::Survey, CivilisationEffectEngine::SurveyStoreId("Civ-01"), EffectStat::SurveyAnomalyFind), 32.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Survey, CivilisationEffectEngine::SurveyStoreId("Civ-01"), EffectStat::SurveyClaimYield), 1.6);
	CHECK_EQ(engine.SurveyStatsOf(world, "Civ-01").size(), 2u);
	CHECK(world.HasComponent<SurveyEffects, EntityId>("Survey-Civ-01"));

	// A market effect lands on the venues of the civilisation that bought it, and the
	// law effect of a warrant lands on the same venue: the licence and the warrant
	// clear the deck, and the deck is where the trade happens.
	CHECK(engine.Apply(world, "Civ-01", "SkillLawDockingLicence", "DockingLicence", EffectOrigin::Skill, 3, errorMessage));
	CHECK(engine.Apply(world, "Civ-01", "SkillTradeTribunal", "TradeTribunal", EffectOrigin::Skill, 1, errorMessage));
	CHECK(engine.Apply(world, "Civ-01", "SkillTradeCargoInsurance", "CargoInsurance", EffectOrigin::Skill, 3, errorMessage));

	const std::map<EffectStat, double> market = engine.MarketStatsOf(world, "Civ-01");

	CHECK_EQ(ReadEffectStat(market, EffectStat::MarketClearance), 60.0);
	CHECK_EQ(ReadEffectStat(market, EffectStat::MarketBypass), 15.0);
	CHECK_EQ(ReadEffectStat(market, EffectStat::MarketInsurance), 12.0);
	CHECK_EQ(ReadEffectStat(market, EffectStat::MarketRiskPremium), 0.0);
	CHECK_EQ(market.size(), 3u);
	CHECK(world.HasComponent<MarketEffects, EntityId>("Civ-01"));

	// A second venue of the same civilisation carries the same clearance: the licence
	// is a law of the world, not a rule of one place.
	AddVenue(world, "Beacon-Station", "Civ-01");
	CHECK(engine.Apply(world, "Civ-01", "SkillLawDockingLicence", "DockingLicence", EffectOrigin::Skill, 3, errorMessage));

	CHECK_EQ(engine.ReadStat(world, EffectDomain::Market, "Beacon-Station", EffectStat::MarketClearance), 60.0);
	CHECK_EQ(engine.MarketsOf(world, "Civ-01").size(), 2u);
	CHECK_EQ(engine.MarketsOf(world, "Civ-02").size(), 0u);
	CHECK_EQ(engine.MarketStatsOf(world, "Civ-02").size(), 0u);

	// A body effect lands on the bodies the civilisation owns, and on no one else's:
	// the domes of Abigail Prime are not the domes of Beacon Reach.
	CHECK(engine.Apply(world, "Civ-01", "SkillHabitatDomes", "HabitatDomes", EffectOrigin::Skill, 2, errorMessage));
	CHECK(engine.Apply(world, "Civ-01", "SkillServicePublicMedicine", "PublicMedicine", EffectOrigin::Skill, 4, errorMessage));
	CHECK(engine.Apply(world, "Civ-02", "SkillHabitatDomes", "HabitatDomes", EffectOrigin::Skill, 1, errorMessage));

	CHECK_EQ(engine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::PopCapBonus), 24.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::PublicHealth), 32.0);
	CHECK_EQ(engine.BodyStatsOf(world, "Civ-01").size(), 2u);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Body, "Civ-02", EffectStat::PopCapBonus), 12.0);
	CHECK_EQ(engine.BodyStatsOf(world, "Civ-02").size(), 1u);
	CHECK_EQ(engine.BodiesOf(world, "Civ-01").size(), 1u);
	CHECK(world.HasComponent<BodyEffects, EntityId>("Civ-01"));

	// A source, the tier it was bought at, and the store it landed on: the report a
	// player reads when the question is "what did my tree buy me?".
	const std::map<std::string, EffectSource> marketSources = engine.SourcesOn(world, EffectDomain::Market, "Civ-01");

	CHECK_EQ(marketSources.size(), 3u);
	CHECK_EQ(marketSources.at("CargoInsurance").tier, 3);
	CHECK_EQ(marketSources.at("CargoInsurance").skillId, std::string("SkillTradeCargoInsurance"));
	CHECK(marketSources.at("DockingLicence").origin == EffectOrigin::Skill);
	CHECK_EQ(engine.SourcesOn(world, EffectDomain::Body, "Civ-01").size(), 2u);
	CHECK_EQ(engine.SourcesOn(world, EffectDomain::Yard, "Yard-Civ-01").size(), 1u);
	CHECK_EQ(engine.SourcesOn(world, EffectDomain::Route, "Lanes-Civ-01").size(), 1u);
}


TEST_CASE(the_civilisation_engine_and_the_ship_engine_split_the_tree_between_them)
{
	EcsWorld world;
	CivilisationEffectEngine engine;
	std::string errorMessage;

	AddCivilisation(world, "Civ-01", "Abigail Prime");
	AddVenue(world, "Civ-01", "Civ-01");

	// A ship system is the ship engine's: the engine that writes stores refuses it, and
	// the message names the engine that does write hulls, so a caller that hands in the
	// whole tree is told which half it asked for.
	CHECK(!engine.Apply(world, "Civ-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));

	CHECK_CONTAINS(errorMessage, "ShieldFront");
	CHECK_CONTAINS(errorMessage, "ShipEffectEngine");
	CHECK_CONTAINS(errorMessage, "Ship domain");
	CHECK_EQ(engine.BodiesOf(world, "Civ-01").size(), 1u);
	CHECK_EQ(engine.SourcesOn(world, EffectDomain::Body, "Civ-01").size(), 0u);

	// A node of the tree reads the same way: the gate buys nothing and is not an error,
	// the ship system is the ship engine's and is not an error, and the yard is this
	// engine's, so one loop walks the whole tree.
	bool bought = false;

	CHECK(engine.ApplySkill(world, "Civ-01", Node("Age02", "Age02", "AgeGate", 1, 1, "", 0), 1, bought, errorMessage));
	CHECK(!bought);

	CHECK(engine.ApplySkill(
	    world, "Civ-01", Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 2), 2, bought,
	    errorMessage));

	CHECK(!bought);

	CHECK(engine.ApplySkill(
	    world, "Civ-01", Node("SkillShipShipyardsDrydock", "Age04", "ShipYard", 3, 4, "Drydock", 3), 3, bought,
	    errorMessage));

	CHECK(bought);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, "Yard-Civ-01", EffectStat::DrydockThroughput), 30.0);

	// A hull id that is not a body, a civilisation that is not an entity, a tier below
	// 1, and an effect the catalogue has no word for.
	CHECK(!engine.Apply(world, "Ship-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Ship-01");
	CHECK(!engine.Apply(world, "Civ-99", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-99");
	CHECK(!engine.Apply(world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 0, errorMessage));
	CHECK_CONTAINS(errorMessage, "tier");
	CHECK(!engine.Apply(world, "Civ-01", "SkillShipShipyardsDrydock", "NoSuchEffect", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "NoSuchEffect");
	CHECK(!engine.ApplySkill(world, "Civ-01", SkillNode{}, 1, bought, errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");

	CHECK(engine.Apply(world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 2, errorMessage));
	CHECK(!engine.Unapply(world, "Civ-01", "SkillShipShipyardsRiggingLoft", "Drydock", errorMessage));
	CHECK_CONTAINS(errorMessage, "SkillShipShipyardsRiggingLoft");
	CHECK(engine.Unapply(world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", errorMessage));
	CHECK(!engine.Unapply(world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", errorMessage));
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, "Yard-Civ-01", EffectStat::DrydockThroughput), 0.0);
	CHECK_EQ(engine.SourcesOn(world, EffectDomain::Yard, "Yard-Civ-01").size(), 0u);
}


TEST_CASE(a_bought_tree_writes_the_stores_of_the_civilisation_that_bought_it)
{
	EcsWorld world;
	CivilisationEffectEngine engine;
	std::string errorMessage;

	AddCivilisation(world, "Civ-01", "Abigail Prime");
	AddCivilisation(world, "Civ-02", "Beacon Reach");
	AddVenue(world, "Civ-01", "Civ-01");

	const std::vector<SkillNode> nodes = {
	    Node("SkillShipShipyardsDrydock", "Age04", "ShipYard", 3, 4, "Drydock", 2),
	    Node("SkillPiracyConvoy", "Age03", "ShipHull", 3, 2, "Convoy", 3),
	    Node("SkillSurveyDeepSurvey", "Age02", "ShipSystem", 5, 1, "DeepSurvey", 4),
	    Node("SkillHabitatDomes", "Age03", "Habitat", 4, 6, "HabitatDomes", 1),
	    Node("SkillLawDockingLicence", "Age02", "Law", 4, 1, "DockingLicence", 2),
	    Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 3),
	    Node("Age02", "Age02", "AgeGate", 1, 1, "", 0),
	};

	ProgressionManager progression = Tree(nodes, 40);

	CHECK(progression.TryUpgrade("SkillShipShipyardsDrydock", errorMessage));
	CHECK(progression.TryUpgrade("SkillPiracyConvoy", errorMessage));
	CHECK(progression.TryUpgrade("SkillSurveyDeepSurvey", errorMessage));
	CHECK(progression.TryUpgrade("SkillSurveyDeepSurvey", errorMessage));
	CHECK(progression.TryUpgrade("SkillSurveyDeepSurvey", errorMessage));
	CHECK(progression.TryUpgrade("SkillSurveyDeepSurvey", errorMessage));
	CHECK(progression.TryUpgrade("SkillHabitatDomes", errorMessage));
	CHECK(progression.TryUpgrade("SkillHabitatDomes", errorMessage));
	CHECK(progression.TryUpgrade("SkillLawDockingLicence", errorMessage));
	CHECK(progression.TryUpgrade("SkillLawDockingLicence", errorMessage));
	CHECK(progression.TryUpgrade("SkillLawDockingLicence", errorMessage));
	CHECK_EQ(progression.GetCurrentTier("SkillShipShieldsFront"), 0);

	unsigned applied = 0;
	unsigned skipped = 0;

	CHECK(engine.ApplyProgression(world, progression, "Civ-01", applied, skipped, errorMessage));

	// The yard at 2, the lanes at 3, the claim at 4, the domes at 1, the licence at 3:
	// five stores, in the five places the domains keep.
	CHECK_EQ(applied, 5u);

	// The ship system no player bought, the age gate that names no effect, and the
	// nodes of a domain this engine has no store for.
	CHECK_EQ(skipped, 2u);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, "Yard-Civ-01", EffectStat::DrydockSlots), 2.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Route, "Lanes-Civ-01", EffectStat::RouteRiskPremium), -36.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Survey, "Survey-Civ-01", EffectStat::SurveyAnomalyFind), 32.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::PopCapBonus), 12.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 40.0);
	CHECK_EQ(engine.MarketsOf(world, "Civ-01").size(), 1u);

	// The stores are entities of the world, one per civilisation, and the ids they
	// answer to are the ids a report prints.
	CHECK(world.Has("Lanes-Civ-01"));
	CHECK(world.Has("Survey-Civ-01"));
	CHECK(world.Has("Yard-Civ-01"));
	CHECK_EQ(CivilisationEffectEngine::LaneStoreId("Civ-01"), std::string("Lanes-Civ-01"));
	CHECK_EQ(CivilisationEffectEngine::SurveyStoreId("Civ-01"), std::string("Survey-Civ-01"));
	CHECK_EQ(CivilisationEffectEngine::YardStoreId("Civ-01"), std::string("Yard-Civ-01"));

	CHECK_EQ(engine.BodiesOf(world, "Civ-01").size(), 1u);
	CHECK_EQ(engine.BodiesOf(world, "Civ-02").size(), 0u);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, "Yard-Civ-02", EffectStat::DrydockSlots), 0.0);

	// The tiers a node bought are one source, at the tier the pool paid for: the
	// survey node at 4 tiers is one source at tier 4, not four sources at tier 1.
	const std::map<std::string, EffectSource> survey = engine.SourcesOn(world, EffectDomain::Survey, "Survey-Civ-01");

	CHECK_EQ(survey.size(), 1u);
	CHECK_EQ(survey.at("DeepSurvey").tier, 4);

	// The respec road, half of it: one skill taken off every store it touched.
	CHECK_EQ(engine.UnapplySkill(world, "SkillSurveyDeepSurvey", errorMessage), 1u);

	CHECK_EQ(engine.ReadStat(world, EffectDomain::Survey, "Survey-Civ-01", EffectStat::SurveyAnomalyFind), 0.0);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Yard, "Yard-Civ-01", EffectStat::DrydockSlots), 2.0);
	CHECK_EQ(engine.UnapplySkill(world, "SkillSurveyDeepSurvey", errorMessage), 0u);

	// The report a player reads when the question is what a whole tree bought.
	const std::string out = engine.DescribeCivilisation(world, "Civ-01");

	CHECK_CONTAINS(out, "Civ-01");
	CHECK_CONTAINS(out, "Front shield bank");
	CHECK_CONTAINS(out, "Drydock throughput");
	CHECK_CONTAINS(out, "lane risk premium");
	CHECK_CONTAINS(out, "-36");
	CHECK_CONTAINS(out, "Abigail Prime");
	CHECK_CONTAINS(out, "population capacity bonus");
	CHECK_CONTAINS(out, "deck clearance");
	CHECK_CONTAINS(out, "Lanes-Civ-01");

	CHECK_CONTAINS(engine.DescribeCivilisation(world, "Civ-99"), "no entity");
	CHECK(engine.DescribeStore(world, EffectDomain::Route, "Lanes-Civ-01").find("Convoy") != std::string::npos);
	CHECK(engine.DescribeStore(world, EffectDomain::Route, "Lanes-Civ-99").find("no entity") != std::string::npos);

	CHECK(!engine.ApplyProgression(world, progression, "Civ-99", applied, skipped, errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-99");
}


TEST_CASE(a_tier_the_pool_cannot_pay_and_a_gate_the_tree_has_not_open_buy_nothing)
{
	EcsWorld world;
	CivilisationEffectEngine engine;
	ShipEffectEngine ships;
	std::string errorMessage;

	AddCivilisation(world, "Civ-01", "Abigail Prime");
	AddVenue(world, "Civ-01", "Civ-01");
	CHECK(ships.BuildShip(world, "Ship-01", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));

	// A node behind a gate the tree has not opened, and a node the pool cannot pay:
	// the ProgressionManager says both in its own words, and the effect engines read
	// the tiers it reports, so a node at tier 0 writes nothing anywhere.
	const std::vector<SkillNode> nodes = {
	    Node("Age02", "Age02", "AgeGate", 1, 1, "", 0),
	    Node("SkillLawDockingLicence", "Age02", "Law", 4, 1, "DockingLicence", 1),
	    Node("Age04", "Age04", "AgeGate", 1, 1, "", 0),
	    Node("SkillShipShipyardsDrydock", "Age04", "ShipYard", 3, 40, "Drydock", 1),
	};

	ProgressionManager progression = Tree(nodes, 2);

	CHECK(progression.TryUpgrade("Age02", errorMessage));
	CHECK(progression.TryUpgrade("SkillLawDockingLicence", errorMessage));
	CHECK(!progression.TryUpgrade("SkillShipShipyardsDrydock", errorMessage));
	CHECK_EQ(progression.GetCurrentTier("SkillShipShipyardsDrydock"), 0);

	unsigned applied = 0;
	unsigned skipped = 0;

	CHECK(engine.ApplyProgression(world, progression, "Civ-01", applied, skipped, errorMessage));

	CHECK_EQ(applied, 1u);
	CHECK_EQ(skipped, 3u);
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 20.0);
	CHECK_EQ(engine.SourcesOn(world, EffectDomain::Yard, "Yard-Civ-01").size(), 0u);
	CHECK(!world.Has("Yard-Civ-01"));
	CHECK_EQ(engine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::PopCapBonus), 0.0);

	CHECK(ships.ApplyProgression(world, progression, "Ship-01", applied, skipped, errorMessage));

	CHECK_EQ(applied, 0u);
	CHECK_EQ(skipped, 4u);
	CHECK_EQ(ships.GetStat(world, "Ship-01", EffectStat::HullIntegrity), 120.0);
	CHECK(ships.FittedEffects(world, "Ship-01").empty());
}

int main()
{
	return civtest::Run("EffectTests");
}
