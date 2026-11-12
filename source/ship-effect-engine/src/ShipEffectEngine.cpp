#include "Effects/ShipEffectEngine.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


namespace ManifestCore
{
namespace
{
// The source entity of one system on one ship: one entity per ship per effect, so a
// hull carries a set of systems and never a bag of them, and a view over
// EffectSource answers "what is on this ship" without a scan of a container.
std::string SourceIdFor(const std::string &ShipId, const std::string &EffectId)
{
	return ShipId + ":" + EffectId;
}

bool HasSlot(const ShipSlots &Slots, const std::string &EffectId)
{
	return std::find(Slots.fitableEffects.begin(), Slots.fitableEffects.end(), EffectId) !=
	       Slots.fitableEffects.end();
}
} // namespace

ShipEffectEngine::ShipEffectEngine(EffectCatalogue Catalogue) : catalogue(std::move(Catalogue))
{
}


const EffectCatalogue &ShipEffectEngine::Catalogue() const
{
	return catalogue;
}


void ShipEffectEngine::SetCatalogue(EffectCatalogue Catalogue)
{
	catalogue = std::move(Catalogue);
}


ShipClassSpec ShipEffectEngine::LongHaulTrader()
{
	// The hull the 25 ship systems of data/skill-tree/spaceship.yaml are bought
	// against: the long-haul trader of the README, the ship that carries cargo between
	// worlds and has to survive the pirates who live off that trade.
	//
	// The numbers are a working hull, chosen so the tree's three tiers of a system
	// move that system by a half to a third of what it starts at: tier 3 of a shield
	// bank is a screen worth half again as much, not a new ship, which is exactly what
	// the file says a tier buys - "thickness, not a new slot".
	ShipClassSpec trader;
	trader.hullId = "HullTraderLongHaul";
	trader.className = "Long-haul trader";
	trader.description = "The cargo hauler the trade lanes run on";

	trader.baseStats = {
	    {EffectStat::ShieldFront, 120.0},
	    {EffectStat::ShieldSide, 80.0},
	    {EffectStat::ShieldRear, 60.0},

	    {EffectStat::GunFront, 24.0},
	    {EffectStat::GunRear, 12.0},
	    {EffectStat::LaserFront, 12.0},
	    {EffectStat::LaserRear, 6.0},

	    {EffectStat::DriveThrust, 420.0},
	    {EffectStat::ManoeuvreJets, 48.0},
	    {EffectStat::AttitudeControl, 10.0},

	    {EffectStat::SensorLongRange, 4000.0},
	    {EffectStat::SensorFireControl, 800.0},
	    {EffectStat::SensorCommercial, 1500.0},

	    {EffectStat::NavigationComputer, 2.0},
	    {EffectStat::TraderComputer, 1.5},

	    {EffectStat::CargoVolume, 900.0},
	    {EffectStat::FuelCapacity, 60.0},
	    {EffectStat::SalvageCapacity, 15.0},

	    {EffectStat::LifeSupportDays, 45.0},

	    {EffectStat::StealthRating, 0.25},
	    {EffectStat::EmissionsControl, 0.2},
	    {EffectStat::Countermeasures, 0.15},

	    // The crew stats start at 1.0, because they are multipliers: a crew that is
	    // trained, that can fight damage and that has spares does more of the same
	    // work, and a crew that has bought none of it does its base work.
	    {EffectStat::CrewTraining, 1.0},
	    {EffectStat::DamageControl, 1.0},
	    {EffectStat::SparesCapacity, 1.0},
	};

	trader.fitableEffects = {
	    "ShieldFront",      "ShieldSide",         "ShieldRear",      "GunFront",
	    "GunRear",          "LaserFront",         "LaserRear",       "EngineThrust",
	    "ManoeuvringJets",  "AttitudeControl",    "SensorLongRange", "SensorFireControl",
	    "SensorCommercial", "ComputerNavigation", "ComputerTrader",  "CargoHolds",
	    "FuelBunkers",      "SalvageRig",         "LifeSupport",     "StealthCoating",
	    "EmissionsControl", "Countermeasures",    "CrewTraining",    "DamageControl",
	    "SparesLockers",
	};

	return trader;
}


bool ShipEffectEngine::BuildShip(
    EcsWorld &World, const std::string &ShipId, const ShipClassSpec &Hull,
    const std::string &OperatorId, std::string &ErrorMessage)
{
	if (ShipId.empty())
	{
		ErrorMessage =
		    "ShipEffectEngine: the shipId is empty; a hull is named by the id the UI reports";

		return false;
	}

	if (Hull.hullId.empty() || Hull.baseStats.empty())
	{
		ErrorMessage =
		    "ShipEffectEngine: the hull '" + Hull.className + "' has no hullId or no base stats";

		return false;
	}

	if (OperatorId.empty())
	{
		ErrorMessage =
		    "Ship '" + ShipId +
		    "': the operator is empty; the operator decides whose technology reaches the ship";

		return false;
	}

	EcsWorld::Entity ship = EcsWorld::Null;

	if (!World.Create(ShipId, ship, ErrorMessage))
	{
		return false;
	}

	ShipInfo info;
	info.shipId = ShipId;
	info.hullId = Hull.hullId;
	info.className = Hull.className;
	info.description = Hull.description;
	info.operatorId = OperatorId;

	World.Add<ShipInfo>(ship, std::move(info));
	World.Add<ShipBaseStats>(ship, ShipBaseStats{Hull.baseStats});
	World.Add<ShipStats>(ship, ShipBaseStats{Hull.baseStats}.values);
	World.Add<ShipSlots>(ship, ShipSlots{Hull.fitableEffects});
	World.Add<ShipSystems>(ship);

	return true;
}


bool ShipEffectEngine::HasShip(const EcsWorld &World, const std::string &ShipId) const
{
	return World.HasComponent<ShipInfo>(ShipId);
}


std::vector<std::string> ShipEffectEngine::ShipsOf(
    const EcsWorld &World, const std::string &OperatorId) const
{
	std::vector<std::string> ships;

	// The view is packed, so the order inside it is EnTT's, not the id order: the ids
	// are collected and sorted, which is the order a fleet reports and the order the
	// tests read.
	const auto view = World.View<ShipInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const ShipInfo *info = World.Get<ShipInfo>(entity);

		if (info == nullptr || (!OperatorId.empty() && info->operatorId != OperatorId))
		{
			continue;
		}

		ships.push_back(info->shipId);
	}

