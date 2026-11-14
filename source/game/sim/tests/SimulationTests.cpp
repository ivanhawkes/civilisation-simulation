#include "TestHelpers.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <jettison/core/Hash.h>
#include <manifest/sim/Simulation.h>


namespace
{
Manifest::Core::Ages MakeAges(std::size_t count)
{
	Manifest::Core::Ages ages;

	for (std::size_t index = 0; index < count; ++index)
	{
		Manifest::Core::AgeNode age;
		age.ageId = "Age" + std::to_string(index);
		age.displayName = "Age " + std::to_string(index);
		ages.push_back(age);
	}

	return ages;
}

Manifest::Core::Categories MakeCategories(std::size_t count)
{
	Manifest::Core::Categories categories;

	for (std::size_t index = 0; index < count; ++index)
	{
		Manifest::Core::CategoryNode category;
		category.categoryId = "Category" + std::to_string(index);
		category.displayName = category.categoryId;
		categories.push_back(category);
	}

	return categories;
}

Manifest::Core::Items MakeItems(std::size_t count)
{
	Manifest::Core::Items items;

	for (std::size_t index = 0; index < count; ++index)
	{
		Manifest::Core::ItemNode item;
		item.itemId = "Good" + std::to_string(index);
		item.displayName = item.itemId;
		item.basePrice = std::to_string(10.0 * static_cast<double>(index + 1));
		items.push_back(item);
	}

	return items;
}

Manifest::Core::UniverseNamePools MakePools(std::size_t namesPerPool)
{
	Manifest::Core::UniverseNamePools pools;

	std::vector<std::string> latin;
	std::vector<std::string> greek;

	for (std::size_t index = 0; index < namesPerPool; ++index)
	{
		latin.push_back("Latin" + std::to_string(index));
		greek.push_back("Greek" + std::to_string(index));
	}

	pools.AddPool("latin", latin);
	pools.AddPool("greek", greek);

	return pools;
}

Manifest::Core::GenerateOptions MakeOptions(unsigned count, unsigned seed)
{
	Manifest::Core::GenerateOptions options;
	options.civilisationCount = count;
	options.seed = seed;

	return options;
}

struct Scenario
{
	Manifest::Core::Ages ages = MakeAges(4);
	Manifest::Core::Categories categories = MakeCategories(6);
	Manifest::Core::Items items = MakeItems(12);
	Manifest::Core::UniverseNamePools pools = MakePools(20);
};

std::string Dump(const Manifest::Core::Civilisation &civ)
{
	std::string text = civ.id + "|" + civ.name + "|" + civ.pool + "|" + civ.ageId + "|" +
	                   std::to_string(civ.population) + "|" + std::to_string(civ.treasury);

	for (const auto &entry : civ.production)
	{
		text += "|p" + entry.first + std::to_string(entry.second);
	}

	for (const auto &entry : civ.demand)
	{
		text += "|d" + entry.first + std::to_string(entry.second);
	}

	for (const auto &specialty : civ.specialties)
	{
		text += "|s" + specialty;
	}

	return text;
}
} // namespace


