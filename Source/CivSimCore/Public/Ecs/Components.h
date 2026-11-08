#pragma once

#include "Effects/EffectTypes.h"
#include <cstddef>
#include <map>
#include <string>
#include <vector>


namespace CivSimCore
{
// The components of the entity store: what a body, a ship, a market and an order
// *are*, as data.
//
// There is deliberately no behaviour here. An ECS earns its keep when the data and the
// systems that read it are separate, so a component is a plain struct with no
// constructor, no invariant and no virtual function: a component is a fact a system
// can read without asking the entity's permission, and a new system can be written
// over the same store without touching a line of these.
//
// The fields keep the names the rest of the core uses - itemId, civilisationId,
// skillId, ageId - and the units it counts in, so a component read out of the store
// and a Civilisation read out of the economy model say the same thing in the same
// words.

// What a body is, which decides what it may do.
enum class BodyKind
{
	Planet,    // a world: it hosts a market for its own people
	Satellite, // a body in orbit around a planet: it hosts a market for the station trade
	Star       // the sun of a system: no people, so no market
};

const char *DescribeBodyKind(BodyKind Kind);

// The side of a market an order sits on.
enum class OrderSide
{
	Buy, // wants the good, and pays at or under its limit price
	Sell // has the good, and accepts at or above its limit price
};

const char *DescribeOrderSide(OrderSide Side);

// The life of an order in a book.
enum class OrderStatus
{
	Active,    // resting in the book, open to a fill
	Filled,    // every unit of it traded; the order leaves the book and enters the log
	Cancelled, // taken off the book by the trader who posted it
	Expired    // the book kept it for orderLifeTicks and no one traded it
};

const char *DescribeOrderStatus(OrderStatus Status);

// ─── Bodies: planets, satellites, stars ───

// A place in a generated solar system.
struct BodyInfo
{
	std::string bodyId; // "Civ-01", "Moon-Civ-01"

	BodyKind kind{BodyKind::Planet};

	std::string displayName; // "Abigail Prime"

	// A satellite's planet: "" for a planet, "Civ-01" for a moon of that world. It is
	// the venue rule - a satellite with no planet of its own has no trade to host.
	std::string primaryBodyId;

	std::string systemName;     // "Abigail Prime", the solar system the body sits in
	std::string civilisationId; // the world that owns it: "Civ-01"
	std::string ageId;          // the AgeNode the world sits in, "" when none
};

// The orbit of a satellite, and the only reason a satellite is a venue: it sits above
// a planet's gravity well, where the ships that come up from the planet's markets pass.
struct Orbit
{
	std::string primaryId; // the planet it orbits
	double radiusKm{0.0};
	double periodDays{0.0};
};

// Population in world units, not people, the way Civilisation counts it: demand and
// production scale with it and the age sets the ceiling.
struct Population
{
	double worldUnits{1.0};
	double ceiling{1.0}; // the habitat and the age's bound on the world
};

struct Treasury
{
	long long credits{0};
};

// Item id -> units held. The same map Civilisation carries, so a market settles
// against the stockpile the economy model reads.
struct Stockpile
{
	std::map<std::string, double> units;
};

// Item id -> units per tick, the production deck and the demand deck of a world.
struct Production
{
	std::map<std::string, double> perTick;
};

struct Demand
{
	std::map<std::string, double> perTick;
};

// ─── Ships ───

// A hull: the long-haul trader, the modular hull a yard puts together, the raider a
// pirate builds.
struct ShipInfo
{
	std::string shipId;
	std::string hullId; // "HullTraderLongHaul", the key of a ShipClassSpec
	std::string className;
	std::string description;

	// The civilisation that owns and flies the ship: the operator decides which
	// civilisation's technology reaches the ship and which world's market is home.
	std::string operatorId;

	std::string homeMarketId; // the venue the ship berths at, "" when it has none
};

// The hull as built, before any system is fitted: the base every stat is computed from.
struct ShipBaseStats
{
	std::map<EffectStat, double> values;
};

// The hull as it flies: the base plus every fitted system and every technology of the
// operator. Written by ShipEffectEngine::Recompute and read by everything else, so a
// combat, a cargo and a fuel system never adds a tier to itself.
struct ShipStats
{
	std::map<EffectStat, double> values;
};

// The systems a hull has a slot for, by effect id. A skill effect is a system and a
// hull carries a countable number of systems; a technology is knowledge and needs no
// slot, so this is the list a hull can refuse.
struct ShipSlots
{
	std::vector<std::string> fitableEffects;
};

// One system fitted to a hull.
struct FittedSystem
{
	std::string skillId; // the node that bought it, "SkillShipShieldsFront"
	std::string displayName;
	EffectOrigin origin{EffectOrigin::Skill};
	int tier{0}; // 0: not fitted, which is why a fitted system is never 0
};

// The systems a hull carries, keyed by effect id, so the read-out iterates in sorted
// order and a hull with no forward screen has no entry, not an entry at tier 0.
struct ShipSystems
{
	std::map<std::string, FittedSystem> fitted;
};

// ─── Effect sources, as entities ───

// A tier bought, on the entity that bought it. Each source is an entity in its own
// right, because EnTT stores one component of a type per entity and a ship that has
// bought twenty five systems has twenty five sources on it: an aggregate component
// would be a bag of facts a system cannot view.
struct EffectSource
{
	std::string skillId;
	std::string effectId;
	EffectOrigin origin{EffectOrigin::Skill};
	int tier{1}; // the tier bought: a source at tier 0 buys nothing
};

// The store a source lands on, and which domain it lands there as: an effect of the
// Ship domain writes a ShipStats, one of the Market domain writes a MarketEffects.
struct EffectOn
{
	std::string targetId; // the ship, market, yard, lane, claim or body it improves
	EffectDomain domain{EffectDomain::Ship};
};

// ─── Marketplaces ───

// A marketplace at a venue: the planet or the satellite in orbit around a planet whose
// people trade there. The component sits on the venue's body entity, because a market
// is a fact about a place and an order names its venue by body id.
struct MarketplaceInfo
{
	std::string marketId; // the venue's body id, which is the market's id
	std::string venueName;
	std::string civilisationId;