	std::sort(ships.begin(), ships.end());

	return ships;
}


bool ShipEffectEngine::Fit(
    EcsWorld &World, const std::string &ShipId, const std::string &SkillId, const std::string &EffectId,
    EffectOrigin Origin, int Tier, std::string &ErrorMessage)
{
	EcsWorld::Entity ship = EcsWorld::Null;

	if (!ResolveShip(World, ShipId, ship, ErrorMessage))
	{
		return false;
	}

	if (EffectId.empty())
	{
		ErrorMessage = "Ship '" + ShipId + "': the effectId is empty";

		return false;
	}

	if (SkillId.empty())
	{
		ErrorMessage = "Ship '" + ShipId +
		               "': the skillId is empty; a system on a hull names the node that bought it";

		return false;
	}

	if (Tier <= 0)
	{
		ErrorMessage = "Ship '" + ShipId + "': '" + EffectId + "' at tier " + std::to_string(Tier) +
		               "; a tier is 1 or above";

		return false;
	}

	const EffectDefinition *definition = nullptr;

	if (!ResolveShipEffect(EffectId, definition, ErrorMessage))
	{
		// The message the engine writes is about the effect; the ship is the reader of
		// it, and a fleet call has to say which hull refused.
		ErrorMessage = "Ship '" + ShipId + "': " + ErrorMessage;

		return false;
	}

	if (Origin == EffectOrigin::Skill)
	{
		const ShipSlots *slots = World.Get<ShipSlots>(ship);

		if (slots != nullptr && !HasSlot(*slots, EffectId))
		{
			const ShipInfo *info = World.Get<ShipInfo>(ship);
			const std::string className = info != nullptr ? info->className : "hull";

			ErrorMessage = "Ship '" + ShipId + "': the " + className + " has no slot for '" + EffectId +
			               "'; a system needs a slot, and a technology needs none";

			return false;
		}
	}

	const std::string sourceId = SourceIdFor(ShipId, EffectId);
	EcsWorld::Entity source = EcsWorld::Null;

	if (World.Has(sourceId))
	{
		// A refit: the same system, a different tier, the same entity.
		source = World.Find(sourceId);

		EffectSource *existing = World.Get<EffectSource>(source);

		if (existing == nullptr)
		{
			ErrorMessage =
			    "Ship '" + ShipId + "': '" + sourceId + "' exists and carries no EffectSource";

			return false;
		}

		existing->skillId = SkillId;
		existing->effectId = EffectId;
		existing->origin = Origin;
		existing->tier = Tier;
	}
	else if (!World.Create(sourceId, source, ErrorMessage))
	{
		return false;
	}
	else
	{
		EffectSource newSource;
		newSource.skillId = SkillId;
		newSource.effectId = EffectId;
		newSource.origin = Origin;
		newSource.tier = Tier;

		World.Add<EffectSource>(source, std::move(newSource));
	}

	World.Put<EffectOn>(source, EffectOn{ShipId, EffectDomain::Ship});

	ShipSystems *systems = World.Get<ShipSystems>(ship);

	if (systems == nullptr)
	{
		systems = World.Add<ShipSystems>(ship);

		if (systems == nullptr)
		{
			ErrorMessage = "Ship '" + ShipId + "': no ShipSystems and none could be added";

			return false;
		}
	}

	// The systems a hull carries are the systems a slot was spent on. A technology is
	// knowledge, and the hull flies a technique it has no frame for, so the technique
	// never enters the list a yard refits and a UI prints under "Systems" - and a
	// technique that lands on an effect the hull has fitted as a system takes the system
	// off: the source is one per effect, the tier of it is now the technique's, and the
	// slot the system spent is the hull's to spend again.
	if (Origin == EffectOrigin::Skill)
	{
		FittedSystem fitted;
		fitted.skillId = SkillId;
		fitted.displayName = definition->displayName;
		fitted.origin = Origin;
		fitted.tier = Tier;

		systems->fitted[EffectId] = fitted;
	}
	else
	{
		systems->fitted.erase(EffectId);
	}

	return Recompute(World, ShipId, ErrorMessage);
}


