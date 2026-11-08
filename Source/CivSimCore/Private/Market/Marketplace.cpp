#include "Market/Marketplace.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>


namespace CivSimCore
{
namespace
{
// A quantity at or under this is nothing: the book counts in whole units of a good,
// and the arithmetic of doubles leaves a residue under the dust of a fill.
constexpr double quantityEpsilon = 1e-9;

double OpenQuantity(const MarketOrderInfo &Order)
{
	return Order.quantity - Order.filled;
}

bool IsFulfilled(const MarketOrderInfo &Order)
{
	return OpenQuantity(Order) <= quantityEpsilon;
}

// The resting-first rule: of two orders that meet, the one that reached the book first
// is the maker and its limit price is the price of the fill.
bool RestsFirst(const MarketOrderInfo &Left, const MarketOrderInfo &Right)
{
	if (Left.placedTick != Right.placedTick)
	{
		return Left.placedTick < Right.placedTick;
	}

	return Left.orderId < Right.orderId;
}

// The units at the best price of one side of a book: the depth a quoter sees, which is
// the size a marketable order can take before the price moves.
// The units resting at the best price, and the number of the orders that quote it: the
// screen that shows a price shows the size of the pile at it in units and in orders, and
// the orders in the whole book are the pile below the best price, which is a number a
// screen has no use for.
unsigned CountAtLevel(const EcsWorld &World, const std::vector<EcsWorld::Entity> &Orders, double Price)
{
	unsigned count = 0;

	for (const EcsWorld::Entity entity : Orders)
	{
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);

		if (info != nullptr && std::abs(info->limitPrice - Price) <= quantityEpsilon)
		{
			++count;
		}
	}

	return count;
}

double DepthAtLevel(const EcsWorld &World, const std::vector<EcsWorld::Entity> &Orders, double Price)
{
	double depth = 0.0;

	for (const EcsWorld::Entity entity : Orders)
	{
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);

		if (info != nullptr && std::abs(info->limitPrice - Price) <= quantityEpsilon)
		{
			depth += OpenQuantity(*info);
		}
	}

	return depth;
}
} // namespace


MarketplaceService::MarketplaceService(MarketplaceOptions Options) : options(std::move(Options))
{
}


const MarketplaceOptions &MarketplaceService::Options() const
{
	return options;
}


void MarketplaceService::SetOptions(MarketplaceOptions Options)
{
	options = std::move(Options);
}


std::string MarketplaceService::VenueBlockReason(const EcsWorld &World, const std::string &BodyId)
{
	if (BodyId.empty())
	{
		return "the body id is empty: a marketplace sits at a body, and a body is an entity of this world";
	}

	if (!World.Has(BodyId))
	{
		return "'" + BodyId + "' is not an entity of this world: a venue is a body, and a body is an entity";
	}

	const BodyInfo *body = World.Get<BodyInfo>(BodyId);

	if (body == nullptr)
	{
		return "'" + BodyId + "' carries no BodyInfo: a market sits at a planet or at a satellite in orbit around one";
	}

	if (World.HasComponent<MarketplaceInfo>(BodyId))
	{
		return "'" + BodyId + "' already hosts a marketplace";
	}

	if (body->civilisationId.empty())
	{
		return "'" + BodyId + "' belongs to no civilisation: the venue's fee, its risk and its take all belong to a world";
	}

	switch (body->kind)
	{
	case BodyKind::Planet:
		return std::string{};

	case BodyKind::Satellite:
		if (body->primaryBodyId.empty())
		{
			return "the satellite '" + BodyId +
			       "' orbits no planet: a station hosts a market because it sits above a world's gravity well, and "
			       "this one sits above nothing";
		}

		if (body->primaryBodyId == body->bodyId)
		{
			return "the satellite '" + BodyId + "' orbits itself";
		}

		{
			const BodyInfo *primary = World.Get<BodyInfo>(body->primaryBodyId);

			if (primary == nullptr)
			{
				return "the satellite '" + BodyId + "' orbits '" + body->primaryBodyId +
				       "', which is not a body of this world";
			}

			if (primary->kind != BodyKind::Planet)
			{
				return "the satellite '" + BodyId + "' orbits " + DescribeBodyKind(primary->kind) + " '" +
				       body->primaryBodyId + "': the station trade is the trade of the ships that come up a planet's "
				       "gravity well, and a star has no market at the bottom of its";
			}
		}

		return std::string{};

	case BodyKind::Star:
		return "'" + BodyId + "' is a star: no people, so no market";
	}

	return "'" + BodyId + "' is no kind of body this core knows";
}


bool MarketplaceService::Open(EcsWorld &World, const std::string &BodyId, std::string &ErrorMessage)
{
	const std::string reason = VenueBlockReason(World, BodyId);

	if (!reason.empty())
	{
		ErrorMessage = "Marketplace: " + reason;

		return false;
	}

	const BodyInfo *body = World.Get<BodyInfo>(BodyId);

	MarketplaceInfo venue;
	venue.marketId = BodyId;
	venue.venueName = body->displayName;
	venue.civilisationId = body->civilisationId;
	venue.atPlanet = body->kind == BodyKind::Planet;
	venue.atSatellite = body->kind == BodyKind::Satellite;
	venue.baseListingFeeBps = options.listingFeeBps;
	venue.orderLifeTicks = options.orderLifeTicks;
	venue.openedTick = 0;

	World.Put<MarketplaceInfo>(BodyId, std::move(venue));

	// The venue's own record of itself: the quote table, the volume, the prices the
	// fills left behind, the log. A market that has never traded has a record and no
	// quotes, which is the reading a UI prints as "nothing traded here yet".
	World.Put<MarketRecord>(BodyId, MarketRecord{});

	return true;
}


bool MarketplaceService::Close(EcsWorld &World, const std::string &BodyId, std::string &ErrorMessage)
{
	MarketplaceInfo *venue = World.Get<MarketplaceInfo>(BodyId);

	if (venue == nullptr)
	{
		ErrorMessage = "Marketplace: '" + BodyId + "' hosts no marketplace";

		return false;
	}

	// Every order of the book goes back to the trader that posted it: a venue that
	// closes in the middle of a session does not keep the goods or the credits of the
	// orders it was holding.
	std::vector<std::string> orders;

	const auto view = World.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const OrderAt *at = World.Get<OrderAt>(entity);

		if (at != nullptr && at->marketId == BodyId)
		{
			const std::string name = World.NameOf(entity);

			if (!name.empty())
			{
				orders.push_back(name);
			}
		}
	}

	for (const auto &orderId : orders)
	{
		if (!RetireOrder(World, orderId, ErrorMessage))
		{
			return false;
		}
	}

	// The venue closes; the market effects of the civilisation stay bought. The tiers
	// were paid for, and a venue that reopens reads the same clearance and the same
	// risk the player bought, so the component is left where it is.
	World.Remove<MarketRecord>(BodyId);
	World.Remove<MarketplaceInfo>(BodyId);

	return true;
}


bool MarketplaceService::HasMarket(const EcsWorld &World, const std::string &BodyId) const
{
	return World.HasComponent<MarketplaceInfo>(BodyId);
}


std::vector<std::string> MarketplaceService::MarketIds(const EcsWorld &World, const std::string &CivilisationId) const
{
	std::vector<std::string> markets;

	const auto view = World.View<MarketplaceInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(entity);

		if (venue == nullptr || (!CivilisationId.empty() && venue->civilisationId != CivilisationId))
		{
			continue;
		}

		markets.push_back(venue->marketId);
	}

	std::sort(markets.begin(), markets.end());

	return markets;
}


std::size_t MarketplaceService::MarketCount(const EcsWorld &World) const
{
	return World.ComponentCount<MarketplaceInfo>();
}


const MarketplaceInfo *MarketplaceService::MarketAt(const EcsWorld &World, const std::string &BodyId) const
{
	return World.Get<MarketplaceInfo>(BodyId);
}


