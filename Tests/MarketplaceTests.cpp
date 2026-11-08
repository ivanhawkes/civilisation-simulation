#include "Core/ProgressionManager.h"
#include "Core/SimulationConfig.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/CivilisationEffects.h"
#include "Market/Marketplace.h"
#include "TestFramework.h"
#include <string>
#include <vector>

namespace
{
using namespace CivSimCore;

// A world of the scaffold's own making: the credits and the cargo a market settles
// against, in the components the market reads.
std::string AddWorld(
    MarketplaceService &service, EcsWorld &world, const std::string &civilisationId, const std::string &planetName,
    long long credits, const std::map<std::string, double> &cargo)
{
	std::string errorMessage;

	if (!MarketplaceService::AddPlanet(world, civilisationId, planetName, planetName + " System", civilisationId, "Age02", errorMessage))
	{
		return errorMessage;
	}

	if (Treasury *treasury = world.Get<Treasury>(civilisationId))
	{
		treasury->credits = credits;
	}

	if (Stockpile *stock = world.Get<Stockpile>(civilisationId))
	{
		stock->units = cargo;
	}

	return {};
}

OrderRequest Order(
    const std::string &marketId, const std::string &traderId, const std::string &itemId, OrderSide side,
    double quantity, double limitPrice, long long tick)
{
	OrderRequest request;

	request.marketId = marketId;
	request.civilisationId = traderId;
	request.itemId = itemId;
	request.side = side;
	request.quantity = quantity;
	request.limitPrice = limitPrice;
	request.tick = tick;

	return request;
}

long long Credits(const EcsWorld &world, const std::string &civilisationId)
{
	const Treasury *treasury = world.Get<Treasury>(civilisationId);

	return treasury != nullptr ? treasury->credits : 0;
}

double Units(const EcsWorld &world, const std::string &civilisationId, const std::string &itemId)
{
	const Stockpile *stock = world.Get<Stockpile>(civilisationId);

	if (stock == nullptr)
	{
		return 0.0;
	}

	const auto entry = stock->units.find(itemId);

	return entry != stock->units.end() ? entry->second : 0.0;
}

VenueBasis Planet(const std::string &civilisationId = "Civ-01")
{
	VenueBasis basis;

	basis.bodyId = civilisationId;
	basis.venueKind = BodyKind::Planet;
	basis.civilisationId = civilisationId;
	basis.ageId = "Age02";

	return basis;
}
} // namespace


TEST_CASE(a_marketplace_sits_at_a_planet_and_at_a_station_in_orbit_above_one, "MarketplaceService::Open")
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}));
	CHECK(service.VenueBlockReason(world, "Civ-01").empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));
	CHECK(service.HasMarket(world, "Civ-01"));
	CHECK_EQ(service.MarketCount(world), 1u);
	CHECK_EQ(service.MarketIds(world, {}).size(), 1u);
	CHECK_EQ(service.MarketIds(world, "Civ-01").size(), 1u);
	CHECK_EQ(service.MarketIds(world, "Civ-02").size(), 0u);

	const MarketplaceInfo *venue = service.MarketAt(world, "Civ-01");

	CHECK(venue != nullptr);
	CHECK_EQ(venue->marketId, std::string("Civ-01"));
	CHECK_EQ(venue->venueName, std::string("Abigail Prime"));
	CHECK_EQ(venue->civilisationId, std::string("Civ-01"));
	CHECK(venue->atPlanet);
	CHECK(!venue->atSatellite);
	CHECK_EQ(venue->baseListingFeeBps, 40);
	CHECK_EQ(venue->orderLifeTicks, 24);
	CHECK_EQ(venue->venueTake, 0LL);

	// A station above a planet is a place the ships of that planet put in and pull out
	// of, and the trade of a station is the trade of those ships.
	CHECK(MarketplaceService::AddSatellite(world, "Beacon-Station", "Civ-01", "Beacon Station", 420000.0, 4.1, errorMessage));
	CHECK(service.VenueBlockReason(world, "Beacon-Station").empty());
	CHECK(service.Open(world, "Beacon-Station", errorMessage));
	CHECK_EQ(service.MarketCount(world), 2u);
	CHECK_EQ(service.MarketIds(world, "Civ-01").size(), 2u);
	CHECK_EQ(service.MarketIds(world, "Civ-01")[0], std::string("Beacon-Station"));

	const MarketplaceInfo *station = service.MarketAt(world, "Beacon-Station");

	CHECK(station != nullptr);
	CHECK(station->atSatellite);
	CHECK(!station->atPlanet);
	CHECK_EQ(station->civilisationId, std::string("Civ-01"));

	const Orbit *orbit = world.Get<Orbit>("Beacon-Station");

	CHECK(orbit != nullptr);
	CHECK_EQ(orbit->primaryBodyId, std::string("Civ-01"));
	CHECK_EQ(orbit->radiusKm, 420000.0);

	// A star has no market: no people, and the gravity well of a star is a well no
	// market sits at the bottom of.
	CHECK(!MarketplaceService::AddPlanet(world, "Star-01", "Abigail", "Abigail Prime System", "", "", errorMessage));
	CHECK_CONTAINS(errorMessage, "civilisation");

	{
		std::string created;

		CHECK(world.Create("Star-01", created));
		world.Add<BodyInfo>(
		    "Star-01", BodyInfo{"Star-01", BodyKind::Star, "Abigail", {}, "Abigail Prime System", "Civ-01", "Age05"});

		CHECK(!service.Open(world, "Star-01", errorMessage));
		CHECK_CONTAINS(errorMessage, "star");
		CHECK_CONTAINS(errorMessage, "Star-01");
		CHECK(!service.HasMarket(world, "Star-01"));
	}

	// A station that orbits a star is the same argument one step further: the station
	// trade is the trade of the ships that come up a planet's well, and a star has no
	// market at the bottom of its.
	CHECK(MarketplaceService::AddPlanet(world, "Civ-02", "Beacon Reach", "Beacon Reach System", "Civ-02", "Age02", errorMessage));
	{
		std::string created;

		CHECK(world.Create("Moon-01", created));
		world.Add<BodyInfo>(
		    "Moon-01", BodyInfo{"Moon-01", BodyKind::Satellite, "Moon-01", "Star-01", "Abigail Prime System", "Civ-01", {}});

		CHECK(!service.Open(world, "Moon-01", errorMessage));
		CHECK_CONTAINS(errorMessage, "gravity well");
		CHECK_CONTAINS(errorMessage, "Moon-01");
		CHECK_CONTAINS(errorMessage, "Star-01");
	}

	// A station in the scaffold's own form, with the body rule written into the entity
	// the venue is addressed by.
	CHECK(MarketplaceService::AddSatellite(world, "Moon-02", "Star-01", "Moon-02", 1.0, 1.0, errorMessage));
	CHECK_CONTAINS(errorMessage, "star");
	CHECK_CONTAINS(errorMessage, "Star-01");
	CHECK(!world.Has("Moon-02"));

	// A station above nothing: the ships that put in and pull out of it come up from
	// nowhere, and a venue that cannot say whose trade it is takes no order.
	{
		std::string created;

		CHECK(world.Create("Station-01", created));
		world.Add<BodyInfo>(
		    "Station-01", BodyKind{"Station-01", BodyKind::Satellite, "Station-01", {}, "Abigail Prime System", "Civ-01", {}});

		CHECK(!service.Open(world, "Station-01", errorMessage));
		CHECK_CONTAINS(errorMessage, "orbits no planet");
		CHECK_CONTAINS(errorMessage, "Station-01");
	}

	// A body that belongs to no one: the fee, the risk and the take of a venue all
	// belong to a world, and so does the take.
	CHECK(world.Create("Body-01", errorMessage));
	world.Add<BodyInfo>("Body-01", BodyInfo{"Body-01", BodyKind::Planet, "Body-01", {}, "Nowhere", "", {}});
	CHECK(!service.Open(world, "Body-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "no civilisation");

	// Not a body at all: an order, a ship, a lane store.
	CHECK(world.Create("Order-9", errorMessage));
	CHECK(!service.Open(world, "Order-9", errorMessage));
	CHECK_CONTAINS(errorMessage, "BodyInfo");
	CHECK(!service.Open(world, "Nowhere-Prime", errorMessage));
	CHECK_CONTAINS(errorMessage, "Nowhere-Prime");
	CHECK_CONTAINS(errorMessage, "entity");
	CHECK(!service.Open(world, "", errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");

	// A venue twice: one market per body, which is the rule that says a body's venue
	// is the venue of that body.
	CHECK(!service.Open(world, "Civ-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "already");
	CHECK_EQ(service.MarketCount(world), 2u);

	CHECK(service.Close(world, "Civ-01", errorMessage));
	CHECK(!service.HasMarket(world, "Civ-01"));
	CHECK(!service.Close(world, "Civ-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "no marketplace");
	CHECK_EQ(service.MarketCount(world), 1u);
	CHECK_EQ(service.MarketIds(world, "Civ-01").size(), 1u);

	// The venue's record of itself, opened with the venue and gone with it.
	CHECK(world.Get<MarketRecord>("Beacon-Station") != nullptr);
	CHECK(world.Get<MarketRecord>("Civ-01") == nullptr);
}


TEST_CASE(a_resting_order_holds_the_goods_and_the_credits_it_promises, "MarketplaceService::PostOrder")
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 1000, {{"grain", 100.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// The seller's side: the goods leave the seller's stockpile when the order posts,
	// and sit in the order until it fills, leaves the book, or expires.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 40.0, 12.0, 1), orderId, errorMessage));

	CHECK_EQ(orderId, std::string("Order-1"));
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 60.0);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(service.OrdersOf(world, "Civ-01", "Civ-01").size(), 1u);
	CHECK_EQ(service.OrdersOf(world, "Civ-01", "").size(), 1u);
	CHECK_EQ(service.OrdersOf(world, "", "Civ-01").size(), 1u);
	CHECK(service.OrdersOf(world, "Civ-01", "Civ-02").empty());

	// A sell order the seller cannot cover is a promise the venue will not keep: the
	// message says who, how much is held, how much was posted.
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 200.0, 12.0, 1), orderId, errorMessage));

	CHECK_CONTAINS(errorMessage, "Civ-01");
	CHECK_CONTAINS(errorMessage, "iron_ore");
	CHECK_CONTAINS(errorMessage, "60");
	CHECK_CONTAINS(errorMessage, "200");
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 60.0);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);

	// The buyer's side: the credits leave the treasury at the limit price, which is the
	// worst the order can pay, and the venue's fee is in the reservation, because the
	// fee is paid on top of the price of the goods.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 30.0, 10.0, 1), orderId, errorMessage));

	CHECK_EQ(orderId, std::string("Order-2"));
	CHECK_EQ(Credits(world, "Civ-02"), 698);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(service.OpenOrderCount(world, ""), 2u);

	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 1000.0, 10.0, 1), orderId, errorMessage));

	CHECK_CONTAINS(errorMessage, "Civ-02");
	CHECK_CONTAINS(errorMessage, "credits");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 0.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "limit price");
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, -3.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "negative");
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 0.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "nothing");
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-02", "", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "itemId");
	CHECK(!service.PostOrder(world, Order("", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");
	CHECK(!service.PostOrder(world, Order("Civ-03", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-03");
	CHECK_CONTAINS(errorMessage, "Open");
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-03");
	CHECK_CONTAINS(errorMessage, "Stockpile");
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 10.0, 0), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "Stockpile");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// A cancel: the order leaves the book, and the goods and the credits it held are
	// the trader's again, in full, at the price it posted at.
	CHECK(service.CancelOrder(world, "Order-1", errorMessage));

	CHECK(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).empty());
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 100.0);
	CHECK(!service.CancelOrder(world, "Order-1", errorMessage));
	CHECK_CONTAINS(errorMessage, "filled");
	CHECK(!service.CancelOrder(world, "Order-99", errorMessage));
	CHECK_CONTAINS(errorMessage, "Order-99");
	CHECK(!service.CancelOrder(world, "", errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");

	CHECK(service.CancelOrder(world, "Order-2", errorMessage));

	CHECK_EQ(Credits(world, "Civ-02"), 1000);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 0u);
	CHECK(service.OrdersOf(world, "Civ-01", "Civ-02").empty());

	// The venue's own rules: a market of small trades does not take an order of a
	// quarter of a unit, and a venue that has set a price floor does not take an order
	// under it.
	service.SetOptions(MarketplaceOptions{});
	service.Options();

	MarketplaceOptions floors;
	floors.minQuantity = 5.0;
	floors.minPrice = 1.0;
	service.SetOptions(floors);

	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 4.0, 12.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "5");
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 0.5, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "price");
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 5.0, 1.0, 1), orderId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);

	service.SetOptions(MarketplaceOptions{});
}