bool ShipEffectEngine::FitSkill(
    EcsWorld &World, const std::string &ShipId, const SkillNode &Node, int Tier, bool &Bought,
    std::string &ErrorMessage)
{
	Bought = false;

	if (Node.skillId.empty())
	{
		ErrorMessage = "Ship '" + ShipId + "': a skill node with no skillId";

		return false;
	}

	// An age gate carries no targetEffectID, and its own file says why: the gate opens
	// a set of goods and drives no effect of its own. Asking for no effect is not a
	// failure, so the whole tree walks in one loop.
	if (Node.targetEffectID.empty())
	{
		return true;
	}

	const EffectDefinition *definition = catalogue.Find(Node.targetEffectID);

	if (definition == nullptr)
	{
		ErrorMessage = "Ship '" + ShipId + "': node '" + Node.skillId + "' asks for effect '" +
		               Node.targetEffectID + "', which the catalogue does not define";

		return false;
	}

	// The yards, the lanes, the claims, the markets and the habitats are effects of the
	// civilisation, not systems on a hull: the node is bought, and this ship does not
	// carry it.
	if (definition->domain != EffectDomain::Ship)
	{
		return true;
	}

	const EffectOrigin origin =
	    Node.nodeType == NodeType::Tech ? EffectOrigin::Technology : EffectOrigin::Skill;
	const bool ok = Fit(World, ShipId, Node.skillId, Node.targetEffectID, origin, Tier, ErrorMessage);

	Bought = ok;

	return ok;
}


