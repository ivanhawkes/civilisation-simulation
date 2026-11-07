#include "Core/Simulation.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <random>
#include <sstream>
#include <utility>


namespace CivSimCore
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

constexpr const char *kSystemSuffixes[] = {"Prime",   "Alpha", "Beta", "Gamma", "Delta",
                                           "Epsilon", "Zeta",  "Eta",  "Theta", "Iota"};

double Clamp(double Value, double Low, double High)
{
	return std::max(Low, std::min(High, Value));
}

double LookUp(const std::map<std::string, double> &Values, const std::string &Key)
{
	const auto entry = Values.find(Key);

	return entry != Values.end() ? entry->second : 0.0;
}

std::string LowerCase(std::string Text)
{
	std::transform(
	    Text.begin(), Text.end(), Text.begin(),
	    [](unsigned char character) { return static_cast<char>(std::tolower(character)); });

	return Text;
}

std::string PaddedIndex(unsigned Index)
{
	const std::string digits = std::to_string(Index);

	return digits.size() >= 2 ? digits : "0" + digits;
}

// Finds a world by id ("Civ-03") or by name, case insensitive. -1 when unknown.
int FindCivilisationIndex(const std::vector<Civilisation> &Civilisations, const std::string &IdOrName)
{
	const std::string wanted = LowerCase(IdOrName);

	for (std::size_t index = 0; index < Civilisations.size(); ++index)
	{
		if (LowerCase(Civilisations[index].id) == wanted ||
		    LowerCase(Civilisations[index].name) == wanted)
		{
			return static_cast<int>(index);
		}
	}

	return -1;
}
} // namespace


std::string FormatNumber(double Value)
{
	char buffer[32];
	std::snprintf(buffer, sizeof(buffer), "%.2f", Value);

	return buffer;
}


std::vector<std::pair<std::string, std::string>> Simulation::BuildNameDeck(
    const UniverseNamePools &Names, unsigned seed) const
{
	std::vector<std::pair<std::string, std::string>> deck;

	for (const auto &poolName : Names.PoolNames())
	{
		for (const auto &name : Names.Find(poolName))
		{
			deck.emplace_back(poolName, name);
		}
	}

	std::mt19937_64 engine(seed);
	std::shuffle(deck.begin(), deck.end(), engine);

	return deck;
}