TEST_CASE(a_post_that_crosses_the_book_trades_at_the_price_that_was_resting, "MarketplaceService::PostOrder")
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 40.0, 12.0, 1), orderId, errorMessage));

	// The buyer who posts at the seller's price has sold: the order that crossed is out
	// of the book and in the fill log, and the price of the fill is the price the
	// resting order named, because the resting order was the book when the buyer came
	// looking.
	const std::string buyId = "Order-2";
	std::vector<TradeFill> fills;

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 40.0, 12.0, 2), orderId, errorMessage));

	CHECK_EQ(orderId, buyId);
	CHECK(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).empty());
	CHECK(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).empty());
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 0u);

	fills = service.Fills(world, "Civ-01");

	CHECK_EQ(fills.size(), 1u);
	CHECK_EQ(fills[0].marketId, std::string("Civ-01"));
	CHECK_EQ(fills[0].itemId, std::string("iron_ore"));
	CHECK_EQ(fills[0].makerOrderId, std::string("Order-1"));
	CHECK_EQ(fills[0].takerOrderId, buyId);
	CHECK_EQ(fills[0].sellerId, std::string("Civ-01"));
	CHECK_EQ(fills[0].buyerId, std::string("Civ-02"));
	CHECK_EQ(fills[0].quantity, 40.0);
	CHECK_EQ(fills[0].unitPrice, 12.0);
	CHECK_EQ(fills[0].gross, 480);

	// The venue is paid by both sides, because the berth and the deck are the venue's,
	// and the money and the goods move on the spot.
	CHECK_EQ(fills[0].makerFee, 2);
	CHECK_EQ(fills[0].takerFee, 2);
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 4);
	CHECK_EQ(Credits(world, "Civ-01"), 1478);
	CHECK_EQ(Credits(world, "Civ-02"), 36);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 60.0);
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 40.0);

	// The quote table of a venue whose whole book just traded: the last price, its tick
	// and the volume, and no best bid and no best ask, because nobody is quoting.
	const MarketQuote *quote = service.QuoteFor(world, "Civ-01", "iron_ore");

	CHECK(quote != nullptr);
	CHECK_EQ(quote->bestAsk, 0.0);
	CHECK_EQ(quote->bestBid, 0.0);
	CHECK_EQ(quote->mid, 0.0);
	CHECK_EQ(quote->spreadBps, 0.0);
	CHECK_EQ(quote->lastPrice, 12.0);
	CHECK_EQ(quote->lastTick, 2);
	CHECK_EQ(quote->volume, 40.0);
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "iron_ore"), 12.0);
	CHECK_EQ(service.SpreadBps(world, "Civ-01", "iron_ore"), 0.0);

	const std::string filled = service.DescribeOrder(world, buyId);

	CHECK(filled.find("no order") != std::string::npos);
	CHECK_CONTAINS(service.DescribeMarket(world, "Civ-01"), "iron_ore");
	CHECK_CONTAINS(service.DescribeMarket(world, "Civ-01"), "12");
	CHECK_CONTAINS(service.DescribeMarket(world, "Civ-99"), "no such venue");

	// Price then time: of two bids at one price, the one that posted first is the
	// maker, and its price is the price of the fill.
	std::string first;
	std::string second;

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 40.0, 10.0, 1), first, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 40.0, 10.0, 1), second, errorMessage));

	CHECK_LT(first, second);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy)[0], first);

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 30.0, 10.0, 3), orderId, errorMessage));

	fills = service.Fills(world, "Civ-01");

	CHECK_EQ(fills.size(), 2u);
	CHECK_EQ(fills[1].makerOrderId, first);
	CHECK_EQ(fills[1].takerOrderId, orderId);
	CHECK_EQ(fills[1].unitPrice, 10.0);
	CHECK_EQ(fills[1].quantity, 30.0);
	CHECK_EQ(fills[1].tick, 3);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 2u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// A cancel that lifts the price at the top of the book lets the orders behind it
	// meet: the bid that was second is the bid a resting ask can trade, and the venue
	// crosses them as soon as the price that was in the way goes away.
	std::string askId;

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 20.0, 11.0, 4), askId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 3u);

	CHECK(service.CancelOrder(world, first, errorMessage));

	fills = service.Fills(world, "Civ-01");

	CHECK_EQ(fills.size(), 3u);
	CHECK_EQ(fills[2].makerOrderId, second);
	CHECK_EQ(fills[2].unitPrice, 10.0);
	CHECK_EQ(fills[2].quantity, 10.0);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 1u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 1u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 70.0);
	CHECK_EQ(Credits(world, "Civ-01"), 1576);

	CHECK(!service.CancelOrder(world, askId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
}


TEST_CASE(an_order_that_walks_the_book_pays_the_best_price_first, "MarketplaceService::ExecuteOrder")
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 1000, {}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 30.0}}));
	CHECK(AddWorld(service, world, "Civ-03", "Cinder Port", 1000, {{"iron_ore", 30.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// The book, resting first: three asks at 10, 11 and 12, and two bids at 9.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 11.0, 2), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 3), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 8.0, 9.0, 1), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 2.0, 9.0, 2), orderId, errorMessage));

	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 3u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 2u);
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 0);

	// The order that walks the book: the aggressor pays the resting prices, best
	// first, and leaves nothing resting behind.
	const std::vector<TradeFill> fills = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 12.0, 0.0, 5), errorMessage);

	CHECK_EQ(fills.size(), 2u);
	CHECK_EQ(fills[0].unitPrice, 10.0);
	CHECK_EQ(fills[0].quantity, 10.0);
	CHECK_EQ(fills[0].makerOrderId, std::string("Order-1"));
	CHECK_EQ(fills[0].takerOrderId, std::string("Order-6"));
	CHECK_EQ(fills[0].sellerId, std::string("Civ-02"));
	CHECK_EQ(fills[0].buyerId, std::string("Civ-01"));
	CHECK_EQ(fills[0].gross, 100);
	CHECK_EQ(fills[0].tick, 5);
	CHECK_EQ(fills[1].unitPrice, 11.0);
	CHECK_EQ(fills[1].quantity, 2.0);
	CHECK_EQ(fills[1].makerOrderId, std::string("Order-2"));
	CHECK_EQ(fills[1].gross, 22);
	CHECK_EQ(fills[1].tick, 5);

	// The walk leaves no order of its own in the book: an order that was posted to
	// take is an order that took, and it left with the taking.
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 4u);
	CHECK_EQ(service.OrdersOf(world, "Civ-01", "Civ-01").size(), 0u);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 2u);
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 8);

	// The taker paid 100 at 10 and 22 at 11, and the venue's 40 bps of each: the fee
	// of the walk is the taker's fee, and the fee of the resting orders is theirs.
	CHECK_EQ(Credits(world, "Civ-01"), 1000 - 100 - 4 - 22 - 1);
	CHECK_EQ(Credits(world, "Civ-02"), 1000 + 100 - 4 + 22 - 1);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 12.0);
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 8.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->volume, 12.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->lastPrice, 11.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->lastTick, 5);

	// The best ask after the walk: the 11 ask with 8 units left, the 12 ask, and the
	// bids at 9 that the walk never reached.
	double bestAsk = 0.0;
	double bestBid = 0.0;

	CHECK(service.BestAsk(world, "Civ-01", "iron_ore", bestAsk));
	CHECK(service.BestBid(world, "Civ-01", "iron_ore", bestBid));
	CHECK_EQ(bestAsk, 11.0);
	CHECK_EQ(bestBid, 9.0);
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 0.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->askOrders, 2u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bidOrders, 2u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->askDepth, 8.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bidDepth, 10.0);

	// The walk's own limit: an order that names a price refuses the prices above it,
	// and the book is sorted best first, so the first price it refuses is the end of
	// the walk.
	const std::vector<TradeFill> limited = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 40.0, 10.5, 6), errorMessage);

	CHECK_EQ(limited.size(), 0u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 4u);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 2u);
	CHECK_EQ(Credits(world, "Civ-01"), 873);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 12.0);

	// A sell order walks the bids: the best bid first, at the bids' own prices, and
	// the second bid at the second bid's price, which is the same rule read from the
	// other side of the book.
	const std::vector<TradeFill> sold = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 12.0, 0.0, 7), errorMessage);

	CHECK_EQ(sold.size(), 2u);
	CHECK_EQ(sold[0].unitPrice, 9.0);
	CHECK_EQ(sold[0].quantity, 8.0);
	CHECK_EQ(sold[0].makerOrderId, std::string("Order-4"));
	CHECK_EQ(sold[0].sellerId, std::string("Civ-02"));
	CHECK_EQ(sold[0].buyerId, std::string("Civ-03"));
	CHECK_EQ(sold[1].unitPrice, 9.0);
	CHECK_EQ(sold[1].quantity, 2.0);
	CHECK_EQ(sold[1].makerOrderId, std::string("Order-5"));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 0.0);
	CHECK_EQ(Units(world, "Civ-03", "iron_ore"), 10.0);

	// A walk of one unit of a book that has 8, and the order it walked leaves 8 in
	// the book: the walk stops when it has taken all it asked for, and the resting
	// order it left behind is the order the next walk meets.
	const std::vector<TradeFill> part = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 3.0, 0.0, 8), errorMessage);

	CHECK_EQ(part.size(), 1u);
	CHECK_EQ(part[0].quantity, 3.0);
	CHECK_EQ(part[0].unitPrice, 11.0);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 2u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->askDepth, 5.0);
	CHECK(service.BestAsk(world, "Civ-01", "iron_ore", bestAsk));
	CHECK_EQ(bestAsk, 11.0);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// A walk against a book with nothing on the other side: the order that walks the
	// book has to be given a limit to be posted at, and a book with no bids has no
	// limit for a sell to walk, which is the message that says why nothing happened.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 10.0, 6.0, 9), orderId, errorMessage));
	CHECK(service.ExecuteOrder(world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 10.0, 0.0, 9), errorMessage).empty());
	CHECK(!service.ExecuteOrder(world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 10.0, 0.0, 9), errorMessage).empty());
	CHECK_CONTAINS(errorMessage, "grain");
	CHECK_CONTAINS(errorMessage, "price");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 3u);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 5u);

	CHECK(service.ExecuteOrder(world, Order("Civ-01", "Civ-01", "grain", OrderSide::Sell, 10.0, 6.0, 10), errorMessage).empty());
	CHECK(!service.ExecuteOrder(world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 0.0, 0.0, 10), errorMessage).empty());
	CHECK_CONTAINS(errorMessage, "nothing");
	CHECK(!service.ExecuteOrder(world, Order("Civ-99", "Civ-01", "grain", OrderSide::Buy, 10.0, 10.0, 10), errorMessage).empty());
	CHECK_CONTAINS(errorMessage, "Civ-99");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 3u);
}


