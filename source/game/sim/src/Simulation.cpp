#include <manifest/sim/Simulation.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <format>
#include <limits>
#include <sstream>
#include <utility>

#include <jettison/core/Assert.h>
#include <jettison/math/Random.h>


namespace Manifest::Core
{
namespace
{
// Price model: a scarce good costs more, capped so one shortage cannot make a price
// explode. A world's own deficit pushes its local price up, its surplus pushes it
// down, and the gap between two worlds is the trade.
constexpr double kPriceElasticity = 0.5;
constexpr double kMinPriceFactor = 0.2;
constexpr double kMaxPriceFactor = 5.0;
constexpr double kLocalPriceBias = 0.9;
constexpr double kFreightRate = 0.08;
constexpr double kTradeFraction = 0.8;
constexpr unsigned kRoutesPerTick = 25;

// Demand weight for the goods a population consumes whatever its age makes of it: the
// essentials, the medicines and the knowledge goods. Medicine and education are demand,
// not a craft to be tiered: a world that can make antibiotics consumes them, a world
// that cannot buys them, and the second is the trade the model exists to find.
constexpr double kEssentialDemandWeight = 2.5;

constexpr const char *kSystemSuffixes[] = {"Prime",   "Alpha", "Beta", "Gamma", "Delta",
                                           "Epsilon", "Zeta",  "Eta",  "Theta", "Iota"};

// The categories whose goods a world wants at every age, made by that world or bought
// from one that can. The ids are the loaded categories, so a good in an unknown category
// is wanted at the ordinary rate.
constexpr const char *kEssentialCategories[] = {
    "CategoryWater",
    "CategoryConsumables",
    "CategoryMedicine",
    "CategoryPharmaceuticals",
    "CategoryMedicalEquipment",
    "CategoryDrugs",
    "CategorySeeds",
    "CategoryFuels",
    "CategoryInformation",
    "CategoryWriting"};

// Fisher-Yates with the engine's pinned distribution: std::shuffle is allowed to
// draw differently across standard libraries, even with the same generator.
template <typename T> void Shuffle(std::vector<T> &values, Jettison::Math::Random &random)
{
	JETTISON_VERIFY(
	    values.size() <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()));
	for (std::size_t remaining = values.size(); remaining > 1; --remaining)
	{
		const auto index =
		    static_cast<std::size_t>(random.Range(0, static_cast<std::int32_t>(remaining - 1)));
		std::swap(values[remaining - 1], values[index]);
	}
}

double Clamp(double value, double low, double high)
{
	return std::max(low, std::min(high, value));
}

double LookUp(const std::map<std::string, double> &values, const std::string &key)
{
	const auto entry = values.find(key);

	return entry != values.end() ? entry->second : 0.0;
}

std::string LowerCase(std::string text)
{
	std::transform(
	    text.begin(), text.end(), text.begin(),
	    [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

	return text;
}

std::string PaddedIndex(unsigned index)
{
	const std::string digits = std::to_string(index);

	return digits.size() >= 2 ? digits : "0" + digits;
}

bool IsEssentialDemand(const std::string &categoryId)
{
	for (const auto *category : kEssentialCategories)
	{
		if (categoryId == category)
		{
			return true;
		}
	}

	return false;
}

// The age each good belongs in, keyed by the age's ageId. The merged documents write
// the age link out of the age anchor each item carries, so the link is a plain field by
// the time the loader is done with it, and it is optional: a good with no age of its own
// is in range at every age.
std::map<std::string, std::size_t> BuildAgeRanks(const std::vector<AgeNode> &ages)
{
	std::map<std::string, std::size_t> ranks;

	for (std::size_t index = 0; index < ages.size(); ++index)
	{
		ranks[ages[index].ageId] = index;
	}

	return ranks;
}

// Finds a world by id ("Civ-03") or by name, case insensitive. -1 when unknown.
int FindCivilisationIndex(const std::vector<Civilisation> &civilisations, const std::string &idOrName)
{
	const std::string wanted = LowerCase(idOrName);

	for (std::size_t index = 0; index < civilisations.size(); ++index)
	{
		if (LowerCase(civilisations[index].id) == wanted ||
		    LowerCase(civilisations[index].name) == wanted)
		{
			return static_cast<int>(index);
		}
	}

	return -1;
}
} // namespace


std::string FormatNumber(Decimal value)
{
	return FormatNumber(value.ToDouble());
}


std::string FormatNumber(double value)
{
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%.2f", value);

	return buffer;
}


std::vector<std::pair<std::string, std::string>> Simulation::BuildNameDeck(
    const UniverseNamePools &names, unsigned seed) const
{
	std::vector<std::pair<std::string, std::string>> deck;

	for (const auto &poolName : names.PoolNames())
	{
		for (const auto &name : names.Find(poolName))
		{
			deck.emplace_back(poolName, name);
		}
	}

	Jettison::Math::Random engine(seed);
	Shuffle(deck, engine);

	return deck;
}


bool Simulation::Generate(
    const Ages &ages, const Categories &categories, const Items &items, const UniverseNamePools &names,
    const GenerateOptions &options, std::string &errorMessage)
{
	errorMessage.clear();

	if (ages.empty())
	{
		errorMessage = "generation needs at least one age.";
		return false;
	}

	if (categories.empty())
	{
		errorMessage = "generation needs at least one category.";
		return false;
	}

	if (items.empty())
	{
		errorMessage = "generation needs at least one item.";
		return false;
	}

	if (names.Empty())
	{
		errorMessage = "generation needs at least one universe name pool.";
		return false;
	}

	if (options.civilisationCount == 0)
	{
		errorMessage = "the civilisation count must be at least 1.";
		return false;
	}

	if (options.minSpecialties < 1 || options.minSpecialties > options.maxSpecialties)
	{
		errorMessage = "minSpecialties must be between 1 and maxSpecialties.";
		return false;
	}

	if (options.maxSpecialties > categories.size() ||
	    options.maxSpecialties > static_cast<unsigned>(std::numeric_limits<std::int32_t>::max()))
	{
		errorMessage = std::format(
		    "maxSpecialties {} exceeds the {} loaded categories.",
		    std::to_string(options.maxSpecialties), std::to_string(categories.size()));
		return false;
	}

	const std::vector<std::pair<std::string, std::string>> deck = BuildNameDeck(names, options.seed);

	if (deck.size() < options.civilisationCount)
	{
		errorMessage = std::format(
		    "the universe name pools hold {} names: not enough for {} civilisations.",
		    std::to_string(deck.size()), std::to_string(options.civilisationCount));
		return false;
	}

	// Reserve room for the maximum market/local multiplier (5 * 1.9). Validate
	// before replacing the current world, so bad authored prices preserve the run.
	for (const auto &item : items)
	{
		const auto price = Decimal::TryParse(item.basePrice);
		if (!price || *price < 0 || *price > Decimal::Max() / 10)
		{
			errorMessage = std::format(
			    "item '{}' has invalid or oversized basePrice '{}'.", item.itemId, item.basePrice);
			return false;
		}
	}

	m_ages = ages;
	m_categories = categories;
	m_items = items;

	// The ranks are read after the ages are stored, and not before: an empty rank map
	// lets every good into every deck, which is the gate doing nothing while looking as
	// if it worked.
	const std::map<std::string, std::size_t> ageRanks = BuildAgeRanks(m_ages);
	m_civilisations.clear();
	m_tick = 0;

	// The engine owns both generator and distributions, so a standard library update
	// cannot change a universe drawn from the same seed.
	Jettison::Math::Random engine(options.seed);

	// Early ages are the common case, so a small universe is a primitive one.
	std::vector<double> ageWeights(m_ages.size());
	double totalWeight = 0.0;
	for (std::size_t index = 0; index < m_ages.size(); ++index)
	{
		ageWeights[index] = 1.0 / static_cast<double>(index + 1);
		totalWeight += ageWeights[index];
	}

	std::vector<std::string> categoryIds;
	categoryIds.reserve(m_categories.size());
	for (const auto &category : m_categories)
	{
		categoryIds.push_back(category.categoryId);
	}

	for (unsigned index = 0; index < options.civilisationCount; ++index)
	{
		Civilisation civ;

		civ.id = "Civ-" + PaddedIndex(index + 1);
		civ.pool = deck[index].first;
		civ.systemName =
		    deck[index].second + " " +
		    kSystemSuffixes[index % (sizeof(kSystemSuffixes) / sizeof(kSystemSuffixes[0]))];
		civ.name = civ.systemName;

		double draw = engine.NextDouble() * totalWeight;
		std::size_t ageIndex = m_ages.size() - 1;
		for (std::size_t candidate = 0; candidate < m_ages.size(); ++candidate)
		{
			draw -= ageWeights[candidate];
			if (draw <= 0.0)
			{
				ageIndex = candidate;
				break;
			}
		}

		civ.ageIndex = ageIndex;
		civ.ageId = m_ages[ageIndex].ageId;

		const double era = 1.0 + static_cast<double>(ageIndex);
		civ.population = (0.05 + engine.NextDouble() * 1.95) * era;

		std::vector<std::string> shuffledCategories = categoryIds;
		Shuffle(shuffledCategories, engine);


		const unsigned specialtyTotal = std::min(
		    static_cast<unsigned>(shuffledCategories.size()),
		    static_cast<unsigned>(engine.Range(
		        static_cast<std::int32_t>(options.minSpecialties),
		        static_cast<std::int32_t>(options.maxSpecialties))));
		civ.specialties.assign(
		    shuffledCategories.begin(),
		    shuffledCategories.begin() + static_cast<std::ptrdiff_t>(specialtyTotal));

		// A later age makes a wider range of goods: the deck holds the goods this world's
		// age can make, so a world at AgeStone does not print antibiotics, and the goods of
		// an unknown age are in every deck.
		std::vector<std::size_t> goodsDeck;
		goodsDeck.reserve(m_items.size());
		for (std::size_t item = 0; item < m_items.size(); ++item)
		{
			const auto rank = ageRanks.find(m_items[item].ageId);
			if (rank == ageRanks.end() || rank->second <= ageIndex)
			{
				goodsDeck.push_back(item);
			}
		}
		Shuffle(goodsDeck, engine);

		const std::size_t goods =
		    std::min(goodsDeck.size(), static_cast<std::size_t>(4) + 3 * ageIndex);
		for (std::size_t made = 0; made < goods; ++made)
		{
			const ItemNode &item = m_items[goodsDeck[made]];
			const double units = (0.4 + engine.NextDouble() * 1.2) * era * civ.population;

			civ.production[item.itemId] = units;
			civ.stockpile[item.itemId] = units * 8.0;
		}

		for (const auto &item : m_items)
		{
			const auto made = civ.production.find(item.itemId);
			if (made != civ.production.end())
			{
				// A world consumes part of what it makes, leaving the surplus to trade.
				civ.demand[item.itemId] = made->second * (0.2 + engine.NextDouble() * 0.4);

				continue;
			}

			// A world buys the goods it does not make: the essentials, the medicines and
			// the knowledge goods at the essential rate, the rest at the ordinary one.
			const double weight = IsEssentialDemand(item.categoryId) ? kEssentialDemandWeight : 1.0;
			const double need = civ.population * (0.05 + engine.NextDouble() * 0.45) * weight;

			civ.demand[item.itemId] = need;

			// A starting inventory, so a world does not open every tick in shortage.
			civ.stockpile[item.itemId] = need * 4.0;
		}

		civ.treasury = static_cast<long long>(500.0 * era * civ.population);

		m_civilisations.push_back(std::move(civ));
	}

	m_generated = true;

	return true;
}


bool Simulation::IsGenerated() const
{
	return m_generated;
}


long long Simulation::GetTick() const
{
	return m_tick;
}


const std::vector<Civilisation> &Simulation::GetCivilisations() const
{
	return m_civilisations;
}


const std::vector<ItemNode> &Simulation::GetItems() const
{
	return m_items;
}


const Civilisation *Simulation::FindCivilisation(const std::string &idOrName) const
{
	const int index = FindCivilisationIndex(m_civilisations, idOrName);

	return index >= 0 ? &m_civilisations[static_cast<std::size_t>(index)] : nullptr;
}


Decimal Simulation::BasePrice(const ItemNode &item)
{
	const auto price = Decimal::TryParse(item.basePrice);
	return price && *price >= 0 ? *price : Decimal{0};
}


const ItemNode *Simulation::FindItem(const std::string &itemId) const
{
	for (const auto &item : m_items)
	{
		if (item.itemId == itemId)
		{
			return &item;
		}
	}

	return nullptr;
}


double Simulation::GetSupply(const std::string &itemId) const
{
	double supply = 0.0;

	for (const auto &civ : m_civilisations)
	{
		supply += LookUp(civ.production, itemId);
	}

	return supply;
}


double Simulation::GetDemand(const std::string &itemId) const
{
	double demand = 0.0;

	for (const auto &civ : m_civilisations)
	{
		demand += LookUp(civ.demand, itemId);
	}

	return demand;
}


Decimal Simulation::GetPrice(const std::string &itemId) const
{
	const ItemNode *item = FindItem(itemId);
	if (item == nullptr)
	{
		return Decimal{0};
	}

	const Decimal basePrice = BasePrice(*item);
	if (basePrice <= 0)
	{
		return Decimal{0};
	}

	const double supply = GetSupply(itemId);
	const double demand = GetDemand(itemId);

	const double factor = Clamp(
	    std::pow(demand / std::max(supply, 0.001), kPriceElasticity), kMinPriceFactor, kMaxPriceFactor);

	// Quantity ratios retain the harness's double model. Only the resulting price
	// is quantized to microcredits, nearest with halfway values away from zero.
	const auto price = Decimal::TryFromDouble(basePrice.ToDouble() * factor);
	JETTISON_VERIFY(price.has_value());
	return *price;
}


Decimal Simulation::GetLocalPrice(const std::string &civilisationId, const std::string &itemId) const
{
	const Civilisation *civ = FindCivilisation(civilisationId);
	const Decimal marketPrice = GetPrice(itemId);

	if (civ == nullptr || marketPrice <= 0)
	{
		return Decimal{0};
	}

	const double made = LookUp(civ->production, itemId);
	const double need = LookUp(civ->demand, itemId);

	// Compare the world's own balance with the average world, so the bias is a share of
	// the typical economy and not of the whole universe.
	const double average = (GetSupply(itemId) + GetDemand(itemId)) /
	                       static_cast<double>(std::max<std::size_t>(m_civilisations.size(), 1));
	if (average <= 0.0)
	{
		return marketPrice;
	}

	const double bias = Clamp(kLocalPriceBias * (need - made) / average, -0.9, 0.9);

	const auto price = Decimal::TryFromDouble(marketPrice.ToDouble() * (1.0 + bias));
	JETTISON_VERIFY(price.has_value());
	return *price;
}


std::vector<TradeRoute> Simulation::PlanTrade(unsigned limit) const
{
	std::vector<TradeRoute> routes;

	if (!m_generated || limit == 0)
	{
		return routes;
	}

	for (const auto &item : m_items)
	{
		const std::string &itemId = item.itemId;

		const Civilisation *seller = nullptr;
		const Civilisation *buyer = nullptr;
		double surplus = 0.0;
		double deficit = 0.0;

		for (const auto &civ : m_civilisations)
		{
			const double balance = LookUp(civ.production, itemId) - LookUp(civ.demand, itemId);

			if (balance > surplus)
			{
				surplus = balance;
				seller = &civ;
			}

			if (-balance > deficit)
			{
				deficit = -balance;
				buyer = &civ;
			}
		}

		if (seller == nullptr || buyer == nullptr || surplus <= 0.0 || deficit <= 0.0)
		{
			continue;
		}

		const double quantity =
		    std::min(std::min(surplus, deficit) * kTradeFraction, LookUp(seller->stockpile, itemId));
		if (quantity <= 0.0)
		{
			continue;
		}

		const Decimal unitSellPrice = GetLocalPrice(seller->id, itemId);
		const Decimal unitBuyPrice = GetLocalPrice(buyer->id, itemId);
		const double freight = GetPrice(itemId).ToDouble() * kFreightRate * quantity;
		const double profit = quantity * (unitBuyPrice.ToDouble() - unitSellPrice.ToDouble()) - freight;

		if (profit <= 0.0)
		{
			continue;
		}

		TradeRoute route;
		route.itemId = itemId;
		route.sellerId = seller->id;
		route.buyerId = buyer->id;
		route.quantity = quantity;
		route.unitSellPrice = unitSellPrice;
		route.unitBuyPrice = unitBuyPrice;
		route.freight = freight;
		route.profit = profit;

		routes.push_back(route);
	}

	std::sort(
	    routes.begin(), routes.end(),
	    [](const TradeRoute &left, const TradeRoute &right)
	    {
		    if (left.profit != right.profit)
		    {
			    return left.profit > right.profit;
		    }

		    if (left.itemId != right.itemId)
		    {
			    return left.itemId < right.itemId;
		    }

		    return left.sellerId < right.sellerId;
	    });

	if (routes.size() > limit)
	{
		routes.resize(limit);
	}

	return routes;
}


bool Simulation::AdvanceTick(std::string &errorMessage)
{
	errorMessage.clear();

	if (!m_generated)
	{
		errorMessage = "nothing to advance: generate the universe first.";
		return false;
	}

	for (auto &civ : m_civilisations)
	{
		for (const auto &entry : civ.production)
		{
			civ.stockpile[entry.first] += entry.second;
		}

		double unmet = 0.0;
		for (const auto &entry : civ.demand)
		{
			double &stock = civ.stockpile[entry.first];

			const double taken = std::min(stock, entry.second);
			stock -= taken;
			unmet += entry.second - taken;
		}

		if (unmet > 0.0)
		{
			civ.unmetDemand += unmet;
			++civ.shortageTicks;
		}
	}

	for (const auto &route : PlanTrade(kRoutesPerTick))
	{
		const int sellerIndex = FindCivilisationIndex(m_civilisations, route.sellerId);
		const int buyerIndex = FindCivilisationIndex(m_civilisations, route.buyerId);

		if (sellerIndex < 0 || buyerIndex < 0)
		{
			continue;
		}

		Civilisation &seller = m_civilisations[static_cast<std::size_t>(sellerIndex)];
		Civilisation &buyer = m_civilisations[static_cast<std::size_t>(buyerIndex)];

		const double cost = route.quantity * route.unitBuyPrice.ToDouble();
		if (LookUp(seller.stockpile, route.itemId) < route.quantity ||
		    static_cast<double>(buyer.treasury) < cost)
		{
			continue;
		}

		const double revenue = route.quantity * route.unitSellPrice.ToDouble();
		// 2^63 rounds to double but LLONG_MAX does not. Check the exclusive bound
		// before casting, then check the integer addition before moving goods.
		if (!std::isfinite(cost) || !std::isfinite(revenue) || cost < 0.0 || revenue < 0.0 ||
		    cost >= 0x1p63 || revenue >= 0x1p63)
		{
			continue;
		}
		const auto paid = static_cast<long long>(cost);
		const auto received = static_cast<long long>(revenue);
		if (buyer.treasury < paid || seller.treasury > std::numeric_limits<long long>::max() - received)
		{
			continue;
		}

		seller.stockpile[route.itemId] -= route.quantity;
		buyer.stockpile[route.itemId] += route.quantity;

		seller.treasury += received;
		buyer.treasury -= paid;
	}

	++m_tick;

	return true;
}


double Simulation::GetPriceIndex() const
{
	double ratioTotal = 0.0;
	std::size_t counted = 0;

	for (const auto &item : m_items)
	{
		const Decimal basePrice = BasePrice(item);
		if (basePrice <= 0)
		{
			continue;
		}

		ratioTotal += GetPrice(item.itemId).ToDouble() / basePrice.ToDouble();
		++counted;
	}

	return counted > 0 ? ratioTotal / static_cast<double>(counted) : 0.0;
}


std::string Simulation::StatusLine() const
{
	if (!m_generated)
	{
		return "no universe: load the configuration and run generate.";
	}

	double population = 0.0;
	double unmet = 0.0;
	unsigned shortageTicks = 0;
	long long treasury = 0;

	for (const auto &civ : m_civilisations)
	{
		population += civ.population;
		unmet += civ.unmetDemand;
		shortageTicks += civ.shortageTicks;
		treasury += civ.treasury;
	}

	std::ostringstream line;
	line << "tick " << m_tick << ", " << m_civilisations.size() << " worlds, population "
	     << FormatNumber(population) << " units, price index " << FormatNumber(GetPriceIndex())
	     << ", shortages " << FormatNumber(unmet) << " units over " << shortageTicks
	     << " world ticks, treasury " << treasury;

	return line.str();
}


std::string Simulation::DescribeCivilisation(const std::string &idOrName) const
{
	const Civilisation *civ = FindCivilisation(idOrName);
	if (civ == nullptr)
	{
		return "no civilisation '" + idOrName + "': run generate, then list.";
	}

	std::ostringstream line;
	line << civ->id << " " << civ->name << " [" << civ->pool << "] age " << civ->ageId << "\n"
	     << "  population " << FormatNumber(civ->population) << " units, treasury " << civ->treasury
	     << ", goods produced " << civ->production.size() << "/" << m_items.size() << "\n"
	     << "  specialties:";

	for (const auto &specialty : civ->specialties)
	{
		line << " " << specialty;
	}

	line << "\n  needs not produced:";

	std::vector<std::pair<double, std::string>> gaps;
	for (const auto &entry : civ->demand)
	{
		if (civ->production.find(entry.first) == civ->production.end())
		{
			gaps.emplace_back(entry.second, entry.first);
		}
	}
	std::sort(
	    gaps.begin(), gaps.end(), [](const auto &left, const auto &right)
	    { return left.first != right.first ? left.first > right.first : left.second < right.second; });

	std::size_t shown = 0;
	for (const auto &gap : gaps)
	{
		if (shown++ == 5)
		{
			break;
		}

		line << " " << gap.second << " " << FormatNumber(gap.first) << "/"
		     << FormatNumber(LookUp(civ->stockpile, gap.second));
	}

	line << "\n  shortages " << FormatNumber(civ->unmetDemand) << " units over " << civ->shortageTicks
	     << " ticks";

	return line.str();
}
} // namespace Manifest::Core
