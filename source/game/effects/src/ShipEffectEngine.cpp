#include <manifest/effects/ShipEffectEngine.h>

#include <algorithm>
#include <format>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


namespace Manifest::Core
{
namespace
{
// The source entity of one system on one ship: one entity per ship per effect, so a
// hull carries a set of systems and never a bag of them, and a view over
// EffectSource answers "what is on this ship" without a scan of a container.
std::string SourceIdFor(const std::string &shipId, const std::string &effectId)
{
	return shipId + ":" + effectId;
}

bool HasSlot(const ShipSlots &slots, const std::string &effectId)
{
	return std::find(slots.fitableEffects.begin(), slots.fitableEffects.end(), effectId) !=
	       slots.fitableEffects.end();
}
} // namespace

ShipEffectEngine::ShipEffectEngine(EffectCatalogue catalogue) : m_catalogue(std::move(catalogue))
{
}


const EffectCatalogue &ShipEffectEngine::Catalogue() const
{
	return m_catalogue;
}


void ShipEffectEngine::SetCatalogue(EffectCatalogue catalogue)
{
	m_catalogue = std::move(catalogue);
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
    EcsWorld &world, const std::string &shipId, const ShipClassSpec &hull,
    const std::string &operatorId, std::string &errorMessage)
{
	if (shipId.empty())
	{
		errorMessage =
		    "ShipEffectEngine: the shipId is empty; a hull is named by the id the UI reports";

		return false;
	}

	if (hull.hullId.empty() || hull.baseStats.empty())
	{
		errorMessage = std::format(
		    "ShipEffectEngine: the hull '{}' has no hullId or no base stats", hull.className);

		return false;
	}

	if (operatorId.empty())
	{
		errorMessage = std::format(
		    "Ship '{}': the operator is empty; the operator decides whose technology reaches the ship",
		    shipId);

		return false;
	}

	EcsWorld::Entity ship = EcsWorld::Null;

	if (!world.Create(shipId, ship, errorMessage))
	{
		return false;
	}

	ShipInfo info;
	info.shipId = shipId;
	info.hullId = hull.hullId;
	info.className = hull.className;
	info.description = hull.description;
	info.operatorId = operatorId;

	world.Add<ShipInfo>(ship, std::move(info));
	world.Add<ShipBaseStats>(ship, ShipBaseStats{hull.baseStats});
	world.Add<ShipStats>(ship, ShipBaseStats{hull.baseStats}.values);
	world.Add<ShipSlots>(ship, ShipSlots{hull.fitableEffects});
	world.Add<ShipSystems>(ship);

	return true;
}


bool ShipEffectEngine::HasShip(const EcsWorld &world, const std::string &shipId) const
{
	return world.HasComponent<ShipInfo>(shipId);
}


std::vector<std::string> ShipEffectEngine::ShipsOf(
    const EcsWorld &world, const std::string &operatorId) const
{
	std::vector<std::string> ships;

	// The view is packed, so the order inside it is EnTT's, not the id order: the ids
	// are collected and sorted, which is the order a fleet reports and the order the
	// tests read.
	const auto view = world.View<ShipInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *info = world.Get<ShipInfo>(entity);

		if (info == nullptr || (!operatorId.empty() && info->operatorId != operatorId))
		{
			continue;
		}

		ships.push_back(info->shipId);
	}

	std::sort(ships.begin(), ships.end());

	return ships;
}


