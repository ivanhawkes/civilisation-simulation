#include "Effects/CivilisationEffects.h"
#include <set>
#include <sstream>
#include <string>
#include <utility>


namespace ManifestCore
{
CivilisationEffectEngine::CivilisationEffectEngine(EffectCatalogue Catalogue)
    : catalogue(std::move(Catalogue))
{
}


const EffectCatalogue &CivilisationEffectEngine::Catalogue() const
{
	return catalogue;
}


void CivilisationEffectEngine::SetCatalogue(EffectCatalogue Catalogue)
{
	catalogue = std::move(Catalogue);
}


std::string CivilisationEffectEngine::LaneStoreId(const std::string &CivilisationId)
{
	return "Lanes-" + CivilisationId;
}


std::string CivilisationEffectEngine::SurveyStoreId(const std::string &CivilisationId)
{
	return "Survey-" + CivilisationId;
}


std::string CivilisationEffectEngine::YardStoreId(const std::string &CivilisationId)
{
	return "Yard-" + CivilisationId;
}


std::vector<std::string> CivilisationEffectEngine::MarketsOf(
    const EcsWorld &World, const std::string &CivilisationId) const
{
	std::vector<std::string> markets;

	const auto view = World.View<MarketplaceInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const MarketplaceInfo *market = World.Get<MarketplaceInfo>(entity);

		if (market == nullptr || (!CivilisationId.empty() && market->civilisationId != CivilisationId))
		{
			continue;
		}

		markets.push_back(market->marketId);
	}

	std::sort(markets.begin(), markets.end());

	return markets;
}


std::vector<std::string> CivilisationEffectEngine::BodiesOf(
    const EcsWorld &World, const std::string &CivilisationId) const
{
	std::vector<std::string> bodies;

	const auto view = World.View<BodyInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const BodyInfo *body = World.Get<BodyInfo>(entity);

		if (body == nullptr || (!CivilisationId.empty() && body->civilisationId != CivilisationId))
		{
			continue;
		}

		bodies.push_back(body->bodyId);
	}

	std::sort(bodies.begin(), bodies.end());

	return bodies;
}