TEST_CASE(the_book_is_the_book_the_quoters_read, "MarketplaceService::RefreshQuotes")
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 10000, {{"iron_ore", 100.0}, {"grain", 100.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 10000, {{"iron_ore", 100.0}, {"grain", 100.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 1), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 2), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 13.0, 3), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 8.0, 10.0, 4), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 6.0, 9.0, 5), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 20.0, 4.0, 6), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 15.0, 5.0, 7), orderId, errorMessage));

	CHECK(service.RefreshQuotes(world, "Civ-01", errorMessage));

	const std::vector<MarketQuote> quotes = service.Quotes(world, "Civ-01");

	// Every good the book quotes, in sorted item order, which is the order a list of
	// goods on a screen is written in.
	CHECK_EQ(quotes.size(), 2u);
	CHECK_EQ(quotes[0].itemId, std::string("grain"));
	CHECK_EQ(quotes[1].itemId, std::string("iron_ore"));

	const MarketQuote *iron = service.QuoteFor(world, "Civ-01", "iron_ore");

	CHECK(iron != nullptr);
	CHECK_EQ(iron->bestAsk, 12.0);
	CHECK_EQ(iron->bestBid, 10.0);
	CHECK_EQ(iron->askOrders, 2u);
	CHECK_EQ(iron->bidOrders, 2u);

	// The depth at the best price, which is the size a marketable order takes before
	// the price moves.
	CHECK_EQ(iron->askDepth, 20.0);
	CHECK_EQ(iron->bidDepth, 8.0);

	// A price the fill log has never seen: the mid of the two quotes, and the gap
	// between them in basis points of that mid, which is the number beside the price.
	CHECK_EQ(iron->mid, 11.0);
	CHECK_NEAR(iron->spreadBps, 1818.1818181818, 1e-9);
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "iron_ore"), 11.0);
	CHECK_EQ(service.SpreadBps(world, "Civ-01", "iron_ore"), iron->spreadBps);
	CHECK_EQ(iron->lastPrice, 0.0);
	CHECK_EQ(iron->lastTick, 0);
	CHECK_EQ(iron->volume, 0.0);

	const MarketQuote *grain = service.QuoteFor(world, "Civ-01", "grain");

	CHECK(grain != nullptr);
	CHECK_EQ(grain->bestAsk, 5.0);
	CHECK_EQ(grain->bestBid, 4.0);
	CHECK_EQ(grain->mid, 4.5);
	CHECK_NEAR(grain->spreadBps, 2222.2222222222, 1e-9);
	CHECK_EQ(grain->askDepth, 15.0);
	CHECK_EQ(grain->bidDepth, 20.0);

	// One side of a book: the reference price of a good nobody is buying is the ask,
	// and a good nobody is quoting has a mid of nothing, which is what a screen shows
	// as "nobody is bidding for it".
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "water", OrderSide::Sell, 5.0, 30.0, 8), orderId, errorMessage));
	CHECK(service.RefreshQuotes(world, "Civ-01", errorMessage));

	const MarketQuote *water = service.QuoteFor(world, "Civ-01", "water");

	CHECK(water != nullptr);
	CHECK_EQ(water->bestAsk, 30.0);
	CHECK_EQ(water->bestBid, 0.0);
	CHECK_EQ(water->mid, 0.0);
	CHECK_EQ(water->spreadBps, 0.0);
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "water"), 30.0);
	CHECK_EQ(service.Quotes(world, "Civ-01").size(), 3u);
	CHECK(service.QuoteFor(world, "Civ-01", "grain") != nullptr);
	CHECK(service.QuoteFor(world, "Civ-01", "unobtainium") == nullptr);
	CHECK(!service.BestAsk(world, "Civ-01", "grain", bestAsk));
	CHECK(service.BestBid(world, "Civ-01", "grain", bestBid));
	CHECK_EQ(bestBid, 4.0);
	CHECK(!service.MidPrice(world, "Civ-01", "water", bestAsk));
	CHECK(!service.BestAsk(world, "Civ-99", "iron_ore", bestAsk));
	CHECK(!service.BestBid(world, "Civ-99", "iron_ore", bestBid));
	CHECK(!service.MidPrice(world, "Civ-99", "iron_ore", bestAsk));
	CHECK_EQ(service.ReferencePrice(world, "Civ-99", "iron_ore"), 0.0);
	CHECK_EQ(service.SpreadBps(world, "Civ-99", "iron_ore"), 0.0);
	CHECK(service.Quotes(world, "Civ-99").empty());
	CHECK(service.Fills(world, "Civ-99").empty());

	// The whole book cancelled: the quotes keep the price of the last trade, and a
	// table that dropped a good nobody is quoting would print nothing beside a good
	// that just traded.
	CHECK(service.CancelOrder(world, orderId, errorMessage));

	while (service.OpenOrderCount(world, "Civ-01") > 0)
	{
		const std::vector<MarketOrderInfo> orders = service.OrdersOf(world, "Civ-01", "");

		for (const MarketOrderInfo &order : orders)
		{
			service.CancelOrder(world, order.orderId, errorMessage);
		}
	}

	CHECK(service.RefreshQuotes(world, "Civ-01", errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 0u);
	CHECK_EQ(service.Quotes(world, "Civ-01").size(), 3u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestAsk, 0.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestBid, 0.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->volume, 0.0);
	CHECK(!service.BestAsk(world, "Civ-01", "iron_ore", bestAsk));
	CHECK(!service.BestBid(world, "Civ-01", "iron_ore", bestBid));
	CHECK(!service.MidPrice(world, "Civ-01", "iron_ore", bestAsk));
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "iron_ore"), 0.0);

	CHECK(service.CancelOrder(world, "Order-1", errorMessage));
	CHECK(!service.RefreshQuotes(world, "Civ-99", errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-99");
	CHECK(service.MatchBook(world, "Civ-01", errorMessage).empty());
	CHECK(service.MatchBook(world, "Civ-99", errorMessage).empty());
	CHECK_CONTAINS(errorMessage, "Civ-99");

	// The fill of a book that crossed at the quote the table was built from: the last
	// price and the volume move, and the mid that was the reference goes to the price
	// the trade left behind.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 9), orderId, errorMessage));

	const std::vector<TradeFill> fills = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 0.0, 10), errorMessage);

	CHECK_EQ(fills.size(), 1u);
	CHECK_EQ(fills[0].unitPrice, 12.0);
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "iron_ore"), 12.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->volume, 10.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->lastTick, 10);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "water")->volume, 0.0);
}


