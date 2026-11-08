#pragma once

#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Ecs/Components.h"
#include "Ecs/EcsWorld.h"
#include "Effects/CivilisationEffects.h"
#include "Effects/EffectTypes.h"
#include <cstddef>
#include <map>
#include <string>
#include <vector>


namespace CivSimCore
{
// What a marketplace is: what it charges for a fill, and how long it keeps an order.
//
// A marketplace does not exist on its own. It sits at a venue: a planet with people of
// its own, or a satellite in orbit around a planet, where the station trade is the
// trade of the ships that come up the gravity well. There is no floating market in
// empty space, and VenueBlockReason says so to a caller that asks for one.
struct MarketplaceOptions
{
	// What the venue takes on a fill, in basis points of the gross, charged to both
	// sides of it. A berth at a dock is the scarce thing, the dock is the venue's, so
	// the venue is paid for it.
	int listingFeeBps{40};

	// The premium a cargo carries at this venue, in basis points: the risk of the
	// lane, the risk of the venue, and the underwriter's share of both. The risk of
	// the pirates and the wardens moves it, which is the number a trader feels when a
	// world buys a pardon.
	int insuranceBps{120};

	// How long the book keeps a resting order before it leaves: the goods and the
	// credits it had held go back to the trader that posted it.
	int orderLifeTicks{20};

	// A sell order has to be covered by the seller's stockpile and a buy order by the
	// buyer's treasury. The reservation is what makes a fill never fail for want of
	// the thing the fill is trading, and a market that allows a sell order for cargo
	// no one holds is a market in promises.
	bool requireStock{true};
	bool requireFunds{true};

	// The floor the venue refuses dust under: a berth taken by one unit of water is a
	// berth a grain barge cannot use.
	double minQuantity{0.0};
	double minPrice{0.0};

	// The units one side of one venue's book may hold at a time, before the market
	// effects of the civilisation raise it. 0 is no cap.
	double depthUnits{0.0};
};

// One order, as a caller asks for it.
struct OrderRequest
{
	std::string marketId; // the venue's body id, which is the market's id
	std::string civilisationId;
	std::string itemId;

	OrderSide side{OrderSide::Buy};

	double quantity{0.0};

	// The worst price the trader will accept: a buy order pays at or under it, a sell
	// order takes at or above it. A resting order has to state one; the order that
	// walks the book does not, and takes the best the book offers.
	double limitPrice{0.0};

	long long tick{0};
};

// The book of a venue, and the settlement of what crosses it.
//
// The book is a price-time book: best price first, and inside a price the order that
// posted first. The resting trader is the maker and sets the price of the fill, which
// is the rule every book of this shape keeps, and the trader that comes looking pays
// the taker's fee for the looking.
//
// A fill settles at once, against the components of the economy model: goods leave the
// seller's Stockpile and enter the buyer's, credits leave the buyer's Treasury and
// enter the seller's, the venue's share of the fees is its venueTake, and the fill
// goes into the venue's log. The scaffold trades the goods at the venue: freight to
// the venue and away from it is the economy model's job, the same job it does today
// with basePrice and no freight at all.
//
// Everything in this class is a system over EcsWorld: the service owns its options and
// nothing else, so two services over one world see the same book, and the book is what
// a save game writes out.
class MarketplaceService
{
  public:
	explicit MarketplaceService(MarketplaceOptions Options = {});

	const MarketplaceOptions &Options() const;
	void SetOptions(MarketplaceOptions Options);

	// ─── Venues ───
	// Whether a body may host a marketplace, and why not:
	//
	//   Planet      hosts one: the world has people, and they are the market.
	//   Satellite   hosts one when it is in orbit around a planet: the station above a
	//               gravity well is where the ships that come up from the world's own
	//               market pass. A satellite of no planet - a rock in a star's orbit, a
	//               moon of a moon - has no well to sit above and no people but the
	//               crews that pass, so it hosts no market.
	//   Star        no people, so no market.
	//
	// Empty for a body that may host one. Every reason names the missing thing, because
	// a caller that asks for a market at a star means a station and has written down the
	// wrong body.
	static std::string VenueBlockReason(const EcsWorld &World, const std::string &BodyId);

