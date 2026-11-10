#include "Core/ProgressionManager.h"
#include "Core/SimulationConfig.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/CivilisationEffects.h"
#include "Effects/EffectCatalogue.h"
#include "Effects/EffectTypes.h"
#include "Effects/ShipEffectEngine.h"
#include "Market/Marketplace.h"
#include "TestFramework.h"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
using namespace CivSimCore;

// The 58 effects the shipped tree asks for: every non-null targetEffectID in
// data/skill-tree, in the order the files list them. The tree names the effects, the
// catalogue says what a tier of each is worth, and this is the list the two are
// checked against.
const std::vector<std::string> &ShippedEffectIds()
{
	static const std::vector<std::string> ids = {
	    "AdmiraltyWarrant",
	    "AttitudeControl",
	    "BoardingParties",
	    "CargoHolds",
	    "CargoInsurance",
	    "ClaimRefinery",
	    "ClosedLoop",
	    "ComputerNavigation",
	    "ComputerTrader",
	    "Convoy",
	    "Countermeasures",
	    "CrewTraining",
	    "DamageControl",
	    "DeepSurvey",
	    "DockingLicence",
	    "Drydock",
	    "Education",
	    "EmissionsControl",
	    "EngineThrust",
	    "EscortWing",
	    "ExclusiveClaim",
	    "FencingNetwork",
	    "FuelBunkers",
	    "GunFront",
	    "GunRear",
	    "HabitatDomes",
	    "LaserFront",
	    "LaserRear",
	    "LifeSupport",
	    "ManoeuvringJets",
	    "MiningClaim",
	    "ModularHulls",
	    "PiratePardon",
	    "PirateStanding",
	    "PublicMedicine",
	    "RaiderHulls",
	    "RansomExchange",
	    "RefitDocks",
	    "RouteWard",
	    "SafeHarbours",
	    "SalvageRig",
	    "SensorCommercial",
	    "SensorFireControl",
	    "SensorLongRange",
	    "ShieldFront",
	    "ShieldRear",
	    "ShieldSide",
	    "SparesLockers",
	    "StealthCoating",
	    "TerraformAtmosphere",
	    "TerraformBiosphere",
	    "TradeTribunal",
	    "WardenPatrol",
	    "YardOutfitting",
	};

	return ids;
}

// The tree the shipped data builds, as far as the catalogue check is concerned: one
// node per effect the tree asks for.
Skills ShippedTree()
{
	Skills tree;

	for (const std::string &effectId : ShippedEffectIds())
	{
		SkillNode node;

		node.skillId = "Skill" + effectId;
		node.displayName = effectId;
		node.nodeType = NodeType::Skill;
		node.maxTiers = 1;
		node.resourceCostPerTier = 1;
		node.targetEffectID = effectId;

		tree.push_back(node);
	}

	return tree;
}

// One node of the tree: a skill that buys maxTiers of targetEffectId, one tier at a
// time, for cost points a tier. A node that buys nothing - an age gate - names no
// effect, and a Tech node is knowledge a civilisation knows, not a system a hull fits.
SkillNode Node(
    const std::string &skillId, int maxTiers, int cost, const std::string &targetEffectId,
    NodeType nodeType = NodeType::Skill)
{
	SkillNode node;

	node.skillId = skillId;
	node.displayName = skillId;
	node.nodeType = nodeType;
	node.maxTiers = maxTiers;
	node.resourceCostPerTier = cost;
	node.targetEffectID = targetEffectId;

	return node;
}

// A tree and the points that buy it. Both engines read a ProgressionManager, so a test
// that walks a tree builds one, and the points of the pool pay for the tiers.
ProgressionManager Tree(const std::vector<SkillNode> &nodes, int points)
{
	ProgressionManager progression;
	std::string errorMessage;

	CHECK(progression.Build(nodes, errorMessage));
	progression.SetResources(points);

	return progression;
}

// A world with one civilisation's planet in it, and the market at that planet that the
// market effects land on: a venue a world has to open before a law, a licence or a
// policy can be worth anything.
EcsWorld OpenWorld(const std::string &id, const std::string &name)
{
	EcsWorld world;
	MarketplaceService service;
	std::string errorMessage;

	CHECK(MarketplaceService::AddPlanet(world, id, name, name + " System", id, "Age02", errorMessage));
	CHECK(service.Open(world, id, errorMessage));

	return world;
}

// One effect of one domain, for an engine tested against a catalogue of its own.
EffectDefinition Effect(
    const std::string &effectId, EffectDomain domain, EffectOperator op, EffectStat stat,
    double perTier)
{
	EffectDefinition definition;

	definition.effectId = effectId;
	definition.domain = domain;
	definition.displayName = effectId;
	definition.description = effectId;
	definition.modifiers = {{stat, op, perTier}};

	return definition;
}
} // namespace

TEST_CASE(the_ship_engine_lands_its_effects_on_the_hull)
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	const ShipClassSpec trader = ShipEffectEngine::LongHaulTrader();

	CHECK(engine.BuildShip(world, "Trader", trader, "Civ-01", errorMessage));
	CHECK(engine.HasShip(world, "Trader"));
	CHECK_EQ(engine.ShipsOf(world, "Civ-01").size(), 1u);
	CHECK_EQ(engine.ShipsOf(world, "Civ-02").size(), 0u);
	CHECK_EQ(engine.ShipsOf(world, "").size(), 1u);

	const EcsWorld::Entity ship = world.Find("Trader");

	CHECK(ship != EcsWorld::Null);
	CHECK((world.HasComponent<ShipInfo, ShipBaseStats, ShipStats, ShipSlots, ShipSystems>(ship)));
	CHECK_EQ(world.Get<ShipInfo>(ship)->hullId, std::string("HullTraderLongHaul"));
	CHECK_EQ(world.Get<ShipInfo>(ship)->operatorId, std::string("Civ-01"));

	// The hull flies its base: a long-haul trader that has bought none of the tree is
	// the hull of the README, and the systems read the same numbers the engine writes.
	CHECK_NEAR(trader.baseStats.at(EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::CargoVolume), 900.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::SalvageCapacity), 15.0, 1e-9);
	CHECK_EQ(engine.StatsOf(world, "Trader").size(), engine.BaseStatsOf(world, "Trader").size());

	// A tier of a shield bank is 60 MW of forward screen: thickness, not a new slot,
	// and the system sits in the hull's own list of what it carries.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0 + 2 * 60.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldSide), 80.0, 1e-9);
	CHECK_EQ(engine.FittedTier(world, "Trader", "ShieldFront"), 2);
	CHECK_EQ(engine.FittedEffects(world, "Trader").size(), 1u);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK_NEAR(
	    world.Get<ShipStats>(ship)->values.at(EffectStat::ShieldFront),
	    engine.GetStat(world, "Trader", EffectStat::ShieldFront), 1e-9);

	// A refit replaces the tier: the same system on the hull twice is the same system
	// once, which is what a refit dock does when it fits the bank the hull already has.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK_EQ(engine.FittedTier(world, "Trader", "ShieldFront"), 1);

	// A system is the hull's, keyed by the effect it is: a second skill that buys the
	// same forward screen replaces the tier the hull flies, it does not stack a second
	// screen on the first, and the hull carries one system of that name.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsAuxiliary", "ShieldFront", EffectOrigin::Skill, 3,
	    errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0 + 3 * 60.0, 1e-9);
	CHECK_EQ(engine.FittedTier(world, "Trader", "ShieldFront"), 3);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK_EQ(engine.FittedEffects(world, "Trader").size(), 1u);

	// Taking the system off leaves the base, and the hull carries nothing.
	CHECK(engine.UnfitEffect(world, "Trader", "ShieldFront", errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 0u);
	CHECK_EQ(engine.FittedEffects(world, "Trader").size(), 0u);
	CHECK(engine.UnfitEffect(world, "Trader", "ShieldFront", errorMessage));

	// UnfitSkill is the respec road: every source of one skill comes off every hull it
	// touched, and the numbers of those hulls go back.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipGunFronts", "GunFront", EffectOrigin::Skill, 2, errorMessage));
	CHECK(engine.BuildShip(world, "Trader-2", trader, "Civ-01", errorMessage));
	CHECK(engine.Fit(
	    world, "Trader-2", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 4,
	    errorMessage));
	CHECK_EQ(engine.UnfitSkill(world, "SkillShipShieldsFront", errorMessage), 2u);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::GunFront), 24.0 + 2 * 12.0, 1e-9);
	CHECK_EQ(engine.ShipsOf(world, "Civ-01").size(), 2u);

	// The same tree on the same hull is the same numbers: the store is the base plus
	// the sources in sorted effect order, so a fill log, a UI and a test all agree.
	ShipEffectEngine other;
	EcsWorld otherWorld;
	std::string otherError;

	CHECK(other.BuildShip(otherWorld, "Trader", trader, "Civ-01", otherError));
	CHECK(other.Fit(
	    otherWorld, "Trader", "SkillShipGunFronts", "GunFront", EffectOrigin::Skill, 2, otherError));
	CHECK(engine.Recompute(world, "Trader", errorMessage));
	CHECK(other.Recompute(otherWorld, "Trader", otherError));
	CHECK(engine.StatsOf(world, "Trader") == other.StatsOf(otherWorld, "Trader"));

	CHECK(engine.RecomputeAll(world, errorMessage) >= 2u);
	CHECK(engine.DescribeShip(world, "Trader").find("Long-haul trader") != std::string::npos);

	// The engine is the only thing that writes the hull's stats: a hull that has never
	// had an effect reads its base, and a hull the world does not have reads nothing.
	CHECK_NEAR(engine.GetStat(world, "Nope", EffectStat::ShieldFront), 0.0, 1e-9);
	CHECK_EQ(engine.BaseStatsOf(world, "Nope").size(), 0u);
	CHECK_EQ(engine.StatsOf(world, "Nope").size(), 0u);
	CHECK_EQ(engine.FittedEffects(world, "Nope").size(), 0u);
	CHECK(!engine.DescribeShip(world, "Nope").empty());
}