bool ShipEffectEngine::Fit(
    EcsWorld &world, const std::string &shipId, const std::string &skillId, const std::string &effectId,
    EffectOrigin origin, int tier, std::string &errorMessage)
{
	EcsWorld::Entity ship = EcsWorld::Null;

	if (!ResolveShip(world, shipId, ship, errorMessage))
	{
		return false;
	}

	if (effectId.empty())
	{
		errorMessage = std::format("Ship '{}': the effectId is empty", shipId);

		return false;
	}

	if (skillId.empty())
	{
		errorMessage = std::format(
		    "Ship '{}': the skillId is empty; a system on a hull names the node that bought it",
		    shipId);

		return false;
	}

	if (tier <= 0)
	{
		errorMessage = std::format(
		    "Ship '{}': '{}' at tier {}; a tier is 1 or above", shipId, effectId, std::to_string(tier));

		return false;
	}

	const EffectDefinition *definition = nullptr;

	if (!ResolveShipEffect(effectId, definition, errorMessage))
	{
		// The message the engine writes is about the effect; the ship is the reader of
		// it, and a fleet call has to say which hull refused.
		errorMessage = std::format("Ship '{}': {}", shipId, errorMessage);

		return false;
	}

	if (origin == EffectOrigin::Skill)
	{
		const ShipSlots *slots = world.Get<ShipSlots>(ship);

		if (slots != nullptr && !HasSlot(*slots, effectId))
		{
			const ShipInfo *info = world.Get<ShipInfo>(ship);
			const std::string className = info != nullptr ? info->className : "hull";

			errorMessage = std::format(
			    "Ship '{}': the {} has no slot for '{}'; a system needs a slot, and a technology needs "
			    "none",
			    shipId, className, effectId);

			return false;
		}
	}

	const std::string sourceId = SourceIdFor(shipId, effectId);
	EcsWorld::Entity source = EcsWorld::Null;

	if (world.Has(sourceId))
	{
		// A refit: the same system, a different tier, the same entity.
		source = world.Find(sourceId);

		auto *existing = world.Get<EffectSource>(source);

		if (existing == nullptr)
		{
			errorMessage =
			    std::format("Ship '{}': '{}' exists and carries no EffectSource", shipId, sourceId);

			return false;
		}

		existing->skillId = skillId;
		existing->effectId = effectId;
		existing->origin = origin;
		existing->tier = tier;
	}
	else if (!world.Create(sourceId, source, errorMessage))
	{
		return false;
	}
	else
	{
		EffectSource newSource;
		newSource.skillId = skillId;
		newSource.effectId = effectId;
		newSource.origin = origin;
		newSource.tier = tier;

		world.Add<EffectSource>(source, std::move(newSource));
	}

	world.Put<EffectOn>(source, EffectOn{shipId, EffectDomain::Ship});

	auto *systems = world.Get<ShipSystems>(ship);

	if (systems == nullptr)
	{
		systems = world.Add<ShipSystems>(ship);

		if (systems == nullptr)
		{
			errorMessage = std::format("Ship '{}': no ShipSystems and none could be added", shipId);

			return false;
		}
	}

	// The systems a hull carries are the systems a slot was spent on. A technology is
	// knowledge, and the hull flies a technique it has no frame for, so the technique
	// never enters the list a yard refits and a UI prints under "Systems" - and a
	// technique that lands on an effect the hull has fitted as a system takes the system
	// off: the source is one per effect, the tier of it is now the technique's, and the
	// slot the system spent is the hull's to spend again.
	if (origin == EffectOrigin::Skill)
	{
		FittedSystem fitted;
		fitted.skillId = skillId;
		fitted.displayName = definition->displayName;
		fitted.origin = origin;
		fitted.tier = tier;

		systems->fitted[effectId] = fitted;
	}
	else
	{
		systems->fitted.erase(effectId);
	}

	return Recompute(world, shipId, errorMessage);
}