TEST_CASE(the_deck_of_a_venue_holds_just_so_many_units_of_one_good, "MarketplaceService::DepthUnits")
{
	MarketplaceOptions options;
	options.depthUnits = 30.0;

	MarketplaceService service(options);
	CivilisationEffectEngine effects;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 100000, {{"iron_ore", 1000.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 100000, {{"iron_ore", 1000.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// A venue with a deck of 30 units of a good: the units one side of one good's book
	// may hold, which is the queue the berth can clear in a day.
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 30.0);
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 20.0, 12.0, 1), orderId, errorMessage));
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 15.0, 12.0, 2), orderId, errorMessage));

	CHECK_CONTAINS(errorMessage, "20");
	CHECK_CONTAINS(errorMessage, "30");
	CHECK_CONTAINS(errorMessage, "Sell");
	CHECK_CONTAINS(errorMessage, "iron_ore");

	// The other side of the same book has the same room: the deck is the deck of one
	// side of one good, and a bid of 30 against an ask of 20 is a venue that clears
	// both sides of a book of 30.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 30.0, 12.0, 3), orderId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "grain", OrderSide::Buy, 30.0, 3.0, 4), orderId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 3u);

	// The safe harbours of the law tree: a berth that clears, and a lane that is
	// worth less, is a venue with a deeper book, because the depth a venue has is the
	// depth the harbours and the tribunal buy.
	CHECK(effects.Apply(world, "Civ-01", "SkillPiracySafeHarbours", "SafeHarbours", EffectOrigin::Skill, 1, errorMessage));
	CHECK_EQ(engineReads(effects, world, "Civ-01"), 40.0);

	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 70.0);
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 15.0, 12.0, 5), orderId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 4u);
	CHECK(!service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 20.0, 12.0, 6), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "35");
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -8);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 32);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 8);

	// A venue that has set no depth is a venue that quoters arrive at, not a venue
	// that turns them away.
	service.SetOptions(MarketplaceOptions{});

	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 0.0);
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 500.0, 12.0, 7), orderId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 5u);
	CHECK_EQ(service.DepthUnits(world, "Civ-99"), 0.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-99"), 40);
	CHECK_EQ(service.InsuranceBps(world, "Civ-99"), 0);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-99"), 0);
}


