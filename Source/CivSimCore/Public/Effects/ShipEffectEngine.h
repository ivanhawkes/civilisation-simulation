#pragma once

#include "Core/ProgressionManager.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/EffectCatalogue.h"
#include "Effects/EffectTypes.h"
#include <cstddef>
#include <map>
#include <string>
#include <vector>


namespace CivSimCore
{
// A class of hull: the stats every ship of it starts with, and the systems it has a
// slot for.
//
// The base is the hull that has bought none of the tree: what a long-haul trader is
// worth before the player spends a single tier on it. The ship systems of
// data/skill-tree/spaceship.yaml then improve these numbers a tier at a time, and the
// catalogue says by how much.
struct ShipClassSpec
{
	std::string hullId; // "HullTraderLongHaul", the id ShipInfo carries
	std::string className;
	std::string description;

	std::map<EffectStat, double> baseStats;

	// The effects the hull has a slot for, by effect id. A hull with no slot for an
	// effect refuses the system: a salvage rig cannot be fitted to a hull that has no
	// rig frame, and the shipyard tree is where the hulls with more slots come from.
	std::vector<std::string> fitableEffects;
};

// The engine that puts skill effects and technology effects on ships.
//
// The two origins reach a hull by different roads, and the difference is the point of
// the split:
//
//   Skill        a system the crew train and fit. The hull has to have a slot for it,
//                the tier fitted is the tier bought, the system sits in the hull's
//                ShipSystems and a yard can take it off again.
//   Technology   a technique the civilisation knows. No hull refuses it, it costs no
//                slot, and it reaches every ship of the operator, so a civilisation
//                that has bought a technology flies a fleet of it.
//
// The node type carries the split: `nodeType: Skill` in the data is a system,
// `nodeType: Tech` is knowledge, and `FitSkill` reads the node's own field rather than
// being told.
//
// What the engine writes is always the same: an `EffectSource` entity per effect on
// the ship, and a `ShipStats` component that is the base plus every source. Systems
// read `ShipStats`, so combat, cargo, fuel and a jump all agree on what a hull is
// worth, and none of them adds a tier to itself.
class ShipEffectEngine
{
  public:
	// The effects the engine knows. The default is the catalogue the shipped tree
	// drives, EffectCatalogue::Default.
	explicit ShipEffectEngine(EffectCatalogue Catalogue = EffectCatalogue::Default());

	const EffectCatalogue &Catalogue() const;
	void SetCatalogue(EffectCatalogue Catalogue);

	// The long-haul trader of data/skill-tree/spaceship.yaml: a slot for each of the
	// 25 ship systems, and the base numbers of a hull that has bought none of them.
	static ShipClassSpec LongHaulTrader();

	// Builds one ship of a class: the identity, the base stats, the stats as it flies
	// (a copy of the base), the slots and an empty set of fitted systems. Fails on an
	// empty id, a hull with no base stats, an empty operator, and an id the world
	// already has.
	bool BuildShip(
	    EcsWorld &World, const std::string &ShipId, const ShipClassSpec &Hull, const std::string &OperatorId,
	    std::string &ErrorMessage);

	bool HasShip(const EcsWorld &World, const std::string &ShipId) const;

	// Ships of one operator, in sorted id order; an empty OperatorId asks for every
	// ship in the world.
	std::vector<std::string> ShipsOf(const EcsWorld &World, const std::string &OperatorId) const;

	// Puts one effect on one ship at a tier. Origin decides the rule, as above. Fails
	// when the effect is unknown, when it is not of the Ship domain, when Tier is
	// below 1, when a Skill effect has no slot on that hull, and when the ship is
	// unknown. Re-fitting the same effect on the same ship replaces the tier, which is
	// what a refit dock does.
	bool Fit(
	    EcsWorld &World, const std::string &ShipId, const std::string &SkillId, const std::string &EffectId,
	    EffectOrigin Origin, int Tier, std::string &ErrorMessage);

	// One node of the tree on one ship: the effect is the node's `targetEffectID` and
	// the origin is its `nodeType`. A node with no `targetEffectID` - every age gate -
	// buys nothing on a ship and returns true, because the tree asked for no effect,
	// and an effect of another domain - a yard, a lane, a claim, a market, a habitat -
	// returns true with Bought false, so a whole tree walks in one loop.
	bool FitSkill(
	    EcsWorld &World, const std::string &ShipId, const SkillNode &Node, int Tier, bool &Bought,
	    std::string &ErrorMessage);

	// Every node of the tree that has an effect and a tier above 0, onto one ship, in
	// sorted skill order. Applied counts the systems fitted, Skipped the nodes that
	// asked for no ship effect.
	bool ApplyProgression(
	    EcsWorld &World, const ProgressionManager &Progression, const std::string &ShipId, unsigned &Applied,
	    unsigned &Skipped, std::string &ErrorMessage);

	// The same onto every ship of one operator - the way a technology reaches a fleet.
	// An empty OperatorId asks for every ship.
	bool ApplyProgressionToWorld(
	    EcsWorld &World, const ProgressionManager &Progression, const std::string &OperatorId, unsigned &Applied,
	    unsigned &Skipped, std::string &ErrorMessage);

	// Takes every source of one skill off every ship, and the systems off the hulls:
	// the respec road, where the tiers go back to the pool. Returns the sources taken
	// off, and recomputes every ship it touched.
	unsigned UnfitSkill(EcsWorld &World, const std::string &SkillId, std::string &ErrorMessage);

	// Takes one effect off one ship.
	bool UnfitEffect(EcsWorld &World, const std::string &ShipId, const std::string &EffectId, std::string &ErrorMessage);

	// The base plus every source on the ship, applied in sorted effect id order: the
	// order that says the same tree gives the same numbers on every platform, which is
	// what a fill log, a UI and a test all need.
	bool Recompute(EcsWorld &World, const std::string &ShipId, std::string &ErrorMessage);

	// Every ship in the world. Returns how many were rebuilt.
	unsigned RecomputeAll(EcsWorld &World, std::string &ErrorMessage);

	// The tier of one system on one ship: 0 when the hull does not carry it.
	int FittedTier(const EcsWorld &World, const std::string &ShipId, const std::string &EffectId) const;

	// The systems a hull carries, in sorted effect id order.
	std::vector<std::string> FittedEffects(const EcsWorld &World, const std::string &ShipId) const;

	// The sources on a ship, in sorted effect id order.
	std::vector<EffectSource> SourcesOn(const EcsWorld &World, const std::string &ShipId) const;

	// The stat as the hull flies it: 0.0 when the hull has never had that stat.
	double GetStat(const EcsWorld &World, const std::string &ShipId, EffectStat Stat) const;

	std::map<EffectStat, double> StatsOf(const EcsWorld &World, const std::string &ShipId) const;
	std::map<EffectStat, double> BaseStatsOf(const EcsWorld &World, const std::string &ShipId) const;

	// A hull as lines for the UI: what it is, who flies it, the systems on it by tier,
	// and the stats it flies them with.
	std::string DescribeShip(const EcsWorld &World, const std::string &ShipId) const;

  private:
	// The ship's entity, with the message every entry point needs when it is not a
	// ship.
	bool ResolveShip(const EcsWorld &World, const std::string &ShipId, EcsWorld::Entity &Ship, std::string &ErrorMessage) const;

	// The effect, with the message every entry point needs when the catalogue has it
	// not, or has it in another domain.
	bool ResolveShipEffect(const std::string &EffectId, const EffectDefinition *&Definition, std::string &ErrorMessage) const;

	EffectCatalogue catalogue;
};
} // namespace CivSimCore