bool MarketplaceService::PostOrder(
    EcsWorld &World, const OrderRequest &Order, std::string &OrderId, std::string &ErrorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(World, Order.marketId, ErrorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	if (Order.itemId.empty())
	{
		ErrorMessage = "Marketplace '" + Order.marketId + "': the itemId is empty";

		return false;
	}

	if (Order.limitPrice < 0.0)
	{
		ErrorMessage = "Marketplace '" + Order.marketId + "': a limit price is never negative: a trader that would "
		               "pay negative for a good, or be paid negative to part with one, has a venue's problem and not "
		               "this book's";

		return false;
	}

	if (Order.limitPrice == 0.0)
	{
		ErrorMessage = "Marketplace '" + Order.marketId + "': a resting order has to state a limit price; the order "
		               "that walks the book is ExecuteOrder, and it names no price";

		return false;
	}

	const std::string marketId = Order.marketId;

	if (!CreateOrder(World, Order, marketId, OrderId, ErrorMessage))
	{
		return false;
	}

	// A post that crosses the book trades at once: the seller who posts at or under the
	// best bid has sold, and the order that filled is out of the book and in the log.
	std::vector<TradeFill> fills;
	std::string matchError;

	// A pair of the book that cannot trade a whole unit is the book's reading, not the
	// post's failure: the order rests, and the venue's quote table is the number that
	// says what the book is offering.
	(void)MatchInto(World, marketId, fills, matchError);

	return RefreshQuotes(World, marketId, ErrorMessage);
}


std::vector<TradeFill> MarketplaceService::ExecuteOrder(EcsWorld &World, const OrderRequest &Order, std::string &ErrorMessage)
{
	std::vector<TradeFill> fills;

	const MarketplaceInfo *venue = RequireVenue(World, Order.marketId, ErrorMessage);

	if (venue == nullptr)
	{
		return fills;
	}

	if (Order.itemId.empty())
	{
		ErrorMessage = "Marketplace '" + Order.marketId + "': the itemId is empty";

		return fills;
	}

	if (Order.quantity <= quantityEpsilon)
	{
		ErrorMessage = "Marketplace '" + Order.marketId + "': the quantity has to be above nothing";

		return fills;
	}

	const std::string marketId = Order.marketId;
	const OrderSide opposite = Order.side == OrderSide::Buy ? OrderSide::Sell : OrderSide::Buy;

	// The order that walks the book is an order like any other, and it is posted like
	// any other, with one difference: it leaves nothing resting. Posting it reserves
	// what a walk needs - the goods of a sell, the credits of a buy - so a walk cannot
	// trade a unit the trader has not got, and the reservation that the walk leaves
	// behind goes back at the end.
	OrderRequest resting = Order;

	if (resting.limitPrice <= 0.0)
	{
		// A marketable order names no price, and a resting order has to. The book's
		// worst quote on its own side is the limit: a buy that walks the asks cannot
		// pay more than the dearest ask, and the asks that are worse than the limit it
		// does name are the asks the limit is there to refuse.
		double worst = 0.0;

		if (Order.side == OrderSide::Buy)
		{
			const std::vector<EcsWorld::Entity> asks = ActiveOrders(World, marketId, Order.itemId, OrderSide::Sell);

			if (!asks.empty())
			{
				const MarketOrderInfo *ask = World.Get<MarketOrderInfo>(asks.back());
				worst = ask != nullptr ? ask->limitPrice : 0.0;
			}
		}
		else
		{
			const std::vector<EcsWorld::Entity> bids = ActiveOrders(World, marketId, Order.itemId, OrderSide::Buy);

			if (!bids.empty())
			{
				const MarketOrderInfo *bid = World.Get<MarketOrderInfo>(bids.back());
				worst = bid != nullptr ? bid->limitPrice : 0.0;
			}
		}

		// A book with nothing on the other side has nothing to trade, and the limit of
		// a walk against an empty book is a price no fill can miss.
		resting.limitPrice = worst > 0.0 ? worst : (Order.side == OrderSide::Buy ? 1e18 : 0.0);

		if (resting.limitPrice <= 0.0)
		{
			ErrorMessage = "Marketplace '" + marketId + "': the book of '" + Order.itemId +
			               "' has no bids for a sell order to walk, and a marketable sell order has to name a price";

			return fills;
		}
	}

	std::string takerId;

	if (!CreateOrder(World, resting, marketId, takerId, ErrorMessage))
	{
		return fills;
	}

	EcsWorld::Entity taker = World.Find(takerId);

	if (!World.IsValid(taker))
	{
		ErrorMessage = "Marketplace '" + marketId + "': the order '" + takerId + "' is gone before it walked";

		return fills;
	}

	while (true)
	{
		MarketOrderInfo *walking = World.Get<MarketOrderInfo>(taker);

		if (walking == nullptr || IsFulfilled(*walking))
		{
			break;
		}

		const std::vector<EcsWorld::Entity> book = ActiveOrders(World, marketId, Order.itemId, opposite);

		if (book.empty())
		{
			break;
		}

		const EcsWorld::Entity maker = book.front();
		const MarketOrderInfo *makerInfo = World.Get<MarketOrderInfo>(maker);

		if (makerInfo == nullptr)
		{
			break;
		}

		// The walk's own limit: the asks above a buy limit, and the bids below a sell
		// limit, are refused, and the book is sorted best first, so the first refusal
		// is the end of the walk.
		if (Order.side == OrderSide::Buy && makerInfo->limitPrice > walking->limitPrice + quantityEpsilon)
		{
			break;
		}

		if (Order.side == OrderSide::Sell && makerInfo->limitPrice < walking->limitPrice - quantityEpsilon)
		{
			break;
		}

		if (!FillPair(World, marketId, maker, taker, fills, ErrorMessage))
		{
			// The pair cannot trade a whole unit: the trader on the other side has run
			// out, so the walk has run out of book.
			break;
		}

		// The taker entity can be the one that filled, and a filled order leaves the
		// book, which destroys it: the walk is over with it.
		if (!World.IsValid(taker))
		{
			break;
		}
	}

	if (World.IsValid(taker))
	{
		if (!RetireOrder(World, takerId, ErrorMessage))
		{
			return fills;
		}
	}

	RefreshQuotes(World, marketId, ErrorMessage);

	return fills;
}


bool MarketplaceService::CancelOrder(EcsWorld &World, const std::string &OrderId, std::string &ErrorMessage)
{
	// An empty id names no order at all, which is a caller's error and not an order that
	// is gone: the venue answers it with a message, because a cancel posted with no id is
	// a call that has nothing to take off the book.
	if (OrderId.empty())
	{
		ErrorMessage = "Marketplace: the orderId is empty; a cancel is answered by the id of the "
		               "order, and an id of nothing names no order";

		return false;
	}

	const MarketOrderInfo *info = RequireOrder(World, OrderId, ErrorMessage);

	if (info == nullptr)
	{
		// The order is not on the book. The venues keep the trades they have made in
		// their records, oldest first and capped to a window, and a number that a log
		// names is a number a book has traded: the promise the order kept is in the log,
		// the goods it reserved left the stockpile in the fill, and a refund would pay
		// the trader the cargo twice, so the venue refuses and names the number. A number
		// the venues have never seen, or one that expired or was cancelled, is gone,
		// which is the target state of a cancel, and the venue answers that with the
		// answer that the book is clean.
		for (const EcsWorld::Entity venueEntity : World.View<MarketRecord>())
		{
			const MarketRecord *record = World.Get<MarketRecord>(venueEntity);

			if (record == nullptr)
			{
				continue;
			}

			for (const TradeFill &fill : record->fills)
			{
				if (fill.makerOrderId == OrderId || fill.takerOrderId == OrderId)
				{
					ErrorMessage =
					    "Marketplace: the order '" + OrderId + "' is in the fill log of venue '" + fill.marketId + "': the order traded, its promise is in the log, its reserved goods left the stockpile in the fill, and a withdraw would pay the trader the cargo twice";

					return false;
				}
			}
		}

		// The target state of a cancel is that the order is gone, and an order that
		// expired, was cancelled, or was never posted is gone. A cancel that reports
		// failure for an order that is already gone leaves the caller with a book it
		// cannot clean, and the books of a tick are cleaned by cancelling what the venue
		// listed.
		ErrorMessage.clear();

		return true;
	}

	const OrderAt *at = World.Get<OrderAt>(OrderId);
	const std::string marketId = at != nullptr ? at->marketId : std::string{};

	if (!RetireOrder(World, OrderId, ErrorMessage))
	{
		return false;
	}

	// The cancel lifts a price off the book, and the orders behind it can now meet: the
	// bid that was third is the bid a resting ask can trade.
	std::vector<TradeFill> fills;
	std::string matchError;

	if (!marketId.empty() && HasMarket(World, marketId))
	{
		(void)MatchInto(World, marketId, fills, matchError);

		return RefreshQuotes(World, marketId, ErrorMessage);
	}

	return true;
}


bool MarketplaceService::CreateOrder(
    EcsWorld &World, const OrderRequest &Order, const std::string &MarketId, std::string &OrderId, std::string &ErrorMessage)
{
	OrderId.clear();

	const MarketplaceInfo *venue = RequireVenue(World, MarketId, ErrorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	if (Order.itemId.empty())
	{
		ErrorMessage = "Marketplace '" + MarketId + "': the itemId is empty; a book is a pile of orders in one good";

		return false;
	}

	if (Order.quantity <= quantityEpsilon)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': the quantity has to be above nothing";

		return false;
	}

	if (Order.limitPrice < 0.0)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': a limit price is never negative; a trader that would pay "
	               "negative for a good is a venue's problem, not this book's";

		return false;
	}

	if (options.minQuantity > 0.0 && Order.quantity + quantityEpsilon < options.minQuantity)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': the venue takes no order under " +
	               FormatNumber(options.minQuantity) + " units of a good";

		return false;
	}

	if (options.minPrice > 0.0 && Order.limitPrice > 0.0 && Order.limitPrice + quantityEpsilon < options.minPrice)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': the venue takes no order under a price of " +
	               FormatNumber(options.minPrice);

		return false;
	}

	EcsWorld::Entity trader = EcsWorld::Null;

	if (!RequireTrader(World, Order.civilisationId, trader, ErrorMessage))
	{
		return false;
	}

	// The fee of the moment the order posts: a venue's fee moves with the risk of the
	// place and the clearance of the licence, and an order that posted at 40 bps is an
	// order that pays 40 bps, whatever the venue's charges read at the fill.
	const int feeBps = ListingFeeBps(World, MarketId);

	// The depth one side of one good's book may hold: a venue with a deck of a certain
	// size turns a quoter away when the deck is full, and the harbours and the tribunal
	// are what buy the size.
	const double cap = DepthUnits(World, MarketId);

	if (cap > 0.0)
	{
		double sideDepth = 0.0;

		for (const EcsWorld::Entity entity : ActiveOrders(World, MarketId, Order.itemId, Order.side))
		{
			const MarketOrderInfo *resting = World.Get<MarketOrderInfo>(entity);

			if (resting != nullptr)
			{
				sideDepth += OpenQuantity(*resting);
			}
		}

		if (sideDepth + Order.quantity > cap + quantityEpsilon)
		{
			ErrorMessage = "Marketplace '" + MarketId + "': the " + DescribeOrderSide(Order.side) + " book of '" +
				           Order.itemId + "' holds " + FormatNumber(sideDepth) + " units and the deck of the venue "
				           "holds " + FormatNumber(cap) + ", the depth the harbours and the tribunal buy";

			return false;
		}
	}

	MarketOrderInfo info;
	info.itemId = Order.itemId;
	info.side = Order.side;
	info.status = OrderStatus::Active;
	info.quantity = Order.quantity;
	info.filled = 0.0;
	info.limitPrice = Order.limitPrice;
	info.placedTick = Order.tick;
	// The life of an order is the life the service posting it gives an order at the tick
	// it lands: the venue's own record keeps the life of the venue as it opened, and the
	// service that writes the order is the service whose charges the order is priced by,
	// the same service whose fee the reservation is built on. The life is added plain, and
	// a life of nothing leaves expiresTick at the tick the order was posted: an order that
	// rests without a life is an order that rests until it trades, is cancelled, or the
	// venue closes, which is the reading AdvanceTick keeps and the reason the expiry of an
	// order is read against the tick it was posted at and not against nothing.
	info.expiresTick = Order.tick + options.orderLifeTicks;

	// The reservation, the reason a fill never fails for want of the thing the fill
	// trades: the goods leave the seller's stockpile when the order posts and sit in the
	// order until it fills, leaves the book, or expires, and the credits leave the
	// buyer's treasury at the same time and sit at the worst price the limit allows.
	//
	// RequireTrader checked that the trader carries both components, so neither pointer
	// is null here, and the two pointers are read and written in the same pool for the
	// whole of the reservation.
	Treasury *treasury = World.Get<Treasury>(trader);
	Stockpile *stock = World.Get<Stockpile>(trader);

	if (Order.side == OrderSide::Sell)
	{
		const auto holding = stock->units.find(Order.itemId);
		const double available = holding != stock->units.end() ? holding->second : 0.0;

		if (options.requireStock)
		{
			if (available + quantityEpsilon < Order.quantity)
			{
				ErrorMessage = "Marketplace '" + MarketId + "': the seller '" + Order.civilisationId + "' holds " +
				               FormatNumber(available) + " units of '" + Order.itemId + "' and posts " +
				               FormatNumber(Order.quantity) + "; a sell order has to be covered by the stockpile it "
				               "sells from";

				return false;
			}

			stock->units[Order.itemId] -= Order.quantity;
			info.reservedGoods = Order.quantity;
		}
		// A venue that allows a sell order for cargo no one holds leaves the goods in the
		// seller's stockpile and spends them at the fill, which is the promises venue and
		// is off by default.
	}
	else if (options.requireFunds)
	{
		const long long cost = std::llround(std::ceil(Order.quantity * Order.limitPrice));
		const long long need = cost + FeeFor(cost, feeBps);

		if (treasury->credits < need)
		{
			ErrorMessage = "Marketplace '" + MarketId + "': the buyer '" + Order.civilisationId + "' has " +
			               std::to_string(treasury->credits) + " credits and the order needs " +
			               std::to_string(need) + ", the goods at the limit price and the venue's fee on them";

			return false;
		}

		treasury->credits -= need;
		info.reservedCredits = need;
	}

	OrderId = World.NextId("Order");
	info.orderId = OrderId;

	EcsWorld::Entity order = EcsWorld::Null;

	if (!World.Create(OrderId, order, ErrorMessage))
	{
		// The id the world hands out is never one it has handed out before, so the only
		// way here is the caller's own id in the way, and a reservation that is refused
		// has to go back the way it came.
		if (info.reservedCredits > 0)
		{
			treasury->credits += info.reservedCredits;
		}

		if (info.reservedGoods > quantityEpsilon)
		{
			stock->units[info.itemId] += info.reservedGoods;
		}

		return false;
	}

	World.Add<MarketOrderInfo>(order, std::move(info));
	World.Add<OrderAt>(order, OrderAt{MarketId});
	World.Add<OrderOwner>(order, OrderOwner{Order.civilisationId});

	return true;
}


