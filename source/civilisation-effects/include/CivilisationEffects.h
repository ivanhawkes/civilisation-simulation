#pragma once

#include "Core/ProgressionManager.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/EffectCatalogue.h"
#include "Effects/EffectTypes.h"
#include <map>
#include <string>
#include <vector>


namespace CivSimCore
{
// The effects a civilisation buys for itself, as against the systems it fits to a hull.
//
// A ship effect lands on a ship, and ShipEffectEngine puts it there. The other five
// domains land on stores a civilisation owns:
//
//   Market   the venues at the worlds of the civilisation: the clearance, the
//            insurance, the depth and the risk of trading there.
//   Body     the planets and the satellites: habitat capacity, the closed loop, how
//            far terraforming has come, medicine and schooling.
//   Route    one lane store per civilisation: the risk of sailing between worlds is one
//            number the whole civilisation shares, read from either end of the lane.
//   Survey   one survey store: how far it reaches, and what a claim turns out.
//   Yard     one yard store: the throughput and the slots that decide how many systems
//            a refit can move at one time.
//
// A store's numbers are the sum of the tiers bought, in sorted effect id order, the
// same rule the hull follows, so the same tree writes the same numbers on every
// platform. The base of a market, a lane, a survey and a yard store is nothing: the
// numbers a player starts with live in MarketplaceOptions and the hull specs, and an
// effect moves them from there.
struct CivilisationEffectEngine
{
	explicit CivilisationEffectEngine(EffectCatalogue Catalogue = EffectCatalogue::Default());

	const EffectCatalogue &Catalogue() const;
	void SetCatalogue(EffectCatalogue Catalogue);

	// One effect at a tier, for one civilisation. The effect's domain picks the store.
	// A Ship effect is refused with a message naming ShipEffectEngine, because the two
	// engines share a catalogue and a caller that hands in the whole tree has to be
	// told which half it asked for.
	bool Apply(
	    EcsWorld &World, const std::string &CivilisationId, const std::string &SkillId,
	    const std::string &EffectId, EffectOrigin Origin, int Tier, std::string &ErrorMessage) const;

	// One node of the tree: the node's own targetEffectID and nodeType. A node with no
	// effect - every age gate - buys nothing and returns true, and a Ship effect
	// returns true with Bought false, so a caller can walk the whole tree and the ship
	// systems go to the ship engine.
	bool ApplySkill(
	    EcsWorld &World, const std::string &CivilisationId, const SkillNode &Node, int Tier,
	    bool &Bought, std::string &ErrorMessage) const;

	// Every node of the tree with a civilisation effect, in sorted skill order. The two
	// columns add to the nodes of the tree: Applied counts the effects the walk wrote and
	// Skipped every node it did not write - the gate, which buys nothing, the ship
	// systems, which the ship engine fits, and the nodes the tree has bought no tier of,
	// which buy nothing yet. A walk that wrote nothing has applied 0 and skipped every
	// node of the tree, and a walk of the same tiers twice writes the same numbers. The
	// walk sets the caller's columns, and the ship engine writes them the same way; a
	// walk of a fleet writes the columns of the fleet, which is the sum of the walks of
	// its hulls.
	bool ApplyProgression(
	    EcsWorld &World, const ProgressionManager &Progression, const std::string &CivilisationId,
	    unsigned &Applied, unsigned &Skipped, std::string &ErrorMessage) const;

	// Takes every source of one skill off every civilisation store it touched, and
	// rebuilds the stores: the respec road, half of it. Returns the sources taken off,
	// counted as the ship engine counts them: a law that is law in two worlds is two
	// sources, one at each venue the law lands on, and the road that takes the law off
	// takes both of them and returns 2.
	unsigned UnapplySkill(EcsWorld &World, const std::string &SkillId, std::string &ErrorMessage) const;

	// The shared stores, one per civilisation, created on the first effect that needs
	// them.
	static std::string LaneStoreId(const std::string &CivilisationId);
	static std::string SurveyStoreId(const std::string &CivilisationId);
	static std::string YardStoreId(const std::string &CivilisationId);

	// The stores a civilisation's effects land on, in sorted id order.
	std::vector<std::string> MarketsOf(const EcsWorld &World, const std::string &CivilisationId) const;
	std::vector<std::string> BodiesOf(const EcsWorld &World, const std::string &CivilisationId) const;

	// The stats of one store: empty when the store does not exist.
	std::map<EffectStat, double> MarketStatsOf(
	    const EcsWorld &World, const std::string &VenueBodyId) const;
	std::map<EffectStat, double> BodyStatsOf(const EcsWorld &World, const std::string &BodyId) const;
	std::map<EffectStat, double> RouteStatsOf(
	    const EcsWorld &World, const std::string &CivilisationId) const;
	std::map<EffectStat, double> SurveyStatsOf(
	    const EcsWorld &World, const std::string &CivilisationId) const;
	std::map<EffectStat, double> YardStatsOf(
	    const EcsWorld &World, const std::string &CivilisationId) const;

	// One stat of one store, 0.0 when the store has never seen it: the number a
	// marketplace prices its fee by. The store is named by its own id - the venue's body
	// id for a market, a body id for a world, LaneStoreId, SurveyStoreId and YardStoreId
	// for the three stores a civilisation buys once for itself - and not by the
	// civilisation, because the venue of a world is the world, and a caller that has the
	// store in hand has the id of the store, not the id of the civilisation that owns it.
	double ReadStat(
	    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId, EffectStat Stat) const;

	// The effects a civilisation has bought, as lines for the UI: one per store it
	// owns, each line naming the effects that store flies, the tiers the tree bought of
	// them, and the stats they sum to. A civilisation that owns no venue, no world and
	// has never bought a lane, a survey or a yard has no store to read and no line, and
	// the answer is the empty string: a read-out of a civilisation the world has never
	// heard of prints nothing.
	std::string DescribeCivilisation(const EcsWorld &World, const std::string &CivilisationId) const;

  private:
	// The stats a store carries, keyed by the store's own id. The component is a cache
	// of the sources, written by WriteStore, so reading a store is reading its cache.
	template <typename Component>
	static std::map<EffectStat, double> StoreStats(const EcsWorld &World, const std::string &StoreId)
	{
		const Component *effects = World.Get<Component>(StoreId);

		return effects != nullptr ? effects->values : std::map<EffectStat, double>{};
	}

	// The stores of a domain for a civilisation, with the message every entry point
	// needs when the civilisation owns none of them: an effect cannot land on a venue
	// that was never opened.
	bool ResolveStores(
	    EcsWorld &World, EffectDomain Domain, const std::string &CivilisationId,
	    const std::string &EffectId, std::vector<std::string> &Stores, std::string &ErrorMessage) const;

	// Every source on a store, keyed by effect id, which is the order the modifiers
	// are applied in.
	std::map<std::string, EffectSource> SourcesOn(
	    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId) const;

	// The store's stats, written from its sources.
	bool WriteStore(
	    EcsWorld &World, EffectDomain Domain, const std::string &StoreId,
	    std::string &ErrorMessage) const;

	// One store's read-out: the effects it flies, by the name the catalogue gives them
	// and the tier the tree bought, and the stats they sum to. A store that flies no
	// effect prints the message the caller brings, because "no market effects bought" and
	// "no route effects bought" are answers a UI shows as they are.
	std::string DescribeStore(
	    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId,
	    const std::map<EffectStat, double> &Stats, const char *NoEffectsMessage) const;

	EffectCatalogue catalogue;
};
} // namespace CivSimCore
