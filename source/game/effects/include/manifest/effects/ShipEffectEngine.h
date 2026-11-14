#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <manifest/ecs/Components.h>
#include <manifest/ecs/EcsWorld.h>
#include <manifest/effects/EffectCatalogue.h>
#include <manifest/progression/ProgressionManager.h>
#include <manifest/types/EffectTypes.h>


namespace Manifest::Core
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
	explicit ShipEffectEngine(EffectCatalogue catalogue = EffectCatalogue::Default());

	const EffectCatalogue &Catalogue() const;
	void SetCatalogue(EffectCatalogue catalogue);

	// The long-haul trader of data/skill-tree/spaceship.yaml: a slot for each of the
	// 25 ship systems, and the base numbers of a hull that has bought none of them.
	static ShipClassSpec LongHaulTrader();

	// Builds one ship of a class: the identity, the base stats, the stats as it flies
	// (a copy of the base), the slots and an empty set of fitted systems. Fails on an
	// empty id, a hull with no base stats, an empty operator, and an id the world
	// already has.
	bool BuildShip(
	    EcsWorld &world, const std::string &shipId, const ShipClassSpec &hull,
	    const std::string &operatorId, std::string &errorMessage);

	bool HasShip(const EcsWorld &world, const std::string &shipId) const;

	// Ships of one operator, in sorted id order; an empty OperatorId asks for every
	// ship in the world.
	std::vector<std::string> ShipsOf(const EcsWorld &world, const std::string &operatorId) const;

	// Puts one effect on one ship at a tier. Origin decides the rule, as above. Fails
	// when the effect is unknown, when it is not of the Ship domain, when Tier is
	// below 1, when a Skill effect has no slot on that hull, and when the ship is
	// unknown. Re-fitting the same effect on the same ship replaces the tier, which is
	// what a refit dock does.
	//
	// A Skill effect is written into the hull's ShipSystems as well as into its source:
	// a system the hull carries is a system a slot was spent on, and the list is what a
	// yard refits and what a UI lists under "Systems". A Technology effect writes the
	// source and no system: the hull flies the technique without a frame for it, and a
	// technique that lands on an effect the hull carries as a system frees the slot the
	// system spent, because the source is one per effect and the tier that arrives is
	// the technique's.
	bool Fit(
	    EcsWorld &world, const std::string &shipId, const std::string &skillId,
	    const std::string &effectId, EffectOrigin origin, int tier, std::string &errorMessage);

	// One node of the tree on one ship: the effect is the node's `targetEffectID` and
	// the origin is its `nodeType`. A node with no `targetEffectID` - every age gate -
	// buys nothing on a ship and returns true, because the tree asked for no effect,
	// and an effect of another domain - a yard, a lane, a claim, a market, a habitat -
	// returns true with Bought false, so a whole tree walks in one loop.
	bool FitSkill(
	    EcsWorld &world, const std::string &shipId, const SkillNode &node, int tier, bool &bought,
	    std::string &errorMessage);

	// Every node of the tree, onto one ship, in sorted skill order. The two columns add
	// to the nodes of the tree: Applied counts the systems the hull flies and Skipped
	// every node the walk did not fit - the laws, the lanes, the yards, the claims and
	// the habitats, which are the civilisation's stores and not the hull's, the gates,
	// which buy nothing, and the nodes the tree has bought no tier of, which buy nothing
	// yet. The walk sets the caller's columns, and a fleet walk writes the columns of the
	// fleet, which is the sum of the walks of its hulls.
	bool ApplyProgression(
	    EcsWorld &world, const ProgressionManager &progression, const std::string &shipId,
	    unsigned &applied, unsigned &skipped, std::string &errorMessage);

	// The same onto every ship of one operator - the way a technology reaches a fleet.
	// An empty OperatorId asks for every ship. The columns of a fleet walk are the
	// fleet's: the walk writes them with the sum of the walks of the hulls, so a fleet of
	// two hulls and a tree of nine nodes has eighteen node visits, and two hulls of one
	// tree fly the same systems.
	bool ApplyProgressionToWorld(
	    EcsWorld &world, const ProgressionManager &progression, const std::string &operatorId,
	    unsigned &applied, unsigned &skipped, std::string &errorMessage);

	// Takes every source of one skill off every ship, and the systems off the hulls:
	// the respec road, where the tiers go back to the pool. Returns the sources taken
	// off, and recomputes every ship it touched.
	unsigned UnfitSkill(EcsWorld &world, const std::string &skillId, std::string &errorMessage);

	// Takes one effect off one ship, and leaves the base.
	//
	// A hull that does not carry the system is a hull already in the state this call
	// leaves it, and the answer is true: the respec road calls this for every effect of
	// a skill on every hull the skill reached, and a hull that never carried one of the
	// systems is not a failure on the road. Taking a system off twice is taking it off
	// once, and the hull reads the same numbers both times.
	bool UnfitEffect(
	    EcsWorld &world, const std::string &shipId, const std::string &effectId,
	    std::string &errorMessage);

	// The base plus every source on the ship, applied in sorted effect id order: the
	// order that says the same tree gives the same numbers on every platform, which is
	// what a fill log, a UI and a test all need.
	bool Recompute(EcsWorld &world, const std::string &shipId, std::string &errorMessage);

	// Every ship in the world. Returns how many were rebuilt.
	unsigned RecomputeAll(EcsWorld &world, std::string &errorMessage);

	// The tier of one system on one ship: 0 when the hull does not carry it. A
	// technology is knowledge and not a system, so a hull that flies a technique reads
	// 0 for it and the source of it shows in SourcesOn.
	int FittedTier(const EcsWorld &world, const std::string &shipId, const std::string &effectId) const;

	// The systems a hull carries, in sorted effect id order: the systems a slot was
	// spent on, which are the Skill effects and not the techniques the civilisation
	// knows.
	std::vector<std::string> FittedEffects(const EcsWorld &world, const std::string &shipId) const;

	// The sources on a ship, in sorted effect id order: every effect that moves the
	// hull, the systems in its slots and the technology of its operator alike.
	std::vector<EffectSource> SourcesOn(const EcsWorld &world, const std::string &shipId) const;

	// The stat as the hull flies it: 0.0 when the hull has never had that stat.
	double GetStat(const EcsWorld &world, const std::string &shipId, EffectStat stat) const;

	std::map<EffectStat, double> StatsOf(const EcsWorld &world, const std::string &shipId) const;
	std::map<EffectStat, double> BaseStatsOf(const EcsWorld &world, const std::string &shipId) const;

	// A hull as lines for the UI: what it is, who flies it, the systems on it by tier,
	// and the stats it flies them with.
	std::string DescribeShip(const EcsWorld &world, const std::string &shipId) const;

  private:
	// The ship's entity, with the message every entry point needs when it is not a
	// ship.
	bool ResolveShip(
	    const EcsWorld &world, const std::string &shipId, EcsWorld::Entity &ship,
	    std::string &errorMessage) const;

	// The effect, with the message every entry point needs when the catalogue has it
	// not, or has it in another domain.
	bool ResolveShipEffect(
	    const std::string &effectId, const EffectDefinition *&definition,
	    std::string &errorMessage) const;

	EffectCatalogue m_catalogue;
};
} // namespace Manifest::Core