bool ShipEffectEngine::ApplyProgression(
    EcsWorld &World, const ProgressionManager &Progression, const std::string &ShipId,
    unsigned &Applied, unsigned &Skipped, std::string &ErrorMessage)
{
	// The walk writes the caller's two columns: they are set, not added to, and a caller
	// that arrives with a number in hand is told what this walk did, not what the last
	// walk did. The fleet walk is the exception a fleet needs: a fleet is walked hull by
	// hull, and the counts of the hulls are the count of the fleet.
	Applied = 0;
	Skipped = 0;

	if (!HasShip(World, ShipId))
	{
		ErrorMessage = "ShipEffectEngine: '" + ShipId + "' is not a ship in this world";

		return false;
	}

	// The registry is a map keyed by skill id, so the tree is walked in the same order
	// on every platform and a hull rebuilt from the same tree flies the same numbers.
	for (const auto &entry : Progression.GetRegistry())
	{
		const SkillNode &node = entry.second;
		const int tier = Progression.GetCurrentTier(node.skillId);

		// A node the tree has bought no tier of buys nothing, and the walk walked past
		// it: the node is of the tree, the hull flies no system for it, and the count of
		// the nodes the walk did not fit counts it. The two columns add to the nodes of
		// the tree, on a hull and in a civilisation store alike, which is the reading a UI
		// shows: the tree has nine nodes, the hull flies one system, and the eight it does
		// not are the eight the hull has no use for.
		if (tier <= 0)
		{
			++Skipped;

			continue;
		}

		bool bought = false;

		if (!FitSkill(World, ShipId, node, tier, bought, ErrorMessage))
		{
			ErrorMessage = "Ship '" + ShipId + "', node '" + node.skillId + "': " + ErrorMessage;

			return false;
		}

		if (bought)
		{
			++Applied;
		}
		else
		{
			++Skipped;
		}
	}

	return true;
}


bool ShipEffectEngine::ApplyProgressionToWorld(
    EcsWorld &World, const ProgressionManager &Progression, const std::string &OperatorId,
    unsigned &Applied, unsigned &Skipped, std::string &ErrorMessage)
{
	const std::vector<std::string> ships = ShipsOf(World, OperatorId);

	if (ships.empty())
	{
		ErrorMessage = "ShipEffectEngine: the world holds no ship of operator '" + OperatorId +
		               "'; a technology reaches a fleet, and there is no fleet";

		return false;
	}

	for (const auto &shipId : ships)
	{
		// The hull's walk writes the hull's numbers, and the fleet's columns add them up:
		// a fleet of two hulls and a tree of nine nodes is eighteen node visits, the two
		// hulls read the same tree and fly the same systems, and the reading a UI shows is
		// one walk of the fleet, not a walk of the last hull of it.
		unsigned hullApplied = 0u;
		unsigned hullSkipped = 0u;

		if (!ApplyProgression(World, Progression, shipId, hullApplied, hullSkipped, ErrorMessage))
		{
			return false;
		}

		Applied += hullApplied;
		Skipped += hullSkipped;
	}

	return true;
}


