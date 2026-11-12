#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Core/SimulationConfig.h"
#include "Core/Universe.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/CivilisationEffects.h"
#include "Market/Marketplace.h"
#include "TestFramework.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace
{
using namespace ManifestCore;

// A world of the scaffold's own making: the credits and the cargo a market settles
// against, in the components the market reads.
std::string AddWorld(
    EcsWorld &world, const std::string &civilisationId, const std::string &planetName,
    long long credits, const std::map<std::string, double> &cargo)
{
	std::string errorMessage;

	if (!MarketplaceService::AddPlanet(
	        world, civilisationId, planetName, planetName + " System", civilisationId, "Age02",
	        errorMessage))
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

// The credits of a trader, set by the test and not by the book: the ledger of a fill
// moves the money, and a scenario that follows a fill starts from the pile its numbers
// read, not from the dregs the fill left behind.
void SetCredits(EcsWorld &world, const std::string &civilisationId, long long Amount)
{
	Treasury *treasury = world.Get<Treasury>(civilisationId);

	if (treasury != nullptr)
	{
		treasury->credits = Amount;
	}
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
// The stockpile of a generated world, read the way the market's own stockpile is read:
// a good the world has none of is a good of nought units.
double StockOf(const Civilisation &world, const std::string &itemId)
{
	const auto entry = world.stockpile.find(itemId);

	return entry != world.stockpile.end() ? entry->second : 0.0;
}

// A node of the tree the marketplace reads: the shipped shape, the set it sits in, the
// tiers it opens, the resource each tier costs, and the effect its tiers write.
SkillNode Node(
    const std::string &skillId, unsigned maxTiers, unsigned costPerTier,
    const std::string &targetEffectId)
{
	SkillNode node;

	node.skillId = skillId;
	node.displayName = skillId;
	node.nodeType = NodeType::Skill;
	node.maxTiers = maxTiers;
	node.resourceCostPerTier = costPerTier;
	node.targetEffectID = targetEffectId;

	return node;
}

// A tree and the points that buy it. The marketplace reads a ProgressionManager, so a
// test that walks a tree builds one, and the points of the pool pay for the nodes the
// test names.
ProgressionManager Tree(const std::vector<SkillNode> &nodes, int points)
{
	ProgressionManager progression;
	std::string errorMessage;

	CHECK(progression.Build(nodes, errorMessage));
	progression.SetResources(points);

	return progression;
}

} // namespace

TEST_CASE(a_marketplace_sits_at_a_planet_and_at_a_station_in_orbit_above_one)
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	EcsWorld::Entity created = EcsWorld::Null;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}).empty());
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
	CHECK(
	    MarketplaceService::AddSatellite(
	        world, "Beacon-Station", "Civ-01", "Beacon Station", 420000.0, 4.1, errorMessage));
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
	CHECK_EQ(orbit->primaryId, std::string("Civ-01"));
	CHECK_EQ(orbit->radiusKm, 420000.0);

	// A star has no market: no people, and the gravity well of a star is a well no
	// market sits at the bottom of.
	CHECK(!MarketplaceService::AddPlanet(
	    world, "Star-01", "Abigail", "Abigail Prime System", "", "", errorMessage));
	CHECK_CONTAINS(errorMessage, "civilisation");

	{
		EcsWorld::Entity created = EcsWorld::Null;

		CHECK(world.Create("Star-01", created, errorMessage));
		world.Add<BodyInfo>(
		    "Star-01",
		    BodyInfo{
		        "Star-01", BodyKind::Star, "Abigail", {}, "Abigail Prime System", "Civ-01", "Age05"});

		CHECK(!service.Open(world, "Star-01", errorMessage));
		CHECK_CONTAINS(errorMessage, "star");
		CHECK_CONTAINS(errorMessage, "Star-01");
		CHECK(!service.HasMarket(world, "Star-01"));
	}

	// A station that orbits a star is the same argument one step further: the station
	// trade is the trade of the ships that come up a planet's well, and a star has no
	// market at the bottom of its.
	CHECK(
	    MarketplaceService::AddPlanet(
	        world, "Civ-02", "Beacon Reach", "Beacon Reach System", "Civ-02", "Age02", errorMessage));
	{
		EcsWorld::Entity created = EcsWorld::Null;

		CHECK(world.Create("Moon-01", created, errorMessage));
		world.Add<BodyInfo>(
		    "Moon-01", BodyInfo{
		                   "Moon-01",
		                   BodyKind::Satellite,
		                   "Moon-01",
		                   "Star-01",
		                   "Abigail Prime System",
		                   "Civ-01",
		                   {}});

		CHECK(!service.Open(world, "Moon-01", errorMessage));
		CHECK_CONTAINS(errorMessage, "gravity well");
		CHECK_CONTAINS(errorMessage, "Moon-01");
		CHECK_CONTAINS(errorMessage, "Star-01");
	}

	// A station in the scaffold's own form, with the body rule written into the entity
	// the venue is addressed by: a station in the orbit of a star has no world beneath it
	// to serve, and the scaffold that builds a station in the orbit of a planet builds no
	// station at a star, because the body the venue rule reads would be a body no market
	// can ever open at.
	CHECK(!MarketplaceService::AddSatellite(
	    world, "Moon-02", "Star-01", "Moon-02", 1.0, 1.0, errorMessage));
	CHECK_CONTAINS(errorMessage, "star");
	CHECK_CONTAINS(errorMessage, "Star-01");
	CHECK(!world.Has("Moon-02"));

	// A station above nothing: the ships that put in and pull out of it come up from
	// nowhere, and a venue that cannot say whose trade it is takes no order.
	{
		EcsWorld::Entity created = EcsWorld::Null;

		CHECK(world.Create("Station-01", created, errorMessage));
		world.Add<BodyInfo>(
		    "Station-01", BodyInfo{
		                      "Station-01",
		                      BodyKind::Satellite,
		                      "Station-01",
		                      {},
		                      "Abigail Prime System",
		                      "Civ-01",
		                      {}});

		CHECK(!service.Open(world, "Station-01", errorMessage));
		CHECK_CONTAINS(errorMessage, "orbits no planet");
		CHECK_CONTAINS(errorMessage, "Station-01");
	}

	// A body that belongs to no one: the fee, the risk and the take of a venue all
	// belong to a world, and so does the take.
	CHECK(world.Create("Body-01", created, errorMessage));
	world.Add<BodyInfo>(
	    "Body-01", BodyInfo{"Body-01", BodyKind::Planet, "Body-01", {}, "Nowhere", "", {}});
	CHECK(!service.Open(world, "Body-01", errorMessage));
	CHECK_CONTAINS(errorMessage, "no civilisation");

	// Not a body at all: an order, a ship, a lane store.
	CHECK(world.Create("Order-9", created, errorMessage));
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