bool ShipEffectEngine::FitSkill(
    EcsWorld &world, const std::string &shipId, const SkillNode &node, int tier, bool &bought,
    std::string &errorMessage)
{
	bought = false;

	if (node.skillId.empty())
	{
		errorMessage = std::format("Ship '{}': a skill node with no skillId", shipId);

		return false;
	}

	// An age gate carries no targetEffectID, and its own file says why: the gate opens
	// a set of goods and drives no effect of its own. Asking for no effect is not a
	// failure, so the whole tree walks in one loop.
	if (node.targetEffectID.empty())
	{
		return true;
	}

	const EffectDefinition *definition = m_catalogue.Find(node.targetEffectID);

	if (definition == nullptr)
	{
		errorMessage = std::format(
		    "Ship '{}': node '{}' asks for effect '{}', which the catalogue does not define", shipId,
		    node.skillId, node.targetEffectID);

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
	    node.nodeType == NodeType::Tech ? EffectOrigin::Technology : EffectOrigin::Skill;
	const bool ok = Fit(world, shipId, node.skillId, node.targetEffectID, origin, tier, errorMessage);

	bought = ok;

	return ok;
}


bool ShipEffectEngine::ApplyProgression(
    EcsWorld &world, const ProgressionManager &progression, const std::string &shipId,
    unsigned &applied, unsigned &skipped, std::string &errorMessage)
{
	// The walk writes the caller's two columns: they are set, not added to, and a caller
	// that arrives with a number in hand is told what this walk did, not what the last
	// walk did. The fleet walk is the exception a fleet needs: a fleet is walked hull by
	// hull, and the counts of the hulls are the count of the fleet.
	applied = 0;
	skipped = 0;

	if (!HasShip(world, shipId))
	{
		errorMessage = std::format("ShipEffectEngine: '{}' is not a ship in this world", shipId);

		return false;
	}

	// The registry is a map keyed by skill id, so the tree is walked in the same order
	// on every platform and a hull rebuilt from the same tree flies the same numbers.
	for (const auto &entry : progression.GetRegistry())
	{
		const SkillNode &node = entry.second;
		const int tier = progression.GetCurrentTier(node.skillId);

		// A node the tree has bought no tier of buys nothing, and the walk walked past
		// it: the node is of the tree, the hull flies no system for it, and the count of
		// the nodes the walk did not fit counts it. The two columns add to the nodes of
		// the tree, on a hull and in a civilisation store alike, which is the reading a UI
		// shows: the tree has nine nodes, the hull flies one system, and the eight it does
		// not are the eight the hull has no use for.
		if (tier <= 0)
		{
			++skipped;

			continue;
		}

		bool bought = false;

		if (!FitSkill(world, shipId, node, tier, bought, errorMessage))
		{
			errorMessage = std::format("Ship '{}', node '{}': {}", shipId, node.skillId, errorMessage);

			return false;
		}

		if (bought)
		{
			++applied;
		}
		else
		{
			++skipped;
		}
	}

	return true;
}


bool ShipEffectEngine::ApplyProgressionToWorld(
    EcsWorld &world, const ProgressionManager &progression, const std::string &operatorId,
    unsigned &applied, unsigned &skipped, std::string &errorMessage)
{
	const std::vector<std::string> ships = ShipsOf(world, operatorId);

	if (ships.empty())
	{
		errorMessage = std::format(
		    "ShipEffectEngine: the world holds no ship of operator '{}'; a technology reaches a fleet, "
		    "and there is no fleet",
		    operatorId);

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

		if (!ApplyProgression(world, progression, shipId, hullApplied, hullSkipped, errorMessage))
		{
			return false;
		}

		applied += hullApplied;
		skipped += hullSkipped;
	}

	return true;
}


unsigned ShipEffectEngine::UnfitSkill(
    EcsWorld &world, const std::string &skillId, std::string &errorMessage)
{
	// The sources first, the hulls second, the rebuilds last: a skill that reaches a
	// fleet touches several hulls, and a hull must not be rebuilt while a source of
	// the same skill still sits on it.
	std::vector<std::string> doomed;
	std::set<std::string> touched;

	const auto view = world.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = world.Get<EffectSource>(entity);
		const EffectOn *on = world.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || source->skillId != skillId ||
		    on->domain != EffectDomain::Ship)
		{
			continue;
		}

		const std::string name = world.NameOf(entity);

		if (name.empty())
		{
			continue;
		}

		doomed.push_back(name);
		touched.insert(on->targetId);
	}

	for (const auto &shipId : touched)
	{
		auto *systems = world.Get<ShipSystems>(shipId);

		if (systems == nullptr)
		{
			continue;
		}

		for (auto entry = systems->fitted.begin(); entry != systems->fitted.end();)
		{
			if (entry->second.skillId == skillId)
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
		if (!world.Destroy(name, errorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	for (const auto &shipId : touched)
	{
		if (HasShip(world, shipId) && !Recompute(world, shipId, errorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	return static_cast<unsigned>(doomed.size());
}


bool ShipEffectEngine::UnfitEffect(
    EcsWorld &world, const std::string &shipId, const std::string &effectId, std::string &errorMessage)
{
	EcsWorld::Entity ship = EcsWorld::Null;

	if (!ResolveShip(world, shipId, ship, errorMessage))
	{
		return false;
	}

	const std::string sourceId = SourceIdFor(shipId, effectId);

	// A system the hull does not carry is a system already off the hull, and taking it
	// off again is a clean no-op: the respec road calls this for every effect of the
	// skill, on every hull the skill reached, and a hull that never fitted one of its
	// systems is no failure on the road. The rebuild runs either way, so the hull that
	// carried the system and the hull that never did both read the base and what is left
	// of the rest.
	if (world.Has(sourceId) && !world.Destroy(sourceId, errorMessage))
	{
		return false;
	}

	if (auto *systems = world.Get<ShipSystems>(ship))
	{
		systems->fitted.erase(effectId);
	}

	return Recompute(world, shipId, errorMessage);
}


bool ShipEffectEngine::Recompute(EcsWorld &world, const std::string &shipId, std::string &errorMessage)
{
	EcsWorld::Entity ship = EcsWorld::Null;

	if (!ResolveShip(world, shipId, ship, errorMessage))
	{
		return false;
	}

	const ShipBaseStats *base = world.Get<ShipBaseStats>(ship);

	if (base == nullptr)
	{
		errorMessage =
		    std::format("Ship '{}': no ShipBaseStats; the hull was not built by BuildShip", shipId);

		return false;
	}

	std::map<EffectStat, double> stats = base->values;

	// Keyed by effect id, which is the order the modifiers are applied in: the same
	// tree, the same hull, the same numbers on every platform. A Multiply and a Set
	// depend on what a previous effect left behind, so an order that moved between
	// runs would move the hull with it.
	std::map<std::string, EffectSource> sources;

	const auto view = world.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = world.Get<EffectSource>(entity);
		const EffectOn *on = world.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || on->targetId != shipId ||
		    on->domain != EffectDomain::Ship)
		{
			continue;
		}

		sources[source->effectId] = *source;
	}

	for (const auto &entry : sources)
	{
		const EffectDefinition *definition = m_catalogue.Find(entry.first);

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

	world.Put<ShipStats>(ship, ShipStats{stats});

	return true;
}


unsigned ShipEffectEngine::RecomputeAll(EcsWorld &world, std::string &errorMessage)
{
	const std::vector<std::string> ships = ShipsOf(world, {});
	unsigned rebuilt = 0;

	for (const auto &shipId : ships)
	{
		if (!Recompute(world, shipId, errorMessage))
		{
			return rebuilt;
		}

		++rebuilt;
	}

	return rebuilt;
}


int ShipEffectEngine::FittedTier(
    const EcsWorld &world, const std::string &shipId, const std::string &effectId) const
{
	const auto *systems = world.Get<ShipSystems>(shipId);

	if (systems == nullptr)
	{
		return 0;
	}

	const auto entry = systems->fitted.find(effectId);

	return entry != systems->fitted.end() ? entry->second.tier : 0;
}


std::vector<std::string> ShipEffectEngine::FittedEffects(
    const EcsWorld &world, const std::string &shipId) const
{
	std::vector<std::string> effects;

	const auto *systems = world.Get<ShipSystems>(shipId);

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
    const EcsWorld &world, const std::string &shipId) const
{
	std::map<std::string, EffectSource> ordered;

	const auto view = world.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *source = world.Get<EffectSource>(entity);
		const auto *on = world.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || on->targetId != shipId)
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
    const EcsWorld &world, const std::string &shipId, EffectStat stat) const
{
	return ReadEffectStat(StatsOf(world, shipId), stat);
}


std::map<EffectStat, double> ShipEffectEngine::StatsOf(
    const EcsWorld &world, const std::string &shipId) const
{
	const auto *stats = world.Get<ShipStats>(shipId);

	return stats != nullptr ? stats->values : std::map<EffectStat, double>{};
}


std::map<EffectStat, double> ShipEffectEngine::BaseStatsOf(
    const EcsWorld &world, const std::string &shipId) const
{
	const auto *stats = world.Get<ShipBaseStats>(shipId);

	return stats != nullptr ? stats->values : std::map<EffectStat, double>{};
}


std::string ShipEffectEngine::DescribeShip(const EcsWorld &world, const std::string &shipId) const
{
	const auto *info = world.Get<ShipInfo>(shipId);

	if (info == nullptr)
	{
		return "Ship '" + shipId + "': the world holds no such ship";
	}

	const auto *systems = world.Get<ShipSystems>(info->shipId);

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

	const std::string stats = DescribeEffectStats(StatsOf(world, info->shipId));

	out << (stats.empty() ? "none" : stats);

	return out.str();
}


bool ShipEffectEngine::ResolveShip(
    const EcsWorld &world, const std::string &shipId, EcsWorld::Entity &ship,
    std::string &errorMessage) const
{
	ship = world.Find(shipId);

	if (!world.IsValid(ship))
	{
		errorMessage = std::format("Ship '{}': the world holds no entity of that id", shipId);

		return false;
	}

	if (world.Get<ShipInfo>(ship) == nullptr)
	{
		errorMessage =
		    std::format("Ship '{}': the entity is not a ship; it carries no ShipInfo", shipId);

		return false;
	}

	return true;
}


bool ShipEffectEngine::ResolveShipEffect(
    const std::string &effectId, const EffectDefinition *&definition, std::string &errorMessage) const
{
	definition = m_catalogue.Find(effectId);

	if (definition == nullptr)
	{
		errorMessage = std::format("the catalogue defines no effect '{}'", effectId);

		return false;
	}

	if (definition->domain != EffectDomain::Ship)
	{
		errorMessage = std::format(
		    "'{}' is a {} effect, not a ship effect", effectId,
		    DescribeEffectDomain(definition->domain));

		return false;
	}

	return true;
}
} // namespace Manifest::Core
