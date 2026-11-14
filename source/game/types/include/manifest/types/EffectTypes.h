#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>


namespace Manifest::Core
{
// What a tier of a skill or a technology is worth.
//
// The skill tree carries a `targetEffectID` on every node that buys something the
// engine drives: "ShieldFront", "DockingLicence", "Drydock", "DeepSurvey". The age
// gates carry null, because a gate opens a set of goods and drives no effect of its
// own - `data/skill-tree/ages.yaml` says so in its own header comment. The effect id
// is the join between the tree and the engine, and this is the layer that says what a
// tier of an effect does and which store it lands on.
//
// An effect is a list of modifiers, one per stat. A modifier is signed and costs a
// tier at a time: +40 MW of forward screen per tier, -12 bps of route risk per tier.
// Buying a tier therefore buys a step of the same thing, which is what the data means
// by `maxTiers: 3` on a shield bank: thickness, not a new slot.
//
// The stats are an enum, not strings, so the compiler checks the join between an
// effect and the store it lands on: an effect in the Market domain cannot carry a
// shield stat, and `EffectCatalogue::Register` refuses the mix.

// Where an effect comes from, which decides how it reaches a ship.
enum class EffectOrigin
{
	// A system the ship carries: the hull has to have a slot for it, the tier fitted
	// is the tier bought, and the system can be refitted out at a yard.
	Skill,

	// A technique the civilisation knows: no hull refuses it, every ship of the
	// civilisation gains it, and no slot is spent for it.
	Technology
};

const char *DescribeEffectOrigin(EffectOrigin origin);

// Which store the effect lands on.
enum class EffectDomain
{
	Ship,   // a ship entity in the ECS world
	Yard,   // a yard: the docks that build, fit and refit hulls
	Route,  // a lane between worlds, and the risk of sailing it
	Survey, // the survey and the claims it opens
	Market, // a marketplace at a planet or a satellite
	Body    // a planet, or a satellite in orbit around one
};

const char *DescribeEffectDomain(EffectDomain domain);

// How a modifier moves a stat.
enum class EffectOperator
{
	Add,      // value += perTier x tiers
	Multiply, // value *= 1 + perTier x tiers
	Set       // value = perTier, whatever the tier count, for a stat that caps
};

const char *DescribeEffectOperator(EffectOperator Operator);

// Every stat an effect can move. The order of the enumerators is the display order:
// the ship read-out prints them in this order, so a stat added in the middle moves
// the read-out and a stat appended at the end does not.
enum class EffectStat
{
	// The 25 systems of the long-haul trader, in the order data/skill-tree/spaceship.yaml
	// lays them out: screens by arc, guns and lasers by arc, the drive and the jets, the
	// mast, the two computers, the holds and the bunkers, the rig, the crew and the coating.
	ShieldFront,
	ShieldSide,
	ShieldRear,
	GunFront,
	GunRear,
	LaserFront,
	LaserRear,
	DriveThrust,
	ManoeuvreJets,
	AttitudeControl,
	SensorLongRange,
	SensorFireControl,
	SensorCommercial,
	NavigationComputer,
	TraderComputer,
	CargoVolume,
	FuelCapacity,
	SalvageCapacity,
	LifeSupportDays,
	StealthRating,
	EmissionsControl,
	Countermeasures,
	CrewTraining,
	DamageControl,
	SparesCapacity,

	// The yards: what they build, fit and refit, and the hulls they can put together.
	YardDockThroughput,
	YardFitSlots,
	YardModularHullSlots,
	YardRefitSlots,

	// The lanes. One stat, because the four lane effects are one number seen four ways:
	// the convoy, the escort, the ward and the patrol all make sailing the lane safer.
	RouteRiskPremium,

	// The survey and the claims.
	SurveyRange,
	ClaimCount,
	RefineryThroughput,
	ClaimExclusivity,

	// The marketplace: the risk of trading at a venue, the berths it clears, the premium
	// a cargo carries, the goods it can absorb, the gap it charges between a buyer and a
	// seller, and the share of a cargo a fence will take off book.
	MarketRiskPremium,
	MarketClearance,
	MarketInsurance,
	MarketDepth,
	MarketSpread,
	MarketFenceShare,

	// The worlds and the satellites: how many people a habitat holds, how much of what
	// they breathe and eat a closed loop recovers, how far terraforming has come, and how
	// much of the population medicine and schooling reach.
	BodyHabitatCapacity,
	BodyRecyclingLoop,
	TerraformAtmosphere,
	TerraformBiosphere,
	MedicineCoverage,
	EducationCoverage
};

// Allows a stat keyed map: std::map<EffectStat, double> is the component the effects
// write, and the map is the deterministic iteration the UI and the tests depend on.
inline bool operator<(EffectStat left, EffectStat right)
{
	return static_cast<int>(left) < static_cast<int>(right);
}

// Display name of a stat, for the UI: "Shield Front", "Docking Clearance".
const char *StatName(EffectStat stat);

// Unit a stat counts in: "MW", "bps", "m3", "crew-days".
const char *StatUnit(EffectStat stat);

// Which store a stat belongs to.
EffectDomain StatDomain(EffectStat stat);

// Every stat, in the declaration order above.
const std::vector<EffectStat> &AllEffectStats();

// One modifier of one stat of an effect.
struct EffectModifier
{
	EffectStat stat{EffectStat::ShieldFront};
	EffectOperator op{EffectOperator::Add};

	// What one tier is worth, in the unit of the stat. Signed, so an effect can
	// lower what another raises: SafeHarbours lowers the risk of a venue,
	// RaiderHulls raises it, and both land on MarketRiskPremium.
	double perTier{0.0};
};

// Applies every modifier of an effect, for a tier count, to a stat table. A stat that
// is not in the table starts at 0.0, so an effect lands on a store that has never
// seen it.
void ApplyEffectModifiers(
    std::map<EffectStat, double> &stats, const EffectModifier &modifier, int tiers);

// Reads a stat out of a table: 0.0 when the table has never seen it.
double ReadEffectStat(const std::map<EffectStat, double> &stats, EffectStat stat);

// A stat table as one line for the UI, "Shield Front 200 MW, Cargo Volume 1,500 m3",
// in the order of AllEffectStats, skipping the stats the table does not carry.
std::string DescribeEffectStats(const std::map<EffectStat, double> &stats);

std::string DescribeEffectModifier(const EffectModifier &modifier);

// Number formatting for a stat: three significant digits, no trailing zeros.
std::string FormatEffectStatValue(double value);
} // namespace Manifest::Core