TEST_CASE(the_hull_refuses_a_system_it_has_no_slot_for)
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	const ShipClassSpec trader = ShipEffectEngine::LongHaulTrader();

	CHECK(engine.BuildShip(world, "Trader", trader, "Civ-01", errorMessage));
	CHECK_EQ(trader.fitableEffects.size(), 25u);

	// The hull has a slot for each of the 25 ship systems of the tree, and a system
	// the hull has no frame for does not fit: a salvage rig cannot be fitted to a hull
	// that has no rig frame.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipSalvageRigs", "SalvageRig", EffectOrigin::Skill, 1, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::SalvageCapacity), 15.0 + 10.0, 1e-9);

	// A system outside the 25 has no slot on this hull, and the fit says so.
	EffectCatalogue catalogue = EffectCatalogue::Default();
	CHECK(catalogue.Register(
	    Effect("DroneDefense", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 10.0),
	    errorMessage));
	engine.SetCatalogue(catalogue);
	CHECK(!engine.Fit(
	    world, "Trader", "SkillShipDroneDefense", "DroneDefense", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "slot");

	// A technology is knowledge, not a system: no hull refuses it, and it costs no
	// slot, which is why a civilisation that has bought a technology flies a fleet of
	// it, and why the same effect that a hull refuses as a system it accepts as
	// knowledge.
	CHECK(engine.Fit(
	    world, "Trader", "TechDroneDefense", "DroneDefense", EffectOrigin::Technology, 2,
	    errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0 + 2 * 10.0, 1e-9);

	// The sources on the hull, in sorted effect id order: the rig the hull carries in a
	// slot and the drone defence the civilisation knows, which the hull flies with no
	// frame for it. Both move the hull, so both are sources, and the first of them, by
	// effect id, is the technique - which is knowledge and not a system, so it is not in
	// the systems the yard lists and refits.
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 2u);
	CHECK_EQ(engine.SourcesOn(world, "Trader")[0].effectId, std::string("DroneDefense"));
	CHECK_EQ(engine.SourcesOn(world, "Trader")[0].origin, EffectOrigin::Technology);

	// An effect of another store is not a system at all: the yard, the lane, the claim,
	// the venue and the world belong to the civilisation engine.
	CHECK(!engine.Fit(
	    world, "Trader", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!errorMessage.empty());
	CHECK(!engine.Fit(
	    world, "Trader", "SkillLawDockingLicence", "DockingLicence", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK(!engine.Fit(
	    world, "Trader", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!engine.Fit(
	    world, "Trader", "SkillSurveyDeepScan", "DeepSurvey", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!engine.Fit(
	    world, "Trader", "SkillHabitatDomes", "HabitatDomes", EffectOrigin::Skill, 1, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 140.0, 1e-9);

	// An unknown effect, a tier below 1 and an unknown hull all fail with a message.
	CHECK(!engine.Fit(world, "Trader", "SkillNope", "Nope", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!errorMessage.empty());
	CHECK(!engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 0, errorMessage));
	CHECK(!engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, -1,
	    errorMessage));
	CHECK(!engine.Fit(
	    world, "Nope", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK_CONTAINS(errorMessage, "Nope");
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 140.0, 1e-9);

	// A node of the tree brings its own effect and its own origin: an age gate buys
	// nothing on a hull and is not an error, an effect of another store is skipped the
	// same way, and the node's nodeType is what decides slot against knowledge.
	bool bought = true;

	CHECK(engine.FitSkill(
	    world, "Trader", Node("Age02", 1, 1, "", NodeType::Tech), 1, bought, errorMessage));
	CHECK(!bought);
	CHECK(engine.FitSkill(
	    world, "Trader", Node("SkillLawDockingLicence", 4, 1, "DockingLicence"), 4, bought,
	    errorMessage));
	CHECK(!bought);
	CHECK(engine.FitSkill(
	    world, "Trader", Node("SkillShipShieldsFront", 5, 2, "ShieldFront"), 3, bought, errorMessage));
	CHECK(bought);
	CHECK_NEAR(
	    engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0 + 2 * 10.0 + 3 * 60.0, 1e-9);
	CHECK(engine.FitSkill(
	    world, "Trader", Node("TechSensorLongMasts", 2, 1, "SensorLongRange", NodeType::Tech), 1,
	    bought, errorMessage));
	CHECK(bought);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::SensorLongRange), 4000.0 + 1500.0, 1e-9);

	// The skill a hull carries is the tier of the node, and the systems of one hull
	// come out in sorted effect order, the order a UI and a test both read. A technology
	// is knowledge, not a system: it is a source on the hull, and not one of the systems
	// the hull carries, because no slot was spent for it.
	CHECK_EQ(engine.FittedTier(world, "Trader", "ShieldFront"), 3);
	CHECK_EQ(engine.FittedEffects(world, "Trader").size(), 2u);
	CHECK(engine.FittedTier(world, "Trader", "SensorLongRange") == 0);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 4u);

	// BuildShip is the only way in: it refuses an empty id, a hull with no base, an
	// empty operator and an id the world already has.
	CHECK(!engine.BuildShip(world, "", trader, "Civ-01", errorMessage));
	CHECK(!engine.BuildShip(world, "Trader-3", ShipClassSpec{}, "Civ-01", errorMessage));
	CHECK(!engine.BuildShip(world, "Trader-4", trader, "", errorMessage));
	CHECK(!engine.BuildShip(world, "Trader", trader, "Civ-01", errorMessage));
	CHECK_EQ(engine.ShipsOf(world, "Civ-01").size(), 1u);
}

TEST_CASE(the_shipped_tree_and_the_shipped_catalogue_agree)
{
	const EffectCatalogue catalogue = EffectCatalogue::Default();
	const Skills tree = ShippedTree();
	std::string errorMessage;

	// The join: the tree names the effects by id and says nothing about what a tier is
	// worth, the catalogue says the numbers and nothing about the tree. A name in one
	// and not the other is a node that buys nothing, so the two are checked against
	// each other here.
	CHECK(catalogue.EffectsMissingFrom(tree).empty());
	CHECK(catalogue.CoversTree(tree, errorMessage));
	CHECK_EQ(catalogue.EffectCount(), ShippedEffectIds().size());

	// An effect the catalogue does not define is named, in sorted order, and an age
	// gate with no effect asks for nothing and so is never missing.
	Skills gap = tree;
	SkillNode drone;

	drone.skillId = "SkillShipDroneDefense";
	drone.displayName = "Drone Defense";
	drone.nodeType = NodeType::Skill;
	drone.maxTiers = 2;
	drone.resourceCostPerTier = 1;
	drone.targetEffectID = "DroneDefense";
	gap.push_back(drone);
	gap.push_back(Node("Age03", 1, 1, "", NodeType::Tech));

	CHECK_EQ(catalogue.EffectsMissingFrom(gap).size(), 1u);
	CHECK_EQ(catalogue.EffectsMissingFrom(gap)[0], std::string("DroneDefense"));
	CHECK(!catalogue.CoversTree(gap, errorMessage));
	CHECK_CONTAINS(errorMessage, "DroneDefense");
	CHECK_EQ(catalogue.EffectsMissingFrom(Skills{Node("Age04", 1, 1, "", NodeType::Tech)}).size(), 0u);

	// A catalogue may be bigger than one tree: the effects it defines that this tree
	// does not buy are named, and are not an error.
	CHECK_EQ(
	    catalogue.EffectsUnusedBy(Skills{Node("SkillLawDockingLicence", 4, 1, "DockingLicence")})
	        .size(),
	    catalogue.EffectCount() - 1u);

	// The catalogue the shipped data drives: one effect for every non-null
	// targetEffectID of the tree, in the six stores, and every one of them lands on the
	// store its own domain names.
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Ship).size(), 25u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Yard).size(), 4u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Route).size(), 4u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Survey).size(), 4u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Market).size(), 11u);
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Body).size(), 6u);
	CHECK_EQ(catalogue.EffectIds().size(), catalogue.EffectCount());
	CHECK(catalogue.Validate(errorMessage));

	for (EffectDomain domain :
	     {EffectDomain::Ship, EffectDomain::Yard, EffectDomain::Route, EffectDomain::Survey,
	      EffectDomain::Market, EffectDomain::Body})
	{
		for (const std::string &effectId : catalogue.EffectIdsInDomain(domain))
		{
			const EffectDefinition *definition = catalogue.Find(effectId);

			CHECK(definition != nullptr);
			CHECK_EQ(definition->domain, domain);
			CHECK(definition->modifiers.size() >= 1u);

			for (const EffectModifier &modifier : definition->modifiers)
				CHECK_EQ(StatDomain(modifier.stat), domain);
		}
	}

	// The two engines read the catalogue they are given, and the default is the one the
	// shipped data drives.
	ShipEffectEngine shipEngine;
	CivilisationEffectEngine civEngine;

	CHECK_EQ(shipEngine.Catalogue().EffectCount(), catalogue.EffectCount());
	CHECK_EQ(civEngine.Catalogue().EffectCount(), catalogue.EffectCount());
	CHECK(shipEngine.Catalogue().Find("ShieldFront") != nullptr);
	CHECK(civEngine.Catalogue().Find("SafeHarbours") != nullptr);

	// A catalogue is built, checked and cleared: an empty id, an effect with no
	// modifier, a modifier of zero, a stat of another domain and a duplicate id are all
	// refused, because the join between an effect and its store is what the compiler
	// and this layer exist to check.
	EffectCatalogue custom;

	CHECK(!custom.Register(
	    Effect("", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 10.0),
	    errorMessage));
	CHECK(!custom.Register(
	    EffectDefinition{"NoModifier", EffectDomain::Ship, "No Modifier", "", {}}, errorMessage));
	CHECK(!custom.Register(
	    Effect("ZeroTier", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 0.0),
	    errorMessage));
	CHECK(!custom.Register(
	    Effect("WrongDomain", EffectDomain::Market, EffectOperator::Add, EffectStat::ShieldFront, 10.0),
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "Shield Front");
	CHECK(custom.Register(
	    Effect("DroneDefense", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 10.0),
	    errorMessage));
	CHECK(!custom.Register(
	    Effect("DroneDefense", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 20.0),
	    errorMessage));
	CHECK_EQ(custom.EffectCount(), 1u);
	CHECK_EQ(custom.Find("DroneDefense")->modifiers[0].perTier, 10.0);
	CHECK(custom.Validate(errorMessage));
	CHECK(!custom.Find("Nope"));
	CHECK(!custom.Has("Nope"));

	custom.Clear();
	CHECK_EQ(custom.EffectCount(), 0u);
	CHECK(custom.Build(
	    {Effect("DroneDefense", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 10.0),
	     Effect(
	         "WardenDrones", EffectDomain::Route, EffectOperator::Add, EffectStat::RouteRiskPremium,
	         -4.0)},
	    errorMessage));
	CHECK_EQ(custom.EffectCount(), 2u);
	CHECK_EQ(custom.EffectIds()[0], std::string("DroneDefense"));
	CHECK_EQ(custom.EffectIdsInDomain(EffectDomain::Route).size(), 1u);
	CHECK(custom.Validate(errorMessage));
	CHECK(!custom.Build(
	    {Effect("Broken", EffectDomain::Route, EffectOperator::Add, EffectStat::ShieldFront, 10.0)},
	    errorMessage));
}