	// Opens a marketplace at a body. The venue takes the service's fee, insurance and
	// order life, and it owns the market: the civilisation of the body it sits on.
	bool Open(EcsWorld &World, const std::string &BodyId, std::string &ErrorMessage);

	// Closes one, and every order in its book goes back to the trader that posted it.
	bool Close(EcsWorld &World, const std::string &BodyId, std::string &ErrorMessage);

	bool HasMarket(const EcsWorld &World, const std::string &BodyId) const;

	// The venue ids of every market, sorted; an empty CivilisationId asks for every
	// market in the world.
	std::vector<std::string> MarketIds(const EcsWorld &World, const std::string &CivilisationId) const;
	std::size_t MarketCount(const EcsWorld &World) const;

	const MarketplaceInfo *MarketAt(const EcsWorld &World, const std::string &BodyId) const;

	// ─── The book ───
	// Posts a resting order, and crosses the book it landed on: a seller who posts at
	// or under the best bid trades against it, the way a real book matches a crossing
	// order. The order leaves the book when it has filled, so a filled order is not an
	// answer to "is it in the book" and is an answer to "did it trade".
	bool PostOrder(EcsWorld &World, const OrderRequest &Order, std::string &OrderId, std::string &ErrorMessage);

	// A marketable order: it walks the other side of the book, best price first, until
	// it is filled or the book has nothing left to give it. It leaves nothing resting,
	// and the fills come back best first.
	std::vector<TradeFill> ExecuteOrder(EcsWorld &World, const OrderRequest &Order, std::string &ErrorMessage);

	// Takes a resting order off the book and gives its goods and its credits back to the
	// trader. The venue does not refuse a cancel of an order that is not there: the target
	// state of a cancel is that the order is gone, and the order that expired, the order
	// that was cancelled and the order that was never posted are all gone, so the cancel
	// of one of those is a no-op that says the book is clean. An order the book has traded
	// is not the trader's to withdraw: the promise it kept is in the fill log, the log
	// names the order by its number, and the venue answers the cancel of such a number
	// with the number and the reason, because the goods the order reserved left the
	// stockpile in the fill and a refund would pay the trader the cargo twice. An empty id
	// names no order at all, which is a caller's error and not an order that is gone.
	bool CancelOrder(EcsWorld &World, const std::string &OrderId, std::string &ErrorMessage);

	// Trades every crossing pair at one venue, best price first, until no bid meets an
	// ask. A post crosses the book it landed on, and the venue keeps the book crossed
	// down: a bid that meets an ask is traded at once, so the book's invariant is that
	// the best bid is under the best ask, and a cancel, which takes an order out of a
	// book, can only move the best bid down or the best ask up. A cancel that lifts the
	// best ask therefore lets nothing meet: the pass runs, and the pass finds no pair,
	// which is why a cancel is the trader's goods and credits back and not a trade. The
	// pass is here because the invariant is a rule of this venue and not a law of
	// arithmetic: a book that is opened with orders in it, a book whose fee floor moves,
	// a book a caller has written by hand, all meet the pass, and the pass is the reason
	// a caller never reads a book that crosses itself.
	std::vector<TradeFill> MatchBook(EcsWorld &World, const std::string &MarketId, std::string &ErrorMessage);

	// The book of one good at one venue, on one side, best price first and inside a
	// price by tick then order id. Order ids, so the caller reads the orders with
	// DescribeOrder or gets them from the world.
	std::vector<std::string> Book(
	    const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, OrderSide Side) const;

	// The orders of a trader, of a venue, or of both: sorted by id.
	std::vector<MarketOrderInfo> OrdersOf(
	    const EcsWorld &World, const std::string &MarketId, const std::string &CivilisationId) const;

	std::size_t OpenOrderCount(const EcsWorld &World, const std::string &MarketId) const;

	// ─── Prices ───
	// Rebuilds a venue's quote table from its book, and keeps the prices the fills
	// left behind: the last trade, its tick, the volume ever traded.
	bool RefreshQuotes(EcsWorld &World, const std::string &MarketId, std::string &ErrorMessage);

	// The table as RefreshQuotes left it, one quote per good, sorted by item id. A good
	// enters the table when it is first quoted and never leaves it: the venue has seen it,
	// and its row keeps the numbers of the last trade beside a book that has no orders
	// left in it, which is what a screen shows beside a good that just traded.
	std::vector<MarketQuote> Quotes(const EcsWorld &World, const std::string &MarketId) const;