bool MarketplaceService::MatchInto(
    EcsWorld &World, const std::string &MarketId, std::vector<TradeFill> &FillLog, std::string &ErrorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(World, MarketId, ErrorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	// Every good with a live order on both sides of the book. The set is the order the
	// goods cross in, and it is the same order on every platform.
	std::set<std::string> items;

	const auto view = World.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const OrderAt *at = World.Get<OrderAt>(entity);
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);

		if (at == nullptr || info == nullptr || at->marketId != MarketId || info->status != OrderStatus::Active)
		{
			continue;
		}

		items.insert(info->itemId);
	}

	for (const auto &itemId : items)
	{
		// The book of one good, crossed to the end: a fill that leaves both sides
		// standing lets the next pair meet, and the crossing stops when the best bid
		// no longer reaches the best ask.
		while (true)
		{
			const std::vector<EcsWorld::Entity> asks = ActiveOrders(World, MarketId, itemId, OrderSide::Sell);
			const std::vector<EcsWorld::Entity> bids = ActiveOrders(World, MarketId, itemId, OrderSide::Buy);

			if (asks.empty() || bids.empty())
			{
				break;
			}

			const MarketOrderInfo *ask = World.Get<MarketOrderInfo>(asks.front());
			const MarketOrderInfo *bid = World.Get<MarketOrderInfo>(bids.front());

			if (ask == nullptr || bid == nullptr)
			{
				break;
			}

			if (bid->limitPrice + quantityEpsilon < ask->limitPrice)
			{
				break;
			}

			// The resting-first rule: the order that reached the book first is the
			// maker, and its limit price is the price both sides trade at.
			const bool askRestsFirst = RestsFirst(*ask, *bid);
			const EcsWorld::Entity maker = askRestsFirst ? asks.front() : bids.front();
			const EcsWorld::Entity taker = askRestsFirst ? bids.front() : asks.front();

			if (!FillPair(World, MarketId, maker, taker, FillLog, ErrorMessage))
			{
				// The pair cannot trade a whole unit - the trader on one side has run out
				// of the thing the price is paid in - and the pair is the front of both
				// books, so nothing behind it can trade either.
				ErrorMessage.clear();

				break;
			}
		}
	}

	return true;
}