TEST_CASE("generation_checks_its_inputs")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	CHECK(!simulation.IsGenerated());
	CHECK_EQ(simulation.GetTick(), 0LL);

	CHECK(!simulation.Generate(
	    Manifest::Core::Ages{}, scenario.categories, scenario.items, scenario.pools, MakeOptions(3, 1),
	    errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "at least one age");

	CHECK(!simulation.Generate(
	    scenario.ages, Manifest::Core::Categories{}, scenario.items, scenario.pools, MakeOptions(3, 1),
	    errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "at least one category");

	CHECK(!simulation.Generate(
	    scenario.ages, scenario.categories, Manifest::Core::Items{}, scenario.pools, MakeOptions(3, 1),
	    errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "at least one item");

	Manifest::Core::UniverseNamePools noPools;
	CHECK(!simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, noPools, MakeOptions(3, 1), errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "name pool");

	CHECK(!simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(0, 1),
	    errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "at least 1");

	Manifest::Core::GenerateOptions badBounds = MakeOptions(3, 1);
	badBounds.minSpecialties = 4;
	badBounds.maxSpecialties = 2;
	CHECK(!simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, badBounds, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "minSpecialties");

	Manifest::Core::GenerateOptions tooMany = MakeOptions(3, 1);
	tooMany.maxSpecialties = 99;
	CHECK(!simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, tooMany, errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "categories");

	CHECK(!simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(1000, 1),
	    errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "not enough");

	CHECK(!simulation.IsGenerated());
}

TEST_CASE("generation_is_deterministic")
{
	Scenario scenario;
	std::string errorMessage;

	Manifest::Core::Simulation first;
	Manifest::Core::Simulation second;

	CHECK(first.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(6, 7),
	    errorMessage));
	CHECK(second.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(6, 7),
	    errorMessage));

	CHECK_EQ(first.GetCivilisations().size(), std::size_t(6));
	CHECK_EQ(second.GetCivilisations().size(), std::size_t(6));

	for (std::size_t index = 0; index < first.GetCivilisations().size(); ++index)
	{
		CHECK_EQ(Dump(first.GetCivilisations()[index]), Dump(second.GetCivilisations()[index]));
	}

	// A different seed is a different universe.
	Manifest::Core::Simulation third;
	CHECK(third.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(6, 8),
	    errorMessage));

	bool identical = true;
	for (std::size_t index = 0; index < first.GetCivilisations().size(); ++index)
	{
		identical =
		    identical && Dump(first.GetCivilisations()[index]) == Dump(third.GetCivilisations()[index]);
	}

	CHECK(!identical);
}

TEST_CASE("names_are_unique_and_come_from_the_pools")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(24, 3),
	    errorMessage));

	std::vector<std::string> seen;
	for (const auto &civ : simulation.GetCivilisations())
	{
		CHECK(std::find(seen.begin(), seen.end(), civ.name) == seen.end());
		seen.push_back(civ.name);

		CHECK((civ.pool == "latin" || civ.pool == "greek"));
		CHECK((civ.name.rfind("Latin", 0) == 0 || civ.name.rfind("Greek", 0) == 0));
		CHECK(civ.id.rfind("Civ-", 0) == 0);
		CHECK(!civ.ageId.empty());
		CHECK(!civ.specialties.empty());
		CHECK(civ.specialties.size() <= 4);
		CHECK(civ.population > 0.0);
		CHECK(!civ.production.empty());
		CHECK_EQ(civ.demand.size(), std::size_t(12));
	}

	// Ids are sequential and Find accepts both forms, case insensitively.
	CHECK(simulation.FindCivilisation("Civ-01") != nullptr);
	CHECK(simulation.FindCivilisation("civ-01") != nullptr);
	CHECK(simulation.FindCivilisation(simulation.GetCivilisations()[3].name) != nullptr);
	CHECK(simulation.FindCivilisation("Civ-99") == nullptr);
}

TEST_CASE("scarce_goods_cost_more")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	// One world: the goods it makes are in surplus and the goods it cannot make have
	// no supply at all, so the two groups sit on opposite sides of the base price.
	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(1, 5),
	    errorMessage));

	const Manifest::Core::Civilisation &civ = simulation.GetCivilisations()[0];

	std::size_t checked = 0;
	for (const auto &item : simulation.GetItems())
	{
		const double basePrice = Manifest::Core::Simulation::BasePrice(item);
		CHECK(basePrice > 0.0);

		const double price = simulation.GetPrice(item.itemId);
		const double ratio = price / basePrice;

		if (civ.production.find(item.itemId) == civ.production.end())
		{
			CHECK(ratio > 1.0);
			++checked;
		}
		else
		{
			CHECK(ratio < 1.0);
		}
	}

	CHECK(checked > std::size_t(0));
	CHECK(simulation.GetPriceIndex() > 0.0);
	MANIFEST_CHECK_NEAR(simulation.GetPrice("Nope"), 0.0, 0.0001);
}

TEST_CASE("local_prices_move_with_the_local_balance")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(4, 11),
	    errorMessage));

	for (const auto &civ : simulation.GetCivilisations())
	{
		for (const auto &entry : civ.production)
		{
			// A world that makes the good sells it below the market.
			CHECK(simulation.GetLocalPrice(civ.id, entry.first) < simulation.GetPrice(entry.first));
		}

		for (const auto &item : simulation.GetItems())
		{
			if (civ.production.find(item.itemId) == civ.production.end())
			{
				// A world that needs the good pays above the market.
				CHECK(simulation.GetLocalPrice(civ.id, item.itemId) > simulation.GetPrice(item.itemId));
			}
		}
	}

	MANIFEST_CHECK_NEAR(simulation.GetLocalPrice("Nope", "Good0"), 0.0, 0.0001);
}