TEST_CASE(a_market_effect_moves_the_charge_of_the_venue_it_lands_on, "MarketplaceService::ListingFeeBps")
{
	CivilisationEffectEngine effects;
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 100000, {{"iron_ore", 1000.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 100000, {{"iron_ore", 1000.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));
	CHECK(service.Open(world, "Civ-02", errorMessage));
	CHECK(service.Open(world, "Beacon-Station", errorMessage). == false);

	// The venue's own charges, and the risk of the place and the lane: a plain venue
	// charges its base 40 basis points a side, and takes no insurance.
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), 0);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 8);

	// The licence and the warrant: the deck clears, and the berth is cheaper, one
	// basis point for every two berths a day the licence clears.
	CHECK(effects.Apply(world, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4, errorMessage));

	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 80.0);
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium), 24.0);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), 24);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40 + 24 - 40);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 40);

	// The tribunal: the trade that is settled is a trade worth less, and a venue with
	// a court of record is a venue with a deeper book, because a ship that knows the
	// contract will be kept puts in.
	CHECK(effects.Apply(world, "Civ-01", "SkillTradeTribunal", "TradeTribunal", EffectOrigin::Skill, 2, errorMessage));
	CHECK(effects.Apply(world, "Civ-01", "SkillPiracySafeHarbours", "SafeHarbours", EffectOrigin::Skill, 1, errorMessage));

	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), 24 + 16 - 8);
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketDepth), 60.0);
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketBypass), 30.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40 + 32 - 40);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 8);

	// The hull insurance of the trade tree: a policy written against the hull, in
	// basis points of the hull's value, and a venue that writes the policy is a venue
	// that charges for it.
	CHECK(effects.Apply(world, "Civ-01", "SkillTradeCargoInsurance", "CargoInsurance", EffectOrigin::Skill, 5, errorMessage));

	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 8 + 20);
	CHECK_EQ(service.InsuranceBps(world, "Civ-02"), 8);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 40);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), 0);

	// The fence network of the piracy tree: a market that takes stolen cargo is a
	// market whose risk is higher, and the risk is the fee, which is the way a venue
	// that trades in stolen goods charges for it.
	CHECK(effects.Apply(world, "Civ-01", "SkillPiracyFencingNetwork", "FencingNetwork", EffectOrigin::Skill, 3, errorMessage));

	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), 32 + 18);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40 + 50 - 40);

	// A venue whose charges the tree has driven below nothing charges nothing: the
	// licence clears so many berths a day that the berth is free, and a venue does not
	// pay a trader to take an order.
	CHECK(effects.Apply(world, "Civ-01", "SkillLawDockingLicence", "DockingLicence", EffectOrigin::Skill, 5, errorMessage));
	CHECK(effects.Apply(world, "Civ-01", "SkillPiracyPiratePardon", "PiratePardon", EffectOrigin::Skill, 3, errorMessage));

	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 160.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
	CHECK_GE(service.ListingFeeBps(world, "Civ-01"), 0);

	// A station of the same civilisation reads the same charges, because the licence,
	// the tribunal and the pardon are laws of the world and not rules of one place.
	CHECK(MarketplaceService::AddSatellite(world, "Beacon-Station", "Civ-01", "Beacon Station", 1.0, 1.0, errorMessage));
	CHECK(service.Open(world, "Beacon-Station", errorMessage));

	CHECK_EQ(service.ListingFeeBps(world, "Beacon-Station"), 0);
	CHECK_EQ(service.InsuranceBps(world, "Beacon-Station"), 28);
	CHECK_EQ(service.RiskPremiumBps(world, "Beacon-Station"), 50);
	CHECK_EQ(service.MarketIds(world, "Civ-01").size(), 2u);
	CHECK_EQ(service.MarketIds(world, "Civ-02").size(), 1u);

	// The lanes of the civilisation, read through the venue: a convoy is a lane worth
	// less, and the risk of the place is the risk of the place and the risk of the
	// lane the goods came down.
	CHECK(effects.Apply(world, "Civ-01", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 3, errorMessage));
	CHECK(effects.Apply(world, "Civ-02", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 1, errorMessage));

	CHECK_EQ(service.RiskPremiumBps(world, "Beacon-Station"), 50 - 36);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), -12);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 28);
	CHECK_EQ(service.InsuranceBps(world, "Civ-02"), 8);

	// A ship effect is not a venue's: the market engine reads the stores the civilisation
	// effect engine wrote, and a hull is not a store of a venue.
	CHECK(!effects.Apply(world, "Civ-01", "SkillPiracyRaiderHulls", "RaiderHulls", EffectOrigin::Skill, 2, errorMessage));
	CHECK_CONTAINS(errorMessage, "ShipEffectEngine");
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), 32);
}


TEST_CASE(an_order_that_outlives_its_life_leaves_the_book, "MarketplaceService::AdvanceTick")
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 20.0, 10.0, 10), orderId, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 30.0, 12), orderId, errorMessage));

	const MarketOrderInfo *first = world.Get<MarketOrderInfo>("Order-1");

	CHECK(first != nullptr);
	CHECK_EQ(first->placedTick, 10);

	// The venue's order life, read from the venue's own record of itself: 24 ticks,
	// the berths a day needs, and the order that posted at tick 10 leaves at tick 34.
	CHECK_EQ(first->expiresTick, 34);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Credits(world, "Civ-02"), 798);

	// A tick before the order leaves: the venue's quotes are refreshed, and the order
	// is where it was.
	CHECK_EQ(service.AdvanceTick(world, 33, errorMessage), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Credits(world, "Civ-02"), 798);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestBid, 10.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestAsk, 30.0);

	// The tick the order outlives: the order leaves, and the credits it held are the
	// trader's again, because the reservation of an order that did not fill is not a
	// fee, and the berths it held are the berths this tick's quoters need.
	CHECK_EQ(service.AdvanceTick(world, 34, errorMessage), 1u);

	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(Credits(world, "Civ-02"), 1000);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 0u);
	CHECK_EQ(service.OrdersOf(world, "Civ-01", "Civ-02").size(), 0u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestBid, 0.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestAsk, 30.0);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 0u);

	// An order of a venue that sets no order life never leaves: an order that rests
	// without a life is an order that rests until it trades or is cancelled, and a
	// venue with no order life is a venue that keeps its book.
	MarketplaceOptions noLife;
	noLife.orderLifeTicks = 0;
	service.SetOptions(noLife);

	std::string immortal;

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 5.0, 10.0, 40), immortal, errorMessage));
	CHECK_EQ(world.Get<MarketOrderInfo>(immortal)->expiresTick, 40);
	CHECK_EQ(service.AdvanceTick(world, 1000, errorMessage), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Credits(world, "Civ-02"), 948);
	CHECK_EQ(service.AdvanceTick(world, 1000, errorMessage), 1u);

	service.SetOptions(MarketplaceOptions{});

	// Every venue of the world, in sorted venue order, and the venues of one
	// civilisation are the venues the venue's own record of itself is written for.
	CHECK(service.Open(world, "Civ-02", errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-02", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 8.0, 50), orderId, errorMessage));
	CHECK_EQ(service.AdvanceTick(world, 100, errorMessage), 2u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-02"), 0u);
	CHECK_EQ(Credits(world, "Civ-01"), 918 + 30 - 1);
	CHECK_EQ(Credits(world, "Civ-02"), 1000);

	// The close of a venue in the middle of a session: every order of the book goes
	// back to the trader that posted it, because a venue that closes in the middle of
	// a session does not keep the goods or the credits of the orders it was holding.
	std::string heldSell;
	std::string heldBuy;

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 30.0, 40.0, 60), heldSell, errorMessage));
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 60.0, 61), heldBuy, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 70.0);

	CHECK(service.Close(world, "Civ-01", errorMessage));

	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 0u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 100.0);
	CHECK_EQ(Credits(world, "Civ-02"), 99960);
	CHECK(!world.Has("Order-1"));
	CHECK(world.Has("Civ-01"));
	CHECK(world.Get<MarketEffects>("Civ-01") != nullptr);
	CHECK(world.Get<MarketRecord>("Civ-01") == nullptr);
	CHECK(service.Open(world, "Civ-01", errorMessage));
	CHECK_EQ(service.MarketAt(world, "Civ-01")->baseListingFeeBps, 40);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
}