TEST_CASE(a_resting_order_holds_the_goods_and_the_credits_it_promises)
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 1000, {{"grain", 100.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// The seller's side: the goods leave the seller's stockpile when the order posts,
	// and sit in the order until it fills, leaves the book, or expires.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 40.0, 12.0, 1), orderId,
	    errorMessage));

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
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 200.0, 12.0, 1), orderId,
	    errorMessage));

	CHECK_CONTAINS(errorMessage, "Civ-01");
	CHECK_CONTAINS(errorMessage, "iron_ore");
	CHECK_CONTAINS(errorMessage, "60");
	CHECK_CONTAINS(errorMessage, "200");
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 60.0);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);

	// The buyer's side: the credits leave the treasury at the limit price, which is the
	// worst the order can pay, and the venue's fee is in the reservation, because the
	// fee is paid on top of the price of the goods.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 30.0, 10.0, 1), orderId,
	    errorMessage));

	CHECK_EQ(orderId, std::string("Order-2"));
	CHECK_EQ(Credits(world, "Civ-02"), 698);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(service.OpenOrderCount(world, ""), 2u);

	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 1000.0, 10.0, 1), orderId,
	    errorMessage));

	CHECK_CONTAINS(errorMessage, "Civ-02");
	CHECK_CONTAINS(errorMessage, "credits");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 0.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "limit price");
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, -3.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "negative");
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 0.0, 10.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "nothing");
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "itemId");
	CHECK(!service.PostOrder(
	    world, Order("", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, "empty");
	// A trader of the scaffold's own making that carries credits and no cargo: a venue
	// settles a fill in goods and credits, the goods out of the seller's stockpile and the
	// credits out of the buyer's treasury, and a world that carries one of the two cannot
	// settle, so it cannot post at a venue that settles on the spot.
	{
		EcsWorld::Entity trader = EcsWorld::Null;

		CHECK(world.Create("Civ-03", trader, errorMessage));
		world.Add<Treasury>("Civ-03", Treasury{500});
	}

	CHECK(!service.PostOrder(
	    world, Order("Civ-03", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-03");
	CHECK_CONTAINS(errorMessage, "Open");
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-03");
	CHECK_CONTAINS(errorMessage, "Stockpile");
	// The rule is the venue's settlement rule, and it is the same on the sell side: the
	// trader that carries no cargo has no stockpile to reserve the goods out of, and a
	// venue that requires stock turns the order away before it books, so the book keeps
	// the two orders it had.
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Sell, 10.0, 10.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-03");
	CHECK_CONTAINS(errorMessage, "Stockpile");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// A cancel: the order leaves the book, and the goods and the credits it held are
	// the trader's again, in full, at the price it posted at.
	CHECK(service.CancelOrder(world, "Order-1", errorMessage));

	CHECK(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).empty());
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 100.0);
	// A cancel of an order that is gone is the target state and not a failure: the order
	// that filled, the order that expired, the order that was cancelled and the order that
	// was never posted are all gone, and the books of a tick are cleaned by cancelling what
	// the venue listed, so the venue answers a cancel of a gone order with the answer that
	// the book is clean. An empty id names no order at all, and that is a caller's error.
	CHECK(service.CancelOrder(world, "Order-1", errorMessage));
	CHECK(service.CancelOrder(world, "Order-99", errorMessage));
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

	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 4.0, 12.0, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "5");
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 0.5, 1), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "price");
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 5.0, 1.0, 1), orderId,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);

	service.SetOptions(MarketplaceOptions{});
}


TEST_CASE(a_post_that_crosses_the_book_trades_at_the_price_that_was_resting)
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 40.0, 12.0, 1), orderId,
	    errorMessage));

	// The buyer who posts at the seller's price has sold: the order that crossed is out
	// of the book and in the fill log, and the price of the fill is the price the
	// resting order named, because the resting order was the book when the buyer came
	// looking.
	const std::string buyId = "Order-2";
	std::vector<TradeFill> fills;

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 40.0, 12.0, 2), orderId,
	    errorMessage));

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
	// The seller's treasury is the 1000 it started with, plus the 480 of the sale less
	// the 2 of its fee; the buyer's is the 1000 it started with less the 482 the venue
	// reserved at the post of the order that crossed, and the fill spent that pocket to
	// the last credit, because a buy pays out of the pocket its order holds and the
	// treasury is charged once, at the post. The fee is the venue's 40 bps of the gross,
	// rounded up, because the fraction a venue is owed is not paid out of its own pocket.
	CHECK_EQ(Credits(world, "Civ-01"), 1478);
	CHECK_EQ(Credits(world, "Civ-02"), 518);
	// The goods move on the spot: the seller's stockpile is the 100 units it started
	// with, less the 40 that shipped, and the buyer's is the 100 it started with plus the
	// 40 that arrived, because a fill hands the cargo over at once and the venue keeps
	// nothing of the cargo.
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 60.0);
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 140.0);

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

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 40.0, 10.0, 1), first,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 40.0, 10.0, 1), second,
	    errorMessage));

	CHECK_LT(first, second);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy)[0], first);

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 30.0, 10.0, 3), orderId,
	    errorMessage));

	fills = service.Fills(world, "Civ-01");

	CHECK_EQ(fills.size(), 2u);
	CHECK_EQ(fills[1].makerOrderId, first);
	CHECK_EQ(fills[1].takerOrderId, orderId);
	CHECK_EQ(fills[1].unitPrice, 10.0);
	CHECK_EQ(fills[1].quantity, 30.0);
	CHECK_EQ(fills[1].tick, 3);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 2u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// A cancel takes an order out of the book and its goods and credits go back to the
	// trader that posted them. The venue runs its crossing pass over the book that is
	// left, and the pass has nothing to trade: the book's invariant is that the best bid
	// is under the best ask, and taking the top bid away moves the best bid further from
	// the ask, not closer, so a cancel is settled money and never a new trade.
	std::string askId;

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 20.0, 11.0, 4), askId,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 3u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 2u);

	const long long creditsBefore = Credits(world, "Civ-01");

	CHECK(service.CancelOrder(world, first, errorMessage));

	fills = service.Fills(world, "Civ-01");

	CHECK_EQ(fills.size(), 2u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 1u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy)[0], second);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_CONTAINS(service.DescribeOrder(world, first), "no order");

	// The bid that is withdrawn had paid for 40 units at its own price and had sold 30
	// of them at 10.0; the 10 units it still offers are 100 credits at the same price,
	// and the venue takes nothing for an order that never traded, so the treasury is
	// 100 credits the heavier for the cancel.
	CHECK_EQ(Credits(world, "Civ-01"), creditsBefore + 100);

	// An order the book has already traded is not the trader's to withdraw: the promise
	// it kept is in the fill log, and the fill log names the order by its number.
	CHECK(!service.CancelOrder(world, orderId, errorMessage));
	CHECK_CONTAINS(errorMessage, orderId);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// The quote table of a book with one bid and one ask: the two prices, the middle of
	// them, the space between them in basis points of that middle, and the trades the
	// book has made, which no cancel erases.
	const MarketQuote *quoteAfter = service.QuoteFor(world, "Civ-01", "iron_ore");

	CHECK(quoteAfter != nullptr);
	CHECK_EQ(quoteAfter->bestBid, 10.0);
	CHECK_EQ(quoteAfter->bestAsk, 11.0);
	CHECK_EQ(quoteAfter->mid, 10.5);
	CHECK_NEAR(quoteAfter->spreadBps, 10000.0 / 10.5, 1e-9);
	CHECK_EQ(quoteAfter->lastPrice, 10.0);
	CHECK_EQ(quoteAfter->lastTick, 3);
	CHECK_EQ(quoteAfter->volume, 70.0);
	CHECK_EQ(quoteAfter->bidDepth, 40.0);
	CHECK_EQ(quoteAfter->askDepth, 20.0);
	// The reference price of a good the book quotes on both sides is the middle of the two
	// quotes, and it is the number the quote table calls the mid: a ship, a UI and a test
	// that want one number for the good take it, and they do not care which of the four
	// readings of a price it came from.
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "iron_ore"), 10.5);

	// The seller withdraws the ask that rests, and the 20 units the order had reserved go
	// back to the seller's own stockpile: the goods leave the seller at the post and sit
	// in the order until it fills, leaves the book, or expires, and a withdraw is the
	// goods back at the same price they left at, which is to say nothing of the goods.
	// The seller's tally is the 100 units it started with, the 40 its buy order took in
	// the first trade, and the 30 its filled sell order shipped: 110 units.
	CHECK(service.CancelOrder(world, askId, errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).empty());
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 110.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->bestAsk, 0.0);
}