TEST_CASE("trade_routes_pay_for_themselves")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(8, 2),
	    errorMessage));

	const std::vector<Manifest::Core::TradeRoute> all = simulation.PlanTrade(100);
	CHECK(!all.empty());

	for (const auto &route : all)
	{
		CHECK(route.profit > 0.0);
		CHECK(route.quantity > 0.0);
		CHECK(route.unitBuyPrice > route.unitSellPrice);
		CHECK(route.freight > 0.0);
		CHECK(route.sellerId != route.buyerId);
		CHECK(simulation.FindCivilisation(route.sellerId) != nullptr);
		CHECK(simulation.FindCivilisation(route.buyerId) != nullptr);
	}

	// Sorted best first, and the limit is respected.
	for (std::size_t index = 1; index < all.size(); ++index)
	{
		CHECK(all[index - 1].profit >= all[index].profit);
	}

	CHECK_EQ(simulation.PlanTrade(3).size(), std::size_t(3));
	CHECK_EQ(simulation.PlanTrade(0).size(), std::size_t(0));
}

TEST_CASE("ticks_move_production_consumption_and_trade")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(8, 2),
	    errorMessage));

	CHECK_EQ(simulation.GetTick(), 0LL);
	CHECK_EQ(simulation.PlanTrade(5).size(), std::size_t(5));

	CHECK(simulation.AdvanceTick(errorMessage));
	CHECK_EQ(simulation.GetTick(), 1LL);

	// A world opens with four ticks of stock for the goods it cannot make, so the
	// first tick is clean and the shortages arrive as the stock runs out.
	std::size_t shortageCivs = 0;
	for (const auto &civ : simulation.GetCivilisations())
	{
		CHECK_EQ(civ.shortageTicks, 0u);
		shortageCivs += civ.shortageTicks > 0 ? 1 : 0;
	}
	CHECK_EQ(shortageCivs, std::size_t(0));

	for (int index = 0; index < 10; ++index)
	{
		CHECK(simulation.AdvanceTick(errorMessage));
	}

	CHECK_EQ(simulation.GetTick(), 11LL);

	bool anyShortage = false;
	for (const auto &civ : simulation.GetCivilisations())
	{
		if (civ.shortageTicks > 0)
		{
			anyShortage = true;
			CHECK(civ.unmetDemand > 0.0);
		}
	}
	CHECK(anyShortage);

	MANIFEST_CHECK_CONTAINS(simulation.StatusLine(), "tick 11");
	MANIFEST_CHECK_CONTAINS(simulation.StatusLine(), "8 worlds");
	MANIFEST_CHECK_CONTAINS(simulation.StatusLine(), "price index");
}

TEST_CASE("advancing_needs_a_generated_universe")
{
	Manifest::Core::Simulation simulation;
	std::string errorMessage;

	CHECK(!simulation.AdvanceTick(errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "generate the universe");

	MANIFEST_CHECK_CONTAINS(simulation.StatusLine(), "no universe");
	MANIFEST_CHECK_CONTAINS(simulation.DescribeCivilisation("Civ-01"), "no civilisation");
}

TEST_CASE("describing_a_world")
{
	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;

	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(3, 4),
	    errorMessage));

	const Manifest::Core::Civilisation &civ = simulation.GetCivilisations()[1];
	const std::string text = simulation.DescribeCivilisation(civ.id);

	MANIFEST_CHECK_CONTAINS(text, civ.id);
	MANIFEST_CHECK_CONTAINS(text, civ.name);
	MANIFEST_CHECK_CONTAINS(text, civ.pool);
	MANIFEST_CHECK_CONTAINS(text, civ.ageId);
	MANIFEST_CHECK_CONTAINS(text, "population");
	MANIFEST_CHECK_CONTAINS(text, "specialties");
	MANIFEST_CHECK_CONTAINS(text, "needs not produced");
	MANIFEST_CHECK_CONTAINS(text, "shortages");
}