TEST_CASE(the_marketplace_reads_the_tree_of_the_civilisation_that_owns_the_venue)
{
	CivilisationEffectEngine effects;
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	const std::vector<SkillNode> nodes = {
	    Node("Age02", "Age02", "AgeGate", 1, 1, "", 0),
	    Node("SkillLawDockingLicence", "Age02", "Law", 4, 1, "DockingLicence", 1),
	    Node("SkillTradeCargoInsurance", "Age02", "Trade", 4, 2, "CargoInsurance", 1),
	    Node("SkillPiracyConvoy", "Age03", "ShipHull", 3, 2, "Convoy", 1),
	    Node("SkillShipShipyardsDrydock", "Age04", "ShipYard", 3, 4, "Drydock", 1),
	    Node("SkillShipShieldsFront", "Age02", "ShipSystem", 5, 2, "ShieldFront", 1),
	};

	ProgressionManager progression = Tree(nodes, 30);

	CHECK(progression.TryUpgrade("SkillLawDockingLicence", errorMessage));
	CHECK(progression.TryUpgrade("SkillTradeCargoInsurance", errorMessage));
	CHECK(progression.TryUpgrade("SkillPiracyConvoy", errorMessage));
	CHECK(progression.TryUpgrade("SkillShipShipyardsDrydock", errorMessage));

	unsigned applied = 0;
	unsigned skipped = 0;

	// The service's own entry point for a tree: the civilisation effect engine writes
	// the stores, and the venue reads them. The ship system is the ship engine's, and
	// the age gate names no effect, and a yard is a store of the civilisation and not
	// a charge of a venue, so it lands in a yard store and moves no fee.
	CHECK(service.ApplyProgression(world, progression, effects, "Civ-01", applied, skipped, errorMessage));

	CHECK_EQ(applied, 4u);
	CHECK_EQ(skipped, 2u);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -12);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40 - 12 - 10);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 12);
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 0.0);
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Yard, "Yard-Civ-01", EffectStat::DrydockThroughput), 10.0);
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-02", EffectStat::MarketInsurance), 0.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 40);
	CHECK_EQ(service.InsuranceBps(world, "Civ-02"), 8);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), 0);
	CHECK(service.QuoteFor(world, "Civ-01", "iron_ore") != nullptr);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 0u);

	CHECK(!service.ApplyProgression(world, progression, effects, "Civ-99", applied, skipped, errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-99");

	// A fill at the fee the venue charges at the tick the order posted at: a tree that
	// moves the charge of a venue moves the charge of the orders that post after it,
	// and the orders that are already in the book keep the fee they posted at, because
	// a trader that posted at 40 basis points pays 40 basis points.
	std::string cheap;

	service.SetOptions(MarketplaceOptions{});
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 10.0, 1), cheap, errorMessage));

	const std::vector<TradeFill> fills = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 0.0, 2), errorMessage);

	CHECK_EQ(fills.size(), 1u);
	CHECK_EQ(fills[0].gross, 100);
	CHECK_EQ(fills[0].makerFee, 1);
	CHECK_EQ(fills[0].takerFee, 1);
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 2);
	CHECK_EQ(Credits(world, "Civ-01"), 1000 + 100 - 1);
	CHECK_EQ(Credits(world, "Civ-02"), 1000 - 100 - 1);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 1u);

	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 12);
	CHECK(service.MarketAt(world, "Civ-01")->venueTake > 0);
	CHECK(effects.DescribeCivilisation(world, "Civ-01").find("CargoInsurance") != std::string::npos);
}


TEST_CASE(a_fill_trades_in_whole_units_of_the_price_and_the_fee, "MarketplaceService::FeeFor")
{
	MarketplaceOptions options;
	options.requireFunds = false;

	MarketplaceService service(options);
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(service, world, "Civ-01", "Abigail Prime", 60, {{"iron_ore", 100.0}}));
	CHECK(AddWorld(service, world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}));
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// The rounding rule, in the arithmetic the ledger needs: the venue's 40 basis
	// points of 100 credits is 1 credit, and the fraction it was owed is not paid out
	// of its own pocket.
	CHECK_EQ(MarketplaceService::FeeFor(100, 40), 1);
	CHECK_EQ(MarketplaceService::FeeFor(100, 0), 0);
	CHECK_EQ(MarketplaceService::FeeFor(100, -5), 0);
	CHECK_EQ(MarketplaceService::FeeFor(0, 40), 0);
	CHECK_EQ(MarketplaceService::FeeFor(-5, 40), 0);
	CHECK_EQ(MarketplaceService::FeeFor(1, 1), 1);
	CHECK_EQ(MarketplaceService::FeeFor(9999, 1), 1);
	CHECK_EQ(MarketplaceService::FeeFor(10000, 1), 1);
	CHECK_EQ(MarketplaceService::FeeFor(10001, 1), 2);
	CHECK_EQ(MarketplaceService::FeeFor(250, 40), 1);
	CHECK_EQ(MarketplaceService::FeeFor(1000, 40), 4);
	CHECK_EQ(MarketplaceService::FeeFor(1000000, 1500), 150000);
	CHECK_EQ(MarketplaceService::FeeFor(1000000, 10000), 1000000);

	// The whole a budget pays: the goods and the fee on them, in integer arithmetic,
	// and the units that whole buys. The invariant the tests read is that the whole
	// never costs more than the budget.
	CHECK_NEAR(MarketplaceService::AffordableQuantity(10.0, 40, 104), 10.0, 1e-9);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 40, 100), 9.9);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 0, 100), 10.0);
	CHECK_EQ(MarketplaceService::AffordableQuantity(0.0, 40, 100), 0.0);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 40, 0), 0.0);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 40, -1), 0.0);
	CHECK_NEAR(MarketplaceService::AffordableQuantity(0.25, 40, 1000000), 3984000.0, 1e-6);

	for (const long long budget : {1, 7, 33, 60, 100, 999, 100000, 1000000000LL})
	{
		for (const int fee : {0, 1, 40, 250, 1500, 10000})
		{
			for (const double price : {0.25, 1.0, 9.99, 10.0, 137.0})
			{
				const double quantity = MarketplaceService::AffordableQuantity(price, fee, budget);

				CHECK_GE(quantity, 0.0);

				if (quantity > 0.0)
				{
					const long long gross = std::llround(price * quantity);

					CHECK_LE(gross + MarketplaceService::FeeFor(gross, fee), budget);
					CHECK_LE(quantity, static_cast<double>(budget) / price + 1e-9);
				}
			}
		}
	}

	// The fill of a buy order the buyer's money cannot pay for: a venue that allows a
	// buy order the buyer cannot pay - the game of promises, off by default - takes the
	// money that is there and trades the units it buys, which is the fill that leaves
	// the ledger exact to the last credit.
	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_EQ(Credits(world, "Civ-01"), 60);
	CHECK_EQ(world.Get<MarketOrderInfo>(orderId)->reservedCredits, 0);

	CHECK(service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 10.0, 2), orderId, errorMessage));

	const std::vector<TradeFill> fills = service.Fills(world, "Civ-01");

	CHECK_EQ(fills.size(), 1u);
	CHECK_EQ(fills[0].quantity, 5.9);
	CHECK_EQ(fills[0].unitPrice, 10.0);
	CHECK_EQ(fills[0].gross, 59);
	CHECK_EQ(fills[0].makerFee, 1);
	CHECK_EQ(fills[0].takerFee, 1);
	CHECK_EQ(Credits(world, "Civ-01"), 0);
	CHECK_EQ(Credits(world, "Civ-02"), 1000 + 59 - 1);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 5.9);
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 94.1);
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 2);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 0u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 1u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->volume, 5.9);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->lastPrice, 10.0);

	// A sell order for cargo no one holds: the promises venue, and the goods are taken
	// from where they are, at the fill, and the fill takes only what is there.
	MarketplaceOptions promises;
	promises.requireStock = false;
	promises.requireFunds = false;
	service.SetOptions(promises);

	CHECK(AddWorld(service, world, "Civ-03", "Cinder Port", 1000, {{"grain", 4.0}}));
	CHECK(service.Open(world, "Civ-03", errorMessage));

	std::string promise;

	CHECK(service.PostOrder(world, Order("Civ-03", "Civ-03", "grain", OrderSide::Sell, 100.0, 10.0, 3), promise, errorMessage));
	CHECK_EQ(world.Get<MarketOrderInfo>(promise)->reservedGoods, 0.0);
	CHECK_EQ(Units(world, "Civ-03", "grain"), 4.0);

	const std::vector<TradeFill> promised = service.ExecuteOrder(
	    world, Order("Civ-03", "Civ-01", "grain", OrderSide::Buy, 100.0, 0.0, 4), errorMessage);

	CHECK_EQ(promised.size(), 1u);
	CHECK_EQ(promised[0].quantity, 4.0);
	CHECK_EQ(promised[0].gross, 40);
	CHECK_EQ(Units(world, "Civ-03", "grain"), 0.0);
	CHECK_EQ(Units(world, "Civ-01", "grain"), 4.0);
	CHECK_EQ(Credits(world, "Civ-03"), 1000 + 40 - 1);
	CHECK_EQ(Credits(world, "Civ-01"), 60 - 40 - 1);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-03"), 1u);
	CHECK_EQ(service.Book(world, "Civ-03", "grain", OrderSide::Sell).size(), 1u);

	// A sell order whose seller has nothing to sell trades nothing: the book has the
	// price, the pair cannot trade a whole unit, and the venue's log says so in fills
	// that are not there.
	CHECK(service.PostOrder(world, Order("Civ-03", "Civ-03", "water", OrderSide::Sell, 10.0, 10.0, 5), orderId, errorMessage));

	CHECK(service.ExecuteOrder(world, Order("Civ-03", "Civ-01", "water", OrderSide::Buy, 10.0, 0.0, 6), errorMessage).empty());
	CHECK_EQ(service.OpenOrderCount(world, "Civ-03"), 2u);
	CHECK_EQ(service.Fills(world, "Civ-03").size(), 1u);
	CHECK_EQ(Units(world, "Civ-03", "water"), 0.0);
	CHECK_EQ(Credits(world, "Civ-01"), 19);
	CHECK(service.BestAsk(world, "Civ-03", "water", bestAsk));
	CHECK_EQ(bestAsk, 10.0);

	service.SetOptions(MarketplaceOptions{});
}