TEST_CASE(an_effect_reads_the_domain_of_the_store_it_lands_on)
{
	const EffectCatalogue catalogue = EffectCatalogue::Default();
	std::string errorMessage;

	// Every stat belongs to one store, and the store is the domain of the effect that
	// moves it: the shield is the hull's, the risk of the lane is the lane's, the
	// clearance of a venue is the venue's, the throughput of a dock is the yard's, the
	// reach of a survey is the survey's, the air of a world is the world's.
	CHECK_EQ(StatDomain(EffectStat::ShieldFront), EffectDomain::Ship);
	CHECK_EQ(StatDomain(EffectStat::CargoVolume), EffectDomain::Ship);
	CHECK_EQ(StatDomain(EffectStat::MarketRiskPremium), EffectDomain::Market);
	CHECK_EQ(StatDomain(EffectStat::MarketClearance), EffectDomain::Market);
	CHECK_EQ(StatDomain(EffectStat::RouteRiskPremium), EffectDomain::Route);
	CHECK_EQ(StatDomain(EffectStat::YardDockThroughput), EffectDomain::Yard);
	CHECK_EQ(StatDomain(EffectStat::SurveyRange), EffectDomain::Survey);
	CHECK_EQ(StatDomain(EffectStat::BodyHabitatCapacity), EffectDomain::Body);
	CHECK_EQ(StatDomain(EffectStat::TerraformAtmosphere), EffectDomain::Body);
	CHECK_EQ(StatDomain(AllEffectStats()[0]), EffectDomain::Ship);

	for (EffectStat stat : AllEffectStats())
		CHECK(
		    StatDomain(stat) == EffectDomain::Ship || StatDomain(stat) == EffectDomain::Yard ||
		    StatDomain(stat) == EffectDomain::Route || StatDomain(stat) == EffectDomain::Survey ||
		    StatDomain(stat) == EffectDomain::Market || StatDomain(stat) == EffectDomain::Body);

	// The engine of one store lands on that store and refuses the others: a law, a
	// policy, a licence, a yard, a lane, a claim and a habitat are the civilisation's,
	// and a shield bank, a gun, a mast and a hold are the hull's. The refusal says
	// which engine owns the effect, because a game mode that lands a shield stat at a
	// market has built a rule nothing reads.
	EcsWorld world = OpenWorld("Civ-01", "Abigail Prime");
	CivilisationEffectEngine civEngine;
	ShipEffectEngine shipEngine;
	std::string shipError;
	std::string civError;

	CHECK(
	    shipEngine.BuildShip(world, "Trader", ShipEffectEngine::LongHaulTrader(), "Civ-01", shipError));

	CHECK(!civEngine.Apply(
	    world, "Civ-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, civError));
	CHECK_CONTAINS(civError, "ShipEffectEngine");

	// The same effect the civilisation engine refuses is the hull's own, and the hull
	// has a slot for it: the refusal names the engine, and the engine that owns the
	// effect lands it.
	CHECK(shipEngine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, shipError));
	CHECK(shipError.empty());
	CHECK(!shipEngine.Fit(
	    world, "Trader", "SkillLawDockingLicence", "DockingLicence", EffectOrigin::Skill, 1,
	    shipError));
	CHECK(!shipEngine.Fit(
	    world, "Trader", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 1, shipError));

	// The catalogue names the domain of each effect the shipped tree asks for, and the
	// two spellings of the same number - the domain of the effect and the domain of its
	// stat - are the same store.
	const EffectDefinition *shield = catalogue.Find("ShieldFront");
	const EffectDefinition *licence = catalogue.Find("DockingLicence");
	const EffectDefinition *insurance = catalogue.Find("CargoInsurance");
	const EffectDefinition *safe = catalogue.Find("SafeHarbours");
	const EffectDefinition *raiders = catalogue.Find("RaiderHulls");
	const EffectDefinition *convoy = catalogue.Find("Convoy");
	const EffectDefinition *drydock = catalogue.Find("Drydock");
	const EffectDefinition *scan = catalogue.Find("DeepSurvey");
	const EffectDefinition *air = catalogue.Find("TerraformAtmosphere");

	CHECK(shield != nullptr);
	CHECK_EQ(shield->domain, EffectDomain::Ship);
	CHECK_NEAR(shield->modifiers[0].perTier, 60.0, 1e-9);
	CHECK_EQ(licence->domain, EffectDomain::Market);
	CHECK_EQ(licence->modifiers.size(), 2u);
	CHECK_NEAR(licence->modifiers[0].perTier, 20.0, 1e-9);
	CHECK_NEAR(licence->modifiers[1].perTier, -5.0, 1e-9);
	CHECK_EQ(insurance->domain, EffectDomain::Market);
	CHECK_NEAR(insurance->modifiers[0].perTier, -40.0, 1e-9);
	CHECK_EQ(safe->domain, EffectDomain::Market);
	CHECK_NEAR(safe->modifiers[0].perTier, -14.0, 1e-9);
	CHECK_NEAR(safe->modifiers[1].perTier, 25.0, 1e-9);
	CHECK_NEAR(raiders->modifiers[0].perTier, 22.0, 1e-9);
	CHECK_EQ(convoy->domain, EffectDomain::Route);
	CHECK_NEAR(convoy->modifiers[0].perTier, -12.0, 1e-9);
	CHECK_EQ(drydock->domain, EffectDomain::Yard);
	CHECK_NEAR(drydock->modifiers[0].perTier, 1.0, 1e-9);
	CHECK_EQ(scan->domain, EffectDomain::Survey);
	CHECK_NEAR(scan->modifiers[0].perTier, 12.0, 1e-9);
	CHECK_EQ(air->domain, EffectDomain::Body);
	CHECK_NEAR(air->modifiers[0].perTier, 0.1, 1e-9);

	// The lane is one number seen four ways: the convoy, the escort, the ward and the
	// patrol all make sailing it safer, and all four land on the same stat.
	CHECK_EQ(catalogue.EffectIdsInDomain(EffectDomain::Route).size(), 4u);

	for (const std::string &effectId : catalogue.EffectIdsInDomain(EffectDomain::Route))
	{
		const EffectDefinition *definition = catalogue.Find(effectId);

		CHECK_EQ(definition->modifiers.size(), 1u);
		CHECK_EQ(definition->modifiers[0].stat, EffectStat::RouteRiskPremium);
		CHECK(definition->modifiers[0].perTier < 0.0);
	}

	// The law, the trade offices and the wardens lower the risk of a venue, the pirates
	// raise it, and both land on the same stat: the venue's fee is the sum of what a
	// civilisation has bought on both sides of it.
	double lowered = 0.0;
	double raised = 0.0;

	for (const std::string &effectId : catalogue.EffectIdsInDomain(EffectDomain::Market))
	{
		const EffectDefinition *definition = catalogue.Find(effectId);

		for (const EffectModifier &modifier : definition->modifiers)
		{
			if (modifier.stat != EffectStat::MarketRiskPremium)
				continue;

			if (modifier.perTier < 0.0)
				lowered += modifier.perTier;
			else
				raised += modifier.perTier;
		}
	}

	CHECK(lowered < 0.0);
	CHECK(raised > 0.0);

	// The words the UI prints: every stat has a name and the name is a phrase of words,
	// and the enum's spelling is never printed.
	for (EffectStat stat : AllEffectStats())
	{
		const std::string name = StatName(stat);

		CHECK(name.find("EffectStat") == std::string::npos);
		CHECK(name != "Unknown Stat");
		CHECK(!name.empty());
	}

	CHECK_EQ(std::string(StatName(EffectStat::ShieldFront)), std::string("Shield Front"));
	CHECK_EQ(std::string(StatUnit(EffectStat::ShieldFront)), std::string("MW"));
	CHECK_EQ(std::string(StatUnit(EffectStat::MarketRiskPremium)), std::string("bps"));
	CHECK_EQ(std::string(StatUnit(EffectStat::RouteRiskPremium)), std::string("bps"));
	CHECK_EQ(std::string(StatUnit(EffectStat::SensorLongRange)), std::string("km"));
	CHECK_EQ(std::string(StatUnit(EffectStat::CargoVolume)), std::string("m3"));
	CHECK_EQ(std::string(StatUnit(EffectStat::SurveyRange)), std::string("AU"));
	CHECK_EQ(std::string(StatUnit(EffectStat::BodyHabitatCapacity)), std::string("world units"));
	CHECK_EQ(std::string(DescribeEffectDomain(EffectDomain::Market)), std::string("Market"));
	CHECK_EQ(std::string(DescribeEffectOrigin(EffectOrigin::Technology)), std::string("Technology"));
	CHECK_EQ(std::string(DescribeEffectOperator(EffectOperator::Add)), std::string("add"));
	CHECK_CONTAINS(
	    DescribeEffectModifier({EffectStat::MarketRiskPremium, EffectOperator::Add, -12.0}),
	    "Market Risk");
	CHECK_EQ(AllEffectStats().size(), 46u);
}

TEST_CASE(the_civilisation_engine_lands_its_effects_on_the_stores_its_domain_owns)
{
	CivilisationEffectEngine engine;
	EcsWorld world = OpenWorld("Civ-01", "Abigail Prime");
	std::string errorMessage;

	CHECK_EQ(engine.MarketsOf(world, "Civ-01").size(), 1u);
	CHECK_EQ(engine.BodiesOf(world, "Civ-01").size(), 1u);

	// The venue: a licence is a berth and the clearance it buys, a policy is the
	// premium the cargo carries, and both lower the risk of the place, which is the
	// number the fee is built from.
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4,
	    errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 20.0,
	    1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium), -60.0,
	    1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketInsurance), 0.0, 1e-9);
	CHECK_EQ(engine.MarketStatsOf(world, "Civ-01").size(), 2u);
	CHECK_EQ(engine.MarketsOf(world, "Civ-01").size(), 1u);
	CHECK_EQ(engine.MarketsOf(world, "Civ-02").size(), 0u);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Nope", EffectStat::MarketClearance), 0.0, 1e-9);

	// One source per effect on one store: the same skill applied twice is the same law
	// once, and the venue charges the same fee, which is what keeps a save-load, a UI
	// and a test from adding a tier to a law that is already in force.
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4,
	    errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 20.0,
	    1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium), -60.0,
	    1e-9);
	CHECK_EQ(world.Get<MarketEffects>(world.Find("Civ-01"))->values.size(), 2u);

	// Two sources of one stat on one store: the licence, the bench, the harbours, the
	// crews and the pardon all move the risk of the venue, and the venue's fee is the
	// sum of them.
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillTradeTribunal", "TradeTribunal", EffectOrigin::Skill, 2, errorMessage));
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillPiracySafeHarbours", "SafeHarbours", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketDepth),
	    2 * 40.0 + 25.0, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    -60.0 - 2 * 9.0 - 14.0, 1e-9);

	// The lane: a convoy is the cheapest way to buy safety and buys the most, and the
	// lane is one shared store per civilisation, created by the first effect that needs
	// it, because a lane is the civilisation's, not a world's.
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 2, errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    -24.0, 1e-9);
	CHECK(world.Get<RouteEffects>(CivilisationEffectEngine::LaneStoreId("Civ-01")) != nullptr);
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillPiracyEscortWing", "EscortWing", EffectOrigin::Skill, 1, errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    -24.0 - 9.0, 1e-9);

	// The yard, the survey and the world: each on the store its own domain names, and
	// each a shared store per civilisation except the world's own bodies and venues.
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 3, errorMessage));
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillShipShipyardsOutfitting", "YardOutfitting", EffectOrigin::Skill, 2,
	    errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-01"),
	        EffectStat::YardDockThroughput),
	    3.0, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-01"),
	        EffectStat::YardFitSlots),
	    2.0, 1e-9);

	CHECK(engine.Apply(
	    world, "Civ-01", "SkillSurveyDeepScan", "DeepSurvey", EffectOrigin::Skill, 3, errorMessage));
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillSurveyMiningClaims", "MiningClaim", EffectOrigin::Skill, 2,
	    errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Survey, CivilisationEffectEngine::SurveyStoreId("Civ-01"),
	        EffectStat::SurveyRange),
	    36.0, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Survey, CivilisationEffectEngine::SurveyStoreId("Civ-01"),
	        EffectStat::ClaimCount),
	    4.0, 1e-9);

	CHECK(engine.Apply(
	    world, "Civ-01", "SkillHabitatPublicMedicine", "PublicMedicine", EffectOrigin::Skill, 2,
	    errorMessage));
	CHECK(engine.Apply(
	    world, "Civ-01", "SkillHabitatClosedLoop", "ClosedLoop", EffectOrigin::Skill, 1, errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::MedicineCoverage), 0.5, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::BodyRecyclingLoop), 0.15,
	    1e-9);
	CHECK_EQ(engine.BodyStatsOf(world, "Civ-01").size(), 2u);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Body, "Station-Civ-01", EffectStat::MedicineCoverage), 0.0,
	    1e-9);

	// The same effect on two civilisations: a law is the law of one civilisation, and
	// the venue, the lane, the yard and the survey of another are stores of their own,
	// so the same tree buys a second civilisation the same numbers and no civilisation
	// pays for another's law.
	CHECK(engine.Apply(
	    world, "Civ-02", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 5, errorMessage));
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    -33.0, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-02"),
	        EffectStat::RouteRiskPremium),
	    -60.0, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium), -92.0,
	    1e-9);
	CHECK_NEAR(
	    engine.ReadStat(world, EffectDomain::Market, "Civ-02", EffectStat::MarketRiskPremium), 0.0,
	    1e-9);

	// The respec road: taking one skill off takes every source of it off every store it
	// touched, and the store goes back to what the rest of the tree bought. The convoy
	// law is law in every world of the civilisation, so it has a source at the lane
	// store of each of the two civilisations, and the road takes both of them and says
	// 2 - the count is the sources, and a law that lands in two worlds is two sources.
	CHECK_EQ(engine.UnapplySkill(world, "SkillPiracyConvoy", errorMessage), 2u);
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    -9.0, 1e-9);
	CHECK_NEAR(
	    engine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-02"),
	        EffectStat::RouteRiskPremium),
	    0.0, 1e-9);
	CHECK_EQ(engine.UnapplySkill(world, "SkillNope", errorMessage), 0u);

	// The store a civilisation owns: the read-out names the stores, the effects and the
	// tiers, in words, in a fixed order, and prints no enum spelling.
	const std::string described = engine.DescribeCivilisation(world, "Civ-01");

	CHECK(!described.empty());
	CHECK_CONTAINS(described, "Civ-01");
	CHECK_CONTAINS(described, "Drydock");
	CHECK_CONTAINS(described, std::string(StatName(EffectStat::YardDockThroughput)));
	CHECK_CONTAINS(described, std::string(StatName(EffectStat::MarketClearance)));
	CHECK(described.find("EffectStat") == std::string::npos);
	CHECK(described.find("EffectDomain") == std::string::npos);
	CHECK(engine.DescribeCivilisation(world, "Civ-09").empty());

	// The engine is the only thing that writes the stores: an effect cannot land on a
	// venue that was never opened, and a civilisation with no stores of that domain is
	// refused with a message that says so.
	EcsWorld barren;
	std::string bar;

	CHECK(!engine.Apply(
	    barren, "Civ-09", "SkillLawDockingLicence", "DockingLicence", EffectOrigin::Skill, 1, bar));
	CHECK(!bar.empty());
	CHECK(!engine.Apply(
	    barren, "Civ-09", "SkillHabitatDomes", "HabitatDomes", EffectOrigin::Skill, 1, bar));
	CHECK(!bar.empty());
	CHECK(!engine.Apply(world, "Civ-01", "SkillNope", "Nope", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!engine.Apply(
	    world, "Civ-01", "SkillHabitatDomes", "HabitatDomes", EffectOrigin::Skill, 0, errorMessage));
}