std::vector<TradeFill> MarketplaceService::MatchBook(EcsWorld &World, const std::string &MarketId, std::string &ErrorMessage)
{
	std::vector<TradeFill> fills;

	if (!MatchInto(World, MarketId, fills, ErrorMessage))
	{
		return {};
	}

	return fills;
}


std::vector<std::string> MarketplaceService::Book(
    const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, OrderSide Side) const
{
	std::vector<std::string> orders;

	for (const EcsWorld::Entity entity : ActiveOrders(World, MarketId, ItemId, Side))
	{
		const std::string name = World.NameOf(entity);

		if (!name.empty())
		{
			orders.push_back(name);
		}
	}

	return orders;
}


std::vector<MarketOrderInfo> MarketplaceService::OrdersOf(
    const EcsWorld &World, const std::string &MarketId, const std::string &CivilisationId) const
{
	std::vector<MarketOrderInfo> orders;

	const auto view = World.View<MarketOrderInfo, OrderAt, OrderOwner>();

	for (const EcsWorld::Entity entity : view)
	{
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);
		const OrderAt *at = World.Get<OrderAt>(entity);
		const OrderOwner *owner = World.Get<OrderOwner>(entity);

		if (info == nullptr || at == nullptr || owner == nullptr)
		{
			continue;
		}

		if (!MarketId.empty() && at->marketId != MarketId)
		{
			continue;
		}

		if (!CivilisationId.empty() && owner->civilisationId != CivilisationId)
		{
			continue;
		}

		orders.push_back(*info);
	}

	std::sort(orders.begin(), orders.end(), [](const MarketOrderInfo &left, const MarketOrderInfo &right)
	{ return left.orderId < right.orderId; });

	return orders;
}


std::size_t MarketplaceService::OpenOrderCount(const EcsWorld &World, const std::string &MarketId) const
{
	std::size_t open = 0;

	const auto view = World.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);
		const OrderAt *at = World.Get<OrderAt>(entity);

		if (info != nullptr && at != nullptr && (MarketId.empty() || at->marketId == MarketId) &&
		    info->status == OrderStatus::Active && OpenQuantity(*info) > quantityEpsilon)
		{
			++open;
		}
	}

	return open;
}


bool MarketplaceService::RefreshQuotes(EcsWorld &World, const std::string &MarketId, std::string &ErrorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(World, MarketId, ErrorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	// The record keeps what the fills wrote - the last price, its tick, the volume ever
	// traded - and the quote table is rebuilt from the book: a quote is a fact about
	// the book, and a book that changed is a quote table that changed.
	MarketRecord record;

	const MarketRecord *existing = World.Get<MarketRecord>(MarketId);

	if (existing != nullptr)
	{
		record = *existing;
	}

	// The goods of the table the venue published keep their rows, traded or not: the
	// venue has seen them, and the row of a good whose last order left the book is the row
	// a screen shows beside a good nobody is quoting, at zero depth and with the numbers
	// of the last trade beside it, which is what a market's list of goods is. A good whose
	// last trade is in the record keeps its row even when the venue published no quote for
	// it, because the price of the last trade is the number the UI prints beside it.
	std::set<std::string> items;

	for (const auto &entry : record.quotes)
	{
		items.insert(entry.first);
	}

	for (const auto &entry : record.volume)
	{
		items.insert(entry.first);
	}

	record.quotes.clear();

	const auto view = World.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const OrderAt *at = World.Get<OrderAt>(entity);
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);

		if (at != nullptr && info != nullptr && at->marketId == MarketId)
		{
			items.insert(info->itemId);
		}
	}

	for (const auto &itemId : items)
	{
		MarketQuote quote;
		quote.itemId = itemId;

		const std::vector<EcsWorld::Entity> asks = ActiveOrders(World, MarketId, itemId, OrderSide::Sell);
		const std::vector<EcsWorld::Entity> bids = ActiveOrders(World, MarketId, itemId, OrderSide::Buy);

		if (!asks.empty())
		{
			const MarketOrderInfo *best = World.Get<MarketOrderInfo>(asks.front());

			quote.bestAsk = best != nullptr ? best->limitPrice : 0.0;
			quote.askOrders = CountAtLevel(World, asks, quote.bestAsk);
			quote.askDepth = DepthAtLevel(World, asks, quote.bestAsk);
		}

		if (!bids.empty())
		{
			const MarketOrderInfo *best = World.Get<MarketOrderInfo>(bids.front());

			quote.bestBid = best != nullptr ? best->limitPrice : 0.0;
			quote.bidOrders = CountAtLevel(World, bids, quote.bestBid);
			quote.bidDepth = DepthAtLevel(World, bids, quote.bestBid);
		}

		if (quote.bestAsk > 0.0 && quote.bestBid > 0.0 && quote.bestBid <= quote.bestAsk)
		{
			quote.mid = (quote.bestAsk + quote.bestBid) / 2.0;

			if (quote.mid > 0.0)
			{
				quote.spreadBps = (quote.bestAsk - quote.bestBid) / quote.mid * 10000.0;
			}
		}

		const auto lastPrice = record.lastPrice.find(itemId);
		const auto lastTick = record.lastTick.find(itemId);
		const auto volume = record.volume.find(itemId);

		if (lastPrice != record.lastPrice.end())
		{
			quote.lastPrice = lastPrice->second;
		}

		if (lastTick != record.lastTick.end())
		{
			quote.lastTick = lastTick->second;
		}

		if (volume != record.volume.end())
		{
			quote.volume = volume->second;
		}

		record.quotes[itemId] = quote;
	}

	return World.Put<MarketRecord>(MarketId, std::move(record)) != nullptr;
}


std::vector<MarketQuote> MarketplaceService::Quotes(const EcsWorld &World, const std::string &MarketId) const
{
	std::vector<MarketQuote> quotes;

	const MarketRecord *record = World.Get<MarketRecord>(MarketId);

	if (record == nullptr)
	{
		return quotes;
	}

	for (const auto &entry : record->quotes)
	{
		quotes.push_back(entry.second);
	}

	return quotes;
}


const MarketQuote *MarketplaceService::QuoteFor(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId) const
{
	const MarketRecord *record = World.Get<MarketRecord>(MarketId);

	if (record == nullptr)
	{
		return nullptr;
	}

	const auto entry = record->quotes.find(ItemId);

	return entry != record->quotes.end() ? &entry->second : nullptr;
}


