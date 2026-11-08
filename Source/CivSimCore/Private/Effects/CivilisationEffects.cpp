#include "Effects/CivilisationEffects.h"
#include <set>
#include <sstream>
#include <string>
#include <utility>


namespace CivSimCore
{
CivilisationEffectEngine::CivilisationEffectEngine(EffectCatalogue Catalogue) : catalogue(std::move(Catalogue))
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


std::vector<std::string> CivilisationEffectEngine::MarketsOf(const EcsWorld &World, const std::string &CivilisationId) const
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


std::vector<std::string> CivilisationEffectEngine::BodiesOf(const EcsWorld &World, const std::string &CivilisationId) const
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
    EcsWorld &World, const std::string &CivilisationId, const std::string &SkillId, const std::string &EffectId,
    EffectOrigin Origin, int Tier, std::string &ErrorMessage) const
{
	if (CivilisationId.empty())
	{
		ErrorMessage = "CivilisationEffectEngine: the civilisationId is empty; these effects land on a civilisation";

		return false;
	}

	if (SkillId.empty())
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': the skillId is empty; an effect names the node that bought it";

		return false;
	}

	if (EffectId.empty())
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': the effectId is empty";

		return false;
	}

	if (Tier <= 0)
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': '" + EffectId + "' at tier " + std::to_string(Tier) +
		               "; a tier is 1 or above";

		return false;
	}

	const EffectDefinition *definition = catalogue.Find(EffectId);

	if (definition == nullptr)
	{
		ErrorMessage = "Civilisation '" + CivilisationId + "': the catalogue defines no effect '" + EffectId + "'";

		return false;
	}

	// The two engines share a catalogue, so a caller that walks the whole tree hands
	// the ship systems here as often as it hands the crafts here. The answer is a
	// refusal that names the engine that takes them.
	if (definition->domain == EffectDomain::Ship)
	{
		ErrorMessage =
		    "Civilisation '" + CivilisationId + "': '" + EffectId + "' is a Ship effect; ShipEffectEngine fits it to "
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
		ErrorMessage = "Civilisation '" + CivilisationId + "': node '" + Node.skillId + "' asks for effect '" +
		               Node.targetEffectID + "', which the catalogue does not define";

		return false;
	}

	if (definition->domain == EffectDomain::Ship)
	{
		return true;
	}

	const EffectOrigin origin =
	    Node.nodeType == NodeType::Tech ? EffectOrigin::Technology : EffectOrigin::Skill;
	const bool ok = Apply(World, CivilisationId, Node.skillId, Node.targetEffectID, origin, Tier, ErrorMessage);

	Bought = ok;

	return ok;
}