TEST_CASE(an_order_that_walks_the_book_pays_the_best_price_first)
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 1000, {}).empty());
	CHECK(
	    AddWorld(world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 40.0}, {"grain", 10.0}}).empty());
	CHECK(AddWorld(world, "Civ-03", "Cinder Port", 1000, {{"iron_ore", 30.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// The book, resting first: three asks at 10, 11 and 12, and two bids at 9.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 10.0, 1), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 11.0, 2), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 3), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 8.0, 9.0, 1), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 2.0, 9.0, 2), orderId,
	    errorMessage));

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
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 4);

	// The venue charges the base of its listing fee, the service's 40 bps, plus the risk
	// of trading at the venue: this world has no market effects bought and no lanes, so
	// its risk is nothing and the charge is 40 bps of the gross, rounded up to a whole
	// credit. On the 100 at 10.0 that is 1 credit and on the 22 at 11.0 1 credit, and both
	// sides pay, each the fee of its own side of the trade: 4 credits taken, which is what
	// the venue keeps.
	// The taker paid 100 at 10.0 and 22 at 11.0 and the fee of its own side on each, out
	// of the pocket its order holds: a marketable buy is posted at the dearest ask of the
	// book and reserves the whole walk at that price and the fee on it, and the pocket the
	// walk leaves behind is back in the treasury when the walk's order leaves the book, so
	// a walk charges a trader once and not twice. The resting orders paid their own fees.
	CHECK_EQ(Credits(world, "Civ-01"), 1000 - 100 - 1 - 22 - 1);
	CHECK_EQ(Credits(world, "Civ-02"), 1000 + 100 - 1 + 22 - 1);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 12.0);
	// The seller's stockpile reads 10: its 40 units are the 30 that sit in its three asks,
	// 10 of them at each of 10.0, 11.0 and 12.0, and the 10 that no order holds, and the 12
	// units the fills shipped left the pile at the posts of the asks and not at the fills,
	// because a resting order carries its own cargo.
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 10.0);
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
	// The best ask after the walk: the 11 ask with 8 units left, the 12 ask, and the
	// depth the book quotes. The quote counts the orders at the best price and the units
	// resting at it, not the orders in the whole book, because the depth at the best price
	// is the size a marketable order takes before the price moves and the number at it is
	// the number of traders a screen shows beside the price.
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->askOrders, 1u);
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
	CHECK_EQ(Credits(world, "Civ-01"), 876);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 12.0);

	// A sell order walks the bids: the best bid first, at the bids' own prices, and the
	// second bid at the second bid's price, which is the same rule read from the other
	// side of the book. The seller withdraws its 12.0 ask first: a sell order has to be
	// covered by the stockpile it sells from, and the 12 units of that ask are the units
	// it has to sell, so the withdraw comes first and the walk sells what the withdraw
	// gives back.
	CHECK(service.CancelOrder(world, "Order-3", errorMessage));
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 20.0);

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
	// The bids are both taken, at their own price of 9.0, and the 2 units of the walk
	// that no bid wanted go back to the seller's stockpile: the reservation a walk leaves
	// behind is the trader's at the end of the walk. The bids of the book are gone, and a
	// book with no bids has no best bid to answer. The pile is the 20 the withdraw made, less the 12
	// the walk reserved, plus the 2 it gave back when it found no bid for them, and the 8 of
	// the 11.0 ask are still in the order that rests on the book.
	CHECK(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).empty());
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 10.0);
	CHECK_EQ(Units(world, "Civ-03", "iron_ore"), 40.0);
	CHECK(!service.BestBid(world, "Civ-01", "iron_ore", bestBid));

	// A walk of one unit of a book that has 8, and the order it walked leaves 8 in
	// the book: the walk stops when it has taken all it asked for, and the resting
	// order it left behind is the order the next walk meets.
	const std::vector<TradeFill> part = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 3.0, 0.0, 8), errorMessage);

	CHECK_EQ(part.size(), 1u);
	CHECK_EQ(part[0].quantity, 3.0);
	CHECK_EQ(part[0].unitPrice, 11.0);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 1u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->askDepth, 5.0);
	CHECK(service.BestAsk(world, "Civ-01", "iron_ore", bestAsk));
	CHECK_EQ(bestAsk, 11.0);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 15.0);

	// A marketable order against a book with nothing on the side it walks: a walk names
	// no price, and the book's own worst quote on that side is the price the walk has to
	// be posted at, so the walk of a good the book quotes on one side only has no price to
	// be posted at, and the answer is nothing traded and a message that names the good and
	// the price it lacked. The grain bid just posted is the bid the first walk takes.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 10.0, 6.0, 9), orderId,
	    errorMessage));
	CHECK(service.BestBid(world, "Civ-01", "grain", bestBid));
	CHECK_EQ(bestBid, 6.0);

	const std::vector<TradeFill> grain = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 10.0, 0.0, 9), errorMessage);

	CHECK_EQ(grain.size(), 1u);
	CHECK_EQ(grain[0].quantity, 10.0);
	CHECK_EQ(grain[0].unitPrice, 6.0);
	CHECK_EQ(grain[0].sellerId, std::string("Civ-02"));
	CHECK_EQ(grain[0].buyerId, std::string("Civ-01"));

	// The bid is gone, and the next grain walk has no bid to walk against and no worst
	// bid to name as the price it posts at, which is the message that says why nothing
	// happened, naming the good and the price.
	CHECK(
	    service
	        .ExecuteOrder(
	            world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 10.0, 0.0, 9), errorMessage)
	        .empty());
	CHECK_CONTAINS(errorMessage, "grain");
	CHECK_CONTAINS(errorMessage, "price");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 6u);

	// A walk that names a price and meets no bid at it is the book and nothing more: the
	// order walks, finds nothing, and leaves the book as it came, with its cargo. A walk
	// of nothing units is the venue's rule that an order has to be above nothing, and a
	// walk at a venue that is no venue names the venue in the message.
	CHECK(
	    service
	        .ExecuteOrder(
	            world, Order("Civ-01", "Civ-01", "grain", OrderSide::Sell, 10.0, 6.0, 10), errorMessage)
	        .empty());
	CHECK(service
	          .ExecuteOrder(
	              world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 0.0, 0.0, 10), errorMessage)
	          .empty());
	CHECK_CONTAINS(errorMessage, "nothing");
	CHECK(
	    service
	        .ExecuteOrder(
	            world, Order("Civ-99", "Civ-01", "grain", OrderSide::Buy, 10.0, 10.0, 10), errorMessage)
	        .empty());
	CHECK_CONTAINS(errorMessage, "Civ-99");
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(Units(world, "Civ-01", "grain"), 10.0);
}


TEST_CASE(the_book_is_the_book_the_quoters_read)
{
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 10000, {{"iron_ore", 100.0}, {"grain", 100.0}})
	          .empty());
	CHECK(AddWorld(
	          world, "Civ-02", "Beacon Reach", 10000,
	          {{"iron_ore", 100.0}, {"grain", 100.0}, {"water", 10.0}})
	          .empty());

	CHECK(service.Open(world, "Civ-01", errorMessage));

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 1), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 2), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 13.0, 3), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 8.0, 10.0, 4), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 6.0, 9.0, 5), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 20.0, 4.0, 6), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 15.0, 5.0, 7), orderId,
	    errorMessage));

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
	// The quote names the best price on each side, the number of orders that quote it,
	// which is the number of traders a screen shows beside the price, and the units
	// resting at it: the asks at 12.0 are two orders of 10 units, the bid at 10.0 is one
	// order of 8, and the 9.0 bid is a level below the best bid.
	CHECK_EQ(iron->askOrders, 2u);
	CHECK_EQ(iron->bidOrders, 1u);

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
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "water", OrderSide::Sell, 5.0, 30.0, 8), orderId,
	    errorMessage));
	CHECK(service.RefreshQuotes(world, "Civ-01", errorMessage));

	const MarketQuote *water = service.QuoteFor(world, "Civ-01", "water");

	// The out-parameters of the price reads, for the calls that answer with the price
	// through an argument: one pair, reused by every read, as the test reads them.
	double bestAsk = 0.0;
	double bestBid = 0.0;

	CHECK(water != nullptr);
	CHECK_EQ(water->bestAsk, 30.0);
	CHECK_EQ(water->bestBid, 0.0);
	CHECK_EQ(water->mid, 0.0);
	CHECK_EQ(water->spreadBps, 0.0);
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "water"), 30.0);
	CHECK_EQ(service.Quotes(world, "Civ-01").size(), 3u);
	CHECK(service.QuoteFor(world, "Civ-01", "grain") != nullptr);
	CHECK(service.QuoteFor(world, "Civ-01", "unobtainium") == nullptr);
	CHECK(service.BestAsk(world, "Civ-01", "grain", bestAsk));
	CHECK_EQ(bestAsk, 5.0);
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
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 12.0, 9), orderId,
	    errorMessage));

	const std::vector<TradeFill> fills = service.ExecuteOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 0.0, 10), errorMessage);

	CHECK_EQ(fills.size(), 1u);
	CHECK_EQ(fills[0].unitPrice, 12.0);
	CHECK_EQ(service.ReferencePrice(world, "Civ-01", "iron_ore"), 12.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->volume, 10.0);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->lastTick, 10);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "water")->volume, 0.0);
}