TEST_CASE(a_marketplace_loads_the_world_the_generator_built, "MarketplaceService::LoadSimulation")
{
	const uint64_t seed = 1301;

	SimulationConfig config;

	config.Systems = PlanetarySystemConfig{1, 3, 0, 0};
	config.Economy.worldStartingTreasury = 5000;
	config.Economy.resourcesFactor = 1.0;
	config.Economy.populationCapacityPerResourceUnit = 2000000.0;
	config.Economy.populationBaseConsumptionPerCapita = 0.01;
	config.Economy.productionBaseRate = 0.15;
	config.Economy.demandSatietyBand = 0.25;
	config.Economy.civilisationNames = {"Abigail Prime", "Beacon Reach", "Cinder Port"};
	config.Economy.civilisationSpecies = {"Human", "Human", "Human"};
	config.Economy.civilisationGovernmentTypes = {"Republic", "Federation", "Empire"};
	config.Economy.resources = {"iron_ore", "nickel_ore", "water", "grain"};
	config.Civilisations = CivilisationConfig{3, {}, {}, {}, {}, {}, {}, {}};

	Simulation sim(config);

	CHECK(sim.GenerateWorld(seed));

	const std::vector<Civilisation> &worlds = sim.GetCivilisations();

	CHECK_EQ(worlds.size(), 3u);

	EcsWorld store;
	std::size_t venues = 0;
	std::string errorMessage;

	CHECK(MarketplaceService::LoadSimulation(store, sim, MarketplaceOptions{}, venues, errorMessage));
	CHECK_EQ(venues, worlds.size());
	CHECK_EQ(MarketplaceService::MarketCount(store), worlds.size());

	std::vector<std::string> markets;

	for (const Civilisation &world : worlds)
	{
		markets.push_back(world.id);

		CHECK(store.Has(world.id));
		CHECK(MarketplaceService::HasMarket(store, world.id));

		const MarketplaceInfo *venue = MarketplaceService::MarketAt(store, world.id);

		CHECK(venue != nullptr);
		CHECK_EQ(venue->civilisationId, world.id);
		CHECK_EQ(venue->venueName, world.name);
		CHECK(venue->atPlanet);

		// The economy model's own numbers in the market's own components: the same
		// stockpile, the same treasury, the same decks, so a market settles against the
		// world the tick model produced and consumed, and a trade that takes a tonne of
		// ore takes a tonne out of the stockpile the tick model reads back.
		CHECK_EQ(Credits(store, world.id), world.treasury);
		CHECK_NEAR(Units(store, world.id, "iron_ore"), StockOf(world, "iron_ore"), 1e-12);
		CHECK_EQ(store.Get<Population>(world.id)->current, world.population);
		CHECK_EQ(store.Get<Production>(world.id)->output.size(), world.production.size());
		CHECK_EQ(store.Get<Demand>(world.id)->want.size(), world.demand.size());
	}

	std::sort(markets.begin(), markets.end());

	CHECK_EQ(MarketplaceService::MarketIds(store, {}), markets);
	CHECK_EQ(MarketplaceService::MarketIds(store, worlds[0].id).size(), 1u);
	CHECK_EQ(MarketplaceService::MarketIds(store, "Civ-99").size(), 0u);

	// The market of a loaded world is a venue the loaded world trades at, and the
	// goods of a loaded world are the goods its manifest gives it.
	const std::string venueId = markets[0];
	const std::string buyer = worlds[0].id == venueId ? worlds[1].id : worlds[0].id;
	const std::string seller = worlds[0].id == venueId ? worlds[0].id : worlds[1].id;

	std::string good;

	for (const auto &entry : store.Get<Stockpile>(seller)->units)
	{
		if (entry.second >= 10.0)
		{
			good = entry.first;

			break;
		}
	}

	CHECK(!good.empty());

	const long long buyerBefore = Credits(store, buyer);
	const long long sellerBefore = Credits(store, seller);
	const double buyerGoods = Units(store, buyer, good);
	const double sellerGoods = Units(store, seller, good);

	std::string orderId;

	CHECK(MarketplaceService::PostOrder(store, Order(venueId, seller, good, OrderSide::Sell, 10.0, 25.0, 1), orderId, errorMessage));

	const std::vector<TradeFill> fills = MarketplaceService::ExecuteOrder(
	    store, Order(venueId, buyer, good, OrderSide::Buy, 10.0, 0.0, 2), errorMessage);

	CHECK_EQ(fills.size(), 1u);
	CHECK_EQ(fills[0].itemId, good);
	CHECK_EQ(fills[0].marketId, venueId);
	CHECK_EQ(fills[0].sellerId, seller);
	CHECK_EQ(fills[0].buyerId, buyer);
	CHECK_EQ(fills[0].quantity, 10.0);
	CHECK_EQ(fills[0].unitPrice, 25.0);
	CHECK_EQ(fills[0].gross, 250);
	CHECK_EQ(Credits(store, seller), sellerBefore + 250 - 1);
	CHECK_EQ(Credits(store, buyer), buyerBefore - 250 - 1);
	CHECK_NEAR(Units(store, seller, good), sellerGoods - 10.0, 1e-9);
	CHECK_NEAR(Units(store, buyer, good), buyerGoods + 10.0, 1e-9);
	CHECK_EQ(MarketplaceService::MarketAt(store, venueId)->venueTake, 2);
	CHECK_EQ(MarketplaceService::Fills(store, venueId).size(), 1u);
	CHECK_EQ(MarketplaceService::OpenOrderCount(store, venueId), 0u);
	CHECK_EQ(MarketplaceService::ReferencePrice(store, venueId, good), 25.0);
	CHECK_EQ(MarketplaceService::AdvanceTick(store, 1, errorMessage), worlds.size());
	CHECK(MarketplaceService::DescribeMarket(store, venueId).find(good) != std::string::npos);

	// A loaded world has a station above it: a station is a place the ships of that
	// world put in and pull out of, and a loaded world is a planet the generator put
	// in a system.
	CHECK(MarketplaceService::AddSatellite(store, "Station-" + venueId, venueId, "High Station", 60000.0, 2.5, errorMessage));
	CHECK(MarketplaceService::Open(store, "Station-" + venueId, errorMessage));
	CHECK_EQ(MarketplaceService::MarketIds(store, worlds[0].id).size(), 2u);

	// The world that is already full is not the world this loads into: the ids of a
	// second load collide with the first, and the rule that keeps a fill log honest is
	// that an id names one order.
	CHECK(!MarketplaceService::LoadSimulation(store, sim, MarketplaceOptions{}, venues, errorMessage));
	CHECK_CONTAINS(errorMessage, "entities");
	CHECK_EQ(venues, 0u);

	Simulation fresh(config);
	EcsWorld empty;

	CHECK(!MarketplaceService::LoadSimulation(empty, fresh, MarketplaceOptions{}, venues, errorMessage));
	CHECK_CONTAINS(errorMessage, "generated");
	CHECK_EQ(venues, 0u);
}