TEST_CASE(a_tree_walk_lands_every_effect_the_tree_buys)
{
	const std::vector<SkillNode> nodes = {
	    Node("Age02", 1, 1, "", NodeType::Tech),
	    Node("SkillShipShieldsFront", 5, 2, "ShieldFront"),
	    Node("SkillShipShipyardsDrydock", 3, 4, "Drydock"),
	    Node("SkillLawDockingLicence", 4, 1, "DockingLicence"),
	    Node("SkillTradeCargoInsurance", 4, 2, "CargoInsurance"),
	    Node("SkillPiracyConvoy", 3, 2, "Convoy"),
	    Node("SkillLawAdmiraltyWarrant", 4, 1, "AdmiraltyWarrant"),
	    Node("SkillHabitatPublicMedicine", 3, 1, "PublicMedicine"),
	    Node("SkillSurveyDeepScan", 3, 1, "DeepSurvey"),
	};

	ProgressionManager progression = Tree(nodes, 60);
	CivilisationEffectEngine civEngine;
	ShipEffectEngine shipEngine;
	EcsWorld world = OpenWorld("Civ-01", "Abigail Prime");
	std::string errorMessage;

	CHECK_EQ(progression.GetResources(), 60);
	CHECK_EQ(progression.GetCurrentTier("SkillShipShieldsFront"), 0);
	CHECK_EQ(progression.GetMaxTiers("SkillShipShieldsFront"), 5);
	CHECK_EQ(progression.NodeIds().size(), nodes.size());

	// A tree walked with no tier bought buys nothing anywhere: every store of the
	// civilisation reads the numbers of a world that has bought none of the tree. Nine
	// nodes walked, none of them bought, and the walk counts the nine: the two columns
	// name the nodes of the tree, the ones the walk wrote and the ones it walked past,
	// and they add to the tree on every walk, here and on a hull.
	unsigned applied = 99u;
	unsigned skipped = 99u;

	CHECK(civEngine.ApplyProgression(world, progression, "Civ-01", applied, skipped, errorMessage));
	CHECK_EQ(applied, 0u);
	CHECK_EQ(skipped, 9u);
	CHECK_EQ(applied + skipped, static_cast<unsigned>(nodes.size()));
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 0.0,
	    1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::MedicineCoverage), 0.0,
	    1e-9);
	CHECK_EQ(civEngine.MarketStatsOf(world, "Civ-01").size(), 0u);

	// The gate opens, the points go down by what the tier cost, and an unknown node, a
	// node at its last tier and a node that is not affordable all say so in words.
	CHECK_EQ(progression.TryUpgrade("Age02"), UpgradeResult::Success);
	CHECK_EQ(progression.GetResources(), 59);
	CHECK_EQ(progression.GetCurrentTier("Age02"), 1);
	CHECK_EQ(progression.TryUpgrade("Age02"), UpgradeResult::MaxTier);
	CHECK_EQ(progression.TryUpgrade("SkillNope"), UpgradeResult::UnknownNode);
	CHECK(progression.CanUpgrade("SkillShipShieldsFront"));
	CHECK(progression.UpgradeBlockReason("SkillShipShieldsFront").empty());

	// The tiers the test names: two of shields, one of the dock, three of the licence,
	// two of the policy, two of the convoy, four of the warrant, and none of the clinic
	// and the scan, which the walk brings when the tree is walked again.
	CHECK_EQ(progression.TryUpgrade("SkillShipShieldsFront"), UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillShipShieldsFront"), UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillShipShipyardsDrydock"), UpgradeResult::Success);

	for (unsigned tier = 0; tier < 3u; ++tier)
		CHECK_EQ(progression.TryUpgrade("SkillLawDockingLicence"), UpgradeResult::Success);
	for (unsigned tier = 0; tier < 2u; ++tier)
		CHECK_EQ(progression.TryUpgrade("SkillTradeCargoInsurance"), UpgradeResult::Success);
	for (unsigned tier = 0; tier < 2u; ++tier)
		CHECK_EQ(progression.TryUpgrade("SkillPiracyConvoy"), UpgradeResult::Success);
	for (unsigned tier = 0; tier < 4u; ++tier)
		CHECK_EQ(progression.TryUpgrade("SkillLawAdmiraltyWarrant"), UpgradeResult::Success);

	CHECK_EQ(progression.GetCurrentTier("SkillShipShieldsFront"), 2);
	CHECK_EQ(progression.GetCurrentTier("SkillLawDockingLicence"), 3);
	CHECK_EQ(progression.GetCurrentTier("SkillSurveyDeepScan"), 0);
	CHECK_EQ(progression.GetResources(), 60 - 1 - 4 - 4 - 3 - 4 - 4 - 4);

	// The civilisation: every node with an effect and a tier above 0 lands on the store
	// its domain names, and the nodes that ask for no civilisation effect - the gate,
	// which buys nothing, and the shield bank, which is the hull's - are the skipped,
	// and with them the clinic and the scan, of which the tree has bought no tier: the
	// tree has nine nodes, the venue bought five of them, and the four it did not buy
	// are the four the walk counts as walked past.
	applied = 0u;
	skipped = 0u;

	CHECK(civEngine.ApplyProgression(world, progression, "Civ-01", applied, skipped, errorMessage));
	CHECK_EQ(applied, 5u);
	CHECK_EQ(skipped, 4u);
	CHECK_EQ(applied + skipped, static_cast<unsigned>(nodes.size()));
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance),
	    3 * 20.0 + 4 * 5.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    3 * -5.0 + 2 * -10.0 + 4 * -15.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketInsurance),
	    2 * -40.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    2 * -12.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-01"),
	        EffectStat::YardDockThroughput),
	    1.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        world, EffectDomain::Survey, CivilisationEffectEngine::SurveyStoreId("Civ-01"),
	        EffectStat::SurveyRange),
	    0.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::MedicineCoverage), 0.0,
	    1e-9);

	// The hull: the same tree on a hull buys the systems and skips the laws, which is
	// the split the two engines exist to make. The shield bank is the one system this
	// tree buys, and the eight nodes that buy a law, a lane, a yard, a survey, a clinic
	// or nothing are the ones the hull skips.
	CHECK(shipEngine.BuildShip(
	    world, "Trader", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));
	applied = 0u;
	skipped = 0u;

	CHECK(shipEngine.ApplyProgression(world, progression, "Trader", applied, skipped, errorMessage));
	CHECK_EQ(applied, 1u);
	CHECK_EQ(skipped, 8u);
	CHECK_NEAR(shipEngine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0 + 2 * 60.0, 1e-9);
	CHECK_EQ(shipEngine.FittedTier(world, "Trader", "ShieldFront"), 2);
	CHECK_EQ(shipEngine.FittedEffects(world, "Trader").size(), 1u);

	applied = 0u;
	skipped = 0u;

	// A technology reaches every hull of the operator at once, which is why a
	// civilisation that has bought a technique flies a fleet of it.
	CHECK(shipEngine.BuildShip(
	    world, "Trader-2", ShipEffectEngine::LongHaulTrader(), "Civ-01", errorMessage));
	CHECK(shipEngine.ApplyProgressionToWorld(
	    world, progression, "Civ-01", applied, skipped, errorMessage));
	CHECK_EQ(applied, 2u);
	CHECK_EQ(skipped, 16u);
	CHECK_NEAR(shipEngine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_NEAR(shipEngine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_EQ(shipEngine.ShipsOf(world, "Civ-01").size(), 2u);

	// A tree walked twice is the same store: the sources are keyed by the effect they
	// bought, so a second walk replaces what the first bought and adds nothing, which is
	// what a save-load, a UI and a test all need.
	const double clearance =
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance);

	applied = 0u;
	skipped = 0u;

	CHECK(civEngine.ApplyProgression(world, progression, "Civ-01", applied, skipped, errorMessage));
	CHECK_EQ(applied, 5u);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance),
	    clearance, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    3 * -5.0 + 2 * -10.0 + 4 * -15.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    2 * -12.0, 1e-9);

	applied = 0u;
	skipped = 0u;

	CHECK(shipEngine.ApplyProgression(world, progression, "Trader", applied, skipped, errorMessage));
	CHECK_NEAR(shipEngine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_EQ(shipEngine.FittedTier(world, "Trader", "ShieldFront"), 2);

	// A tier bought after the walk is the tier the next walk brings: the same stores,
	// and the nodes that were at tier 0 buy their first tier now.
	CHECK_EQ(progression.TryUpgrade("SkillSurveyDeepScan"), UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillHabitatPublicMedicine"), UpgradeResult::Success);
	applied = 0u;
	skipped = 0u;

	CHECK(civEngine.ApplyProgression(world, progression, "Civ-01", applied, skipped, errorMessage));
	CHECK_EQ(applied, 7u);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        world, EffectDomain::Survey, CivilisationEffectEngine::SurveyStoreId("Civ-01"),
	        EffectStat::SurveyRange),
	    12.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Body, "Civ-01", EffectStat::MedicineCoverage), 0.25,
	    1e-9);

	// The respec road reaches the tree too: taking a skill off takes its source off
	// every store it touched, the store goes back to what the rest of the tree bought,
	// and taking it off again takes nothing.
	CHECK_EQ(civEngine.UnapplySkill(world, "SkillLawAdmiraltyWarrant", errorMessage), 1u);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance),
	    3 * 20.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    3 * -5.0 + 2 * -10.0, 1e-9);
	CHECK_EQ(civEngine.UnapplySkill(world, "SkillLawAdmiraltyWarrant", errorMessage), 0u);
	CHECK_EQ(civEngine.UnapplySkill(world, "SkillNope", errorMessage), 0u);
	CHECK_EQ(shipEngine.UnfitSkill(world, "SkillShipShieldsFront", errorMessage), 2u);
	CHECK_NEAR(shipEngine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_NEAR(shipEngine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 120.0, 1e-9);
}