TEST_CASE(the_deck_of_a_venue_holds_just_so_many_units_of_one_good)
{
	MarketplaceOptions options;
	options.depthUnits = 30.0;

	MarketplaceService service(options);
	CivilisationEffectEngine effects;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 100000, {{"iron_ore", 1000.0}}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 100000, {{"iron_ore", 1000.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// A venue with a deck of 30 units of a good: the units one side of one good's book
	// may hold, which is the queue the berth can clear in a day.
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 30.0);
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 20.0, 12.0, 1), orderId,
	    errorMessage));
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 15.0, 12.0, 2), orderId,
	    errorMessage));

	CHECK_CONTAINS(errorMessage, "20.00");
	CHECK_CONTAINS(errorMessage, "30.00");
	CHECK_CONTAINS(errorMessage, "Sell");
	CHECK_CONTAINS(errorMessage, "iron_ore");

	// The other side of the same book has the same room: the deck is the deck of one
	// side of one good. A bid of 30 posted at 12.0 against an ask resting at 12.0 crosses
	// the book it landed on, trades 20 units at the price that was resting, and rests with
	// 10 left: the ask the deck had room for is in the fill log and out of the book, and
	// the book that is left has the bid on it and nothing to meet it with. The grain bid of
	// 30 at 3.0 lands on a book with no grain on it and rests whole.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 30.0, 12.0, 3), orderId,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "grain", OrderSide::Buy, 30.0, 3.0, 4), orderId,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// The safe harbours of the law tree: a berth that clears, and a lane that is
	// worth less, is a venue with a deeper book, because the depth a venue has is the
	// depth the harbours and the tribunal buy. The reading is the market store the
	// effect landed on, and the deck of the venue is the deck of the options plus it.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillPiracySafeHarbours", "SafeHarbours", EffectOrigin::Skill, 1,
	    errorMessage));
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketDepth), 25.0);

	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 55.0);
	// The Sell side of iron_ore: the ask that traded at tick 3 left the book in the fill,
	// the deck measures what the side holds now, and the 15-unit ask at 12.0 that meets the
	// 10-unit bid resting at 12.0 trades 10 units at the resting price and rests with 5.
	// The side then holds 5 of the 55 the harbours and the tribunal buy, and a 60-unit ask
	// would put it at 65, which is over the deck.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 15.0, 12.0, 5), orderId,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK(!service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 60.0, 12.0, 6), orderId,
	    errorMessage));
	CHECK_CONTAINS(errorMessage, "55.00");
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -14);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 26);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 120);

	// A venue that has set no depth is a venue that quoters arrive at, not a venue
	// that turns them away: the deck of the options is the deck, and the market
	// effects only raise it, so a venue with no deck set holds what its traders bring.
	// The deck is off when the options put no number on it: the venue quotes depth is the
	// depth the harbours buy, and a venue that has set no deck is a venue that quoters
	// arrive at, not a venue that turns them away, so the 500-unit ask that walks in is
	// posted. The count is the two orders the book had plus the one that just landed: the
	// grain bid, the 5-unit ask that rested, and the 500-unit ask, because the bid that
	// met the 5-unit ask is in the fill log and out of the book.
	service.SetOptions(MarketplaceOptions{});

	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 0.0);
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 500.0, 12.0, 7), orderId,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 3u);
	CHECK_EQ(service.DepthUnits(world, "Civ-99"), 0.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-99"), 40);
	CHECK_EQ(service.InsuranceBps(world, "Civ-99"), 0);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-99"), 0);
}
TEST_CASE(a_market_effect_moves_the_charge_of_the_venue_it_lands_on)
{
	MarketplaceOptions options;
	options.depthUnits = 100.0;

	MarketplaceService service(options);
	CivilisationEffectEngine effects;
	EcsWorld world;
	std::string errorMessage;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 100000, {{"iron_ore", 1000.0}}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 100000, {{"iron_ore", 1000.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));
	CHECK(service.Open(world, "Civ-02", errorMessage));

	// The venue's own charges, and the risk of the place and the lane: a plain venue
	// charges its base 40 basis points a side, insures the cargo at 120 basis points
	// of the gross, and keeps a deck of 100 units.
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), 0);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 120);
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 100.0);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), 0);
	CHECK_EQ(service.InsuranceBps(world, "Civ-02"), 120);

	// A station with no planet beneath it is no venue at all: the station trade is the
	// trade of the ships that come up a planet's well, and a station above nothing has
	// no well to come up. The message names the station.
	CHECK(!service.Open(world, "Beacon-Station", errorMessage));
	CHECK_CONTAINS(errorMessage, "Beacon-Station");
	CHECK_EQ(service.MarketIds(world, "Civ-01").size(), 1u);

	// A venue that has opened has a record of itself and no rows in it: a market that
	// has never traded has a quote table with nothing quoted, which is what the UI
	// prints as nothing traded here yet.
	CHECK(service.QuoteFor(world, "Civ-01", "iron_ore") == nullptr);
	CHECK_EQ(service.Quotes(world, "Civ-01").size(), 0u);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 0u);
	double mid = 0.0;

	CHECK(!service.MidPrice(world, "Civ-01", "iron_ore", mid));
	CHECK_EQ(mid, 0.0);

	// The licence and the warrant: the deck clears, and the berth is cheaper, one
	// basis point off the fee for every two berths a day the licence clears, and the
	// commission takes the risk of the place off the fee as well.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4,
	    errorMessage));

	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketClearance), 20.0,
	    1e-9);
	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium), -60.0,
	    1e-9);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 40);

	// The tribunal and the harbours: the trade that is settled is a trade worth less,
	// and a venue with a court of record and decks a ship can land cargo on is a venue
	// with a deeper book, because a ship that knows the contract will be kept puts in.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillTradeTribunal", "TradeTribunal", EffectOrigin::Skill, 2, errorMessage));
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillPiracySafeHarbours", "SafeHarbours", EffectOrigin::Skill, 1,
	    errorMessage));

	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketDepth), 105.0, 1e-9);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60 - 2 * 9 - 14);
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 205.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 120);

	// The hull insurance of the trade tree: a policy written against the hull, in
	// basis points of the hull's value, and a venue that writes the policy is a venue
	// that charges for it. The policy is worth more than the base premium of the
	// venue, and a premium the policy has driven below nothing is a premium of nothing.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillTradeCargoInsurance", "CargoInsurance", EffectOrigin::Skill, 5,
	    errorMessage));

	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 0);
	CHECK_EQ(service.InsuranceBps(world, "Civ-02"), 120);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 40);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), 0);
	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketInsurance), -200.0,
	    1e-9);

	// The policy is a market effect like any other, and its term lands on the same stat
	// the licence, the bench and the harbours land on: 10 basis points off the risk of
	// the place for every tier of it. The premium is the line that clamps at nothing and
	// the risk of the place is not: a venue that charges nothing for a policy that
	// covers the whole premium of the venue is still the safer venue to trade at, and
	// the fee of the venue prices the risk of the place with the policy in it.
	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketRiskPremium),
	    -60.0 - 2 * 9.0 - 14.0 - 5 * 10.0, 1e-9);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60 - 2 * 9 - 14 - 5 * 10);

	// The fence network of the piracy tree: a market that takes stolen cargo is a
	// market whose risk is higher, and the risk is the fee, which is the way a venue
	// that trades in stolen goods charges for it. Three tiers of the network are 3 * 8
	// basis points on the risk of the place, and the 0.15 a tier of fence share is the
	// fraction of a cargo a fence takes off book: the share is no basis point of any
	// charge of the venue, and no charge of the venue moves with it.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillPiracyFencingNetwork", "FencingNetwork", EffectOrigin::Skill, 3,
	    errorMessage));

	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60 - 2 * 9 - 14 - 5 * 10 + 3 * 8);
	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Market, "Civ-01", EffectStat::MarketFenceShare), 3 * 0.15,
	    1e-9);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);

	// The risk of the venue is the risk of the place and the risk of the lane the
	// goods came down: the convoy of the trade tree is a lane effect, it lands on the
	// one lane store the civilisation owns, and the venue at either end of the lane
	// reads it.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillPiracyConvoy", "Convoy", EffectOrigin::Skill, 2, errorMessage));

	CHECK_NEAR(
	    effects.ReadStat(
	        world, EffectDomain::Route, CivilisationEffectEngine::LaneStoreId("Civ-01"),
	        EffectStat::RouteRiskPremium),
	    -2 * 12.0, 1e-9);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60 - 2 * 9 - 14 - 5 * 10 + 3 * 8 - 2 * 12);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), 0);

	// A venue whose charges the tree has driven below nothing charges nothing: the
	// fee of the venue is clamped at nothing, and a venue that charges nothing takes
	// nothing from the fill, which is the same ledger read from the other side.
	std::string warrant;

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 10.0, 1), warrant,
	    errorMessage));
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);

	// The market effects of a tree that names no market are not the marketplace's: the
	// shield is the ship engine's, and the yard is a store of the civilisation, so the
	// fee of the venue is the fee of the venue and the tree of the ship is the tree of
	// the ship.
	CHECK(!effects.Apply(
	    world, "Civ-01", "SkillShipShieldsFront", "ShieldFront", EffectOrigin::Skill, 2, errorMessage));
	CHECK_CONTAINS(errorMessage, "ShipEffectEngine");
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillShipShipyardsDrydock", "Drydock", EffectOrigin::Skill, 1, errorMessage));

	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60 - 2 * 9 - 14 - 5 * 10 + 3 * 8 - 2 * 12);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
	CHECK_NEAR(
	    effects.ReadStat(
	        world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-01"),
	        EffectStat::YardDockThroughput),
	    1.0, 1e-9);
	CHECK_NEAR(
	    effects.ReadStat(world, EffectDomain::Ship, "Civ-01", EffectStat::ShieldFront), 0.0, 1e-9);

	// A venue that never opened has no charges at all: the venue of a world that did
	// not open is no venue, and the charges of a body with no venue are the base
	// charges of the service, because the venue is where the charges live.
	CHECK(!service.Open(world, "Civ-99", errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-99");
	CHECK_EQ(service.ListingFeeBps(world, "Civ-99"), 40);
	CHECK_EQ(service.InsuranceBps(world, "Civ-99"), 0);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-99"), 0);
	CHECK_EQ(service.DepthUnits(world, "Civ-99"), 0.0);
	CHECK_EQ(service.Fills(world, "Civ-99").size(), 0u);
}

TEST_CASE(an_order_that_outlives_its_life_leaves_the_book)
{
	MarketplaceService service;
	CivilisationEffectEngine effects;
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 20.0, 10.0, 10), orderId,
	    errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 30.0, 12), orderId,
	    errorMessage));

	const MarketOrderInfo *first = world.Get<MarketOrderInfo>("Order-1");

	CHECK(first != nullptr);
	CHECK_EQ(first->placedTick, 10);

	// The order life of the venue is 24 ticks, the berths a day needs, and the order
	// that posted at tick 10 leaves the book at tick 34.
	CHECK_EQ(first->expiresTick, 34);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);

	// The reservation of a resting buy order is the goods at the limit price and the fee
	// of the buyer's own side of the trade, not the fee of both sides: the trader that
	// posted is the maker of the fill it may one day be in, and the taker's fee is the
	// price of a trade the book has not seen yet. 20 units at 10.0 is 200 credits, 40 bps
	// of 200 is 0.8 of a credit, and the venue is owed the whole credit it rounds up to,
	// because the fraction is not paid out of its own pocket. The pocket of the order is
	// 201, and the treasury of Civ-02 is 1000 less that.
	CHECK_EQ(Credits(world, "Civ-02"), 1000 - 200 - 1);

	// A tick before the order leaves: the venue's quotes are refreshed, and the order
	// is where it was.
	CHECK_EQ(service.AdvanceTick(world, 33, errorMessage), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Credits(world, "Civ-02"), 1000 - 200 - 1);
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
	// without a life is an order that rests until it trades, is cancelled, or the venue
	// closes, and a venue with no order life is a venue that keeps its book. The life is
	// added plain, so the order posted at tick 40 under a service that sets no life
	// carries an expiresTick of 40: the expiry of an order with no life is the tick it
	// was posted at, and that is what the sweep reads as "never".
	MarketplaceOptions noLife;
	noLife.orderLifeTicks = 0;
	service.SetOptions(noLife);

	std::string immortal;

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 5.0, 10.0, 40), immortal,
	    errorMessage));
	CHECK_EQ(world.Get<MarketOrderInfo>(immortal)->expiresTick, 40);
	CHECK_EQ(service.AdvanceTick(world, 1000, errorMessage), 1u);

	// The sell of Civ-01 that posted at tick 12 under the venue's 24-tick life left the
	// book at tick 36 and is long gone by tick 1000. The order that posted at tick 40
	// under a service that sets no life is the one order the book keeps.
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);

	// 5 units at 10.0 is 50 credits, 40 bps of 50 is 0.2 of one, and the venue is owed
	// the whole credit it rounds up to: the pocket of the order that never leaves is 51.
	CHECK_EQ(Credits(world, "Civ-02"), 1000 - 51);
	CHECK_EQ(service.AdvanceTick(world, 1000, errorMessage), 1u);

	service.SetOptions(MarketplaceOptions{});

	// Every venue of the world, in sorted venue order, and the venues of one
	// civilisation are the venues the venue's own record of itself is written for.
	CHECK(service.Open(world, "Civ-02", errorMessage));
	CHECK(service.PostOrder(
	    world, Order("Civ-02", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 8.0, 50), orderId,
	    errorMessage));
	CHECK_EQ(service.AdvanceTick(world, 100, errorMessage), 2u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 1u);
	CHECK_EQ(service.OpenOrderCount(world, "Civ-02"), 0u);
	CHECK_EQ(Credits(world, "Civ-01"), 1000);

	// The 51 the order that never left holds is still in its pocket at tick 100: the
	// sweep of a venue with a life sweeps the orders that have one, and the immortal
	// rests on.
	CHECK_EQ(Credits(world, "Civ-02"), 1000 - 51);

	// The market effects of the civilisation that owns a venue are the civilisation's,
	// not the venue's: the four tiers of the warrant are 20 berths a day of clearance and
	// 60 bps off the risk of the place, and the fee of the venue is 40 base, less the risk
	// and less a basis point for every two berths the warrant clears, which is below
	// nothing and so nothing. The venue is open, and the effect lands on it.
	CHECK(effects.Apply(
	    world, "Civ-01", "SkillLawAdmiraltyWarrant", "AdmiraltyWarrant", EffectOrigin::Skill, 4,
	    errorMessage));
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -60);

	// The close of a venue in the middle of a session: every order of the book goes
	// back to the trader that posted it, because a venue that closes in the middle of
	// a session does not keep the goods or the credits of the orders it was holding.
	// The order that has traded is not in the book to be given back: the 10 units of the
	// ask that the crossing buy took at 40.0 left the seller's order in the fill, and the
	// close returns the 20 that rested on it, not the 30 the trader posted.
	std::string heldSell;
	std::string heldBuy;

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 30.0, 40.0, 60), heldSell,
	    errorMessage));

	// The buy posted at 60.0 crosses the ask resting at 40.0: the resting trader is the
	// maker and sets the price, and the crossing order takes the 10 units it wants of the
	// 30 the ask offers. The fill is the venue's 40.0, a gross of 400 credits, and the fee
	// of a venue the warrant has made free is nothing on each side: the buyer pays the
	// 400 of its fill out of the 600 its order holds, and the seller is paid its gross.
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 10.0, 60.0, 61), heldBuy,
	    errorMessage));
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 70.0);

	CHECK(service.Close(world, "Civ-01", errorMessage));

	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 0u);

	// The 100 units Civ-01 came to the venue with, less the 10 the fill sold at 40.0.
	CHECK_EQ(Units(world, "Civ-01", "iron_ore"), 90.0);

	// Civ-02 at 600: the 1000 it came with, less the 51 the immortal buy holds, less the
	// 600 the crossing buy reserved at a venue that charges nothing, and back the 200 the
	// completed order gave up out of its pocket and the 51 the close refunded.
	CHECK_EQ(Credits(world, "Civ-02"), 600);
	CHECK(!world.Has("Order-1"));
	CHECK(world.Has("Civ-01"));

	// The venue closes, and the market effects of its civilisation stay where the tree
	// put them: the tiers were paid for, and the component of the market store is not the
	// venue's to take away with it. What goes with the venue is the record of the venue:
	// the quotes, the volume and the fill log.
	CHECK(world.Get<MarketEffects>("Civ-01") != nullptr);
	CHECK(world.Get<MarketRecord>("Civ-01") == nullptr);
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// The venue reopens with the base the service carries, and reads the clearance and
	// the risk the world bought: a venue that reopens charges the world's tree, not the
	// 40 bps of a venue that has bought nothing.
	CHECK_EQ(service.MarketAt(world, "Civ-01")->baseListingFeeBps, 40);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 0);
}