	bool atPlanet{false};
	bool atSatellite{false};

	// What the venue charges on a fill, in basis points, before the risk and the
	// clearance the civilisation has bought move it.
	int baseListingFeeBps{40};

	// How long the book keeps a resting order before it leaves: the berth a berthed
	// order occupies is the scarce thing at a dock.
	int orderLifeTicks{20};

	long long openedTick{0};

	// What the venue has taken in fees: the licence and the deck are the venue's
	// income, and the treasury of the world that hosts it.
	long long venueTake{0};
};

// The market effects a civilisation has bought, summed: the clearance, the insurance,
// the depth, the risk of the venues it owns.
struct MarketEffects
{
	std::map<EffectStat, double> values;
};

// The effects of the Body domain on a world or a satellite: habitat capacity, the
// closed loop, how far terraforming has come, medicine and schooling.
struct BodyEffects
{
	std::map<EffectStat, double> values;
};

// The effects of the Route domain, which a civilisation buys once for all its lanes,
// carried on one entity per civilisation: the risk of sailing between worlds is the
// same lane read from either end of it.
struct RouteEffects
{
	std::map<EffectStat, double> values;
};

// The effects of the Survey domain, on one store per civilisation: how far a survey
// reaches and what a claim turns out.
struct SurveyEffects
{
	std::map<EffectStat, double> values;
};

// The effects of the Yard domain, on one store per civilisation: the throughput and
// the slots the yards of a civilisation have, which is what decides how many systems a
// refit can move at one time.
struct YardEffects
{
	std::map<EffectStat, double> values;
};

// ─── Orders, as entities ───

// One order in a book. An order is an entity, not a row in a map, because a book is a
// pile of orders that each have a life of their own: a fill retires one order and
// leaves its neighbours resting, and a quote reads the book without asking the orders
// about anything but their price.
struct MarketOrderInfo
{
	std::string orderId; // the entity's own id, "Order-7"
	std::string itemId;

	OrderSide side{OrderSide::Buy};
	OrderStatus status{OrderStatus::Active};

	double quantity{0.0}; // the units the trader asked for
	double filled{0.0};   // the units that have traded
	double limitPrice{0.0};

	// What the venue holds against the order while it rests: goods for a sell order,
	// credits for a buy order, so a trader cannot spend twice what it has and a fill
	// never fails for want of the thing it is buying.
	double reservedGoods{0.0};
	long long reservedCredits{0};

	long long placedTick{0};
	long long expiresTick{0};
};

// The venue an order rests at, by body id.
struct OrderAt
{
	std::string marketId;
};

// Who posted it: a civilisation, whose treasury and stockpile settle against the book.
struct OrderOwner
{
	std::string civilisationId;
};

// ─── Quotes and fills ───

// One good at one venue, as the book reads it.
struct MarketQuote
{
	std::string itemId;

	double bestAsk{0.0};
	double bestBid{0.0};
	double mid{0.0};
	double spreadBps{0.0}; // the gap the venue charges a round trip, in basis points

	double askDepth{0.0}; // units at the best price on each side
	double bidDepth{0.0};
	unsigned askOrders{0};
	unsigned bidOrders{0};

	double lastPrice{0.0};
	long long lastTick{0};
	double volume{0.0}; // units traded at this venue, ever
};

// One fill: a maker's resting order meeting a taker at the maker's price.
struct TradeFill
{
	std::string marketId;
	std::string itemId;
	std::string makerOrderId; // the order that was resting
	std::string takerOrderId; // the order that came looking for it
	std::string sellerId;
	std::string buyerId;

	double quantity{0.0};
	double unitPrice{0.0}; // the maker's limit price: the resting trader sets the price

	long long gross{0};
	long long makerFee{0};
	long long takerFee{0};

	long long tick{0};
};

// The book's record of itself, for the UI and the tests: a quote per good, the traded
// volume, and the fills, newest last, capped so a long game does not grow the store.
struct MarketRecord
{
	std::map<std::string, MarketQuote> quotes;

	std::map<std::string, double> volume;
	std::map<std::string, double> lastPrice;
	std::map<std::string, long long> lastTick;

	std::vector<TradeFill> fills;
};

// How many fills a venue remembers.
inline constexpr std::size_t marketFillLogSize = 64;
} // namespace CivSimCore