TEST_CASE(the_same_posts_in_the_same_order_give_the_same_fills)
{
	auto Run = [](EcsWorld &world, MarketplaceService &service)
	{
		std::string errorMessage;
		std::string orderId;

		AddWorld(service, world, "Civ-01", "Abigail Prime", 100000, {{"iron_ore", 1000.0}, {"grain", 500.0}});
		AddWorld(service, world, "Civ-02", "Beacon Reach", 100000, {{"iron_ore", 1000.0}, {"grain", 500.0}});
		AddWorld(service, world, "Civ-03", "Cinder Port", 100000, {{"iron_ore", 1000.0}, {"grain", 500.0}});

		service.Open(world, "Civ-01", errorMessage);
		service.Open(world, "Civ-02", errorMessage);

		// The posts in one order, the marketable orders in one order, the ticks in one
		// order: the fills, the treasuries, the stockpiles and the quote tables of two
		// worlds built from one list of posts have to be one answer.
		service.PostOrder(world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 30.0, 12.0, 1), orderId, errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 20.0, 11.0, 1), orderId, errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 25.0, 11.0, 1), orderId, errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 40.0, 6.0, 2), orderId, errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 30.0, 6.0, 2), orderId, errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-03", "grain", OrderSide::Sell, 25.0, 5.0, 2), orderId, errorMessage);
		service.ExecuteOrder(world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 40.0, 0.0, 3), errorMessage);
		service.ExecuteOrder(world, Order("Civ-01", "Civ-02", "grain", OrderSide::Buy, 20.0, 7.0, 3), errorMessage);
		service.PostOrder(world, Order("Civ-02", "Civ-01", "iron_ore", OrderSide::Buy, 15.0, 9.0, 4), orderId, errorMessage);
		service.ExecuteOrder(world, Order("Civ-02", "Civ-02", "iron_ore", OrderSide::Sell, 40.0, 0.0, 5), errorMessage);
		service.CancelOrder(world, "Order-6", errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 60.0, 13.0, 6), orderId, errorMessage);
		service.ExecuteOrder(world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Sell, 100.0, 0.0, 7), errorMessage);
		service.PostOrder(world, Order("Civ-01", "Civ-01", "water", OrderSide::Buy, 12.0, 3.25, 8), orderId, errorMessage);
		service.ExecuteOrder(world, Order("Civ-01", "Civ-02", "water", OrderSide::Sell, 30.0, 0.0, 9), errorMessage);
		service.RefreshQuotes(world, "Civ-01", errorMessage);
		service.RefreshQuotes(world, "Civ-02", errorMessage);
		service.AdvanceTick(world, 34, errorMessage);
		service.AdvanceTick(world, 40, errorMessage);
	};

	EcsWorld first;
	EcsWorld second;
	MarketplaceService serviceFirst;
	MarketplaceService serviceSecond;

	Run(first, serviceFirst);
	Run(second, serviceSecond);

	const std::vector<TradeFill> fillsOne = serviceFirst.Fills(first, "Civ-01");
	const std::vector<TradeFill> fillsTwo = serviceSecond.Fills(second, "Civ-01");

	CHECK(!fillsOne.empty());
	CHECK_EQ(fillsOne.size(), fillsTwo.size());

	for (std::size_t index = 0; index < fillsOne.size(); ++index)
	{
		CHECK_EQ(fillsOne[index].makerOrderId, fillsTwo[index].makerOrderId);
		CHECK_EQ(fillsOne[index].takerOrderId, fillsTwo[index].takerOrderId);
		CHECK_EQ(fillsOne[index].quantity, fillsTwo[index].quantity);
		CHECK_EQ(fillsOne[index].unitPrice, fillsTwo[index].unitPrice);
		CHECK_EQ(fillsOne[index].gross, fillsTwo[index].gross);
		CHECK_EQ(fillsOne[index].makerFee, fillsTwo[index].makerFee);
		CHECK_EQ(fillsOne[index].tick, fillsTwo[index].tick);
	}

	CHECK_EQ(serviceFirst.Fills(first, "Civ-02").size(), serviceSecond.Fills(second, "Civ-02").size());
	CHECK_EQ(serviceFirst.MarketAt(first, "Civ-01")->venueTake, serviceSecond.MarketAt(second, "Civ-01")->venueTake);
	CHECK_EQ(Credits(first, "Civ-01"), Credits(second, "Civ-01"));
	CHECK_EQ(Credits(first, "Civ-02"), Credits(second, "Civ-02"));
	CHECK_EQ(Credits(first, "Civ-03"), Credits(second, "Civ-03"));
	CHECK_EQ(Units(first, "Civ-03", "iron_ore"), Units(second, "Civ-03", "iron_ore"));
	CHECK_EQ(Units(first, "Civ-02", "grain"), Units(second, "Civ-02", "grain"));
	CHECK_EQ(serviceFirst.OpenOrderCount(first, "Civ-01"), serviceSecond.OpenOrderCount(second, "Civ-01"));
	CHECK_EQ(serviceFirst.Quotes(first, "Civ-01").size(), serviceSecond.Quotes(second, "Civ-01").size());
	CHECK_EQ(serviceFirst.OrdersOf(first, "Civ-01", "").size(), serviceSecond.OrdersOf(second, "Civ-01", "").size());
	CHECK_EQ(serviceFirst.DescribeMarket(first, "Civ-01"), serviceSecond.DescribeMarket(second, "Civ-01"));
	CHECK_EQ(serviceFirst.DescribeMarket(first, "Civ-02"), serviceSecond.DescribeMarket(second, "Civ-02"));
	CHECK_EQ(first.Ids(), second.Ids());

	// The quote table of one venue, read twice in the order the table is kept in, and
	// printed as the table the screen shows: the same numbers, the same order, the same
	// words.
	const std::vector<MarketQuote> quotesOne = serviceFirst.Quotes(first, "Civ-01");

	CHECK(quotesOne.size() >= 2);

	for (const MarketQuote &quote : quotesOne)
	{
		const MarketQuote *other = serviceSecond.QuoteFor(second, "Civ-01", quote.itemId);

		CHECK(other != nullptr);
		CHECK_EQ(quote.bestAsk, other->bestAsk);
		CHECK_EQ(quote.bestBid, other->bestBid);
		CHECK_EQ(quote.mid, other->mid);
		CHECK_EQ(quote.spreadBps, other->spreadBps);
		CHECK_EQ(quote.askDepth, other->askDepth);
		CHECK_EQ(quote.bidDepth, other->bidDepth);
		CHECK_EQ(quote.volume, other->volume);
		CHECK_EQ(quote.lastPrice, other->lastPrice);
		CHECK_EQ(quote.lastTick, other->lastTick);
	}

	// The orders that are left in the book, in the order the book keeps them: price
	// then time, and the id that breaks the last tie, which is the rule that says the
	// book of a run is the book of every run.
	const std::vector<std::string> asksOne = serviceFirst.Book(first, "Civ-01", "iron_ore", OrderSide::Sell);
	const std::vector<std::string> asksTwo = serviceSecond.Book(second, "Civ-01", "iron_ore", OrderSide::Sell);

	CHECK_EQ(asksOne, asksTwo);
	CHECK_EQ(serviceFirst.Book(first, "Civ-01", "iron_ore", OrderSide::Buy), serviceSecond.Book(second, "Civ-01", "iron_ore", OrderSide::Buy));

	for (const std::string &id : asksOne)
	{
		CHECK_EQ(serviceFirst.DescribeOrder(first, id), serviceSecond.DescribeOrder(second, id));
	}

	CHECK_EQ(serviceFirst.InsuranceBps(first, "Civ-01"), serviceSecond.InsuranceBps(second, "Civ-01"));
	CHECK_EQ(serviceFirst.ListingFeeBps(first, "Civ-01"), serviceSecond.ListingFeeBps(second, "Civ-01"));
	CHECK_EQ(serviceFirst.DepthUnits(first, "Civ-01"), serviceSecond.DepthUnits(second, "Civ-01"));
}

int main()
{
	return civtest::Run("MarketplaceTests");
}