TEST_CASE(the_marketplace_reads_the_tree_of_the_civilisation_that_owns_the_venue)
{
	CivilisationEffectEngine effects;
	MarketplaceService service;
	EcsWorld world;
	std::string errorMessage;

	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}).empty());
	CHECK(service.Open(world, "Civ-01", errorMessage));

	// Civ-02 is the control the case reads its charges against: a venue of its own, and
	// a civilisation that buys no tier of anything. It is opened before the tree is
	// applied, because a Market effect lands on the venues of the civilisation that owns
	// them, so the tree of Civ-01 cannot reach a venue of Civ-02, and the control keeps
	// the base charges to the end. Insurance is a charge of an open venue: the base of a
	// venue's insurance is read with the venue record, and a world with no venue has no
	// charge of a venue to read, which is the reading of "Civ-99" below.
	CHECK(service.Open(world, "Civ-02", errorMessage));

	const std::vector<SkillNode> nodes = {
	    Node("Age02", 1, 1, ""),
	    Node("SkillLawDockingLicence", 4, 1, "DockingLicence"),
	    Node("SkillTradeCargoInsurance", 4, 2, "CargoInsurance"),
	    Node("SkillPiracyConvoy", 3, 2, "Convoy"),
	    Node("SkillShipShipyardsDrydock", 3, 4, "Drydock"),
	    Node("SkillShipShieldsFront", 5, 2, "ShieldFront"),
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
	CHECK(service.ApplyProgression(
	    world, progression, effects, "Civ-01", applied, skipped, errorMessage));

	CHECK_EQ(applied, 4u);
	CHECK_EQ(skipped, 2u);

	// The charges of the venue: the risk of the place is the licence and the policy, the
	// risk of the lane is the convoy, and the fee is the base fee, that risk, and the
	// clearance the licence buys at half a basis point for every berth a day it clears.
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-01"), -5 - 10 - 12);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-01"), 40 - 27 - 10);
	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 120 - 40);
	CHECK_EQ(service.DepthUnits(world, "Civ-01"), 0.0);
	CHECK_EQ(
	    effects.ReadStat(
	        world, EffectDomain::Yard, CivilisationEffectEngine::YardStoreId("Civ-01"),
	        EffectStat::YardDockThroughput),
	    1.0);
	CHECK_EQ(effects.ReadStat(world, EffectDomain::Market, "Civ-02", EffectStat::MarketInsurance), 0.0);
	CHECK_EQ(service.ListingFeeBps(world, "Civ-02"), 40);
	CHECK_EQ(service.InsuranceBps(world, "Civ-02"), 120);
	CHECK_EQ(service.RiskPremiumBps(world, "Civ-02"), 0);
	CHECK(service.QuoteFor(world, "Civ-01", "iron_ore") == nullptr);
	CHECK_EQ(service.Fills(world, "Civ-01").size(), 0u);

	CHECK(!service.ApplyProgression(
	    world, progression, effects, "Civ-99", applied, skipped, errorMessage));
	CHECK_CONTAINS(errorMessage, "Civ-99");

	// A fill at the fee the venue charges at the tick the order posted at: a tree that
	// moves the charge of a venue moves the charge of the orders that post after it,
	// and the orders that are already in the book keep the fee they posted at, because
	// a trader that posted at 40 basis points pays 40 basis points.
	std::string cheap;

	service.SetOptions(MarketplaceOptions{});
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 10.0, 10.0, 1), cheap,
	    errorMessage));

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

	CHECK_EQ(service.InsuranceBps(world, "Civ-01"), 80);
	CHECK(service.MarketAt(world, "Civ-01")->venueTake > 0);

	// The read-out of the venue's owner: every store of the civilisation, with the
	// effects it flies, the tier the tree bought of each, and the stats they sum to. The
	// insurance is on the line of the venue that charges it, the convoy on the lane the
	// goods come down, and the drydock in the yard store that builds the hulls: the
	// read-out prints the words the catalogue spells, and no enum and no effect id.
	const std::string described = effects.DescribeCivilisation(world, "Civ-01");

	CHECK_CONTAINS(described, "Cargo Insurance x1");
	CHECK_CONTAINS(described, "Docking Licence x1");
	CHECK_CONTAINS(described, "Convoy x1");
	CHECK_CONTAINS(described, "Drydock x1");
	CHECK_CONTAINS(described, std::string(StatName(EffectStat::MarketInsurance)));
	CHECK_CONTAINS(described, std::string(StatName(EffectStat::RouteRiskPremium)));
	CHECK_CONTAINS(described, "Market Civ-01");
	CHECK_CONTAINS(described, "Lanes " + CivilisationEffectEngine::LaneStoreId("Civ-01"));
	CHECK_CONTAINS(described, "Yards " + CivilisationEffectEngine::YardStoreId("Civ-01"));
	CHECK(described.find("CargoInsurance") == std::string::npos);
	CHECK(described.find("EffectStat") == std::string::npos);
	CHECK(described.find("EffectDomain") == std::string::npos);

	// The read-out of the control: a venue that bought nothing flies no effect, so the
	// line of the venue says so, and a civilisation the world has never heard of prints
	// nothing at all.
	const std::string control = effects.DescribeCivilisation(world, "Civ-02");

	CHECK_CONTAINS(control, "Market Civ-02");
	CHECK_CONTAINS(control, "no market effects bought");
	CHECK(control.find("Cargo Insurance") == std::string::npos);
	CHECK(effects.DescribeCivilisation(world, "Civ-99").empty());
}