TEST_CASE(an_effect_applies_the_way_its_operator_says)
{
	// What a tier buys, and how a stat starts when nothing has written it: the three
	// operators, the tier count, and the empty table.
	std::map<EffectStat, double> stats;

	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 0.0, 1e-9);
	CHECK(stats.find(EffectStat::CargoVolume) == stats.end());

	ApplyEffectModifiers(stats, {EffectStat::CargoVolume, EffectOperator::Add, 300.0}, 3);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 900.0, 1e-9);
	ApplyEffectModifiers(stats, {EffectStat::CargoVolume, EffectOperator::Add, 300.0}, 1);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 1200.0, 1e-9);

	// A multiplier is a share of what the stat already is: a bay of 1,200 that grows by
	// half the bay twice more is 2,400, and a stat the table has never seen starts at
	// 0.0, so a share of nothing is nothing, which is what a habitat dome built on a
	// world with no capacity is worth.
	ApplyEffectModifiers(stats, {EffectStat::CargoVolume, EffectOperator::Multiply, 0.5}, 2);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 2400.0, 1e-9);
	ApplyEffectModifiers(stats, {EffectStat::CrewTraining, EffectOperator::Multiply, 0.25}, 2);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CrewTraining), 0.0, 1e-9);

	// A set caps: whatever the tier count, the stat is the number, which is what a stat
	// that caps means, and the set leaves the stat for an Add after it to move.
	ApplyEffectModifiers(stats, {EffectStat::CargoVolume, EffectOperator::Set, 100.0}, 4);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 100.0, 1e-9);
	ApplyEffectModifiers(stats, {EffectStat::CargoVolume, EffectOperator::Set, 55.0}, 1);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 55.0, 1e-9);
	ApplyEffectModifiers(stats, {EffectStat::CargoVolume, EffectOperator::Add, -5.0}, 1);
	CHECK_NEAR(ReadEffectStat(stats, EffectStat::CargoVolume), 50.0, 1e-9);
	CHECK_EQ(stats.size(), 2u);

	// The same modifiers on the same table reach the same numbers in the same order:
	// the determinism the whole layer is built on, and the read-out of the two tables
	// says the same words.
	std::map<EffectStat, double> mirror;

	ApplyEffectModifiers(mirror, {EffectStat::CargoVolume, EffectOperator::Add, 300.0}, 3);
	ApplyEffectModifiers(mirror, {EffectStat::CargoVolume, EffectOperator::Add, 300.0}, 1);
	ApplyEffectModifiers(mirror, {EffectStat::CargoVolume, EffectOperator::Multiply, 0.5}, 2);
	ApplyEffectModifiers(mirror, {EffectStat::CargoVolume, EffectOperator::Set, 100.0}, 4);
	ApplyEffectModifiers(mirror, {EffectStat::CargoVolume, EffectOperator::Set, 55.0}, 1);
	ApplyEffectModifiers(mirror, {EffectStat::CargoVolume, EffectOperator::Add, -5.0}, 1);
	ApplyEffectModifiers(mirror, {EffectStat::CrewTraining, EffectOperator::Multiply, 0.25}, 2);
	CHECK(stats == mirror);
	CHECK(DescribeEffectStats(stats) == DescribeEffectStats(mirror));

	// The read-out: the stats the table carries, in the order of the enum, with the
	// name, the value and the unit of each, and the stats it does not carry left out.
	const std::string described = DescribeEffectStats(stats);

	CHECK_CONTAINS(described, "Cargo Volume");
	CHECK_CONTAINS(described, "m3");
	CHECK_CONTAINS(described, "Crew Training");
	CHECK(described.find("Shield Front") == std::string::npos);
	CHECK(DescribeEffectStats(std::map<EffectStat, double>{}).empty());

	// The engine applies the operators it is given, on a hull with a slot for each of
	// them: an Add is a step a tier at a time, a Multiply is a share of the hull's own
	// base, and a Set is the number whatever the tiers.
	EffectCatalogue catalogue;
	std::string errorMessage;

	CHECK(catalogue.Build(
	    {Effect("ShieldFront", EffectDomain::Ship, EffectOperator::Add, EffectStat::ShieldFront, 60.0),
	     Effect(
	         "ShieldSide", EffectDomain::Ship, EffectOperator::Multiply, EffectStat::ShieldSide, 0.5),
	     Effect("ShieldRear", EffectDomain::Ship, EffectOperator::Set, EffectStat::ShieldRear, 1200.0)},
	    errorMessage));

	ShipEffectEngine engine;
	EcsWorld world;

	// The hull of the case is the shipped long-haul trader with a slot added for the one
	// synthetic system of the catalogue of one below: the slot rule is the shipped rule,
	// a system needs a frame on the hull to sit in, and the case is about what a tier of
	// an operator buys, not about which hulls carry which frames. The 25 ship systems of
	// the tree are the trader's own slots, and the first block fits them by the names the
	// shipped catalogue gives them.
	ShipClassSpec trader = ShipEffectEngine::LongHaulTrader();

	trader.fitableEffects.push_back("ScreenShare");

	engine.SetCatalogue(catalogue);
	CHECK(engine.BuildShip(world, "Trader", trader, "Civ-01", errorMessage));
	CHECK(engine.Fit(
	    world, "Trader", "SkillShieldStep", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0 + 2 * 60.0, 1e-9);
	CHECK(engine.Fit(
	    world, "Trader", "SkillScreenShare", "ShieldSide", EffectOrigin::Skill, 2, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldSide), 80.0 * 2.0, 1e-9);
	CHECK(engine.Fit(
	    world, "Trader", "SkillScreenCap", "ShieldRear", EffectOrigin::Skill, 3, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldRear), 1200.0, 1e-9);
	CHECK(engine.UnfitEffect(world, "Trader", "ShieldRear", errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldRear), 60.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldSide), 160.0, 1e-9);

	// A catalogue of one effect, fitted at every tier the tree reaches: the operator and
	// the tier count in the numbers of one hull, tier by tier, and the numbers of a
	// second hull of the same class, which are the same numbers.
	EffectCatalogue single;

	CHECK(single.Build(
	    {Effect(
	        "ScreenShare", EffectDomain::Ship, EffectOperator::Multiply, EffectStat::ShieldFront, 0.5)},
	    errorMessage));
	engine.SetCatalogue(single);
	CHECK(engine.BuildShip(world, "Frigate", trader, "Civ-01", errorMessage));

	for (int tier = 1; tier <= 4; ++tier)
	{
		CHECK(engine.Fit(
		    world, "Frigate", "SkillScreenShare", "ScreenShare", EffectOrigin::Skill, tier,
		    errorMessage));
		CHECK_NEAR(
		    engine.GetStat(world, "Frigate", EffectStat::ShieldFront), 120.0 * (1.0 + 0.5 * tier),
		    1e-9);
		CHECK(engine.Fit(
		    world, "Trader", "SkillScreenShare", "ScreenShare", EffectOrigin::Skill, tier,
		    errorMessage));
		CHECK_NEAR(
		    engine.GetStat(world, "Trader", EffectStat::ShieldFront),
		    engine.GetStat(world, "Frigate", EffectStat::ShieldFront), 1e-9);
	}

	// The catalogue is the source of what exists: an effect it does not define is not a
	// system, not knowledge, and not a stat, and the fit says so in a message.
	CHECK(!engine.Fit(
	    world, "Frigate", "SkillShieldStep", "ShieldFront", EffectOrigin::Skill, 1, errorMessage));
	CHECK(!errorMessage.empty());
	CHECK(!engine.Fit(
	    world, "Frigate", "SkillShieldStep", "ShieldFront", EffectOrigin::Technology, 1, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Frigate", EffectStat::ShieldFront), 360.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 360.0, 1e-9);

	// The civilisation engine applies the same operators on the store its domain names:
	// a habitat is a share of the people a world holds, a set is the state a world has
	// reached, a clinic is a step of coverage, and a stat the store has never seen
	// reads 0.0.
	EffectCatalogue civCatalogue;

	CHECK(civCatalogue.Register(
	    Effect(
	        "HabitatShare", EffectDomain::Body, EffectOperator::Multiply,
	        EffectStat::BodyHabitatCapacity, 0.25),
	    errorMessage));
	CHECK(civCatalogue.Register(
	    Effect(
	        "AirShare", EffectDomain::Body, EffectOperator::Set, EffectStat::TerraformAtmosphere, 0.75),
	    errorMessage));
	CHECK(civCatalogue.Register(
	    Effect(
	        "CoverageStep", EffectDomain::Body, EffectOperator::Add, EffectStat::MedicineCoverage,
	        0.25),
	    errorMessage));

	CivilisationEffectEngine civEngine;
	EcsWorld civWorld = OpenWorld("Civ-01", "Abigail Prime");

	civEngine.SetCatalogue(civCatalogue);
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillHabitatDomes", "HabitatShare", EffectOrigin::Skill, 4, errorMessage));
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillTerraformAir", "AirShare", EffectOrigin::Skill, 4, errorMessage));
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillHabitatClinics", "CoverageStep", EffectOrigin::Skill, 2,
	    errorMessage));
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Body, "Civ-01", EffectStat::TerraformAtmosphere),
	    0.75, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Body, "Civ-01", EffectStat::MedicineCoverage), 0.5,
	    1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Body, "Civ-01", EffectStat::BodyHabitatCapacity),
	    0.0, 1e-9);
	CHECK_EQ(civEngine.BodyStatsOf(civWorld, "Civ-01").size(), 3u);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 0.0,
	    1e-9);
	CHECK_EQ(civEngine.MarketsOf(civWorld, "Civ-01").size(), 1u);
	CHECK_EQ(civEngine.BodyStatsOf(civWorld, "Station-Civ-01").size(), 0u);
	CHECK(
	    civEngine.DescribeCivilisation(civWorld, "Civ-01").find("Terraform Atmosphere") !=
	    std::string::npos);
}

