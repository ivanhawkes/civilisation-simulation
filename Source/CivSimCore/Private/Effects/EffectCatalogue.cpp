#include "Effects/EffectCatalogue.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <utility>


namespace CivSimCore
{
namespace
{
EffectDefinition MakeEffect(
    std::string EffectId, EffectDomain Domain, std::string DisplayName, std::string Description,
    std::vector<EffectModifier> Modifiers)
{
	EffectDefinition definition;
	definition.effectId = std::move(EffectId);
	definition.domain = Domain;
	definition.displayName = std::move(DisplayName);
	definition.description = std::move(Description);
	definition.modifiers = std::move(Modifiers);

	return definition;
}

// A modifier that adds perTier of the stat for every tier bought. The numbers below
// are the scaffold's working figures, chosen so that three tiers of a system move the
// stat by a half to a third of its base value: a shield bank at tier 3 is a screen
// worth half again, not a new ship.
EffectModifier Add(EffectStat Stat, double PerTier)
{
	EffectModifier modifier;
	modifier.stat = Stat;
	modifier.op = EffectOperator::Add;
	modifier.perTier = PerTier;

	return modifier;
}

EffectModifier Multiply(EffectStat Stat, double PerTier)
{
	EffectModifier modifier;
	modifier.stat = Stat;
	modifier.op = EffectOperator::Multiply;
	modifier.perTier = PerTier;

	return modifier;
}

// The effects every node in the shipped tree asks for, by domain. The ids are the
// `targetEffectID` strings of data/skill-tree, spelled there and here, and
// EffectsMissingFrom is the check that the two spellings agree.
std::vector<EffectDefinition> ShipEffectDefinitions()
{
	const EffectDomain ship = EffectDomain::Ship;

	return {
	    MakeEffect(
	        "ShieldFront", ship, "Forward Shield Bank",
	        "The screen that faces the pirate you are running down", {Add(EffectStat::ShieldFront, 60.0)}),
	    MakeEffect(
	        "ShieldSide", ship, "Beam Shield Bank",
	        "The screen that covers the flanks when a raider circles you", {Add(EffectStat::ShieldSide, 45.0)}),
	    MakeEffect(
	        "ShieldRear", ship, "Aft Shield Bank",
	        "The screen that covers the drive when you run for it", {Add(EffectStat::ShieldRear, 35.0)}),
	    MakeEffect(
	        "GunFront", ship, "Forward Gun Battery", "The guns that reach what is ahead of you",
	        {Add(EffectStat::GunFront, 12.0)}),
	    MakeEffect(
	        "GunRear", ship, "Aft Gun Battery", "The guns that reach what is running you down",
	        {Add(EffectStat::GunRear, 8.0)}),
	    MakeEffect(
	        "LaserFront", ship, "Forward Laser Mount", "The range the guns do not reach, ahead",
	        {Add(EffectStat::LaserFront, 6.0)}),
	    MakeEffect(
	        "LaserRear", ship, "Aft Laser Mount", "The range the guns do not reach, astern",
	        {Add(EffectStat::LaserRear, 4.0)}),
	    MakeEffect(
	        "EngineThrust", ship, "Drive", "The thrust that makes a jump and outruns a raider",
	        {Add(EffectStat::DriveThrust, 140.0)}),
	    MakeEffect(
	        "ManoeuvringJets", ship, "Manoeuvre Jets", "The working in space: docking, station keeping, berthing",
	        {Add(EffectStat::ManoeuvreJets, 24.0)}),
	    MakeEffect(
	        "AttitudeControl", ship, "Attitude Thrusters", "Pointing the ship, and the guns with it",
	        {Add(EffectStat::AttitudeControl, 7.0)}),
	    MakeEffect(
	        "SensorLongRange", ship, "Long Range Mast", "The dish that sees the lane ahead",
	        {Add(EffectStat::SensorLongRange, 1500.0)}),
	    MakeEffect(
	        "SensorFireControl", ship, "Fire Control Array", "The dish that sees the gun's target",
	        {Add(EffectStat::SensorFireControl, 300.0)}),
	    MakeEffect(
	        "SensorCommercial", ship, "Commercial Array", "The dish that reads a market from orbit",
	        {Add(EffectStat::SensorCommercial, 600.0)}),
	    MakeEffect(
	        "ComputerNavigation", ship, "Navigation Computer", "The jumps the drive can pay for",
	        {Add(EffectStat::NavigationComputer, 1.5)}),
	    MakeEffect(
	        "ComputerTrader", ship, "Trading AI", "The cargo calls that make the haul pay",
	        {Add(EffectStat::TraderComputer, 1.2)}),
	    MakeEffect(
	        "CargoHolds", ship, "Cargo Holds", "The size of a run", {Add(EffectStat::CargoVolume, 300.0)}),
	    MakeEffect(
	        "FuelBunkers", ship, "Fuel Bunkers", "The range of a run, and the fuel the drive burns",
	        {Add(EffectStat::FuelCapacity, 18.0)}),
	    MakeEffect(
	        "SalvageRig", ship, "Salvage Rig", "Going and bringing back the cargo of a hull that cannot",
	        {Add(EffectStat::SalvageCapacity, 10.0)}),
	    MakeEffect(
	        "LifeSupport", ship, "Life Support", "The days a crew can be out",
	        {Add(EffectStat::LifeSupportDays, 15.0)}),
	    MakeEffect(
	        "StealthCoating", ship, "Stealth Coating", "Whether anyone sees the ship doing it",
	        {Add(EffectStat::StealthRating, 0.15)}),
	    MakeEffect(
	        "EmissionsControl", ship, "Emissions Control", "The heat and the noise the ship gives off",
	        {Add(EffectStat::EmissionsControl, 0.12)}),
	    MakeEffect(
	        "Countermeasures", ship, "Countermeasures", "The answer to a lock, chaff, flare and burn",
	        {Add(EffectStat::Countermeasures, 0.12)}),
	    MakeEffect(
	        "CrewTraining", ship, "Crew Training", "Working a bigger hull with the same crew",
	        {Multiply(EffectStat::CrewTraining, 0.15)}),
	    MakeEffect(
	        "DamageControl", ship, "Damage Control", "Keeping a ship flying with a screen down",
	        {Multiply(EffectStat::DamageControl, 0.15)}),
	    MakeEffect(
	        "SparesLockers", ship, "Spares Lockers", "Doing it twice before the next yard",
	        {Multiply(EffectStat::SparesCapacity, 0.12)}),
	};
}


std::vector<EffectDefinition> YardEffectDefinitions()
{
	const EffectDomain yard = EffectDomain::Yard;

	return {
	    MakeEffect(
	        "Drydock", yard, "Drydock", "The cradle a hull is built and mended in",
	        {Add(EffectStat::YardDockThroughput, 1.0)}),
	    MakeEffect(
	        "YardOutfitting", yard, "Outfitting Line", "The systems a yard can fit at once",
	        {Add(EffectStat::YardFitSlots, 1.0)}),
	    MakeEffect(
	        "ModularHulls", yard, "Modular Hulls", "The hulls a yard can put together from sections",
	        {Add(EffectStat::YardModularHullSlots, 1.0)}),
	    MakeEffect(
	        "RefitDocks", yard, "Refit Docks", "The systems a yard can take off and swap at once",
	        {Add(EffectStat::YardRefitSlots, 1.0)}),
	};
}


std::vector<EffectDefinition> RouteEffectDefinitions()
{
	const EffectDomain route = EffectDomain::Route;

	// The four lane effects move one stat: the risk of sailing a lane. A convoy is
	// the cheapest way to buy it and buys the most, a warden patrol is the dearest
	// way and buys the least, and the numbers say so.
	return {
	    MakeEffect(
	        "Convoy", route, "Convoy", "Trading ships sailing together, guarded",
	        {Add(EffectStat::RouteRiskPremium, -12.0)}),
	    MakeEffect(
	        "EscortWing", route, "Escort Wing", "The warships that sail the lane with you",
	        {Add(EffectStat::RouteRiskPremium, -9.0)}),
	    MakeEffect(
	        "RouteWard", route, "Route Ward", "The claim that a lane is watched, and by whom",
	        {Add(EffectStat::RouteRiskPremium, -7.0)}),
	    MakeEffect(
	        "WardenPatrol", route, "Warden Patrol", "The patrol that answers a call on the lane",
	        {Add(EffectStat::RouteRiskPremium, -5.0)}),
	};
}


std::vector<EffectDefinition> SurveyEffectDefinitions()
{
	const EffectDomain survey = EffectDomain::Survey;

	return {
	    MakeEffect(
	        "DeepSurvey", survey, "Deep Survey", "How far a survey reaches from the world it launches from",
	        {Add(EffectStat::SurveyRange, 12.0)}),
	    MakeEffect(
	        "MiningClaim", survey, "Mining Claim", "The claim a world can work at one time",
	        {Add(EffectStat::ClaimCount, 2.0)}),
	    MakeEffect(
	        "ClaimRefinery", survey, "Claim Refinery", "The ore a claim can turn into a trade good",
	        {Add(EffectStat::RefineryThroughput, 150.0)}),
	    MakeEffect(
	        "ExclusiveClaim", survey, "Exclusive Claim", "The claim no one else may work",
	        {Add(EffectStat::ClaimExclusivity, 1.0)}),
	};
}


std::vector<EffectDefinition> MarketEffectDefinitions()
{
	const EffectDomain market = EffectDomain::Market;

	// Market effects move the numbers a marketplace prices by. Signed: the law, the
	// trade offices and the wardens lower the risk of trading at a venue, the pirates
	// raise it, and both land on MarketRiskPremium, so the venue's fee is the sum of
	// what a civilisation has bought on both sides of it.
	return {
	    MakeEffect(
	        "DockingLicence", market, "Docking Licence", "The permit, the berth and the clearance for a ship",
	        {Add(EffectStat::MarketClearance, 20.0), Add(EffectStat::MarketRiskPremium, -5.0)}),
	    MakeEffect(
	        "CargoInsurance", market, "Cargo Insurance", "The premium that covers a cargo",
	        {Add(EffectStat::MarketInsurance, -40.0), Add(EffectStat::MarketRiskPremium, -10.0)}),
	    MakeEffect(
	        "FencingNetwork", market, "Fencing Network", "The outlets that buy what is not the seller's to sell",
	        {Add(EffectStat::MarketFenceShare, 0.15), Add(EffectStat::MarketRiskPremium, 8.0)}),
	    MakeEffect(
	        "AdmiraltyWarrant", market, "Admiralty Warrant", "The commission that makes a ship lawful",
	        {Add(EffectStat::MarketClearance, 5.0), Add(EffectStat::MarketRiskPremium, -15.0)}),
	    MakeEffect(
	        "PiratePardon", market, "Pirate Pardon", "The pardon that buys a raider's standing back",
	        {Add(EffectStat::MarketRiskPremium, -12.0)}),
	    MakeEffect(
	        "PirateStanding", market, "Standing Among The Crews", "The name a ship has where the crews drink",
	        {Add(EffectStat::MarketRiskPremium, -6.0), Add(EffectStat::MarketFenceShare, 0.05)}),
	    MakeEffect(
	        "TradeTribunal", market, "Trade Tribunal", "The bench that decides what a cargo was worth",
	        {Add(EffectStat::MarketRiskPremium, -9.0), Add(EffectStat::MarketDepth, 40.0)}),
	    MakeEffect(
	        "SafeHarbours", market, "Safe Harbours", "The decks a ship can land cargo on",
	        {Add(EffectStat::MarketRiskPremium, -14.0), Add(EffectStat::MarketDepth, 25.0)}),
	    MakeEffect(
	        "RaiderHulls", market, "Raider Hulls", "The fast hulls that live off the trade",
	        {Add(EffectStat::MarketRiskPremium, 22.0)}),
	    MakeEffect(
	        "BoardingParties", market, "Boarding Parties", "The crews that take a hull and what is in it",
	        {Add(EffectStat::MarketRiskPremium, 18.0), Add(EffectStat::MarketFenceShare, 0.1)}),
	    MakeEffect(
	        "RansomExchange", market, "Ransom Exchange", "The market that buys a ship back from its captors",
	        {Add(EffectStat::MarketInsurance, 25.0), Add(EffectStat::MarketRiskPremium, 6.0)}),
	};
}


std::vector<EffectDefinition> BodyEffectDefinitions()
{
	const EffectDomain body = EffectDomain::Body;

	return {
	    MakeEffect(
	        "HabitatDomes", body, "Habitat Domes", "The people a world or a satellite can hold",
	        {Multiply(EffectStat::BodyHabitatCapacity, 0.25)}),
	    MakeEffect(
	        "ClosedLoop", body, "Closed Loop", "The air and the water a habitat recovers instead of buying",
	        {Add(EffectStat::BodyRecyclingLoop, 0.15)}),
	    MakeEffect(
	        "TerraformAtmosphere", body, "Terraform Atmosphere", "The air a world breathes on its own",
	        {Add(EffectStat::TerraformAtmosphere, 0.1)}),
	    MakeEffect(
	        "TerraformBiosphere", body, "Terraform Biosphere", "The ecology a world grows on its own",
	        {Add(EffectStat::TerraformBiosphere, 0.08)}),
	    MakeEffect(
	        "PublicMedicine", body, "Public Medicine", "The share of a population a clinic reaches",
	        {Add(EffectStat::MedicineCoverage, 0.25)}),
	    MakeEffect(
	        "Education", body, "Education", "The share of a population a school reaches",
	        {Add(EffectStat::EducationCoverage, 0.25)}),
	};
}
} // namespace

bool EffectCatalogue::Register(const EffectDefinition &Definition, std::string &ErrorMessage)
{
	if (Definition.effectId.empty())
	{
		ErrorMessage = "Effect: the effectId is empty; a targetEffectID never names an empty id";

		return false;
	}

	if (Definition.modifiers.empty())
	{
		ErrorMessage = "Effect '" + Definition.effectId + "': no modifiers; an effect that moves no stat does nothing";

		return false;
	}

	for (const auto &modifier : Definition.modifiers)
	{
		if (modifier.perTier == 0.0)
		{
			ErrorMessage = "Effect '" + Definition.effectId + "': a modifier of " + StatName(modifier.stat) +
			               " is zero per tier; a tier of an effect has to be worth something";

			return false;
		}

		// The mix that says the effect and the stat were written by two people who
		// never spoke: a Market effect moving a shield stat is a typo, not a design.
		if (StatDomain(modifier.stat) != Definition.domain)
		{
			ErrorMessage =
			    "Effect '" + Definition.effectId + "' is a " + DescribeEffectDomain(Definition.domain) +
			    " effect and moves " + StatName(modifier.stat) + ", a " +
			    DescribeEffectDomain(StatDomain(modifier.stat)) + " stat";

			return false;
		}
	}

	if (registry.find(Definition.effectId) != registry.end())
	{
		ErrorMessage = "Effect '" + Definition.effectId + "': the catalogue already defines it";

		return false;
	}

	registry[Definition.effectId] = Definition;

	return true;
}


bool EffectCatalogue::Build(const std::vector<EffectDefinition> &Definitions, std::string &ErrorMessage)
{
	registry.clear();

	for (const auto &definition : Definitions)
	{
		if (!Register(definition, ErrorMessage))
		{
			registry.clear();

			return false;
		}
	}

	return Validate(ErrorMessage);
}


void EffectCatalogue::Clear()
{
	registry.clear();
}


bool EffectCatalogue::Has(const std::string &EffectId) const
{
	return registry.find(EffectId) != registry.end();
}


const EffectDefinition *EffectCatalogue::Find(const std::string &EffectId) const
{
	const auto entry = registry.find(EffectId);

	return entry != registry.end() ? &entry->second : nullptr;
}


std::vector<std::string> EffectCatalogue::EffectIds() const
{
	std::vector<std::string> ids;
	ids.reserve(registry.size());

	for (const auto &entry : registry)
	{
		ids.push_back(entry.first);
	}

	return ids;
}


std::vector<std::string> EffectCatalogue::EffectIdsInDomain(EffectDomain Domain) const
{
	std::vector<std::string> ids;

	for (const auto &entry : registry)
	{
		if (entry.second.domain == Domain)
		{
			ids.push_back(entry.first);
		}
	}

	return ids;
}


std::size_t EffectCatalogue::EffectCount() const
{
	return registry.size();
}


bool EffectCatalogue::Validate(std::string &ErrorMessage) const
{
	for (const auto &entry : registry)
	{
		for (const auto &modifier : entry.second.modifiers)
		{
			if (StatDomain(modifier.stat) != entry.second.domain)
			{
				ErrorMessage = "Effect '" + entry.first + "': a " + DescribeEffectDomain(entry.second.domain) +
				               " effect moves " + StatName(modifier.stat) + ", a " +
				               DescribeEffectDomain(StatDomain(modifier.stat)) + " stat";

				return false;
			}
		}
	}

	return true;
}


std::vector<std::string> EffectCatalogue::EffectsMissingFrom(const Skills &Tree) const
{
	// The ids the tree asks for, in sorted order and without duplicates: two crafts
	// can name the same effect, the yards all sit on the same gate.
	std::set<std::string> asked;

	for (const auto &node : Tree)
	{
		if (!node.targetEffectID.empty())
		{
			asked.insert(node.targetEffectID);
		}
	}

	std::vector<std::string> missing;

	for (const auto &effectId : asked)
	{
		if (!Has(effectId))
		{
			missing.push_back(effectId);
		}
	}

	return missing;
}


std::vector<std::string> EffectCatalogue::EffectsUnusedBy(const Skills &Tree) const
{
	std::set<std::string> asked;

	for (const auto &node : Tree)
	{
		if (!node.targetEffectID.empty())
		{
			asked.insert(node.targetEffectID);
		}
	}

	std::vector<std::string> unused;

	for (const auto &entry : registry)
	{
		if (asked.find(entry.first) == asked.end())
		{
			unused.push_back(entry.first);
		}
	}

	return unused;
}


bool EffectCatalogue::CoversTree(const Skills &Tree, std::string &ErrorMessage) const
{
	const std::vector<std::string> missing = EffectsMissingFrom(Tree);

	if (missing.empty())
	{
		return true;
	}

	std::ostringstream out;
	out << "the catalogue defines no effect for " << missing.size() << " id(s) the tree asks for: ";

	for (std::size_t index = 0; index < missing.size(); ++index)
	{
		out << (index == 0 ? "" : ", ") << missing[index];
	}

	ErrorMessage = out.str();

	return false;
}


EffectCatalogue EffectCatalogue::Default()
{
	std::vector<EffectDefinition> definitions;

	for (const auto &group : {ShipEffects(), YardEffects(), RouteEffects(), SurveyEffects(), MarketEffects(),
	                          BodyEffects()})
	{
		definitions.insert(definitions.end(), group.begin(), group.end());
	}

	EffectCatalogue catalogue;
	std::string errorMessage;

	// The default catalogue is the shipped data's join, so it cannot be broken by a
	// caller: a duplicate id here is a bug in this file, and the Build message would
	// name it. The catalogue is built once per call and is cheap.
	if (!catalogue.Build(definitions, errorMessage))
	{
		return EffectCatalogue{};
	}

	return catalogue;
}


std::vector<EffectDefinition> EffectCatalogue::ShipEffects()
{
	return ShipEffectDefinitions();
}


std::vector<EffectDefinition> EffectCatalogue::YardEffects()
{
	return YardEffectDefinitions();
}


std::vector<EffectDefinition> EffectCatalogue::RouteEffects()
{
	return RouteEffectDefinitions();
}


std::vector<EffectDefinition> EffectCatalogue::SurveyEffects()
{
	return SurveyEffectDefinitions();
}


std::vector<EffectDefinition> EffectCatalogue::MarketEffects()
{
	return MarketEffectDefinitions();
}


std::vector<EffectDefinition> EffectCatalogue::BodyEffects()
{
	return BodyEffectDefinitions();
}
} // namespace CivSimCore