TEST_CASE(a_fill_trades_in_whole_units_of_the_price_and_the_fee)
{
	MarketplaceOptions options;
	options.requireFunds = false;

	MarketplaceService service(options);
	EcsWorld world;
	std::string errorMessage;
	std::string orderId;
	double bestAsk = 0.0;

	// Civ-01 buys and buys no iron_ore of its own: the 5.9 units the fill trades are the
	// whole of its pile, and Civ-02 sells out of the 100 units it is given.
	CHECK(AddWorld(world, "Civ-01", "Abigail Prime", 60, {}).empty());
	CHECK(AddWorld(world, "Civ-02", "Beacon Reach", 1000, {{"iron_ore", 100.0}}).empty());
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
	// never costs more than the budget: the floor of budget * 10000 / (10000 + bps) is
	// the goods, the fee is the whole it rounds up to, and the two add back to the
	// penny the budget started with.
	// 104 at 40 bps: 104 * 10000 / 10040 is 103 credits of goods, 10.3 units at 10.0,
	// and 103 + 1 of fee pays 104 exactly.
	CHECK_NEAR(MarketplaceService::AffordableQuantity(10.0, 40, 104), 10.3, 1e-9);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 40, 100), 9.9);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 0, 100), 10.0);
	CHECK_EQ(MarketplaceService::AffordableQuantity(0.0, 40, 100), 0.0);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 40, 0), 0.0);
	CHECK_EQ(MarketplaceService::AffordableQuantity(10.0, 40, -1), 0.0);
	// A million at 40 bps: 1000000 * 10000 / 10040 is 996015 credits of goods, 3984060
	// units at 0.25, and 996015 + 3985 of fee pays the million exactly.
	CHECK_NEAR(MarketplaceService::AffordableQuantity(0.25, 40, 1000000), 3984060.0, 1e-6);

	for (const long long budget : {1LL, 7LL, 33LL, 60LL, 100LL, 999LL, 100000LL, 1000000000LL})
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
	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Buy, 10.0, 10.0, 1), orderId,
	    errorMessage));
	CHECK_EQ(Credits(world, "Civ-01"), 60);
	CHECK_EQ(world.Get<MarketOrderInfo>(orderId)->reservedCredits, 0);

	CHECK(service.PostOrder(
	    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 10.0, 10.0, 2), orderId,
	    errorMessage));

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
	// The sell of 10 units reserved its cargo into the order at the post, so the pile of
	// Civ-02 is 90 while the order that rests on the book holds the 4.1 it has not
	// traded: 94.1 units of the good between the pile and the book, and the 5.9 that
	// filled in the pile of the buyer.
	CHECK_EQ(Units(world, "Civ-02", "iron_ore"), 90.0);
	CHECK_EQ(service.MarketAt(world, "Civ-01")->venueTake, 2);
	// The fill traded all the money of the buyer and 5.9 of the 10 units each order
	// carried: both sides of the pair keep the 4.1 they have left to trade, and a tick
	// trades nothing, so the two rest on the book until a post crosses them again.
	CHECK_EQ(service.OpenOrderCount(world, "Civ-01"), 2u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Buy).size(), 1u);
	CHECK_EQ(service.Book(world, "Civ-01", "iron_ore", OrderSide::Sell).size(), 1u);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->volume, 5.9);
	CHECK_EQ(service.QuoteFor(world, "Civ-01", "iron_ore")->lastPrice, 10.0);

	// A sell order for cargo no one holds: the promises venue, and the goods are taken
	// from where they are, at the fill, and the fill takes only what is there. The
	// promises scenario starts from the money its numbers read: 60 credits, the 60 the
	// trader came with, and the pile the fill above left behind is not the pile a grain
	// buy at 10.0 buys 4 units of - 60 * 10000 / 10040 is 59 credits of goods, and the
	// seller holds 4 units, which is the cargo the fill takes and the 41 credits it pays.
	SetCredits(world, "Civ-01", 60);

	MarketplaceOptions promises;
	promises.requireStock = false;
	promises.requireFunds = false;
	service.SetOptions(promises);

	CHECK(AddWorld(world, "Civ-03", "Cinder Port", 1000, {{"grain", 4.0}}).empty());
	CHECK(service.Open(world, "Civ-03", errorMessage));

	std::string promise;

	CHECK(service.PostOrder(
	    world, Order("Civ-03", "Civ-03", "grain", OrderSide::Sell, 100.0, 10.0, 3), promise,
	    errorMessage));
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
	CHECK(service.PostOrder(
	    world, Order("Civ-03", "Civ-03", "water", OrderSide::Sell, 10.0, 10.0, 5), orderId,
	    errorMessage));

	CHECK(service
	          .ExecuteOrder(
	              world, Order("Civ-03", "Civ-01", "water", OrderSide::Buy, 10.0, 0.0, 6), errorMessage)
	          .empty());
	CHECK_EQ(service.OpenOrderCount(world, "Civ-03"), 2u);
	CHECK_EQ(service.Fills(world, "Civ-03").size(), 1u);
	CHECK_EQ(Units(world, "Civ-03", "water"), 0.0);
	CHECK_EQ(Credits(world, "Civ-01"), 19);
	CHECK(service.BestAsk(world, "Civ-03", "water", bestAsk));
	CHECK_EQ(bestAsk, 10.0);

	service.SetOptions(MarketplaceOptions{});
}