TEST_CASE("base_price_must_be_a_number")
{
	Manifest::Core::ItemNode item;
	item.itemId = "Odd";
	item.basePrice = "five pounds";
	MANIFEST_CHECK_NEAR(Manifest::Core::Simulation::BasePrice(item), 0.0, 0.0001);

	item.basePrice = "5.6312";
	MANIFEST_CHECK_NEAR(Manifest::Core::Simulation::BasePrice(item), 5.6312, 0.0001);

	item.basePrice.clear();
	MANIFEST_CHECK_NEAR(Manifest::Core::Simulation::BasePrice(item), 0.0, 0.0001);

	Scenario scenario;
	std::string errorMessage;
	Manifest::Core::Simulation simulation;
	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(2, 6),
	    errorMessage));

	CHECK(simulation.FindItem("Good3") != nullptr);
	CHECK(simulation.FindItem("Nope") == nullptr);
	MANIFEST_CHECK_NEAR(simulation.GetPrice("Nope"), 0.0, 0.0001);
	MANIFEST_CHECK_NEAR(simulation.GetLocalPrice("Civ-01", "Nope"), 0.0, 0.0001);

	MANIFEST_CHECK_CONTAINS(Manifest::Core::FormatNumber(12.3456), "12.35");
}

TEST_CASE("a_world_makes_only_the_goods_of_its_age_and_earlier")
{
	// The merged documents write an ageId onto every good, and the economy reads it as a
	// production gate: a world at AgeStone has no antibiotics to make. A good with no age of
	// its own is in every deck, because a missing link is not a licence to make nothing.
	Scenario scenario;
	scenario.ages = MakeAges(4);

	std::vector<std::pair<std::string, std::size_t>> ranks;

	// The scenario's default goods carry no links, so the list starts empty: a good that
	// shares an id with a linked one of the same number is the test's own bug, and the
	// production map cannot tell the pair apart.
	scenario.items.clear();
	for (std::size_t index = 0; index < 12; ++index)
	{
		Manifest::Core::ItemNode item;
		item.itemId = "Good" + std::to_string(index);
		item.displayName = item.itemId;
		item.basePrice = std::to_string(10.0 * static_cast<double>(index + 1));
		item.ageId = "Age" + std::to_string(index % 4);

		ranks.emplace_back(item.itemId, index % 4);
		scenario.items.push_back(item);
	}

	Manifest::Core::ItemNode timeless;
	timeless.itemId = "Timeless";
	timeless.displayName = "Timeless";
	timeless.basePrice = "40";
	scenario.items.push_back(timeless);

	std::string errorMessage;
	Manifest::Core::Simulation simulation;
	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(40, 7),
	    errorMessage));

	const std::size_t latestAge = scenario.ages.size() - 1;

	std::size_t earliestWorlds = 0;
	std::size_t latestWorlds = 0;
	std::size_t madeLateGoods = 0;
	std::size_t madeTimelessGoods = 0;

	for (const auto &civ : simulation.GetCivilisations())
	{
		if (civ.ageIndex == 0)
		{
			++earliestWorlds;
		}

		if (civ.ageIndex == latestAge)
		{
			++latestWorlds;
		}

		for (const auto &entry : civ.production)
		{
			if (entry.first == timeless.itemId)
			{
				++madeTimelessGoods;

				continue;
			}

			std::size_t rank = latestAge;
			for (const auto &candidate : ranks)
			{
				if (candidate.first == entry.first)
				{
					rank = candidate.second;
				}
			}

			// The gate: a good is made only by a world at its age index or later.
			CHECK(rank <= civ.ageIndex);

			if (rank == latestAge)
			{
				++madeLateGoods;
			}
		}
	}

	// The gate is a bound and not a blank: the late goods are made by the late worlds, and
	// the good with no age of its own is made anywhere.
	CHECK(earliestWorlds > std::size_t(0));
	CHECK(latestWorlds > std::size_t(0));
	CHECK(madeLateGoods > std::size_t(0));
	CHECK(madeTimelessGoods > std::size_t(0));
}