bool CivilisationEffectEngine::Apply(
    EcsWorld &World, const std::string &CivilisationId, const std::string &SkillId,
    const std::string &EffectId, EffectOrigin Origin, int Tier, std::string &ErrorMessage) const
{
	if (CivilisationId.empty())
	{
		ErrorMessage = "CivilisationEffectEngine: the civilisationId is empty; these effects land on a "
		               "civilisation";

		return false;
	}

	if (SkillId.empty())
	{
		ErrorMessage = "Civilisation '" + CivilisationId +
		               "': the skillId is empty; an effect names the node that bought it";

		return false;
	}

	if (EffectId.empty())
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': the effectId is empty";

		return false;
	}

	if (Tier <= 0)
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': '" + EffectId + "' at tier " +
		               std::to_string(Tier) + "; a tier is 1 or above";

		return false;
	}

	const EffectDefinition *definition = catalogue.Find(EffectId);

	if (definition == nullptr)
	{
		ErrorMessage =
		    "Civilisation '" + CivilisationId + "': the catalogue defines no effect '" + EffectId + "'";

		return false;
	}

	// The two engines share a catalogue, so a caller that walks the whole tree hands
	// the ship systems here as often as it hands the crafts here. The answer is a
	// refusal that names the engine that takes them.
	if (definition->domain == EffectDomain::Ship)
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': '" + EffectId +
		               "' is a Ship effect; ShipEffectEngine fits it to "
		               "a hull, and a civilisation carries no hull";

		return false;
	}

	std::vector<std::string> stores;

	if (!ResolveStores(World, definition->domain, CivilisationId, EffectId, stores, ErrorMessage))
	{
		return false;
	}

	for (const auto &store : stores)
	{
		// One source per civilisation, per store and per effect: a civilisation that
		// owns three worlds and buys a tribunal buys one tribunal at each of them, and
		// each venue reads its own.
		const std::string sourceId = CivilisationId + ":" + store + ":" + EffectId;

		EcsWorld::Entity source = EcsWorld::Null;

		if (World.Has(sourceId))
		{
			source = World.Find(sourceId);

			EffectSource *existing = World.Get<EffectSource>(source);

			if (existing == nullptr)
			{
				ErrorMessage = "Civilisation '" + CivilisationId + "': '" + sourceId +
				               "' exists and carries no EffectSource";

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

		World.Put<EffectOn>(source, EffectOn{store, definition->domain});
	}

	for (const auto &store : stores)
	{
		if (!WriteStore(World, definition->domain, store, ErrorMessage))
		{
			return false;
		}
	}

	return true;
}


bool CivilisationEffectEngine::ApplySkill(
    EcsWorld &World, const std::string &CivilisationId, const SkillNode &Node, int Tier, bool &Bought,
    std::string &ErrorMessage) const
{
	Bought = false;

	if (Node.skillId.empty())
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': a skill node with no skillId";

		return false;
	}

	// The gates carry no targetEffectID, and the ship systems belong to the hulls: in
	// both cases the node is bought and this civilisation carries no effect of it,
	// which is not a failure, because the tree asked for nothing here.
	if (Node.targetEffectID.empty())
	{
		return true;
	}

	const EffectDefinition *definition = catalogue.Find(Node.targetEffectID);

	if (definition == nullptr)
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': node '" + Node.skillId +
		               "' asks for effect '" + Node.targetEffectID +
		               "', which the catalogue does not define";

		return false;
	}

	if (definition->domain == EffectDomain::Ship)
	{
		return true;
	}

	const EffectOrigin origin =
	    Node.nodeType == NodeType::Tech ? EffectOrigin::Technology : EffectOrigin::Skill;
	const bool ok =
	    Apply(World, CivilisationId, Node.skillId, Node.targetEffectID, origin, Tier, ErrorMessage);

	Bought = ok;

	return ok;
}