	// One good: null when the venue has no quote table, or none for that good.
	const MarketQuote *QuoteFor(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId) const;

	bool BestAsk(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, double &Price) const;
	bool BestBid(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, double &Price) const;

	// The middle of the book, for a good with both sides: false when the book has one
	// side or none.
	bool MidPrice(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, double &Price) const;

	// The gap a round trip costs, in basis points of the mid: 0.0 when the book has
	// one side.
	double SpreadBps(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId) const;

	// What a venue would trade a good at: the mid when the book has both sides, the
	// side it has when it has one, the last trade when it has no orders left, 0.0 when
	// the venue has never seen the good. The reading of a price by a ship, a UI and a
	// test, which do not care which of the four it came from.
	double ReferencePrice(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId) const;

	// ─── What the effects of the tree move ───
	// The risk of trading at a venue, in basis points: the market effects of the
	// civilisation that owns it - the warrant, the tribunal, the harbours, the raiders,
	// the fences, the ransom exchange - plus the risk of the lanes it sails. Signed:
	// the law lowers it and the pirates raise it, and a civilisation that has bought
	// both has paid for the sum.
	int RiskPremiumBps(const EcsWorld &World, const std::string &MarketId) const;

	// What the venue takes on a fill: the venue's base, plus the risk of trading there,
	// less the credit the docking licence and the warrant buy - one basis point off the
	// fee for every two berths a day the licence clears - and never below nothing.
	int ListingFeeBps(const EcsWorld &World, const std::string &MarketId) const;

	// What a cargo carries at a venue: the base premium, moved by the market effects.
	int InsuranceBps(const EcsWorld &World, const std::string &MarketId) const;

	// The units one side of the book may hold, the venue's own cap moved by the depth
	// the tribunal and the harbours buy. 0 is no cap.
	double DepthUnits(const EcsWorld &World, const std::string &MarketId) const;

	// The market, body and route effects of a civilisation, onto its venues, its worlds
	// and its lanes, and then every venue's quote table rebuilt, because the clearance
	// of a venue moves the fee and the fee is the number the UI shows beside the price.
	bool ApplyProgression(
	    EcsWorld &World, const ProgressionManager &Progression, const CivilisationEffectEngine &Effects,
	    const std::string &CivilisationId, unsigned &Applied, unsigned &Skipped, std::string &ErrorMessage);

	// ─── Tick ───
	// One tick at every venue: the orders that have outlived their order life leave the
	// book, their goods and their credits go back to the trader that posted them, and
	// each venue's quote table is rebuilt. Returns the venues ticked.
	unsigned AdvanceTick(EcsWorld &World, long long Tick, std::string &ErrorMessage);

	// The fills of a venue, oldest first, the last marketFillLogSize of them.
	std::vector<TradeFill> Fills(const EcsWorld &World, const std::string &MarketId) const;

	// ─── UI ───
	// A venue as lines: what it is, what it charges, the book of its most traded goods.
	std::string DescribeMarket(const EcsWorld &World, const std::string &MarketId) const;

	// An order as one line: "Order-7 Buy 40 iron_ore @ 12.5 at Market-Civ-01 for Civ-01, 12 filled".
	std::string DescribeOrder(const EcsWorld &World, const std::string &OrderId) const;

	// ─── The generated universe, in the store ───
	// One body per generated world, in the order the simulation lists them: the
	// identity, the population, the treasury, the stockpile, the production deck and
	// the demand, then a marketplace at every world that may host one. The economy
	// model keeps running its own way; the store is where the markets, the ships and
	// the effects live.
	//
	// Fails when the world is not empty, because the ids of a second load collide with
	// the first, and VenuesOpened counts what opened: a world that may not host a
	// market leaves the count short, which is the point of the satellite rule.
	static bool LoadSimulation(
	    EcsWorld &World, const Simulation &Sim, const MarketplaceOptions &Options, std::size_t &VenuesOpened,
	    std::string &ErrorMessage);

	// A planet as a body, with the identity the venue rule reads. Adds the components a
	// market settles against - an empty treasury, an empty stockpile - so a world
	// created here is a world a trader can post against.
	static bool AddPlanet(
	    EcsWorld &World, const std::string &BodyId, const std::string &DisplayName, const std::string &SystemName,
	    const std::string &CivilisationId, const std::string &AgeId, std::string &ErrorMessage);