bool MarketplaceService::BestAsk(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, double &Price) const
{
	const MarketQuote *quote = QuoteFor(World, MarketId, ItemId);

	if (quote == nullptr || quote->bestAsk <= 0.0)
	{
		return false;
	}

	Price = quote->bestAsk;

	return true;
}


bool MarketplaceService::BestBid(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, double &Price) const
{
	const MarketQuote *quote = QuoteFor(World, MarketId, ItemId);

	if (quote == nullptr || quote->bestBid <= 0.0)
	{
		return false;
	}

	Price = quote->bestBid;

	return true;
}


bool MarketplaceService::MidPrice(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, double &Price) const
{
	const MarketQuote *quote = QuoteFor(World, MarketId, ItemId);

	if (quote == nullptr || quote->mid <= 0.0)
	{
		return false;
	}

	Price = quote->mid;

	return true;
}


double MarketplaceService::SpreadBps(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId) const
{
	const MarketQuote *quote = QuoteFor(World, MarketId, ItemId);

	return quote != nullptr ? quote->spreadBps : 0.0;
}


double MarketplaceService::ReferencePrice(const EcsWorld &World, const std::string &MarketId, const std::string &ItemId) const
{
	const MarketQuote *quote = QuoteFor(World, MarketId, ItemId);

	if (quote == nullptr)
	{
		return 0.0;
	}

	if (quote->mid > 0.0)
	{
		return quote->mid;
	}

	if (quote->bestAsk > 0.0)
	{
		return quote->bestAsk;
	}

	if (quote->bestBid > 0.0)
	{
		return quote->bestBid;
	}

	return quote->lastPrice;
}


int MarketplaceService::RiskPremiumBps(const EcsWorld &World, const std::string &MarketId) const
{
	const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);

	if (venue == nullptr)
	{
		return 0;
	}

	// The venue reads the components its civilisation's tiers wrote, and the lanes of
	// the same civilisation: the risk of trading at a venue is the risk of the place
	// and the risk of the lane the goods came down. The components are written by
	// CivilisationEffectEngine, and this reads them, which is the half of the split
	// that keeps a marketplace from knowing anything about the skill tree.
	double risk = 0.0;

	if (const MarketEffects *effects = World.Get<MarketEffects>(MarketId))
	{
		risk += ReadEffectStat(effects->values, EffectStat::MarketRiskPremium);
	}

	const auto laneId = CivilisationEffectEngine::LaneStoreId(venue->civilisationId);

	if (const RouteEffects *lanes = World.Get<RouteEffects>(laneId))
	{
		risk += ReadEffectStat(lanes->values, EffectStat::RouteRiskPremium);
	}

	return static_cast<int>(std::llround(risk));
}


int MarketplaceService::ListingFeeBps(const EcsWorld &World, const std::string &MarketId) const
{
	const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);

	if (venue == nullptr)
	{
		return options.listingFeeBps;
	}

	int fee = venue->baseListingFeeBps + RiskPremiumBps(World, MarketId);

	// The credit for the berth: one basis point off the fee for every two berths a day
	// the licence and the warrant clear, because a licence that clears the deck faster
	// is a deck that fills the same goods at a cheaper berth.
	if (const MarketEffects *effects = World.Get<MarketEffects>(MarketId))
	{
		const double clearance = ReadEffectStat(effects->values, EffectStat::MarketClearance);

		fee -= static_cast<int>(clearance / 2.0);
	}

	return fee > 0 ? fee : 0;
}


int MarketplaceService::InsuranceBps(const EcsWorld &World, const std::string &MarketId) const
{
	const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);
	int bps = venue != nullptr ? options.insuranceBps : 0;

	if (const MarketEffects *effects = World.Get<MarketEffects>(MarketId))
	{
		bps += static_cast<int>(ReadEffectStat(effects->values, EffectStat::MarketInsurance));
	}

	return bps > 0 ? bps : 0;
}


double MarketplaceService::DepthUnits(const EcsWorld &World, const std::string &MarketId) const
{
	const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);

	if (venue == nullptr || options.depthUnits <= 0.0)
	{
		// No cap: a venue that has set no depth is a venue that quoters arrive at, not
		// a venue that turns them away, and the scaffold's default is that.
		return 0.0;
	}

	double depth = options.depthUnits;

	if (const MarketEffects *effects = World.Get<MarketEffects>(MarketId))
	{
		depth += ReadEffectStat(effects->values, EffectStat::MarketDepth);
	}

	return depth > 0.0 ? depth : 0.0;
}


bool MarketplaceService::ApplyProgression(
    EcsWorld &World, const ProgressionManager &Progression, const CivilisationEffectEngine &Effects,
    const std::string &CivilisationId, unsigned &Applied, unsigned &Skipped, std::string &ErrorMessage)
{
	if (!Effects.ApplyProgression(World, Progression, CivilisationId, Applied, Skipped, ErrorMessage))
	{
		return false;
	}

	// The clearance of a venue moves its fee, and the fee is the number beside the
	// price in every quote the UI shows, so a tree that has been bought is a set of
	// venues whose charges have to be re-read.
	for (const auto &marketId : Effects.MarketsOf(World, CivilisationId))
	{
		if (!RefreshQuotes(World, marketId, ErrorMessage))
		{
			return false;
		}
	}

	return true;
}


unsigned MarketplaceService::AdvanceTick(EcsWorld &World, long long Tick, std::string &ErrorMessage)
{
	unsigned venues = 0;

	for (const auto &marketId : MarketIds(World, {}))
	{
		std::vector<std::string> expired;

		const auto view = World.View<MarketOrderInfo, OrderAt>();

		for (const EcsWorld::Entity entity : view)
		{
			const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);
			const OrderAt *at = World.Get<OrderAt>(entity);

			if (info == nullptr || at == nullptr || at->marketId != marketId)
			{
				continue;
			}

			// An order leaves the book when the tick reaches the expiry its life gave it.
			// An expiry that does not run past the tick the order was posted at is an order
			// posted at a venue with no order life, and the sweep passes it: a venue that
			// sets no order life is a venue that keeps its book, and the order that rests
			// without a life rests until it trades, is cancelled, or the venue closes.
			if (info->status == OrderStatus::Active && info->expiresTick > info->placedTick && info->expiresTick <= Tick)
			{
				const std::string name = World.NameOf(entity);

				if (!name.empty())
				{
					expired.push_back(name);
				}
			}
		}

		// The orders that have outlived their order life leave, and their goods and
		// their credits go back: the berths they held are the berths this tick's
		// quoters need.
		for (const auto &orderId : expired)
		{
			if (!RetireOrder(World, orderId, ErrorMessage))
			{
				return venues;
			}
		}

		if (!RefreshQuotes(World, marketId, ErrorMessage))
		{
			return venues;
		}

		++venues;
	}

	return venues;
}


std::vector<TradeFill> MarketplaceService::Fills(const EcsWorld &World, const std::string &MarketId) const
{
	std::vector<TradeFill> fills;

	const MarketRecord *record = World.Get<MarketRecord>(MarketId);

	if (record == nullptr)
	{
		return fills;
	}

	return record->fills;
}


