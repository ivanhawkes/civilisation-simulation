#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <manifest/config/SimulationConfig.h>
#include <manifest/universe/Universe.h>


namespace Manifest::Core
{
// A generated planet civilisation. Each civilisation is a world in a generated solar
// system that produces some of the trade goods and needs the rest.
struct Civilisation
{
	std::string id; // "Civ-01"

	// Planet name, drawn from a universe name pool, for example "Abigail Prime".
	std::string name;

	std::string systemName; // "Abigail Prime"
	std::string pool;       // the pool the name came from: latin, greek, ...

	std::string ageId; // the loaded AgeNode this world sits in
	std::size_t ageIndex{0};

	// Category labels the economy is known for. The merged configuration carries no
	// item to category link, so these are the world's identity, not a production filter.
	std::vector<std::string> specialties;

	// Population in world units, not people: demand and production scale with it, and
	// the era sets the ceiling, so a world at AgeSublimation dwarfs a world at AgeStone.
	double population{1.0};
	long long treasury{0}; // credits

	// Item id -> units per tick. Production only lists the goods this economy makes, and
	// what it can make is bounded by its age: the deck holds the goods whose ageId sits at
	// this world's age index or earlier, so a world at AgeStone has no antibiotics to make.
	// A good with no ageId is in every deck.
	std::map<std::string, double> production;

	// Item id -> units per tick. A world consumes part of what it makes and buys the
	// rest. The goods of the essential categories - the water, food and fuels a population
	// runs on, the medicines and the knowledge goods - are wanted at every age, so their
	// demand is weighted up: medicine and education are demand, not a craft.
	std::map<std::string, double> demand;

	// Item id -> units held.
	std::map<std::string, double> stockpile;

	// Demand that could not be met, and the number of ticks that happened.
	double unmetDemand{0.0};
	unsigned shortageTicks{0};
};

struct GenerateOptions
{
	unsigned seed{1};
	unsigned civilisationCount{8};
	unsigned minSpecialties{2};
	unsigned maxSpecialties{4};
};

// One suggested movement of goods from a surplus world to a deficit world.
struct TradeRoute
{
	std::string itemId;
	std::string sellerId;
	std::string buyerId;

	double quantity{0.0};
	double unitSellPrice{0.0}; // paid to the seller
	double unitBuyPrice{0.0};  // charged to the buyer
	double freight{0.0};
	double profit{0.0};
};

// A small deterministic supply and demand model over the loaded configuration.
//
// Generation seeds each world with a name from the universe pools, an age, a handful of
// category specialities and an economy: the goods it produces, how much of everything
// else it needs, and a starting stockpile. The age gates what a world can make, the
// category weights what it wants, and prices follow scarcity: the market price of a
// good rises with aggregate demand and falls with aggregate supply, and the price at one
// world moves with that world's own surplus or deficit. The difference between two
// worlds is the trade the player can make, and a tick moves production, consumption and
// the resulting trades.
//
// The model is a development harness: it is reproducible from the seed and it is
// deliberately small enough to reason about. It reads basePrice from every good, plus
// the ageId and categoryId links the merged documents write out. Those links are
// optional: goods without them are in every deck and wanted at the ordinary rate.
class Simulation
{
  public:
	// Generates the civilisations, replacing any previous run. Deterministic: the same
	// configuration and options give the same worlds, names and economies. A world's age
	// bounds the goods it can produce, and the essential categories are demanded at every
	// age, so a world that cannot make medicine has to buy it.
	//
	// Fails when any input is empty, when the civilisation count is 0, when the pools
	// cannot supply enough distinct names, or when the specialty bounds do not fit the
	// loaded categories.
	bool Generate(
	    const Ages &ages, const Categories &categories, const Items &items,
	    const UniverseNamePools &names, const GenerateOptions &options, std::string &errorMessage);

	bool IsGenerated() const;
	long long GetTick() const;

	const std::vector<Civilisation> &GetCivilisations() const;
	const std::vector<ItemNode> &GetItems() const;

	// Looks a world up by id ("Civ-03") or by name, case insensitive.
	const Civilisation *FindCivilisation(const std::string &idOrName) const;

	// basePrice parsed as a number; 0.0 when the field is not a number.
	static double BasePrice(const ItemNode &item);

	const ItemNode *FindItem(const std::string &itemId) const;

	// Aggregate supply and demand of a good across every world, in units per tick.
	double GetSupply(const std::string &itemId) const;
	double GetDemand(const std::string &itemId) const;

	// Market price: the base price scaled by how scarce the good is.
	double GetPrice(const std::string &itemId) const;

	// Price at one world: the market price moved by that economy's own surplus or
	// deficit. The gap between two worlds is what makes a trade pay.
	double GetLocalPrice(const std::string &civilisationId, const std::string &itemId) const;

	// The best routes by profit, best first, at most Limit. Routes that cannot pay their
	// freight are not returned.
	std::vector<TradeRoute> PlanTrade(unsigned limit) const;

	// One tick: production, consumption, then the trade of the best routes. Fails when
	// nothing has been generated.
	bool AdvanceTick(std::string &errorMessage);

	// One line for the UI: tick, worlds, population, price index, shortages, treasury.
	std::string StatusLine() const;

	// A world as several lines for the UI.
	std::string DescribeCivilisation(const std::string &idOrName) const;

	// Mean market price divided by mean base price: 1.0 when nothing has moved.
	double GetPriceIndex() const;

  private:
	// A deterministic deck of (pool, name) pairs, so names never repeat in a run.
	std::vector<std::pair<std::string, std::string>> BuildNameDeck(
	    const UniverseNamePools &names, unsigned seed) const;

	std::vector<Civilisation> m_civilisations;
	std::vector<ItemNode> m_items;
	std::vector<AgeNode> m_ages;
	std::vector<CategoryNode> m_categories;
	long long m_tick{0};
	bool m_generated{false};
};

// Number formatting for the UI: two decimals, thousands separators are left to the
// front-end.
std::string FormatNumber(double value);
} // namespace Manifest::Core