bool Simulation::Generate(
    const Ages &Ages, const Categories &Categories, const Items &Items, const UniverseNamePools &Names,
    const GenerateOptions &Options, std::string &ErrorMessage)
{
	ErrorMessage.clear();

	if (Ages.empty())
	{
		ErrorMessage = "generation needs at least one age.";
		return false;
	}

	if (Categories.empty())
	{
		ErrorMessage = "generation needs at least one category.";
		return false;
	}

	if (Items.empty())
	{
		ErrorMessage = "generation needs at least one item.";
		return false;
	}

	if (Names.Empty())
	{
		ErrorMessage = "generation needs at least one universe name pool.";
		return false;
	}

	if (Options.civilisationCount == 0)
	{
		ErrorMessage = "the civilisation count must be at least 1.";
		return false;
	}

	if (Options.minSpecialties < 1 || Options.minSpecialties > Options.maxSpecialties)
	{
		ErrorMessage = "minSpecialties must be between 1 and maxSpecialties.";
		return false;
	}

	if (Options.maxSpecialties > Categories.size())
	{
		ErrorMessage = "maxSpecialties " + std::to_string(Options.maxSpecialties) + " exceeds the " +
		               std::to_string(Categories.size()) + " loaded categories.";
		return false;
	}

	const std::vector<std::pair<std::string, std::string>> deck = BuildNameDeck(Names, Options.seed);

	if (deck.size() < Options.civilisationCount)
	{
		ErrorMessage = "the universe name pools hold " + std::to_string(deck.size()) +
		               " names: not enough for " + std::to_string(Options.civilisationCount) +
		               " civilisations.";
		return false;
	}

	ages = Ages;
	categories = Categories;
	items = Items;
	civilisations.clear();
	tick = 0;

	// Deterministic for a given standard library: one seeded engine drives every draw,
	// in a fixed order.
	std::mt19937_64 engine(Options.seed);
	std::uniform_real_distribution<double> unit(0.0, 1.0);

	// Early ages are the common case, so a small universe is a primitive one.
	std::vector<double> ageWeights(ages.size());
	double totalWeight = 0.0;
	for (std::size_t index = 0; index < ages.size(); ++index)
	{
		ageWeights[index] = 1.0 / static_cast<double>(index + 1);
		totalWeight += ageWeights[index];
	}

	std::vector<std::string> categoryIds;
	categoryIds.reserve(categories.size());
	for (const auto &category : categories)
	{
		categoryIds.push_back(category.uniqueId);
	}

	for (unsigned index = 0; index < Options.civilisationCount; ++index)
	{
		Civilisation civ;

		civ.id = "Civ-" + PaddedIndex(index + 1);
		civ.pool = deck[index].first;
		civ.systemName =
		    deck[index].second + " " +
		    kSystemSuffixes[index % (sizeof(kSystemSuffixes) / sizeof(kSystemSuffixes[0]))];
		civ.name = civ.systemName;

		double draw = unit(engine) * totalWeight;
		std::size_t ageIndex = ages.size() - 1;
		for (std::size_t candidate = 0; candidate < ages.size(); ++candidate)
		{
			draw -= ageWeights[candidate];
			if (draw <= 0.0)
			{
				ageIndex = candidate;
				break;
			}
		}

		civ.ageIndex = ageIndex;
		civ.ageId = ages[ageIndex].uniqueId;

		const double era = 1.0 + static_cast<double>(ageIndex);

		std::uniform_real_distribution<double> populationDraw(0.05, 2.0);
		civ.population = populationDraw(engine) * era;

		std::vector<std::string> shuffledCategories = categoryIds;
		std::shuffle(shuffledCategories.begin(), shuffledCategories.end(), engine);

		std::uniform_int_distribution<unsigned> specialtyCount(
		    Options.minSpecialties, Options.maxSpecialties);
		const unsigned specialtyTotal =
		    std::min(static_cast<unsigned>(shuffledCategories.size()), specialtyCount(engine));
		civ.specialties.assign(
		    shuffledCategories.begin(),
		    shuffledCategories.begin() + static_cast<std::ptrdiff_t>(specialtyTotal));

		// A later age makes a wider range of goods.
		std::vector<std::size_t> goodsDeck(items.size());
		for (std::size_t item = 0; item < items.size(); ++item)
		{
			goodsDeck[item] = item;
		}
		std::shuffle(goodsDeck.begin(), goodsDeck.end(), engine);

		const std::size_t goods = std::min(items.size(), static_cast<std::size_t>(4) + 3 * ageIndex);

		std::uniform_real_distribution<double> productivity(0.4, 1.6);
		for (std::size_t made = 0; made < goods; ++made)
		{
			const ItemNode &item = items[goodsDeck[made]];
			const double units = productivity(engine) * era * civ.population;

			civ.production[item.uniqueId] = units;
			civ.stockpile[item.uniqueId] = units * 8.0;
		}

		for (const auto &item : items)
		{
			const auto made = civ.production.find(item.uniqueId);
			if (made != civ.production.end())
			{
				// A world consumes part of what it makes, leaving the surplus to trade.
				std::uniform_real_distribution<double> selfUse(0.2, 0.6);
				civ.demand[item.uniqueId] = made->second * selfUse(engine);

				continue;
			}

			std::uniform_real_distribution<double> needDraw(0.05, 0.5);
			const double need = civ.population * needDraw(engine);

			civ.demand[item.uniqueId] = need;

			// A starting inventory, so a world does not open every tick in shortage.
			civ.stockpile[item.uniqueId] = need * 4.0;
		}

		civ.treasury = static_cast<long long>(500.0 * era * civ.population);

		civilisations.push_back(std::move(civ));
	}

	generated = true;

	return true;
}


bool Simulation::IsGenerated() const
{
	return generated;
}


long long Simulation::GetTick() const
{
	return tick;
}


const std::vector<Civilisation> &Simulation::GetCivilisations() const
{
	return civilisations;
}


const std::vector<ItemNode> &Simulation::GetItems() const
{
	return items;
}


const Civilisation *Simulation::FindCivilisation(const std::string &IdOrName) const
{
	const int index = FindCivilisationIndex(civilisations, IdOrName);

	return index >= 0 ? &civilisations[static_cast<std::size_t>(index)] : nullptr;
}


double Simulation::BasePrice(const ItemNode &Item)
{
	try
	{
		return std::stod(Item.basePrice);
	}
	catch (const std::exception &)
	{
		return 0.0;
	}
}


const ItemNode *Simulation::FindItem(const std::string &ItemId) const
{
	for (const auto &item : items)
	{
		if (item.uniqueId == ItemId)
		{
			return &item;
		}
	}

	return nullptr;
}


double Simulation::GetSupply(const std::string &ItemId) const
{
	double supply = 0.0;

	for (const auto &civ : civilisations)
	{
		supply += LookUp(civ.production, ItemId);
	}

	return supply;
}


double Simulation::GetDemand(const std::string &ItemId) const
{
	double demand = 0.0;

	for (const auto &civ : civilisations)
	{
		demand += LookUp(civ.demand, ItemId);
	}

	return demand;
}


double Simulation::GetPrice(const std::string &ItemId) const
{
	const ItemNode *item = FindItem(ItemId);
	if (item == nullptr)
	{
		return 0.0;
	}

	const double basePrice = BasePrice(*item);
	if (basePrice <= 0.0)
	{
		return 0.0;
	}

	const double supply = GetSupply(ItemId);
	const double demand = GetDemand(ItemId);

	const double factor = Clamp(
	    std::pow(demand / std::max(supply, 0.001), kPriceElasticity), kMinPriceFactor, kMaxPriceFactor);

	return basePrice * factor;
}