std::string MarketplaceService::DescribeMarket(const EcsWorld &World, const std::string &MarketId) const
{
	const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);

	if (venue == nullptr)
	{
		return "Market '" + MarketId + "': the world holds no such venue";
	}

	const BodyInfo *body = World.Get<BodyInfo>(MarketId);

	std::ostringstream out;

	out << "Market " << venue->marketId << " at " << (venue->venueName.empty() ? venue->marketId : venue->venueName)
	    << " (" << DescribeBodyKind(body != nullptr ? body->kind : BodyKind::Planet) << " of "
	    << venue->civilisationId << ")";

	if (body != nullptr && body->kind == BodyKind::Satellite)
	{
		out << ", in orbit around " << body->primaryBodyId;
	}

	out << "\n  Charges " << ListingFeeBps(World, venue->marketId) << " bps a side (base "
	    << venue->baseListingFeeBps << ", risk " << RiskPremiumBps(World, venue->marketId)
	    << "), insurance " << InsuranceBps(World, venue->marketId) << " bps, orders rest " << venue->orderLifeTicks
	    << " ticks, take " << venue->venueTake << " credits\n";

	out << "  " << OpenOrderCount(World, venue->marketId) << " open orders, ";

	const std::vector<MarketQuote> quotes = Quotes(World, venue->marketId);

	out << quotes.size() << " goods quoted";

	if (quotes.empty())
	{
		out << ": nothing traded here yet";

		return out.str();
	}

	// The most traded goods first, and inside a volume the item id: a venue quotes
	// dozens of goods and a screen shows a handful.
	std::vector<MarketQuote> ranked = quotes;

	std::sort(ranked.begin(), ranked.end(), [](const MarketQuote &left, const MarketQuote &right)
	{
		if (left.volume != right.volume)
		{
			return left.volume > right.volume;
		}

		return left.itemId < right.itemId;
	});

	for (std::size_t index = 0; index < ranked.size() && index < 5; ++index)
	{
		const MarketQuote &quote = ranked[index];

		out << "\n  " << quote.itemId << ": ask " << FormatNumber(quote.bestAsk) << " bid "
		    << FormatNumber(quote.bestBid) << ", mid " << FormatNumber(quote.mid) << ", spread "
		    << FormatNumber(quote.spreadBps) << " bps, depth " << FormatNumber(quote.askDepth) << "/"
		    << FormatNumber(quote.bidDepth) << ", last " << FormatNumber(quote.lastPrice) << " at tick "
		    << quote.lastTick << ", volume " << FormatNumber(quote.volume);
	}

	return out.str();
}


std::string MarketplaceService::DescribeOrder(const EcsWorld &World, const std::string &OrderId) const
{
	const MarketOrderInfo *info = World.Get<MarketOrderInfo>(OrderId);

	if (info == nullptr)
	{
		return "Order '" + OrderId + "': no order in the world by that name - it filled, expired, was cancelled, or "
		       "was never posted";
	}

	const OrderAt *at = World.Get<OrderAt>(OrderId);
	const OrderOwner *owner = World.Get<OrderOwner>(OrderId);

	std::ostringstream out;

	out << "Order " << info->orderId << " " << DescribeOrderSide(info->side) << " " << FormatNumber(info->quantity)
	    << " " << info->itemId << " @ " << FormatNumber(info->limitPrice) << " at "
	    << (at != nullptr ? at->marketId : std::string("<no venue>")) << " for "
	    << (owner != nullptr ? owner->civilisationId : std::string("<no trader>")) << ", "
	    << DescribeOrderStatus(info->status) << ", " << FormatNumber(info->filled) << " filled, posted at tick "
	    << info->placedTick << ", leaves at tick " << info->expiresTick;

	return out.str();
}


bool MarketplaceService::LoadSimulation(
    EcsWorld &World, const Simulation &Sim, const MarketplaceOptions &Options, std::size_t &VenuesOpened,
    std::string &ErrorMessage)
{
	VenuesOpened = 0;

	if (World.EntityCount() != 0)
	{
		ErrorMessage = "MarketplaceService: the world holds " + std::to_string(World.EntityCount()) +
		               " entities: the generated universe loads into a fresh world, because the ids of a second "
		               "load collide with the first";

		return false;
	}

	if (!Sim.IsGenerated())
	{
		ErrorMessage = "MarketplaceService: the simulation has not generated any worlds";

		return false;
	}

	MarketplaceService service(Options);

	for (const Civilisation &world : Sim.GetCivilisations())
	{
		if (!AddPlanet(World, world.id, world.name, world.systemName, world.id, world.ageId, ErrorMessage))
		{
			ErrorMessage = "MarketplaceService: " + ErrorMessage;

			return false;
		}

		// The economy model's own numbers, in the store's own components: the same
		// stockpile, the same treasury, the same decks, so a market settles against the
		// world the tick model produced and consumed.
		World.Put<Population>(world.id, Population{world.population, world.population});
		World.Put<Treasury>(world.id, Treasury{world.treasury});
		World.Put<Stockpile>(world.id, Stockpile{world.stockpile});
		World.Put<Production>(world.id, Production{world.production});
		World.Put<Demand>(world.id, Demand{world.demand});

		if (!service.Open(World, world.id, ErrorMessage))
		{
			ErrorMessage = "MarketplaceService: world '" + world.id + "': " + ErrorMessage;

			return false;
		}

		++VenuesOpened;
	}

	return true;
}


bool MarketplaceService::AddPlanet(
    EcsWorld &World, const std::string &BodyId, const std::string &DisplayName, const std::string &SystemName,
    const std::string &CivilisationId, const std::string &AgeId, std::string &ErrorMessage)
{
	if (BodyId.empty())
	{
		ErrorMessage = "MarketplaceService: the bodyId is empty";

		return false;
	}

	if (CivilisationId.empty())
	{
		ErrorMessage = "MarketplaceService: the planet '" + BodyId +
		               "' belongs to no civilisation, and a venue's fee, risk and take belong to a world";

		return false;
	}

	EcsWorld::Entity body = EcsWorld::Null;

	if (!World.Create(BodyId, body, ErrorMessage))
	{
		return false;
	}

	BodyInfo info;
	info.bodyId = BodyId;
	info.kind = BodyKind::Planet;
	info.displayName = DisplayName;
	info.systemName = SystemName;
	info.civilisationId = CivilisationId;
	info.ageId = AgeId;

	World.Add<BodyInfo>(body, std::move(info));

	// A world created here settles: an empty treasury and an empty stockpile are the
	// facts a market needs to post against, and a body that cannot settle is a body a
	// trader cannot trade at.
	World.Add<Treasury>(body, Treasury{0});
	World.Add<Stockpile>(body, Stockpile{});
	World.Add<Population>(body, Population{1.0, 1.0});

	return true;
}


bool MarketplaceService::AddSatellite(
    EcsWorld &World, const std::string &SatelliteId, const std::string &PlanetBodyId, const std::string &DisplayName,
    double RadiusKm, double PeriodDays, std::string &ErrorMessage)
{
	if (SatelliteId.empty())
	{
		ErrorMessage = "MarketplaceService: the satelliteId is empty";

		return false;
	}

	const BodyInfo *planet = World.Get<BodyInfo>(PlanetBodyId);

	if (planet == nullptr)
	{
		ErrorMessage = "MarketplaceService: the satellite '" + SatelliteId + "' orbits '" + PlanetBodyId +
		               "', which is not a body of this world";

		return false;
	}

	if (planet->kind != BodyKind::Planet)
	{
		ErrorMessage = "MarketplaceService: the satellite '" + SatelliteId + "' orbits " +
		               DescribeBodyKind(planet->kind) + " '" + PlanetBodyId +
		               "'; the station trade is the trade of the ships that come up a planet's gravity well, and a "
		               "star has no market at the bottom of its";

		return false;
	}

	EcsWorld::Entity body = EcsWorld::Null;

	if (!World.Create(SatelliteId, body, ErrorMessage))
	{
		return false;
	}

	BodyInfo info;
	info.bodyId = SatelliteId;
	info.kind = BodyKind::Satellite;
	info.displayName = DisplayName.empty() ? SatelliteId : DisplayName;
	info.primaryBodyId = planet->bodyId;
	info.systemName = planet->systemName;

	// A station belongs to the world beneath it: the crews that trade there are the
	// crews of the ships that come up from that world's market, and the fee, the risk
	// and the take of the station's venue are that civilisation's.
	info.civilisationId = planet->civilisationId;

	World.Add<BodyInfo>(body, std::move(info));
	World.Add<Orbit>(body, Orbit{planet->bodyId, RadiusKm, PeriodDays});
	World.Add<Treasury>(body, Treasury{0});
	World.Add<Stockpile>(body, Stockpile{});

	return true;
}


