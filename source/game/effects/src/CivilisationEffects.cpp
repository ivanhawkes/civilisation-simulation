#include <manifest/effects/CivilisationEffects.h>

#include <format>

#include <set>
#include <sstream>
#include <string>
#include <utility>


namespace Manifest::Core
{
CivilisationEffectEngine::CivilisationEffectEngine(EffectCatalogue catalogue)
    : m_catalogue(std::move(catalogue))
{
}


const EffectCatalogue &CivilisationEffectEngine::Catalogue() const
{
	return m_catalogue;
}


void CivilisationEffectEngine::SetCatalogue(EffectCatalogue catalogue)
{
	m_catalogue = std::move(catalogue);
}


std::string CivilisationEffectEngine::LaneStoreId(const std::string &civilisationId)
{
	return "Lanes-" + civilisationId;
}


std::string CivilisationEffectEngine::SurveyStoreId(const std::string &civilisationId)
{
	return "Survey-" + civilisationId;
}


std::string CivilisationEffectEngine::YardStoreId(const std::string &civilisationId)
{
	return "Yard-" + civilisationId;
}


std::vector<std::string> CivilisationEffectEngine::MarketsOf(
    const EcsWorld &world, const std::string &civilisationId) const
{
	std::vector<std::string> markets;

	const auto view = world.View<MarketplaceInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *market = world.Get<MarketplaceInfo>(entity);

		if (market == nullptr || (!civilisationId.empty() && market->civilisationId != civilisationId))
		{
			continue;
		}

		markets.push_back(market->marketId);
	}

	std::sort(markets.begin(), markets.end());

	return markets;
}


std::vector<std::string> CivilisationEffectEngine::BodiesOf(
    const EcsWorld &world, const std::string &civilisationId) const
{
	std::vector<std::string> bodies;

	const auto view = world.View<BodyInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *body = world.Get<BodyInfo>(entity);

		if (body == nullptr || (!civilisationId.empty() && body->civilisationId != civilisationId))
		{
			continue;
		}

		bodies.push_back(body->bodyId);
	}

	std::sort(bodies.begin(), bodies.end());

	return bodies;
}