double Simulation::GetLocalPrice(const std::string &CivilisationId, const std::string &ItemId) const
{
	const Civilisation *civ = FindCivilisation(CivilisationId);
	const double marketPrice = GetPrice(ItemId);

	if (civ == nullptr || marketPrice <= 0.0)
	{
		return 0.0;
	}

	const double made = LookUp(civ->production, ItemId);
	const double need = LookUp(civ->demand, ItemId);

	// Compare the world's own balance with the average world, so the bias is a share of
	// the typical economy and not of the whole universe.
	const double average =
	    (GetSupply(ItemId) + GetDemand(ItemId)) / std::max<std::size_t>(civilisations.size(), 1);
	if (average <= 0.0)
	{
		return marketPrice;
	}

	const double bias = Clamp(kLocalPriceBias * (need - made) / average, -0.9, 0.9);

	return marketPrice * (1.0 + bias);
}


std::vector<TradeRoute> Simulation::PlanTrade(unsigned Limit) const
{
	std::vector<TradeRoute> routes;

	if (!generated || Limit == 0)
	{
		return routes;
	}

	for (const auto &item : items)
	{
		const std::string &itemId = item.uniqueId;

		const Civilisation *seller = nullptr;
		const Civilisation *buyer = nullptr;
		double surplus = 0.0;
		double deficit = 0.0;

		for (const auto &civ : civilisations)
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

		const double unitSellPrice = GetLocalPrice(seller->id, itemId);
		const double unitBuyPrice = GetLocalPrice(buyer->id, itemId);
		const double freight = GetPrice(itemId) * kFreightRate * quantity;
		const double profit = quantity * (unitBuyPrice - unitSellPrice) - freight;

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

	if (routes.size() > Limit)
	{
		routes.resize(Limit);
	}

	return routes;
}


bool Simulation::AdvanceTick(std::string &ErrorMessage)
{
	ErrorMessage.clear();

	if (!generated)
	{
		ErrorMessage = "nothing to advance: generate the universe first.";
		return false;
	}

	for (auto &civ : civilisations)
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
		const int sellerIndex = FindCivilisationIndex(civilisations, route.sellerId);
		const int buyerIndex = FindCivilisationIndex(civilisations, route.buyerId);

		if (sellerIndex < 0 || buyerIndex < 0)
		{
			continue;
		}

		Civilisation &seller = civilisations[static_cast<std::size_t>(sellerIndex)];
		Civilisation &buyer = civilisations[static_cast<std::size_t>(buyerIndex)];

		const double cost = route.quantity * route.unitBuyPrice;
		if (LookUp(seller.stockpile, route.itemId) < route.quantity ||
		    static_cast<double>(buyer.treasury) < cost)
		{
			continue;
		}

		seller.stockpile[route.itemId] -= route.quantity;
		buyer.stockpile[route.itemId] += route.quantity;

		seller.treasury += static_cast<long long>(route.quantity * route.unitSellPrice);
		buyer.treasury -= static_cast<long long>(cost);
	}

	++tick;

	return true;
}


double Simulation::GetPriceIndex() const
{
	double ratioTotal = 0.0;
	std::size_t counted = 0;

	for (const auto &item : items)
	{
		const double basePrice = BasePrice(item);
		if (basePrice <= 0.0)
		{
			continue;
		}

		ratioTotal += GetPrice(item.uniqueId) / basePrice;
		++counted;
	}

	return counted > 0 ? ratioTotal / static_cast<double>(counted) : 0.0;
}


std::string Simulation::StatusLine() const
{
	if (!generated)
	{
		return "no universe: load the configuration and run generate.";
	}

	double population = 0.0;
	double unmet = 0.0;
	unsigned shortageTicks = 0;
	long long treasury = 0;

	for (const auto &civ : civilisations)
	{
		population += civ.population;
		unmet += civ.unmetDemand;
		shortageTicks += civ.shortageTicks;
		treasury += civ.treasury;
	}

	std::ostringstream line;
	line << "tick " << tick << ", " << civilisations.size() << " worlds, population "
	     << FormatNumber(population) << " units, price index " << FormatNumber(GetPriceIndex())
	     << ", shortages " << FormatNumber(unmet) << " units over " << shortageTicks
	     << " world ticks, treasury " << treasury;

	return line.str();
}


std::string Simulation::DescribeCivilisation(const std::string &IdOrName) const
{
	const Civilisation *civ = FindCivilisation(IdOrName);
	if (civ == nullptr)
	{
		return "no civilisation '" + IdOrName + "': run generate, then list.";
	}

	std::ostringstream line;
	line << civ->id << " " << civ->name << " [" << civ->pool << "] age " << civ->ageId << "\n"
	     << "  population " << FormatNumber(civ->population) << " units, treasury " << civ->treasury
	     << ", goods produced " << civ->production.size() << "/" << items.size() << "\n"
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
} // namespace CivSimCore