long long MarketplaceService::FeeFor(long long Gross, int FeeBps)
{
	if (Gross <= 0 || FeeBps <= 0)
	{
		return 0;
	}

	// Rounded up: a venue that takes 40 bps of a 100 credit trade takes 1 credit, not
	// 0.4 of one, and the fraction it was owed is not paid out of its own pocket.
	const __int128 paid = static_cast<__int128>(Gross) * FeeBps + 9999;

	return static_cast<long long>(paid / 10000);
}


double MarketplaceService::AffordableQuantity(double Price, int FeeBps, long long Budget)
{
	if (Price <= 0.0 || Budget <= 0)
	{
		return 0.0;
	}

	// The whole the budget pays: the goods and the fee on them, at the fee rate, in
	// integer arithmetic, and the units that whole buys.
	const __int128 rate = 10000 + (FeeBps > 0 ? FeeBps : 0);
	const __int128 whole = static_cast<__int128>(Budget) * 10000 / rate;

	if (whole <= 0)
	{
		return 0.0;
	}

	return static_cast<double>(whole) / Price;
}


const MarketplaceInfo *MarketplaceService::RequireVenue(const EcsWorld &World, const std::string &MarketId, std::string &ErrorMessage) const
{
	if (MarketId.empty())
	{
		ErrorMessage = "Marketplace: the marketId is empty; a market is named by the body id of its venue";

		return nullptr;
	}

	const MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);

	if (venue == nullptr)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': no marketplace at that body; Open it first";

		return nullptr;
	}

	return venue;
}


bool MarketplaceService::RequireTrader(
    const EcsWorld &World, const std::string &CivilisationId, EcsWorld::Entity &Trader, std::string &ErrorMessage) const
{
	Trader = World.Find(CivilisationId);

	if (!World.IsValid(Trader))
	{
		ErrorMessage = "Marketplace: '" + CivilisationId +
		               "' is not an entity of this world: a trader is a world, and a world is a body";

		return false;
	}

	if (World.Get<Treasury>(Trader) == nullptr || World.Get<Stockpile>(Trader) == nullptr)
	{
		ErrorMessage = "Marketplace: '" + CivilisationId +
		               "' carries no Treasury and Stockpile, and a trader without both cannot settle a fill";

		return false;
	}

	return true;
}


const MarketOrderInfo *MarketplaceService::RequireOrder(const EcsWorld &World, const std::string &OrderId, std::string &ErrorMessage) const
{
	if (OrderId.empty())
	{
		ErrorMessage = "Marketplace: the orderId is empty";

		return nullptr;
	}

	const MarketOrderInfo *info = World.Get<MarketOrderInfo>(OrderId);

	if (info == nullptr)
	{
		ErrorMessage = "Marketplace: '" + OrderId +
		               "' is no order of this world: it filled, expired, was cancelled, or was never posted";

		return nullptr;
	}

	return info;
}


std::vector<EcsWorld::Entity> MarketplaceService::ActiveOrders(
    const EcsWorld &World, const std::string &MarketId, const std::string &ItemId, OrderSide Side) const
{
	std::vector<EcsWorld::Entity> orders;

	const auto view = World.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const MarketOrderInfo *info = World.Get<MarketOrderInfo>(entity);
		const OrderAt *at = World.Get<OrderAt>(entity);

		if (info == nullptr || at == nullptr)
		{
			continue;
		}

		if (!MarketId.empty() && at->marketId != MarketId)
		{
			continue;
		}

		if (!ItemId.empty() && info->itemId != ItemId)
		{
			continue;
		}

		if (info->side != Side || info->status != OrderStatus::Active)
		{
			continue;
		}

		if (OpenQuantity(*info) <= quantityEpsilon)
		{
			continue;
		}

		orders.push_back(entity);
	}

	// Price then time, the rule every book of this shape keeps: the best price first,
	// and inside a price the order that posted first. The order id breaks the last tie,
	// so the book is one order on every platform, and a fill log that says "Order-7
	// filled first" says it in every run.
	const EcsWorld *world = &World;

	std::sort(orders.begin(), orders.end(), [world, Side](EcsWorld::Entity left, EcsWorld::Entity right)
	{
		const MarketOrderInfo *a = world->Get<MarketOrderInfo>(left);
		const MarketOrderInfo *b = world->Get<MarketOrderInfo>(right);

		if (a == nullptr || b == nullptr)
		{
			return false;
		}

		// A sell book runs cheapest first and a buy book richest first, which is one
		// ascending sort with the buy side's price turned over.
		const double keyA = Side == OrderSide::Sell ? a->limitPrice : -a->limitPrice;
		const double keyB = Side == OrderSide::Sell ? b->limitPrice : -b->limitPrice;

		if (keyA != keyB)
		{
			return keyA < keyB;
		}

		if (a->placedTick != b->placedTick)
		{
			return a->placedTick < b->placedTick;
		}

		return a->orderId < b->orderId;
	});

	return orders;
}


bool MarketplaceService::RetireOrder(EcsWorld &World, const std::string &OrderId, std::string &ErrorMessage)
{
	const MarketOrderInfo *info = World.Get<MarketOrderInfo>(OrderId);

	if (info == nullptr)
	{
		// The order is gone, and a thing that is gone has left nothing to refund.
		return true;
	}

	const OrderOwner *owner = World.Get<OrderOwner>(OrderId);

	// The reservation goes back: what the book held for an order that did not fill is
	// the trader's again, in full, and the ledger of the fill log is the only record of
	// what did trade.
	if (owner != nullptr)
	{
		EcsWorld::Entity trader = World.Find(owner->civilisationId);

		if (World.IsValid(trader))
		{
			if (info->reservedCredits > 0)
			{
				if (Treasury *treasury = World.Get<Treasury>(trader))
				{
					treasury->credits += info->reservedCredits;
				}
			}

			if (info->reservedGoods > quantityEpsilon)
			{
				if (Stockpile *stock = World.Get<Stockpile>(trader))
				{
					stock->units[info->itemId] += info->reservedGoods;
				}
			}
		}
	}

	return World.Destroy(OrderId, ErrorMessage);
}


