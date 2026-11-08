#include "Effects/EffectTypes.h"
#include <array>
#include <sstream>
#include <string>


namespace CivSimCore
{
namespace
{
// The stat table: name, unit and domain, in the declaration order of EffectStat. The
// table is the single place a stat is named, so the UI, the catalogue validation and
// the effect read-out all agree on the spelling, and adding a stat is one row here
// plus one enumerator, not a search for every string that names it.
struct StatInfo
{
	EffectStat stat;
	const char *name;
	const char *unit;
	EffectDomain domain;
};

constexpr std::array<StatInfo, 46> statTable{
    StatInfo{EffectStat::ShieldFront, "Shield Front", "MW", EffectDomain::Ship},
    StatInfo{EffectStat::ShieldSide, "Shield Side", "MW", EffectDomain::Ship},
    StatInfo{EffectStat::ShieldRear, "Shield Rear", "MW", EffectDomain::Ship},
    StatInfo{EffectStat::GunFront, "Gun Front", "MJ", EffectDomain::Ship},
    StatInfo{EffectStat::GunRear, "Gun Rear", "MJ", EffectDomain::Ship},
    StatInfo{EffectStat::LaserFront, "Laser Front", "MJ", EffectDomain::Ship},
    StatInfo{EffectStat::LaserRear, "Laser Rear", "MJ", EffectDomain::Ship},
    StatInfo{EffectStat::DriveThrust, "Drive Thrust", "kN", EffectDomain::Ship},
    StatInfo{EffectStat::ManoeuvreJets, "Manoeuvre Jets", "kN", EffectDomain::Ship},
    StatInfo{EffectStat::AttitudeControl, "Attitude Control", "kN", EffectDomain::Ship},
    StatInfo{EffectStat::SensorLongRange, "Sensor Long Range", "km", EffectDomain::Ship},
    StatInfo{EffectStat::SensorFireControl, "Sensor Fire Control", "km", EffectDomain::Ship},
    StatInfo{EffectStat::SensorCommercial, "Sensor Commercial", "km", EffectDomain::Ship},
    StatInfo{EffectStat::NavigationComputer, "Navigation Computer", "GFLOP/s", EffectDomain::Ship},
    StatInfo{EffectStat::TraderComputer, "Trader Computer", "GFLOP/s", EffectDomain::Ship},
    StatInfo{EffectStat::CargoVolume, "Cargo Volume", "m3", EffectDomain::Ship},
    StatInfo{EffectStat::FuelCapacity, "Fuel Capacity", "t", EffectDomain::Ship},
    StatInfo{EffectStat::SalvageCapacity, "Salvage Capacity", "t", EffectDomain::Ship},
    StatInfo{EffectStat::LifeSupportDays, "Life Support", "crew-days", EffectDomain::Ship},
    StatInfo{EffectStat::StealthRating, "Stealth Coating", "index", EffectDomain::Ship},
    StatInfo{EffectStat::EmissionsControl, "Emissions Control", "index", EffectDomain::Ship},
    StatInfo{EffectStat::Countermeasures, "Countermeasures", "index", EffectDomain::Ship},
    StatInfo{EffectStat::CrewTraining, "Crew Training", "x", EffectDomain::Ship},
    StatInfo{EffectStat::DamageControl, "Damage Control", "x", EffectDomain::Ship},
    StatInfo{EffectStat::SparesCapacity, "Spares Lockers", "x", EffectDomain::Ship},

    StatInfo{EffectStat::YardDockThroughput, "Drydock Throughput", "hull/tick", EffectDomain::Yard},
    StatInfo{EffectStat::YardFitSlots, "Outfitting Slots", "slots", EffectDomain::Yard},
    StatInfo{EffectStat::YardModularHullSlots, "Modular Hull Slots", "slots", EffectDomain::Yard},
    StatInfo{EffectStat::YardRefitSlots, "Refit Slots", "slots", EffectDomain::Yard},

    StatInfo{EffectStat::RouteRiskPremium, "Route Risk", "bps", EffectDomain::Route},

    StatInfo{EffectStat::SurveyRange, "Deep Survey Range", "AU", EffectDomain::Survey},
    StatInfo{EffectStat::ClaimCount, "Mining Claims", "claims", EffectDomain::Survey},
    StatInfo{EffectStat::RefineryThroughput, "Claim Refinery", "t/day", EffectDomain::Survey},
    StatInfo{EffectStat::ClaimExclusivity, "Exclusive Claim", "claims", EffectDomain::Survey},

    StatInfo{EffectStat::MarketRiskPremium, "Market Risk", "bps", EffectDomain::Market},
    StatInfo{EffectStat::MarketClearance, "Docking Clearance", "berths/day", EffectDomain::Market},
    StatInfo{EffectStat::MarketInsurance, "Cargo Insurance", "bps", EffectDomain::Market},
    StatInfo{EffectStat::MarketDepth, "Market Depth", "units", EffectDomain::Market},
    StatInfo{EffectStat::MarketSpread, "Market Spread", "bps", EffectDomain::Market},
    StatInfo{EffectStat::MarketFenceShare, "Fence Reach", "share", EffectDomain::Market},

    StatInfo{EffectStat::BodyHabitatCapacity, "Habitat Capacity", "world units", EffectDomain::Body},
    StatInfo{EffectStat::BodyRecyclingLoop, "Closed Loop", "share", EffectDomain::Body},
    StatInfo{EffectStat::TerraformAtmosphere, "Terraform Atmosphere", "share", EffectDomain::Body},
    StatInfo{EffectStat::TerraformBiosphere, "Terraform Biosphere", "share", EffectDomain::Body},
    StatInfo{EffectStat::MedicineCoverage, "Public Medicine", "share", EffectDomain::Body},
    StatInfo{EffectStat::EducationCoverage, "Education", "share", EffectDomain::Body},
};

static_assert(statTable.size() == 46, "the stat table must cover every EffectStat enumerator");

const StatInfo *FindStatInfo(EffectStat Stat)
{
	for (const auto &info : statTable)
	{
		if (info.stat == Stat)
		{
			return &info;
		}
	}

	return nullptr;
}
} // namespace

const char *DescribeEffectOrigin(EffectOrigin Origin)
{
	switch (Origin)
	{
	case EffectOrigin::Skill:
		return "Skill";
	case EffectOrigin::Technology:
		return "Technology";
	}

	return "Unknown";
}


const char *DescribeEffectDomain(EffectDomain Domain)
{
	switch (Domain)
	{
	case EffectDomain::Ship:
		return "Ship";
	case EffectDomain::Yard:
		return "Yard";
	case EffectDomain::Route:
		return "Route";
	case EffectDomain::Survey:
		return "Survey";
	case EffectDomain::Market:
		return "Market";
	case EffectDomain::Body:
		return "Body";
	}

	return "Unknown";
}


const char *DescribeEffectOperator(EffectOperator Operator)
{
	switch (Operator)
	{
	case EffectOperator::Add:
		return "add";
	case EffectOperator::Multiply:
		return "multiply";
	case EffectOperator::Set:
		return "set";
	}

	return "unknown";
}


const char *StatName(EffectStat Stat)
{
	const StatInfo *info = FindStatInfo(Stat);

	return info != nullptr ? info->name : "Unknown Stat";
}


const char *StatUnit(EffectStat Stat)
{
	const StatInfo *info = FindStatInfo(Stat);

	return info != nullptr ? info->unit : "";
}


EffectDomain StatDomain(EffectStat Stat)
{
	const StatInfo *info = FindStatInfo(Stat);

	return info != nullptr ? info->domain : EffectDomain::Ship;
}


const std::vector<EffectStat> &AllEffectStats()
{
	// Built once, in the table order, so the caller never pays for it per call and
	// the order is the declaration order of the enum.
	static const std::vector<EffectStat> stats = []
	{
		std::vector<EffectStat> built;
		built.reserve(statTable.size());

		for (const auto &info : statTable)
		{
			built.push_back(info.stat);
		}

		return built;
	}();

	return stats;
}


void ApplyEffectModifiers(std::map<EffectStat, double> &Stats, const EffectModifier &Modifier, int Tiers)
{
	if (Tiers <= 0)
	{
		return;
	}

	const double current = ReadEffectStat(Stats, Modifier.stat);
	double &target = Stats[Modifier.stat];

	switch (Modifier.op)
	{
	case EffectOperator::Add:
		target = current + Modifier.perTier * static_cast<double>(Tiers);
		break;

	case EffectOperator::Multiply:
		target = current * (1.0 + Modifier.perTier * static_cast<double>(Tiers));
		break;

	case EffectOperator::Set:
		// A cap does not stack: buying a tier of it raises the ceiling, it does not
		// add the ceiling to itself.
		target = Modifier.perTier;
		break;
	}
}


double ReadEffectStat(const std::map<EffectStat, double> &Stats, EffectStat Stat)
{
	const auto entry = Stats.find(Stat);

	return entry != Stats.end() ? entry->second : 0.0;
}


std::string FormatEffectStatValue(double Value)
{
	std::ostringstream out;

	// Six significant digits, so 1500.0 m3 prints as 1500 and 1.2 GFLOP/s as 1.2,
	// and the whole number of a stat that counts in integers stays a whole number.
	out.precision(6);
	out << Value;

	return out.str();
}


std::string DescribeEffectStats(const std::map<EffectStat, double> &Stats)
{
	std::ostringstream out;
	bool first = true;

	for (const EffectStat stat : AllEffectStats())
	{
		const auto entry = Stats.find(stat);

		if (entry == Stats.end())
		{
			continue;
		}

		if (!first)
		{
			out << ", ";
		}

		out << StatName(stat) << " " << FormatEffectStatValue(entry->second) << " " << StatUnit(stat);
		first = false;
	}

	return out.str();
}


std::string DescribeEffectModifier(const EffectModifier &Modifier)
{
	std::ostringstream out;

	out << DescribeEffectOperator(Modifier.op) << " " << StatName(Modifier.stat);

	const std::string unit = StatUnit(Modifier.stat);

	if (!unit.empty())
	{
		out << " (" << unit << ")";
	}

	out << " " << FormatEffectStatValue(Modifier.perTier) << " per tier";

	return out.str();
}
} // namespace CivSimCore
