#pragma once

#include "Core/SimulationConfig.h"
#include "Effects/EffectTypes.h"
#include <cstddef>
#include <map>
#include <string>
#include <vector>


namespace ManifestCore
{
// One effect: what a tier of it is worth, and which store it lands on.
struct EffectDefinition
{
	// The id a `targetEffectID` in the skill tree names, for example "ShieldFront",
	// "DockingLicence", "Drydock". It is the join between the tree and the engine.
	std::string effectId;

	EffectDomain domain{EffectDomain::Ship};

	std::string displayName;
	std::string description;

	// What one tier buys. An effect usually moves one stat, and an effect that makes
	// a civilisation safer at market moves two: it lowers the risk of the venue and
	// raises the depth the venue can absorb.
	std::vector<EffectModifier> modifiers;
};

// The effects the engine knows, keyed by effect id.
//
// The registry is a std::map keyed by id, so iteration order is the same on every
// platform, the way ProgressionManager keeps the skill tree.
//
// The catalogue is the code side of the join the data declares: data/skill-tree/*.yaml
// names the effects by id and says nothing about what a tier is worth, so the numbers
// live here, next to the stat table that names the units they count in.
class EffectCatalogue
{
  public:
	// Registers one effect. Rejects an empty id, an effect with no modifier, a
	// modifier of zero, a stat that belongs to another domain, and a duplicate id.
	bool Register(const EffectDefinition &Definition, std::string &ErrorMessage);

	// Registers every effect and then checks the whole catalogue. Replaces any state
	// an earlier call built.
	bool Build(const std::vector<EffectDefinition> &Definitions, std::string &ErrorMessage);

	void Clear();

	bool Has(const std::string &EffectId) const;

	// nullptr for an effect the catalogue does not define.
	const EffectDefinition *Find(const std::string &EffectId) const;

	// Effect ids in sorted order.
	std::vector<std::string> EffectIds() const;

	// Effect ids of one domain, in sorted order.
	std::vector<std::string> EffectIdsInDomain(EffectDomain Domain) const;

	std::size_t EffectCount() const;

	// True when every effect lands on the store its domain names, that is when every
	// modifier of every effect has a stat of the effect's own domain.
	bool Validate(std::string &ErrorMessage) const;

	// The join with the tree: the effects the tree asks for and the catalogue does
	// not define, in sorted order. A node with an empty `targetEffectID` - an age gate
	// - asks for no effect, so it is not missing.
	std::vector<std::string> EffectsMissingFrom(const Skills &Tree) const;

	// The effects this catalogue defines that no node in the tree buys, in sorted
	// order. Not an error: a catalogue can be bigger than one tree.
	std::vector<std::string> EffectsUnusedBy(const Skills &Tree) const;

	// True when the catalogue covers the tree: every non-empty `targetEffectID` in it
	// is defined here.
	bool CoversTree(const Skills &Tree, std::string &ErrorMessage) const;

	// The catalogue the shipped data drives: one effect for every non-null
	// `targetEffectID` in data/skill-tree - the 25 systems of the long-haul trader, the
	// 4 yards, the 4 lanes, the 4 survey claims, the 11 market effects of the law, the
	// pirates and the trade offices, the 4 habitats and the 2 services.
	static EffectCatalogue Default();

	// The effects of one domain, for a game mode that ships only some of them.
	static std::vector<EffectDefinition> ShipEffects();
	static std::vector<EffectDefinition> YardEffects();
	static std::vector<EffectDefinition> RouteEffects();
	static std::vector<EffectDefinition> SurveyEffects();
	static std::vector<EffectDefinition> MarketEffects();
	static std::vector<EffectDefinition> BodyEffects();

  private:
	// Keyed by effect id, so the registry iterates in sorted order on every platform.
	std::map<std::string, EffectDefinition> registry;
};
} // namespace ManifestCore