unsigned ShipEffectEngine::UnfitSkill(
    EcsWorld &World, const std::string &SkillId, std::string &ErrorMessage)
{
	// The sources first, the hulls second, the rebuilds last: a skill that reaches a
	// fleet touches several hulls, and a hull must not be rebuilt while a source of
	// the same skill still sits on it.
	std::vector<std::string> doomed;
	std::set<std::string> touched;

	const auto view = World.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = World.Get<EffectSource>(entity);
		const EffectOn *on = World.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || source->skillId != SkillId ||
		    on->domain != EffectDomain::Ship)
		{
			continue;
		}

		const std::string name = World.NameOf(entity);

		if (name.empty())
		{
			continue;
		}

		doomed.push_back(name);
		touched.insert(on->targetId);
	}

	for (const auto &shipId : touched)
	{
		ShipSystems *systems = World.Get<ShipSystems>(shipId);

		if (systems == nullptr)
		{
			continue;
		}

		for (auto entry = systems->fitted.begin(); entry != systems->fitted.end();)
		{
			if (entry->second.skillId == SkillId)
			{
				entry = systems->fitted.erase(entry);
			}
			else
			{
				++entry;
			}
		}
	}

	for (const auto &name : doomed)
	{
		if (!World.Destroy(name, ErrorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	for (const auto &shipId : touched)
	{
		if (HasShip(World, shipId) && !Recompute(World, shipId, ErrorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	return static_cast<unsigned>(doomed.size());
}


bool ShipEffectEngine::UnfitEffect(
    EcsWorld &World, const std::string &ShipId, const std::string &EffectId, std::string &ErrorMessage)
{
	EcsWorld::Entity ship = EcsWorld::Null;

	if (!ResolveShip(World, ShipId, ship, ErrorMessage))
	{
		return false;
	}

	const std::string sourceId = SourceIdFor(ShipId, EffectId);

	// A system the hull does not carry is a system already off the hull, and taking it
	// off again is a clean no-op: the respec road calls this for every effect of the
	// skill, on every hull the skill reached, and a hull that never fitted one of its
	// systems is no failure on the road. The rebuild runs either way, so the hull that
	// carried the system and the hull that never did both read the base and what is left
	// of the rest.
	if (World.Has(sourceId) && !World.Destroy(sourceId, ErrorMessage))
	{
		return false;
	}

	if (ShipSystems *systems = World.Get<ShipSystems>(ship))
	{
		systems->fitted.erase(EffectId);
	}

	return Recompute(World, ShipId, ErrorMessage);
}


bool ShipEffectEngine::Recompute(EcsWorld &World, const std::string &ShipId, std::string &ErrorMessage)
{
	EcsWorld::Entity ship = EcsWorld::Null;

	if (!ResolveShip(World, ShipId, ship, ErrorMessage))
	{
		return false;
	}

	const ShipBaseStats *base = World.Get<ShipBaseStats>(ship);

	if (base == nullptr)
	{
		ErrorMessage = "Ship '" + ShipId + "': no ShipBaseStats; the hull was not built by BuildShip";

		return false;
	}

	std::map<EffectStat, double> stats = base->values;

	// Keyed by effect id, which is the order the modifiers are applied in: the same
	// tree, the same hull, the same numbers on every platform. A Multiply and a Set
	// depend on what a previous effect left behind, so an order that moved between
	// runs would move the hull with it.
	std::map<std::string, EffectSource> sources;

	const auto view = World.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = World.Get<EffectSource>(entity);
		const EffectOn *on = World.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || on->targetId != ShipId ||
		    on->domain != EffectDomain::Ship)
		{
			continue;
		}

		sources[source->effectId] = *source;
	}

	for (const auto &entry : sources)
	{
		const EffectDefinition *definition = catalogue.Find(entry.first);

		if (definition == nullptr)
		{
			// A catalogue swap, mid-game: the system stays fitted and buys nothing,
			// which is the honest reading. The hull's ShipSystems list is where the
			// player is told about it, not a stat that has quietly gone to zero.
			continue;
		}

		for (const auto &modifier : definition->modifiers)
		{
			ApplyEffectModifiers(stats, modifier, entry.second.tier);
		}
	}

	World.Put<ShipStats>(ship, ShipStats{stats});

	return true;
}


unsigned ShipEffectEngine::RecomputeAll(EcsWorld &World, std::string &ErrorMessage)
{
	const std::vector<std::string> ships = ShipsOf(World, {});
	unsigned rebuilt = 0;

	for (const auto &shipId : ships)
	{
		if (!Recompute(World, shipId, ErrorMessage))
		{
			return rebuilt;
		}

		++rebuilt;
	}

	return rebuilt;
}


int ShipEffectEngine::FittedTier(
    const EcsWorld &World, const std::string &ShipId, const std::string &EffectId) const
{
	const ShipSystems *systems = World.Get<ShipSystems>(ShipId);

	if (systems == nullptr)
	{
		return 0;
	}

	const auto entry = systems->fitted.find(EffectId);

	return entry != systems->fitted.end() ? entry->second.tier : 0;
}


std::vector<std::string> ShipEffectEngine::FittedEffects(
    const EcsWorld &World, const std::string &ShipId) const
{
	std::vector<std::string> effects;

	const ShipSystems *systems = World.Get<ShipSystems>(ShipId);

	if (systems == nullptr)
	{
		return effects;
	}

	for (const auto &entry : systems->fitted)
	{
		effects.push_back(entry.first);
	}

	return effects;
}


std::vector<EffectSource> ShipEffectEngine::SourcesOn(
    const EcsWorld &World, const std::string &ShipId) const
{
	std::map<std::string, EffectSource> ordered;

	const auto view = World.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = World.Get<EffectSource>(entity);
		const EffectOn *on = World.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || on->targetId != ShipId)
		{
			continue;
		}

		ordered[source->effectId] = *source;
	}

	std::vector<EffectSource> sources;
	sources.reserve(ordered.size());

	for (const auto &entry : ordered)
	{
		sources.push_back(entry.second);
	}

	return sources;
}


double ShipEffectEngine::GetStat(
    const EcsWorld &World, const std::string &ShipId, EffectStat Stat) const
{
	return ReadEffectStat(StatsOf(World, ShipId), Stat);
}


std::map<EffectStat, double> ShipEffectEngine::StatsOf(
    const EcsWorld &World, const std::string &ShipId) const
{
	const ShipStats *stats = World.Get<ShipStats>(ShipId);

	return stats != nullptr ? stats->values : std::map<EffectStat, double>{};
}


std::map<EffectStat, double> ShipEffectEngine::BaseStatsOf(
    const EcsWorld &World, const std::string &ShipId) const
{
	const ShipBaseStats *stats = World.Get<ShipBaseStats>(ShipId);

	return stats != nullptr ? stats->values : std::map<EffectStat, double>{};
}


std::string ShipEffectEngine::DescribeShip(const EcsWorld &World, const std::string &ShipId) const
{
	const ShipInfo *info = World.Get<ShipInfo>(ShipId);

	if (info == nullptr)
	{
		return "Ship '" + ShipId + "': the world holds no such ship";
	}

	const ShipSystems *systems = World.Get<ShipSystems>(info->shipId);

	std::ostringstream out;

	out << info->shipId << " " << info->className << " of " << info->operatorId;

	if (!info->homeMarketId.empty())
	{
		out << ", home " << info->homeMarketId;
	}

	out << "\n";

	out << "  Systems: ";

	if (systems == nullptr || systems->fitted.empty())
	{
		out << "none: the hull flies as built";
	}
	else
	{
		bool first = true;

		for (const auto &entry : systems->fitted)
		{
			out << (first ? "" : "; ") << entry.second.displayName << " " << entry.second.skillId
			    << " tier " << entry.second.tier << " [" << DescribeEffectOrigin(entry.second.origin)
			    << "]";
			first = false;
		}
	}

	out << "\n  Stats: ";

	const std::string stats = DescribeEffectStats(StatsOf(World, info->shipId));

	out << (stats.empty() ? "none" : stats);

	return out.str();
}


bool ShipEffectEngine::ResolveShip(
    const EcsWorld &World, const std::string &ShipId, EcsWorld::Entity &Ship,
    std::string &ErrorMessage) const
{
	Ship = World.Find(ShipId);

	if (!World.IsValid(Ship))
	{
		ErrorMessage = "Ship '" + ShipId + "': the world holds no entity of that id";

		return false;
	}

	if (World.Get<ShipInfo>(Ship) == nullptr)
	{
		ErrorMessage = "Ship '" + ShipId + "': the entity is not a ship; it carries no ShipInfo";

		return false;
	}

	return true;
}


bool ShipEffectEngine::ResolveShipEffect(
    const std::string &EffectId, const EffectDefinition *&Definition, std::string &ErrorMessage) const
{
	Definition = catalogue.Find(EffectId);

	if (Definition == nullptr)
	{
		ErrorMessage = "the catalogue defines no effect '" + EffectId + "'";

		return false;
	}

	if (Definition->domain != EffectDomain::Ship)
	{
		ErrorMessage = "'" + EffectId + "' is a " + DescribeEffectDomain(Definition->domain) +
		               " effect, not a ship effect";

		return false;
	}

	return true;
}
} // namespace ManifestCore