TEST_CASE(a_marketplace_loads_the_world_the_generator_built)
{
	// The world the generator builds: three civilisations, two ages, two categories,
	// four goods of the first age's deck, and a pool of names with room for the three.
	// The same seed and the same inputs are the same three worlds, which is what a
	// load of a loaded world has to agree with.
	Ages ages;
	Categories categories;
	Items items;
	UniverseNamePools pools;

	for (unsigned index = 0; index < 2; ++index)
	{
		AgeNode age;

		age.ageId = "Age" + std::to_string(index);
		age.displayName = age.ageId;
		ages.push_back(age);
	}

	for (unsigned index = 0; index < 2; ++index)
	{
		CategoryNode category;

		category.categoryId = "Category" + std::to_string(index);
		category.displayName = category.categoryId;
		categories.push_back(category);
	}

	for (const char *good : {"iron_ore", "nickel_ore", "water", "grain"})
	{
		ItemNode item;

		item.itemId = good;
		item.displayName = good;
		item.categoryId = "Category0";
		items.push_back(item);
	}

	pools.AddPool(
	    "latin",
	    {"Abigail Prime", "Beacon Reach", "Cinder Port", "Dark Water", "Ember Reach", "Falcon Port"});

	GenerateOptions options;

	options.civilisationCount = 3;
	options.seed = 1301;
	options.minSpecialties = 1;
	options.maxSpecialties = 1;

	Simulation sim;
	std::string errorMessage;

	CHECK(sim.Generate(ages, categories, items, pools, options, errorMessage));

	const std::vector<Civilisation> worlds = sim.GetCivilisations();

	CHECK_EQ(worlds.size(), 3u);

	MarketplaceService service;
	EcsWorld store;
	std::size_t venues = 0;

	CHECK(MarketplaceService::LoadSimulation(store, sim, MarketplaceOptions{}, venues, errorMessage));
	CHECK_EQ(venues, worlds.size());
	CHECK_EQ(service.MarketCount(store), worlds.size());

	std::vector<std::string> markets;

	for (const Civilisation &world : worlds)
	{
		markets.push_back(world.id);

		CHECK(store.Has(world.id));
		CHECK(service.HasMarket(store, world.id));

		const MarketplaceInfo *venue = service.MarketAt(store, world.id);

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
		CHECK_EQ(store.Get<Population>(world.id)->worldUnits, world.population);
		CHECK_EQ(store.Get<Production>(world.id)->perTick.size(), world.production.size());
		CHECK_EQ(store.Get<Demand>(world.id)->perTick.size(), world.demand.size());
	}

	std::sort(markets.begin(), markets.end());

	CHECK_EQ(service.MarketIds(store, {}), markets);
	CHECK_EQ(service.MarketIds(store, worlds[0].id).size(), 1u);
	CHECK_EQ(service.MarketIds(store, "Civ-99").size(), 0u);

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

	CHECK(service.PostOrder(
	    store, Order(venueId, seller, good, OrderSide::Sell, 10.0, 25.0, 1), orderId, errorMessage));

	const std::vector<TradeFill> fills = service.ExecuteOrder(
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
	CHECK_EQ(service.MarketAt(store, venueId)->venueTake, 2);
	CHECK_EQ(service.Fills(store, venueId).size(), 1u);
	CHECK_EQ(service.OpenOrderCount(store, venueId), 0u);
	CHECK_EQ(service.ReferencePrice(store, venueId, good), 25.0);
	CHECK_EQ(service.AdvanceTick(store, 1, errorMessage), worlds.size());
	CHECK_CONTAINS(service.DescribeMarket(store, venueId), good);

	// A loaded world has a station above it: a station is a place the ships of that
	// world put in and pull out of, and a loaded world is a planet the generator put
	// in a system.
	CHECK(
	    MarketplaceService::AddSatellite(
	        store, "Station-" + venueId, venueId, "High Station", 60000.0, 2.5, errorMessage));
	CHECK(service.Open(store, "Station-" + venueId, errorMessage));
	CHECK_EQ(service.MarketIds(store, worlds[0].id).size(), 2u);

	// The world that is already full is not the world this loads into: the ids of a
	// second load collide with the first, and the rule that keeps a fill log honest is
	// that an id names one order.
	CHECK(!MarketplaceService::LoadSimulation(store, sim, MarketplaceOptions{}, venues, errorMessage));
	CHECK_CONTAINS(errorMessage, "entities");
	CHECK_EQ(venues, 0u);

	Simulation fresh;
	EcsWorld empty;

	CHECK(
	    !MarketplaceService::LoadSimulation(empty, fresh, MarketplaceOptions{}, venues, errorMessage));
	CHECK_CONTAINS(errorMessage, "generated");
	CHECK_EQ(venues, 0u);
}


TEST_CASE(the_same_posts_in_the_same_order_give_the_same_fills)
{
	auto Run = [](EcsWorld &world, MarketplaceService &service)
	{
		std::string errorMessage;
		std::string orderId;

		AddWorld(world, "Civ-01", "Abigail Prime", 100000, {{"iron_ore", 1000.0}, {"grain", 500.0}});
		AddWorld(world, "Civ-02", "Beacon Reach", 100000, {{"iron_ore", 1000.0}, {"grain", 500.0}});
		AddWorld(world, "Civ-03", "Cinder Port", 100000, {{"iron_ore", 1000.0}, {"grain", 500.0}});

		service.Open(world, "Civ-01", errorMessage);
		service.Open(world, "Civ-02", errorMessage);

		// The posts in one order, the marketable orders in one order, the ticks in one
		// order: the fills, the treasuries, the stockpiles and the quote tables of two
		// worlds built from one list of posts have to be one answer.
		service.PostOrder(
		    world, Order("Civ-01", "Civ-01", "iron_ore", OrderSide::Sell, 30.0, 12.0, 1), orderId,
		    errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Sell, 20.0, 11.0, 1), orderId,
		    errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 25.0, 11.0, 1), orderId,
		    errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-01", "grain", OrderSide::Buy, 40.0, 6.0, 2), orderId,
		    errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-02", "grain", OrderSide::Sell, 30.0, 6.0, 2), orderId,
		    errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-03", "grain", OrderSide::Sell, 25.0, 5.0, 2), orderId,
		    errorMessage);
		service.ExecuteOrder(
		    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Buy, 40.0, 0.0, 3), errorMessage);
		service.ExecuteOrder(
		    world, Order("Civ-01", "Civ-02", "grain", OrderSide::Buy, 20.0, 7.0, 3), errorMessage);
		service.PostOrder(
		    world, Order("Civ-02", "Civ-01", "iron_ore", OrderSide::Buy, 15.0, 9.0, 4), orderId,
		    errorMessage);
		service.ExecuteOrder(
		    world, Order("Civ-02", "Civ-02", "iron_ore", OrderSide::Sell, 40.0, 0.0, 5), errorMessage);
		service.CancelOrder(world, "Order-6", errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-02", "iron_ore", OrderSide::Buy, 60.0, 13.0, 6), orderId,
		    errorMessage);
		service.ExecuteOrder(
		    world, Order("Civ-01", "Civ-03", "iron_ore", OrderSide::Sell, 100.0, 0.0, 7), errorMessage);
		service.PostOrder(
		    world, Order("Civ-01", "Civ-01", "water", OrderSide::Buy, 12.0, 3.25, 8), orderId,
		    errorMessage);
		service.ExecuteOrder(
		    world, Order("Civ-01", "Civ-02", "water", OrderSide::Sell, 30.0, 0.0, 9), errorMessage);
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
	CHECK_EQ(
	    serviceFirst.MarketAt(first, "Civ-01")->venueTake,
	    serviceSecond.MarketAt(second, "Civ-01")->venueTake);
	CHECK_EQ(Credits(first, "Civ-01"), Credits(second, "Civ-01"));
	CHECK_EQ(Credits(first, "Civ-02"), Credits(second, "Civ-02"));
	CHECK_EQ(Credits(first, "Civ-03"), Credits(second, "Civ-03"));
	CHECK_EQ(Units(first, "Civ-03", "iron_ore"), Units(second, "Civ-03", "iron_ore"));
	CHECK_EQ(Units(first, "Civ-02", "grain"), Units(second, "Civ-02", "grain"));
	CHECK_EQ(
	    serviceFirst.OpenOrderCount(first, "Civ-01"), serviceSecond.OpenOrderCount(second, "Civ-01"));
	CHECK_EQ(
	    serviceFirst.Quotes(first, "Civ-01").size(), serviceSecond.Quotes(second, "Civ-01").size());
	CHECK_EQ(
	    serviceFirst.OrdersOf(first, "Civ-01", "").size(),
	    serviceSecond.OrdersOf(second, "Civ-01", "").size());
	CHECK_EQ(
	    serviceFirst.DescribeMarket(first, "Civ-01"), serviceSecond.DescribeMarket(second, "Civ-01"));
	CHECK_EQ(
	    serviceFirst.DescribeMarket(first, "Civ-02"), serviceSecond.DescribeMarket(second, "Civ-02"));
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
	const std::vector<std::string> asksOne =
	    serviceFirst.Book(first, "Civ-01", "iron_ore", OrderSide::Sell);
	const std::vector<std::string> asksTwo =
	    serviceSecond.Book(second, "Civ-01", "iron_ore", OrderSide::Sell);

	CHECK_EQ(asksOne, asksTwo);
	CHECK_EQ(
	    serviceFirst.Book(first, "Civ-01", "iron_ore", OrderSide::Buy),
	    serviceSecond.Book(second, "Civ-01", "iron_ore", OrderSide::Buy));

	for (const std::string &id : asksOne)
	{
		CHECK_EQ(serviceFirst.DescribeOrder(first, id), serviceSecond.DescribeOrder(second, id));
	}

	CHECK_EQ(serviceFirst.InsuranceBps(first, "Civ-01"), serviceSecond.InsuranceBps(second, "Civ-01"));
	CHECK_EQ(
	    serviceFirst.ListingFeeBps(first, "Civ-01"), serviceSecond.ListingFeeBps(second, "Civ-01"));
	CHECK_EQ(serviceFirst.DepthUnits(first, "Civ-01"), serviceSecond.DepthUnits(second, "Civ-01"));
}

int main()
{
	return civtest::Run("MarketplaceTests");
}