bool CivilisationEffectEngine::ApplyProgression(
    EcsWorld &World, const ProgressionManager &Progression, const std::string &CivilisationId, unsigned &Applied,
    unsigned &Skipped, std::string &ErrorMessage) const
{
	if (CivilisationId.empty())
	{
		ErrorMessage = "CivilisationEffectEngine: the civilisationId is empty";

		return false;
	}

	for (const auto &entry : Progression.GetRegistry())
	{
		const SkillNode &node = entry.second;
		const int tier = Progression.GetCurrentTier(node.skillId);

		if (tier <= 0)
		{
			++Skipped;

			continue;
		}

		bool bought = false;

		if (!ApplySkill(World, CivilisationId, node, tier, bought, ErrorMessage))
		{
			ErrorMessage = "Civilisation '" + CivilisationId + "', node '" + node.skillId + "': " + ErrorMessage;

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


unsigned CivilisationEffectEngine::UnapplySkill(EcsWorld &World, const std::string &SkillId, std::string &ErrorMessage) const
{
	std::vector<std::string> doomed;
	std::set<std::pair<EffectDomain, std::string>> touched;

	const auto view = World.View<EffectSource, EffectOn>();

	for (const EcsWorld::Entity entity : view)
	{
		const EffectSource *source = World.Get<EffectSource>(entity);
		const EffectOn *on = World.Get<EffectOn>(entity);

		if (source == nullptr || on == nullptr || source->skillId != SkillId || on->domain == EffectDomain::Ship)
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


std::map<EffectStat, double> CivilisationEffectEngine::MarketStatsOf(const EcsWorld &World, const std::string &VenueBodyId) const
{
	const MarketEffects *effects = World.Get<MarketEffects>(VenueBodyId);

	return effects != nullptr ? effects->values : std::map<EffectStat, double>{};
}


std::map<EffectStat, double> CivilisationEffectEngine::BodyStatsOf(const EcsWorld &World, const std::string &BodyId) const
{
	const BodyEffects *effects = World.Get<BodyEffects>(BodyId);

	return effects != nullptr ? effects->values : std::map<EffectStat, double>{};
}


std::map<EffectStat, double> CivilisationEffectEngine::RouteStatsOf(const EcsWorld &World, const std::string &CivilisationId) const
{
	const RouteEffects *effects = World.Get<RouteEffects>(LaneStoreId(CivilisationId));

	return effects != nullptr ? effects->values : std::map<EffectStat, double>{};
}


std::map<EffectStat, double> CivilisationEffectEngine::SurveyStatsOf(const EcsWorld &World, const std::string &CivilisationId) const
{
	const SurveyEffects *effects = World.Get<SurveyEffects>(SurveyStoreId(CivilisationId));

	return effects != nullptr ? effects->values : std::map<EffectStat, double>{};
}


std::map<EffectStat, double> CivilisationEffectEngine::YardStatsOf(const EcsWorld &World, const std::string &CivilisationId) const
{
	const YardEffects *effects = World.Get<YardEffects>(YardStoreId(CivilisationId));

	return effects != nullptr ? effects->values : std::map<EffectStat, double>{};
}


double CivilisationEffectEngine::ReadStat(
    const EcsWorld &World, EffectDomain Domain, const std::string &StoreId, EffectStat Stat) const
{
	switch (Domain)
	{
	case EffectDomain::Market:
		return ReadEffectStat(MarketStatsOf(World, StoreId), Stat);
	case EffectDomain::Body:
		return ReadEffectStat(BodyStatsOf(World, StoreId), Stat);
	case EffectDomain::Route:
		return ReadEffectStat(RouteStatsOf(World, StoreId), Stat);
	case EffectDomain::Survey:
		return ReadEffectStat(SurveyStatsOf(World, StoreId), Stat);
	case EffectDomain::Yard:
		return ReadEffectStat(YardStatsOf(World, StoreId), Stat);
	case EffectDomain::Ship:
		// A ship stat is a hull's, not a civilisation store's, and the hull answers
		// through ShipEffectEngine::GetStat.
		return 0.0;
	}

	return 0.0;
}


std::string CivilisationEffectEngine::DescribeCivilisation(const EcsWorld &World, const std::string &CivilisationId) const
{
	std::ostringstream out;

	out << "Civilisation " << CivilisationId << "\n";

	for (const auto &venue : MarketsOf(World, CivilisationId))
	{
		const std::string stats = DescribeEffectStats(MarketStatsOf(World, venue));

		out << "  Market " << venue << ": " << (stats.empty() ? "no market effects bought" : stats) << "\n";
	}

	for (const auto &body : BodiesOf(World, CivilisationId))
	{
		const std::string stats = DescribeEffectStats(BodyStatsOf(World, body));

		out << "  Body " << body << ": " << (stats.empty() ? "no body effects bought" : stats) << "\n";
	}

	const std::string lanes = DescribeEffectStats(RouteStatsOf(World, CivilisationId));

	out << "  Lanes " << LaneStoreId(CivilisationId) << ": " << (lanes.empty() ? "no route effects bought" : lanes)
	    << "\n";

	const std::string survey = DescribeEffectStats(SurveyStatsOf(World, CivilisationId));

	out << "  Survey " << SurveyStoreId(CivilisationId) << ": "
	    << (survey.empty() ? "no survey effects bought" : survey) << "\n";

	const std::string yards = DescribeEffectStats(YardStatsOf(World, CivilisationId));

	out << "  Yards " << YardStoreId(CivilisationId) << ": " << (yards.empty() ? "no yard effects bought" : yards);

	return out.str();
}


bool CivilisationEffectEngine::ResolveStores(
    EcsWorld &World, EffectDomain Domain, const std::string &CivilisationId, const std::string &EffectId,
    std::vector<std::string> &Stores, std::string &ErrorMessage) const
{
	Stores.clear();

	switch (Domain)
	{
	case EffectDomain::Market:
	{
		Stores = MarketsOf(World, CivilisationId);

		if (Stores.empty())
		{
			ErrorMessage = "Civilisation '" + CivilisationId + "': '" + EffectId +
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
			ErrorMessage = "Civilisation '" + CivilisationId + "': '" + EffectId +
		                   "' is a Body effect and the civilisation owns no body; create a world first";

			return false;
		}

		return true;
	}

	case EffectDomain::Route:
	case EffectDomain::Survey:
	case EffectDomain::Yard:
	{
		const std::string store =
		    Domain == EffectDomain::Route ? LaneStoreId(CivilisationId)
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
		ErrorMessage = "'" + EffectId + "' is a Ship effect: Apply refuses it, and the caller has the catalogue";
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
		ErrorMessage = "CivilisationEffectEngine: the store '" + StoreId + "' is not an entity in this world";

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
	case EffectDomain::Body:
		return World.Put<BodyEffects>(StoreId, BodyEffects{stats}) != nullptr;
	case EffectDomain::Route:
		return World.Put<RouteEffects>(StoreId, RouteEffects{stats}) != nullptr;
	case EffectDomain::Survey:
		return World.Put<SurveyEffects>(StoreId, SurveyEffects{stats}) != nullptr;
	case EffectDomain::Yard:
		return World.Put<YardEffects>(StoreId, YardEffects{stats}) != nullptr;
	case EffectDomain::Ship:
		ErrorMessage = "CivilisationEffectEngine: a Ship store is a hull, and ShipEffectEngine writes it";

		return false;
	}

	ErrorMessage = "CivilisationEffectEngine: no store for the domain";

	return false;
}
} // namespace CivSimCore