bool CivilisationEffectEngine::ApplyProgression(
    EcsWorld &World, const ProgressionManager &Progression, const std::string &CivilisationId,
    unsigned &Applied, unsigned &Skipped, std::string &ErrorMessage) const
{
	// The walk writes the caller's two columns: they are set, not added to, and a caller
	// that arrives with a number in hand is told what this walk did, not what the last
	// walk did. The ship engine writes the same way, and a walk of a fleet writes the
	// columns of the fleet, which is the sum of the walks of its hulls.
	Applied = 0;
	Skipped = 0;

	if (CivilisationId.empty())
	{
		ErrorMessage = "CivilisationEffectEngine: the civilisationId is empty";

		return false;
	}

	for (const auto &entry : Progression.GetRegistry())
	{
		const SkillNode &node = entry.second;
		const int tier = Progression.GetCurrentTier(node.skillId);

		// A node the tree has bought no tier of buys nothing, and the walk walked past
		// it: the node is of the tree, the store flies no effect for it, and the count of
		// the nodes the walk did not write counts it. The two columns add to the nodes of
		// the tree, on a hull and in a civilisation store alike, which is the reading a UI
		// shows: the tree has nine nodes, the venue bought five of them, and the four it
		// did not are the gate, the system of the hull and the two nodes no tier has
		// reached.
		if (tier <= 0)
		{
			++Skipped;

			continue;
		}

		bool bought = false;

		if (!ApplySkill(World, CivilisationId, node, tier, bought, ErrorMessage))
		{
			ErrorMessage =
			    "Civilisation '" + CivilisationId + "', node '" + node.skillId + "': " + ErrorMessage;

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


unsigned CivilisationEffectEngine::UnapplySkill(
    EcsWorld &World, const std::string &SkillId, std::string &ErrorMessage) const
{
	std::vector<std::string> doomed;
	std::set<std::pair<EffectDomain, std::string>> touched;

	const auto view = World.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = World.Get<EffectSource>(entity);
		const EffectOn *on = World.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || source->skillId != SkillId ||
		    on->domain == EffectDomain::Ship)
		{
			continue;
		}

		const std::string name = World.NameOf(entity);

		if (name.empty())
		{
			continue;
		}

		doomed.push_back(name);
		touched.insert({on->domain, on->targetId});
	}

	for (const auto &name : doomed)
	{
		if (!World.Destroy(name, ErrorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	for (const auto &store : touched)
	{
		if (!WriteStore(World, store.first, store.second, ErrorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	return static_cast<unsigned>(doomed.size());
}


std::map<EffectStat, double> CivilisationEffectEngine::MarketStatsOf(
    const EcsWorld &World, const std::string &VenueBodyId) const
{
	return StoreStats<MarketEffects>(World, VenueBodyId);
}


std::map<EffectStat, double> CivilisationEffectEngine::BodyStatsOf(
    const EcsWorld &World, const std::string &BodyId) const
{
	return StoreStats<BodyEffects>(World, BodyId);
}


std::map<EffectStat, double> CivilisationEffectEngine::RouteStatsOf(
    const EcsWorld &World, const std::string &CivilisationId) const
{
	return StoreStats<RouteEffects>(World, LaneStoreId(CivilisationId));
}


std::map<EffectStat, double> CivilisationEffectEngine::SurveyStatsOf(
    const EcsWorld &World, const std::string &CivilisationId) const
{
	return StoreStats<SurveyEffects>(World, SurveyStoreId(CivilisationId));
}


std::map<EffectStat, double> CivilisationEffectEngine::YardStatsOf(
    const EcsWorld &World, const std::string &CivilisationId) const
{
	return StoreStats<YardEffects>(World, YardStoreId(CivilisationId));
}


double CivilisationEffectEngine::ReadStat(
    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId, EffectStat Stat) const
{
	// The store named by its own id, read as the store's component: the three shared
	// stores are read through the StatsOf helpers, which spell the store id from the
	// civilisation, and a caller that arrives with the store id in hand - a marketplace
	// pricing the fee of its own venue - has the store, not the civilisation, and reads
	// it directly. Spelling the store id twice would read a store that does not exist,
	// and 0.0 is the answer a missing store gives.
	switch (Domain)
	{
		case EffectDomain::Market:
			return ReadEffectStat(StoreStats<MarketEffects>(World, StoreId), Stat);
		case EffectDomain::Body: return ReadEffectStat(StoreStats<BodyEffects>(World, StoreId), Stat);
		case EffectDomain::Route: return ReadEffectStat(StoreStats<RouteEffects>(World, StoreId), Stat);
		case EffectDomain::Survey:
			return ReadEffectStat(StoreStats<SurveyEffects>(World, StoreId), Stat);
		case EffectDomain::Yard: return ReadEffectStat(StoreStats<YardEffects>(World, StoreId), Stat);
		case EffectDomain::Ship:
			// A ship stat is a hull's, not a civilisation store's, and the hull answers
			// through ShipEffectEngine::GetStat.
			return 0.0;
	}

	return 0.0;
}


std::string CivilisationEffectEngine::DescribeStore(
    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId,
    const std::map<EffectStat, double> &Stats, const char *NoEffectsMessage) const
{
	// The sources keyed by effect id, which is the order the modifiers are applied in,
	// and so the order the read-out prints them in: a panel that showed the same tree
	// twice has to show the same lines, and the store's cache is a map with no order of
	// its own to print.
	const std::map<std::string, EffectSource> sources = SourcesOn(World, Domain, StoreId);

	if (sources.empty())
	{
		return NoEffectsMessage;
	}

	std::ostringstream out;
	bool first = true;

	for (const auto &entry : sources)
	{
		if (!first)
		{
			out << ", ";
		}

		// The effect by the name the catalogue spells, and the tier the tree bought of
		// it: "Cargo Insurance x2". The read-out is for a person, so it prints the words
		// the catalogue gives the effect and not the id a targetEffectID carries; a
		// source of an effect the catalogue has forgotten prints the id, because the store
		// flies it and the read-out prints what the store flies.
		const EffectDefinition *definition = catalogue.Find(entry.first);
		const std::string name = definition != nullptr && !definition->displayName.empty()
		                             ? definition->displayName
		                             : entry.first;

		out << name << " x" << entry.second.tier;
		first = false;
	}

	// The stats the effects sum to, in the stat-table order the stats print in, in
	// brackets after the effects: the effects are what the civilisation bought and the
	// stats are what the venue charges, and a store answers both questions at once.
	const std::string stats = DescribeEffectStats(Stats);

	if (!stats.empty())
	{
		out << " (" << stats << ")";
	}

	return out.str();
}


std::string CivilisationEffectEngine::DescribeCivilisation(
    const EcsWorld &World, const std::string &CivilisationId) const
{
	const std::vector<std::string> markets = MarketsOf(World, CivilisationId);
	const std::vector<std::string> bodies = BodiesOf(World, CivilisationId);
	const std::string lanes = LaneStoreId(CivilisationId);
	const std::string survey = SurveyStoreId(CivilisationId);
	const std::string yards = YardStoreId(CivilisationId);

	// The stores the civilisation owns: the venues it has opened and the worlds it has
	// made, and the three shared stores it has bought an effect of - a lane, a survey
	// and a yard store exists because an effect of that domain landed on it, and a
	// civilisation that has bought no route effect has no lane store.
	const std::size_t owned =
	    markets.size() + bodies.size() + static_cast<std::size_t>(World.Has(lanes)) +
	    static_cast<std::size_t>(World.Has(survey)) + static_cast<std::size_t>(World.Has(yards));

	// A civilisation with no store has nothing to read out, and the read-out of a
	// civilisation the world has never heard of prints nothing: a UI that asks for a
	// world that is not there is shown an empty panel, not a panel that says "no route
	// effects bought" three times.
	if (owned == 0u)
	{
		return {};
	}

	std::ostringstream out;

	out << "Civilisation " << CivilisationId << "\n";

	for (const auto &venue : markets)
	{
		out << "  Market " << venue << ": "
		    << DescribeStore(
		           World, EffectDomain::Market, venue, MarketStatsOf(World, venue),
		           "no market effects bought")
		    << "\n";
	}

	for (const auto &body : bodies)
	{
		out << "  Body " << body << ": "
		    << DescribeStore(
		           World, EffectDomain::Body, body, BodyStatsOf(World, body), "no body effects bought")
		    << "\n";
	}

	if (World.Has(lanes))
	{
		out << "  Lanes " << lanes << ": "
		    << DescribeStore(
		           World, EffectDomain::Route, lanes, RouteStatsOf(World, CivilisationId),
		           "no route effects bought")
		    << "\n";
	}

	if (World.Has(survey))
	{
		out << "  Survey " << survey << ": "
		    << DescribeStore(
		           World, EffectDomain::Survey, survey, SurveyStatsOf(World, CivilisationId),
		           "no survey effects bought")
		    << "\n";
	}

	if (World.Has(yards))
	{
		out << "  Yards " << yards << ": "
		    << DescribeStore(
		           World, EffectDomain::Yard, yards, YardStatsOf(World, CivilisationId),
		           "no yard effects bought")
		    << "\n";
	}

	return out.str();
}


bool CivilisationEffectEngine::ResolveStores(
    EcsWorld &World, EffectDomain Domain, const std::string &CivilisationId,
    const std::string &EffectId, std::vector<std::string> &Stores, std::string &ErrorMessage) const
{
	Stores.clear();

	switch (Domain)
	{
		case EffectDomain::Market:
		{
			Stores = MarketsOf(World, CivilisationId);

			if (Stores.empty())
			{
				ErrorMessage =
				    "Civilisation '" + CivilisationId + "': '" + EffectId +
				    "' is a Market effect and the civilisation owns no market; open a venue first";

				return false;
			}

			return true;
		}

		case EffectDomain::Body:
		{
			Stores = BodiesOf(World, CivilisationId);

			if (Stores.empty())
			{
				ErrorMessage =
				    "Civilisation '" + CivilisationId + "': '" + EffectId +
				    "' is a Body effect and the civilisation owns no body; create a world first";

				return false;
			}

			return true;
		}

		case EffectDomain::Route:
		case EffectDomain::Survey:
		case EffectDomain::Yard:
		{
			const std::string store = Domain == EffectDomain::Route    ? LaneStoreId(CivilisationId)
			                          : Domain == EffectDomain::Survey ? SurveyStoreId(CivilisationId)
			                                                           : YardStoreId(CivilisationId);

			if (!World.Has(store) && !World.Create(store, ErrorMessage))
			{
				return false;
			}

			// The store is a fact about a civilisation, and the component is the fact: a
			// civilisation with no route effects has no RouteEffects and the store is an
			// empty id, not a component of zeroes.
			if (Domain == EffectDomain::Route)
			{
				World.Put<RouteEffects>(store, RouteEffects{});
			}
			else if (Domain == EffectDomain::Survey)
			{
				World.Put<SurveyEffects>(store, SurveyEffects{});
			}
			else
			{
				World.Put<YardEffects>(store, YardEffects{});
			}

			Stores.push_back(store);

			return true;
		}

		case EffectDomain::Ship:
			ErrorMessage = "'" + EffectId +
			               "' is a Ship effect: Apply refuses it, and the caller has the catalogue";
			return false;
	}

	ErrorMessage = "'" + EffectId + "' names no store";

	return false;
}


std::map<std::string, EffectSource> CivilisationEffectEngine::SourcesOn(
    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId) const
{
	std::map<std::string, EffectSource> sources;

	const auto view = World.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = World.Get<EffectSource>(entity);
		const EffectOn *on = World.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || on->targetId != StoreId || on->domain != Domain)
		{
			continue;
		}

		sources[source->effectId] = *source;
	}

	return sources;
}


bool CivilisationEffectEngine::WriteStore(
    EcsWorld &World, EffectDomain Domain, const std::string &StoreId, std::string &ErrorMessage) const
{
	if (!World.Has(StoreId))
	{
		ErrorMessage =
		    "CivilisationEffectEngine: the store '" + StoreId + "' is not an entity in this world";

		return false;
	}

	std::map<EffectStat, double> stats;

	// The sources keyed by effect id: the key order is the order the modifiers are
	// applied in, and a signed modifier - the risk of a venue, where the law lowers it
	// and the pirates raise it - has to sum in one fixed order to sum to one number.
	for (const auto &entry : SourcesOn(World, Domain, StoreId))
	{
		const EffectDefinition *definition = catalogue.Find(entry.first);

		if (definition == nullptr)
		{
			continue;
		}

		for (const auto &modifier : definition->modifiers)
		{
			ApplyEffectModifiers(stats, modifier, entry.second.tier);
		}
	}

	switch (Domain)
	{
		case EffectDomain::Market:
			return World.Put<MarketEffects>(StoreId, MarketEffects{stats}) != nullptr;
		case EffectDomain::Body: return World.Put<BodyEffects>(StoreId, BodyEffects{stats}) != nullptr;
		case EffectDomain::Route:
			return World.Put<RouteEffects>(StoreId, RouteEffects{stats}) != nullptr;
		case EffectDomain::Survey:
			return World.Put<SurveyEffects>(StoreId, SurveyEffects{stats}) != nullptr;
		case EffectDomain::Yard: return World.Put<YardEffects>(StoreId, YardEffects{stats}) != nullptr;
		case EffectDomain::Ship:
			ErrorMessage =
			    "CivilisationEffectEngine: a Ship store is a hull, and ShipEffectEngine writes it";

			return false;
	}

	ErrorMessage = "CivilisationEffectEngine: no store for the domain";

	return false;
}
} // namespace ManifestCore