	// A satellite in orbit around a planet: the station above a world's gravity well,
	// and the only kind of satellite a marketplace is opened at.
	static bool AddSatellite(
	    EcsWorld &World, const std::string &SatelliteId, const std::string &PlanetBodyId, const std::string &DisplayName,
	    double RadiusKm, double PeriodDays, std::string &ErrorMessage);

	// ─── Helpers, public because a caller that posts an order needs the same numbers ───
	// The fee on a gross, at a basis point rate, rounded up: the venue never pays a
	// fraction of a credit out of its own pocket.
	static long long FeeFor(long long Gross, int FeeBps);

	// The largest quantity a budget buys at a price and a fee: the whole gross the
	// budget pays, and the units that whole buys.
	static double AffordableQuantity(double Price, int FeeBps, long long Budget);

  private:
	// Posts an order and reserves against it, and crosses nothing: the crossing is the
	// caller's, because a resting order crosses its own book and an order that walks the
	// book walks it a price at a time.
	bool CreateOrder(
	    EcsWorld &World, const OrderRequest &Order, const std::string &MarketId, std::string &OrderId,
	    std::string &ErrorMessage);

	// The crossing pass, writing the fills into the caller's log: a post crosses the book
	// it landed on, and the pass runs after a cancel, where it finds a book that is
	// already crossed down and trades nothing, because taking the top of a side away moves
	// that side's best price away from the other side, not towards it.
	bool MatchInto(
	    EcsWorld &World, const std::string &MarketId, std::vector<TradeFill> &FillLog, std::string &ErrorMessage);

	// The venue, with the message every entry point needs when there is none.
	const MarketplaceInfo *RequireVenue(const EcsWorld &World, const std::string &MarketId, std::string &ErrorMessage) const;

	// The trader, with the message every entry point needs when it is not a world of
	// this store: a trader without a treasury and a stockpile cannot settle.
	bool RequireTrader(
	    const EcsWorld &World, const std::string &CivilisationId, EcsWorld::Entity &Trader, std::string &ErrorMessage) const;

	// The order, with the message every entry point needs when it is not a live order.
	const MarketOrderInfo *RequireOrder(const EcsWorld &World, const std::string &OrderId, std::string &ErrorMessage) const;

	// The live orders of one good on one side of one venue's book, best price first,
	// and inside a price by tick then order id.
	std::vector<EcsWorld::Entity> ActiveOrders(
	    const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, OrderSide Side) const;

	// An order that has left the book: the goods and the credits it held go back, and
	// the order is destroyed. The log keeps the fills; a cancel, an expiry and a fill
	// that completed are not in it, because the log is the trades.
	bool RetireOrder(EcsWorld &World, const std::string &OrderId, std::string &ErrorMessage);

	// One pair of orders meeting: the fill, the settlement, the venue's take, the log.
	// False, and nothing written, when the pair cannot trade a whole unit. The maker is
	// the order that was resting, and it sets the price of the fill. The fill spends the
	// reservations the venue holds - the cargo out of the seller's order, the money out of
	// the buyer's pocket - and hands the goods over; what is left of either is what the
	// order gives back when it leaves the book, so the fill never charges a buyer twice.
	bool FillPair(
	    EcsWorld &World, const std::string &MarketId, EcsWorld::Entity Maker, EcsWorld::Entity Taker,
	    std::vector<TradeFill> &FillLog, std::string &ErrorMessage);

	// A trader's side of the settlement: the money, and the goods. The money a buyer pays
	// comes out of the pocket the venue put it in when the order posted, which is the
	// order's reservation, and out of the treasury only at a venue that let the order post
	// without money: the treasury was charged at the post and is credited back what the
	// fill did not spend, so a fill never charges a buyer twice. The seller is paid its
	// gross less the fee of its side, and the cargo it promised left its stockpile at the
	// post and is handed over out of the order.
	bool SettleParty(
	    EcsWorld &World, const std::string &CivilisationId, const std::string &ItemId, double Quantity, long long Gross,
	    long long Fee, long long &OrderCredits, bool Buys, std::string &ErrorMessage);

	MarketplaceOptions options;
};
} // namespace CivSimCore