TEST_CASE(a_skill_bought_twice_costs_once)
{
	ShipEffectEngine engine;
	EcsWorld world;
	std::string errorMessage;

	const ShipClassSpec trader = ShipEffectEngine::LongHaulTrader();

	CHECK(engine.BuildShip(world, "Trader", trader, "Civ-01", errorMessage));
	CHECK(engine.BuildShip(world, "Trader-2", trader, "Civ-01", errorMessage));

	// The same skill on the same hull at the same tier, five times: the hull carries the
	// system once, the source on the hull is one, and the numbers of the hull do not
	// move. A tree walked twice, a save loaded, a UI opened again: none of them buys a
	// shield bank twice.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));

	for (unsigned count = 0; count < 4u; ++count)
		CHECK(engine.Fit(
		    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2,
		    errorMessage));

	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK_EQ(engine.FittedEffects(world, "Trader").size(), 1u);
	CHECK_EQ(engine.FittedTier(world, "Trader", "ShieldFront"), 2);
	CHECK_NEAR(engine.StatsOf(world, "Trader").at(EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_EQ(world.Get<ShipSystems>(world.Find("Trader"))->fitted.size(), 1u);

	// A second skill that buys the same system on the same hull replaces the tier the
	// hull flies: the hull has one forward screen, keyed by the effect it is, and the
	// tier it flies is the tier last fitted, which is what a refit dock does when it
	// fits the bank the hull already has.
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsAuxiliary", "ShieldFront", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK_EQ(engine.FittedTier(world, "Trader", "ShieldFront"), 1);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK_EQ(engine.FittedEffects(world, "Trader").size(), 1u);

	for (unsigned count = 0; count < 3u; ++count)
		CHECK(engine.Fit(
		    world, "Trader", "SkillShipShieldReserve", "ShieldFront", EffectOrigin::Skill, 1,
		    errorMessage));

	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK_EQ(engine.SourcesOn(world, "Trader")[0].skillId, std::string("SkillShipShieldReserve"));

	// Two hulls of one civilisation: a system is the hull's, and a second hull pays for
	// its own, which is what keeps a fleet of three traders from being one trader
	// flying three screens, and keeps the second hull's screen at its own tier.
	CHECK(engine.Fit(
	    world, "Trader-2", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK(engine.Fit(
	    world, "Trader-2", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK(engine.Fit(
	    world, "Trader", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 4, errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 360.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 180.0, 1e-9);
	CHECK_EQ(engine.UnfitSkill(world, "SkillShipShieldsFront", errorMessage), 2u);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader-2", EffectStat::ShieldFront), 120.0, 1e-9);
	CHECK(engine.StatsOf(world, "Trader") == engine.BaseStatsOf(world, "Trader"));
	CHECK(engine.StatsOf(world, "Trader") == engine.StatsOf(world, "Trader-2"));

	// One store of one civilisation, two effects of one stat: the sum of the two, and
	// the sum of the two applied again. The venue's fee is the sum of the laws in
	// force, not the number of times they were passed, and a law passed by one skill
	// and applied twice is the law once.
	CivilisationEffectEngine civEngine;
	EcsWorld civWorld = OpenWorld("Civ-01", "Abigail Prime");

	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4,
	    errorMessage));
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    -60.0, 1e-9);
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillPiracyPiratePardon", "PiratePardon", EffectOrigin::Skill, 3,
	    errorMessage));
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    -60.0 - 36.0, 1e-9);

	for (unsigned count = 0; count < 3u; ++count)
	{
		CHECK(civEngine.Apply(
		    civWorld, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4,
		    errorMessage));
		CHECK(civEngine.Apply(
		    civWorld, "Civ-01", "SkillPiracyPiratePardon", "PiratePardon", EffectOrigin::Skill, 3,
		    errorMessage));
	}

	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    -96.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 20.0,
	    1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-02", EffectStat::MarketRiskPremium),
	    0.0, 1e-9);
	CHECK_EQ(civEngine.MarketsOf(civWorld, "Civ-01").size(), 1u);
	CHECK_EQ(civEngine.MarketsOf(civWorld, "Civ-02").size(), 0u);
	CHECK_EQ(civEngine.BodyStatsOf(civWorld, "Civ-01").size(), 0u);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        civWorld, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    0.0, 1e-9);

	// A lane is one shared store, and three effects of one stat on it, each applied
	// twice by its own skill, are the safety of the lane: once each, and the sum of the
	// three.
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 2, errorMessage));
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillPiracyEscortWing", "EscortWing", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK(civEngine.Apply(
	    civWorld, "Civ-01", "SkillSurveyRouteWard", "RouteWard", EffectOrigin::Skill, 1, errorMessage));

	for (unsigned count = 0; count < 2u; ++count)
	{
		CHECK(civEngine.Apply(
		    civWorld, "Civ-01", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 2, errorMessage));
		CHECK(civEngine.Apply(
		    civWorld, "Civ-01", "SkillPiracyEscortWing", "EscortWing", EffectOrigin::Skill, 1,
		    errorMessage));
		CHECK(civEngine.Apply(
		    civWorld, "Civ-01", "SkillSurveyRouteWard", "RouteWard", EffectOrigin::Skill, 1,
		    errorMessage));
	}

	CHECK_NEAR(
	    civEngine.ReadStat(
	        civWorld, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    2 * -12.0 - 9.0 - 7.0, 1e-9);
	CHECK_EQ(civEngine.UnapplySkill(civWorld, "SkillPiracyConvoy", errorMessage), 1u);
	CHECK_NEAR(
	    civEngine.ReadStat(
	        civWorld, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    -9.0 - 7.0, 1e-9);
	CHECK_EQ(civEngine.UnapplySkill(civWorld, "SkillPiracyConvoy", errorMessage), 0u);

	// A whole tree walked twice: the same tiers, the same stores, the same numbers, in
	// the same order, on both engines, because the store is the sum of the sources the
	// tree bought and not the number of times the tree was walked.
	const std::vector<SkillNode> nodes = {
	    Node("SkillShipShieldsFront", 3, 1, "ShieldFront"),
	    Node("SkillShipGunFronts", 2, 1, "GunFront"),
	    Node("SkillLawDockingLicence", 3, 1, "DockingLicence"),
	};

	ProgressionManager progression = Tree(nodes, 30);

	CHECK_EQ(progression.TryUpgrade("SkillShipShieldsFront"), UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillShipShieldsFront"), UpgradeResult::Success);
	CHECK_EQ(progression.TryUpgrade("SkillLawDockingLicence"), UpgradeResult::Success);

	unsigned firstApplied = 0u;
	unsigned firstSkipped = 0u;
	unsigned secondApplied = 0u;
	unsigned secondSkipped = 0u;

	CHECK(civEngine.ApplyProgression(
	    civWorld, progression, "Civ-01", firstApplied, firstSkipped, errorMessage));
	CHECK(civEngine.ApplyProgression(
	    civWorld, progression, "Civ-01", secondApplied, secondSkipped, errorMessage));
	CHECK_EQ(firstApplied, secondApplied);
	CHECK_EQ(firstSkipped, secondSkipped);
	CHECK_EQ(firstApplied, 1u);
	CHECK_EQ(firstSkipped, 2u);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance),
	    20.0 + 20.0, 1e-9);
	CHECK_NEAR(
	    civEngine.ReadStat(civWorld, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    -96.0 - 5.0, 1e-9);

	CHECK(engine.ApplyProgression(
	    world, progression, "Trader", firstApplied, firstSkipped, errorMessage));
	const double screen = engine.GetStat(world, "Trader", EffectStat::ShieldFront);

	CHECK(engine.ApplyProgression(
	    world, progression, "Trader", secondApplied, secondSkipped, errorMessage));
	CHECK_EQ(firstApplied, secondApplied);
	CHECK_EQ(firstSkipped, secondSkipped);
	CHECK_EQ(firstApplied, 1u);
	CHECK_EQ(firstSkipped, 2u);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), screen, 1e-9);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK_EQ(engine.SourcesOn(world, "Trader").size(), 1u);
	CHECK_EQ(engine.SourcesOn(world, "Trader")[0].tier, 2);
	CHECK_EQ(engine.SourcesOn(world, "Trader")[0].skillId, std::string("SkillShipShieldsFront"));
	CHECK(engine.RecomputeAll(world, errorMessage) >= 2u);
	CHECK(engine.Recompute(world, "Trader", errorMessage));
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::ShieldFront), 240.0, 1e-9);
	CHECK(engine.FittedTier(world, "Trader", "GunFront") == 0);
	CHECK_NEAR(engine.GetStat(world, "Trader", EffectStat::GunFront), 24.0, 1e-9);
}

int main()
{
	return civtest::Run("EffectTests");
}