bool MarketplaceService::FillPair(
    EcsWorld &World, const std::string &MarketId, EcsWorld::Entity Maker, EcsWorld::Entity Taker,
    std::vector<TradeFill> &FillLog, std::string &ErrorMessage)
{
	MarketplaceInfo *venue = World.Get<MarketplaceInfo>(MarketId);
	MarketOrderInfo *maker = World.Get<MarketOrderInfo>(Maker);
	MarketOrderInfo *taker = World.Get<MarketOrderInfo>(Taker);

	if (venue == nullptr || maker == nullptr || taker == nullptr)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': the pair to fill is not a pair of live orders";

		return false;
	}

	if (maker->side == taker->side)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': two " + DescribeOrderSide(maker->side) +
		               " orders cannot trade with each other";

		return false;
	}

	if (maker->itemId != taker->itemId)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': '" + maker->itemId + "' cannot trade against '" + taker->itemId + "'";

		return false;
	}

	const OrderOwner *makerOwner = World.Get<OrderOwner>(Maker);
	const OrderOwner *takerOwner = World.Get<OrderOwner>(Taker);

	if (makerOwner == nullptr || takerOwner == nullptr)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': an order of the pair carries no OrderOwner";

		return false;
	}

	const bool makerSells = maker->side == OrderSide::Sell;
	const std::string &sellerId = makerSells ? makerOwner->civilisationId : takerOwner->civilisationId;
	const std::string &buyerId = makerSells ? takerOwner->civilisationId : makerOwner->civilisationId;

	const EcsWorld::Entity sellerEntity = makerSells ? Maker : Taker;
	const EcsWorld::Entity buyerEntity = makerSells ? Taker : Maker;

	MarketOrderInfo *sellOrder = makerSells ? maker : taker;
	MarketOrderInfo *buyOrder = makerSells ? taker : maker;

	// The maker's limit price is the price: the resting trader set it, and the trader
	// that came looking took the book as it found it.
	const double price = maker->limitPrice;

	if (price <= 0.0)
	{
		ErrorMessage = "Marketplace '" + MarketId + "': the maker '" + maker->orderId + "' names no price";

		return false;
	}

	const std::string itemId = maker->itemId;
	const int feeBps = ListingFeeBps(World, MarketId);

	double quantity = std::min(OpenQuantity(*maker), OpenQuantity(*taker));

	// The goods: a sell order that posted under a venue that requires stock has the
	// units in the reservation, and the fill spends the reservation. A venue that
	// allows a sell order for cargo no one holds - the game of promises, off by default
	// - spends the seller's stockpile at the fill, and takes only what is there.
	if (sellOrder->reservedGoods > quantityEpsilon)
	{
		quantity = std::min(quantity, sellOrder->reservedGoods);
	}
	else if (const Stockpile *stock = World.Get<Stockpile>(sellerEntity))
	{
		const auto holding = stock->units.find(itemId);
		const double available = holding != stock->units.end() ? holding->second : 0.0;

		quantity = std::min(quantity, available);
	}
	else
	{
		quantity = 0.0;
	}

	// The credits: a buy order that posted under a venue that requires funds has the
	// money in the reservation, and the fill spends the reservation at the price it
	// trades at, which is never worse than the limit the reservation was priced at. A
	// venue that allows a buy order the buyer cannot pay - off by default - spends the
	// treasury at the fill, and takes only what the money buys.
	if (buyOrder->reservedCredits > 0)
	{
		quantity = std::min(quantity, AffordableQuantity(price, feeBps, buyOrder->reservedCredits));
	}
	else if (const Treasury *treasury = World.Get<Treasury>(buyerEntity))
	{
		quantity = std::min(quantity, AffordableQuantity(price, feeBps, treasury->credits));
	}
	else
	{
		quantity = 0.0;
	}

	if (quantity <= quantityEpsilon)
	{
		// Nothing of a whole unit can trade: the book has the price and the trader has
		// run out of the thing the price is paid in.
		return false;
	}

	const long long gross = std::llround(price * quantity);
	const long long makerFee = FeeFor(gross, feeBps);
	const long long takerFee = FeeFor(gross, feeBps);

	// Each side pays the fee of its own side of the trade: the resting trader pays the
	// maker's, the trader that came looking pays the taker's, and the venue is paid by
	// both, because the berth and the deck are the venue's.
	// Each side pays the fee of its own side of the trade: the resting trader pays the
	// maker's, the trader that came looking pays the taker's, and the venue is paid by
	// both, because the berth and the deck are the venue's. The buyer's money comes out
	// of the pocket its order holds, and the seller's proceeds go to its treasury.
	if (!SettleParty(
	    World, buyerId, itemId, quantity, gross, buyerId == makerOwner->civilisationId ? makerFee : takerFee,
	    buyOrder->reservedCredits, true, ErrorMessage))
	{
		return false;
	}

	if (!SettleParty(
	    World, sellerId, itemId, quantity, gross, sellerId == makerOwner->civilisationId ? makerFee : takerFee,
	    sellOrder->reservedCredits, false, ErrorMessage))
	{
		return false;
	}

	// A sell order with no reservation spends the seller's stockpile at the fill: the
	// promises venue, and the goods are taken from where they are.
	if (sellOrder->reservedGoods <= quantityEpsilon)
	{
		if (Stockpile *stock = World.Get<Stockpile>(sellerEntity))
		{
			stock->units[itemId] -= quantity;
		}
	}

	sellOrder->reservedGoods = std::max(0.0, sellOrder->reservedGoods - quantity);
	sellOrder->filled += quantity;

	// The buyer's pocket was spent by the settlement, at the price the fill traded at,
	// which is never worse than the limit the pocket was priced at; the cargo left the
	// seller's order the same way, and what is left of either is what the order gives
	// back to its trader when it leaves the book.
	buyOrder->filled += quantity;

	venue->venueTake += makerFee + takerFee;

	TradeFill fill;
	fill.marketId = venue->marketId;
	fill.itemId = itemId;
	fill.makerOrderId = maker->orderId;
	fill.takerOrderId = taker->orderId;
	fill.sellerId = sellerId;
	fill.buyerId = buyerId;
	fill.quantity = quantity;
	fill.unitPrice = price;
	fill.gross = gross;
	fill.makerFee = makerFee;
	fill.takerFee = takerFee;

	// The fill happens at the taker's tick: the trader that came looking is the one
	// that moves now, and the resting order was priced at the tick before it.
	fill.tick = taker->placedTick;

	FillLog.push_back(fill);

	// The venue's record of the trade: the price, its tick, the volume, and the fill in
	// the log, oldest last and capped, so a long game does not grow the store for ever.
	MarketRecord record;

	if (const MarketRecord *existing = World.Get<MarketRecord>(MarketId); existing != nullptr)
	{
		record = *existing;
	}

	record.volume[itemId] += quantity;
	record.lastPrice[itemId] = price;
	record.lastTick[itemId] = fill.tick;
	record.fills.push_back(fill);

	if (record.fills.size() > marketFillLogSize)
	{
		record.fills.erase(record.fills.begin(), record.fills.begin() + static_cast<std::ptrdiff_t>(record.fills.size() - marketFillLogSize));
	}

	World.Put<MarketRecord>(MarketId, std::move(record));

	// An order that has filled leaves the book: its entity goes, and the id stays in
	// the fill log as the number it was posted with.
	for (const EcsWorld::Entity settled : {Maker, Taker})
	{
		const MarketOrderInfo *done = World.Get<MarketOrderInfo>(settled);

		if (done == nullptr || !IsFulfilled(*done))
		{
			continue;
		}

		const std::string name = World.NameOf(settled);

		if (!name.empty())
		{
			MarketOrderInfo settledInfo = *done;
			settledInfo.status = OrderStatus::Filled;

			World.Put<MarketOrderInfo>(settled, std::move(settledInfo));

			if (!RetireOrder(World, name, ErrorMessage))
			{
				return false;
			}
		}
	}

	return true;
}


bool MarketplaceService::SettleParty(
    EcsWorld &World, const std::string &CivilisationId, const std::string &ItemId, double Quantity, long long Gross,
    long long Fee, long long &OrderCredits, bool Buys, std::string &ErrorMessage)
{
	const EcsWorld::Entity trader = World.Find(CivilisationId);

	if (!World.IsValid(trader))
	{
		ErrorMessage = "Marketplace: '" + CivilisationId + "' is not an entity of this world at the fill";

		return false;
	}

	Treasury *treasury = World.Get<Treasury>(trader);
	Stockpile *stock = World.Get<Stockpile>(trader);

	if (treasury == nullptr || stock == nullptr)
	{
		ErrorMessage = "Marketplace: '" + CivilisationId + "' carries no Treasury and Stockpile, and a fill settles in credits and goods";

		return false;
	}

	if (Buys)
	{
		const long long bill = Gross + Fee;

		// The money a buy pays is the money the venue took out of the treasury when the
		// order posted and put in the order, at the worst price the limit allows: the fill
		// spends that pocket, and the treasury pays only what the pocket does not cover,
		// which is nothing at a venue that requires funds. What is left of the pocket goes
		// back to the treasury when the order leaves the book, and a trader that paid at
		// the post is never charged a second time at the fill.
		const long long fromOrder = bill < OrderCredits ? bill : OrderCredits;

		OrderCredits -= fromOrder;
		treasury->credits -= bill - fromOrder;
		stock->units[ItemId] += Quantity;
	}
	else
	{
		// The seller's cargo left its stockpile at the post and is handed over out of the
		// order it was posted in, and the proceeds of the sale arrive at its treasury,
		// less the fee of its own side of the trade.
		treasury->credits += Gross - Fee;
	}

	return true;
}
} // namespace CivSimCore