bool CivilisationEffectEngine::Apply(
    EcsWorld &world, const std::string &civilisationId, const std::string &skillId,
    const std::string &effectId, EffectOrigin origin, int tier, std::string &errorMessage) const
{
	if (civilisationId.empty())
	{
		errorMessage = "CivilisationEffectEngine: the civilisationId is empty; these effects land on a "
		               "civilisation";

		return false;
	}

	if (skillId.empty())
	{
		errorMessage = std::format(
		    "Civilisation '{}': the skillId is empty; an effect names the node that bought it",
		    civilisationId);

		return false;
	}

	if (effectId.empty())
	{
		errorMessage = std::format("Civilisation '{}': the effectId is empty", civilisationId);

		return false;
	}

	if (tier <= 0)
	{
		errorMessage = std::format(
		    "Civilisation '{}': '{}' at tier {}; a tier is 1 or above", civilisationId, effectId,
		    std::to_string(tier));

		return false;
	}

	const EffectDefinition *definition = m_catalogue.Find(effectId);

	if (definition == nullptr)
	{
		errorMessage = std::format(
		    "Civilisation '{}': the catalogue defines no effect '{}'", civilisationId, effectId);

		return false;
	}

	// The two engines share a catalogue, so a caller that walks the whole tree hands
	// the ship systems here as often as it hands the crafts here. The answer is a
	// refusal that names the engine that takes them.
	if (definition->domain == EffectDomain::Ship)
	{
		errorMessage = std::format(
		    "Civilisation '{}': '{}' is a Ship effect; ShipEffectEngine fits it to a hull, and a "
		    "civilisation carries no hull",
		    civilisationId, effectId);

		return false;
	}

	std::vector<std::string> stores;

	if (!ResolveStores(world, definition->domain, civilisationId, effectId, stores, errorMessage))
	{
		return false;
	}

	for (const auto &store : stores)
	{
		// One source per civilisation, per store and per effect: a civilisation that
		// owns three worlds and buys a tribunal buys one tribunal at each of them, and
		// each venue reads its own.
		const std::string sourceId = std::format("{}:{}:{}", civilisationId, store, effectId);

		EcsWorld::Entity source = EcsWorld::Null;

		if (world.Has(sourceId))
		{
			source = world.Find(sourceId);

			auto *existing = world.Get<EffectSource>(source);

			if (existing == nullptr)
			{
				errorMessage = std::format(
				    "Civilisation '{}': '{}' exists and carries no EffectSource", civilisationId,
				    sourceId);

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

		world.Put<EffectOn>(source, EffectOn{store, definition->domain});
	}

	for (const auto &store : stores)
	{
		if (!WriteStore(world, definition->domain, store, errorMessage))
		{
			return false;
		}
	}

	return true;
}


bool CivilisationEffectEngine::ApplySkill(
    EcsWorld &world, const std::string &civilisationId, const SkillNode &node, int tier, bool &bought,
    std::string &errorMessage) const
{
	bought = false;

	if (node.skillId.empty())
	{
		errorMessage = std::format("Civilisation '{}': a skill node with no skillId", civilisationId);

		return false;
	}

	// The gates carry no targetEffectID, and the ship systems belong to the hulls: in
	// both cases the node is bought and this civilisation carries no effect of it,
	// which is not a failure, because the tree asked for nothing here.
	if (node.targetEffectID.empty())
	{
		return true;
	}

	const EffectDefinition *definition = m_catalogue.Find(node.targetEffectID);

	if (definition == nullptr)
	{
		errorMessage = std::format(
		    "Civilisation '{}': node '{}' asks for effect '{}', which the catalogue does not define",
		    civilisationId, node.skillId, node.targetEffectID);

		return false;
	}

	if (definition->domain == EffectDomain::Ship)
	{
		return true;
	}

	const EffectOrigin origin =
	    node.nodeType == NodeType::Tech ? EffectOrigin::Technology : EffectOrigin::Skill;
	const bool ok =
	    Apply(world, civilisationId, node.skillId, node.targetEffectID, origin, tier, errorMessage);

	bought = ok;

	return ok;
}


bool CivilisationEffectEngine::ApplyProgression(
    EcsWorld &world, const ProgressionManager &progression, const std::string &civilisationId,
    unsigned &applied, unsigned &skipped, std::string &errorMessage) const
{
	// The walk writes the caller's two columns: they are set, not added to, and a caller
	// that arrives with a number in hand is told what this walk did, not what the last
	// walk did. The ship engine writes the same way, and a walk of a fleet writes the
	// columns of the fleet, which is the sum of the walks of its hulls.
	applied = 0;
	skipped = 0;

	if (civilisationId.empty())
	{
		errorMessage = "CivilisationEffectEngine: the civilisationId is empty";

		return false;
	}

	for (const auto &entry : progression.GetRegistry())
	{
		const SkillNode &node = entry.second;
		const int tier = progression.GetCurrentTier(node.skillId);

		// A node the tree has bought no tier of buys nothing, and the walk walked past
		// it: the node is of the tree, the store flies no effect for it, and the count of
		// the nodes the walk did not write counts it. The two columns add to the nodes of
		// the tree, on a hull and in a civilisation store alike, which is the reading a UI
		// shows: the tree has nine nodes, the venue bought five of them, and the four it
		// did not are the gate, the system of the hull and the two nodes no tier has
		// reached.
		if (tier <= 0)
		{
			++skipped;

			continue;
		}

		bool bought = false;

		if (!ApplySkill(world, civilisationId, node, tier, bought, errorMessage))
		{
			errorMessage = std::format(
			    "Civilisation '{}', node '{}': {}", civilisationId, node.skillId, errorMessage);

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


unsigned CivilisationEffectEngine::UnapplySkill(
    EcsWorld &world, const std::string &skillId, std::string &errorMessage) const
{
	std::vector<std::string> doomed;
	std::set<std::pair<EffectDomain, std::string>> touched;

	const auto view = world.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = world.Get<EffectSource>(entity);
		const EffectOn *on = world.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || source->skillId != skillId ||
		    on->domain == EffectDomain::Ship)
		{
			continue;
		}

		const std::string name = world.NameOf(entity);

		if (name.empty())
		{
			continue;
		}

		doomed.push_back(name);
		touched.insert({on->domain, on->targetId});
	}

	for (const auto &name : doomed)
	{
		if (!world.Destroy(name, errorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	for (const auto &store : touched)
	{
		if (!WriteStore(world, store.first, store.second, errorMessage))
		{
			return static_cast<unsigned>(doomed.size());
		}
	}

	return static_cast<unsigned>(doomed.size());
}


std::map<EffectStat, double> CivilisationEffectEngine::MarketStatsOf(
    const EcsWorld &world, const std::string &venueBodyId) const
{
	return StoreStats<MarketEffects>(world, venueBodyId);
}


std::map<EffectStat, double> CivilisationEffectEngine::BodyStatsOf(
    const EcsWorld &world, const std::string &bodyId) const
{
	return StoreStats<BodyEffects>(world, bodyId);
}


std::map<EffectStat, double> CivilisationEffectEngine::RouteStatsOf(
    const EcsWorld &world, const std::string &civilisationId) const
{
	return StoreStats<RouteEffects>(world, LaneStoreId(civilisationId));
}


std::map<EffectStat, double> CivilisationEffectEngine::SurveyStatsOf(
    const EcsWorld &world, const std::string &civilisationId) const
{
	return StoreStats<SurveyEffects>(world, SurveyStoreId(civilisationId));
}


std::map<EffectStat, double> CivilisationEffectEngine::YardStatsOf(
    const EcsWorld &world, const std::string &civilisationId) const
{
	return StoreStats<YardEffects>(world, YardStoreId(civilisationId));
}


double CivilisationEffectEngine::ReadStat(
    const EcsWorld &world, EffectDomain domain, const std::string &storeId, EffectStat stat) const
{
	// The store named by its own id, read as the store's component: the three shared
	// stores are read through the StatsOf helpers, which spell the store id from the
	// civilisation, and a caller that arrives with the store id in hand - a marketplace
	// pricing the fee of its own venue - has the store, not the civilisation, and reads
	// it directly. Spelling the store id twice would read a store that does not exist,
	// and 0.0 is the answer a missing store gives.
	switch (domain)
	{
		case EffectDomain::Market:
			return ReadEffectStat(StoreStats<MarketEffects>(world, storeId), stat);
		case EffectDomain::Body: return ReadEffectStat(StoreStats<BodyEffects>(world, storeId), stat);
		case EffectDomain::Route: return ReadEffectStat(StoreStats<RouteEffects>(world, storeId), stat);
		case EffectDomain::Survey:
			return ReadEffectStat(StoreStats<SurveyEffects>(world, storeId), stat);
		case EffectDomain::Yard: return ReadEffectStat(StoreStats<YardEffects>(world, storeId), stat);
		case EffectDomain::Ship:
			// A ship stat is a hull's, not a civilisation store's, and the hull answers
			// through ShipEffectEngine::GetStat.
			return 0.0;
	}

	return 0.0;
}


std::string CivilisationEffectEngine::DescribeStore(
    const EcsWorld &world, EffectDomain domain, const std::string &storeId,
    const std::map<EffectStat, double> &effectStats, const char *noEffectsMessage) const
{
	// The sources keyed by effect id, which is the order the modifiers are applied in,
	// and so the order the read-out prints them in: a panel that showed the same tree
	// twice has to show the same lines, and the store's cache is a map with no order of
	// its own to print.
	const std::map<std::string, EffectSource> sources = SourcesOn(world, domain, storeId);

	if (sources.empty())
	{
		return noEffectsMessage;
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
		const EffectDefinition *definition = m_catalogue.Find(entry.first);
		const std::string name = definition != nullptr && !definition->displayName.empty()
		                             ? definition->displayName
		                             : entry.first;

		out << name << " x" << entry.second.tier;
		first = false;
	}

	// The stats the effects sum to, in the stat-table order the stats print in, in
	// brackets after the effects: the effects are what the civilisation bought and the
	// stats are what the venue charges, and a store answers both questions at once.
	const std::string stats = DescribeEffectStats(effectStats);

	if (!stats.empty())
	{
		out << " (" << stats << ")";
	}

	return out.str();
}


std::string CivilisationEffectEngine::DescribeCivilisation(
    const EcsWorld &world, const std::string &civilisationId) const
{
	const std::vector<std::string> markets = MarketsOf(world, civilisationId);
	const std::vector<std::string> bodies = BodiesOf(world, civilisationId);
	const std::string lanes = LaneStoreId(civilisationId);
	const std::string survey = SurveyStoreId(civilisationId);
	const std::string yards = YardStoreId(civilisationId);

	// The stores the civilisation owns: the venues it has opened and the worlds it has
	// made, and the three shared stores it has bought an effect of - a lane, a survey
	// and a yard store exists because an effect of that domain landed on it, and a
	// civilisation that has bought no route effect has no lane store.
	const std::size_t owned =
	    markets.size() + bodies.size() + static_cast<std::size_t>(world.Has(lanes)) +
	    static_cast<std::size_t>(world.Has(survey)) + static_cast<std::size_t>(world.Has(yards));

	// A civilisation with no store has nothing to read out, and the read-out of a
	// civilisation the world has never heard of prints nothing: a UI that asks for a
	// world that is not there is shown an empty panel, not a panel that says "no route
	// effects bought" three times.
	if (owned == 0u)
	{
		return {};
	}

	std::ostringstream out;

	out << "Civilisation " << civilisationId << "\n";

	for (const auto &venue : markets)
	{
		out << "  Market " << venue << ": "
		    << DescribeStore(
		           world, EffectDomain::Market, venue, MarketStatsOf(world, venue),
		           "no market effects bought")
		    << "\n";
	}

	for (const auto &body : bodies)
	{
		out << "  Body " << body << ": "
		    << DescribeStore(
		           world, EffectDomain::Body, body, BodyStatsOf(world, body), "no body effects bought")
		    << "\n";
	}

	if (world.Has(lanes))
	{
		out << "  Lanes " << lanes << ": "
		    << DescribeStore(
		           world, EffectDomain::Route, lanes, RouteStatsOf(world, civilisationId),
		           "no route effects bought")
		    << "\n";
	}

	if (world.Has(survey))
	{
		out << "  Survey " << survey << ": "
		    << DescribeStore(
		           world, EffectDomain::Survey, survey, SurveyStatsOf(world, civilisationId),
		           "no survey effects bought")
		    << "\n";
	}

	if (world.Has(yards))
	{
		out << "  Yards " << yards << ": "
		    << DescribeStore(
		           world, EffectDomain::Yard, yards, YardStatsOf(world, civilisationId),
		           "no yard effects bought")
		    << "\n";
	}

	return out.str();
}


bool CivilisationEffectEngine::ResolveStores(
    EcsWorld &world, EffectDomain domain, const std::string &civilisationId,
    const std::string &effectId, std::vector<std::string> &stores, std::string &errorMessage) const
{
	stores.clear();

	switch (domain)
	{
		case EffectDomain::Market:
		{
			stores = MarketsOf(world, civilisationId);

			if (stores.empty())
			{
				errorMessage = std::format(
				    "Civilisation '{}': '{}' is a Market effect and the civilisation owns no market; "
				    "open a venue first",
				    civilisationId, effectId);

				return false;
			}

			return true;
		}

		case EffectDomain::Body:
		{
			stores = BodiesOf(world, civilisationId);

			if (stores.empty())
			{
				errorMessage = std::format(
				    "Civilisation '{}': '{}' is a Body effect and the civilisation owns no body; "
				    "create a world first",
				    civilisationId, effectId);

				return false;
			}

			return true;
		}

		case EffectDomain::Route:
		case EffectDomain::Survey:
		case EffectDomain::Yard:
		{
			const std::string store = domain == EffectDomain::Route    ? LaneStoreId(civilisationId)
			                          : domain == EffectDomain::Survey ? SurveyStoreId(civilisationId)
			                                                           : YardStoreId(civilisationId);

			if (!world.Has(store) && !world.Create(store, errorMessage))
			{
				return false;
			}

			// The store is a fact about a civilisation, and the component is the fact: a
			// civilisation with no route effects has no RouteEffects and the store is an
			// empty id, not a component of zeroes.
			if (domain == EffectDomain::Route)
			{
				world.Put<RouteEffects>(store, RouteEffects{});
			}
			else if (domain == EffectDomain::Survey)
			{
				world.Put<SurveyEffects>(store, SurveyEffects{});
			}
			else
			{
				world.Put<YardEffects>(store, YardEffects{});
			}

			stores.push_back(store);

			return true;
		}

		case EffectDomain::Ship:
			errorMessage = std::format(
			    "'{}' is a Ship effect: Apply refuses it, and the caller has the catalogue", effectId);
			return false;
	}

	errorMessage = std::format("'{}' names no store", effectId);

	return false;
}


std::map<std::string, EffectSource> CivilisationEffectEngine::SourcesOn(
    const EcsWorld &world, EffectDomain domain, const std::string &storeId) const
{
	std::map<std::string, EffectSource> sources;

	const auto view = world.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *source = world.Get<EffectSource>(entity);
		const auto *on = world.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || on->targetId != storeId || on->domain != domain)
		{
			continue;
		}

		sources[source->effectId] = *source;
	}

	return sources;
}


bool CivilisationEffectEngine::WriteStore(
    EcsWorld &world, EffectDomain domain, const std::string &storeId, std::string &errorMessage) const
{
	if (!world.Has(storeId))
	{
		errorMessage = std::format(
		    "CivilisationEffectEngine: the store '{}' is not an entity in this world", storeId);

		return false;
	}

	std::map<EffectStat, double> stats;

	// The sources keyed by effect id: the key order is the order the modifiers are
	// applied in, and a signed modifier - the risk of a venue, where the law lowers it
	// and the pirates raise it - has to sum in one fixed order to sum to one number.
	for (const auto &entry : SourcesOn(world, domain, storeId))
	{
		const EffectDefinition *definition = m_catalogue.Find(entry.first);

		if (definition == nullptr)
		{
			continue;
		}

		for (const auto &modifier : definition->modifiers)
		{
			ApplyEffectModifiers(stats, modifier, entry.second.tier);
		}
	}

	switch (domain)
	{
		case EffectDomain::Market:
			return world.Put<MarketEffects>(storeId, MarketEffects{stats}) != nullptr;
		case EffectDomain::Body: return world.Put<BodyEffects>(storeId, BodyEffects{stats}) != nullptr;
		case EffectDomain::Route:
			return world.Put<RouteEffects>(storeId, RouteEffects{stats}) != nullptr;
		case EffectDomain::Survey:
			return world.Put<SurveyEffects>(storeId, SurveyEffects{stats}) != nullptr;
		case EffectDomain::Yard: return world.Put<YardEffects>(storeId, YardEffects{stats}) != nullptr;
		case EffectDomain::Ship:
			errorMessage =
			    "CivilisationEffectEngine: a Ship store is a hull, and ShipEffectEngine writes it";

			return false;
	}

	errorMessage = "CivilisationEffectEngine: no store for the domain";

	return false;
}
} // namespace Manifest::Core