TEST_CASE("medicine_and_knowledge_goods_are_demanded_at_every_age")
{
	// A craft is a thing a world makes. Medicine and education are things a world wants:
	// the goods of the essential categories, the medicines, the knowledge goods, the water
	// and the fuel a population runs on, are wanted at every age. A world that cannot make
	// antibiotics is a buyer of them, and the buying is what moves their price.
	Scenario scenario;
	scenario.ages = MakeAges(1);

	scenario.categories.clear();
	for (const char *id :
	     {"CategoryMedicine", "CategoryInformation", "CategoryMetals", "CategoryCrafts"})
	{
		Manifest::Core::CategoryNode category;
		category.categoryId = id;
		category.displayName = id;
		category.type = id;
		scenario.categories.push_back(category);
	}

	scenario.items.clear();
	for (std::size_t index = 0; index < 30; ++index)
	{
		Manifest::Core::ItemNode essential;
		essential.itemId = "Need" + std::to_string(index);
		essential.displayName = essential.itemId;
		essential.basePrice = "20";
		essential.categoryId = (index % 2 == 0) ? "CategoryMedicine" : "CategoryInformation";
		scenario.items.push_back(essential);

		Manifest::Core::ItemNode ordinary;
		ordinary.itemId = "Ware" + std::to_string(index);
		ordinary.displayName = ordinary.itemId;
		ordinary.basePrice = "20";
		ordinary.categoryId = (index % 2 == 0) ? "CategoryMetals" : "CategoryCrafts";
		scenario.items.push_back(ordinary);
	}

	std::string errorMessage;
	Manifest::Core::Simulation simulation;
	CHECK(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(40, 11),
	    errorMessage));

	double population = 0.0;
	double wantedEssential = 0.0;
	double wantedOrdinary = 0.0;

	for (const auto &civ : simulation.GetCivilisations())
	{
		population += civ.population;

		for (const auto &entry : civ.demand)
		{
			if (entry.first.rfind("Need", 0) == 0)
			{
				wantedEssential += entry.second;
			}
			else
			{
				wantedOrdinary += entry.second;
			}
		}
	}

	CHECK(population > 0.0);
	CHECK(wantedEssential > 0.0);
	CHECK(wantedOrdinary > 0.0);

	const double essentialPerPerson = wantedEssential / population;
	const double ordinaryPerPerson = wantedOrdinary / population;

	// The weight is 2.5 to the person, and the worlds that do make these goods dilute both
	// families with their own self use, so the check is a margin and not the figure.
	CHECK(essentialPerPerson > ordinaryPerPerson * 1.5);

	// A good everybody wants and few can make is bid up: the same base price, and the
	// essential one is the dearer of the pair on the market.
	CHECK(simulation.GetPrice("Need0") > simulation.GetPrice("Ware0"));
}


// Pin the exact state, including fractional stock, so Debug, Release and sanitised
// builds exercise the engine's replay rule rather than comparing rounded UI text.
TEST_CASE("the engine RNG produces the same generation and hundred-tick replay in every build")
{
	Scenario scenario;
	Manifest::Core::Simulation simulation;
	std::string error;
	REQUIRE(simulation.Generate(
	    scenario.ages, scenario.categories, scenario.items, scenario.pools, MakeOptions(8, 42), error));
	const auto stateHash = [&]()
	{
		using Jettison::Core::HashCombine;
		using Jettison::Core::Fnv1a64;
		auto hash = static_cast<std::uint64_t>(simulation.GetTick());
		for (const auto &civ : simulation.GetCivilisations())
		{
			for (const auto &text : {civ.id, civ.name, civ.systemName, civ.pool, civ.ageId})
				hash = HashCombine(hash, Fnv1a64(text));
			hash = HashCombine(hash, civ.ageIndex);
			hash = HashCombine(hash, std::bit_cast<std::uint64_t>(civ.population));
			hash = HashCombine(hash, static_cast<std::uint64_t>(civ.treasury));
			hash = HashCombine(hash, std::bit_cast<std::uint64_t>(civ.unmetDemand));
			hash = HashCombine(hash, civ.shortageTicks);
			for (const auto &text : civ.specialties)
				hash = HashCombine(hash, Fnv1a64(text));
			for (const auto *values : {&civ.production, &civ.demand, &civ.stockpile})
			{
				hash = HashCombine(hash, values->size());
				for (const auto &[id, value] : *values)
				{
					hash = HashCombine(hash, Fnv1a64(id));
					hash = HashCombine(hash, std::bit_cast<std::uint64_t>(value));
				}
			}
		}
		return hash;
	};
	CHECK(stateHash() == 10355632731109243635ULL);
	for (int tick = 0; tick < 100; ++tick)
		REQUIRE(simulation.AdvanceTick(error));
	CHECK(stateHash() == 12410383246888723003ULL);
}
