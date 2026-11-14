#include <manifest/market/Marketplace.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

#include <jettison/core/Assert.h>


namespace Manifest::Core
{
namespace
{
// A quantity at or under this is nothing: the book counts in whole units of a good,
// and the arithmetic of doubles leaves a residue under the dust of a fill.
constexpr double quantityEpsilon = 1e-9;

std::optional<long long> CheckedFeeFor(long long gross, int feeBps)
{
	if (gross <= 0 || feeBps <= 0)
	{
		return 0;
	}
	const __int128 fee = (static_cast<__int128>(gross) * feeBps + 9999) / 10000;
	if (fee > std::numeric_limits<long long>::max())
	{
		return std::nullopt;
	}
	return static_cast<long long>(fee);
}

std::optional<long long> WholeCost(Decimal price, double quantity, bool roundUp)
{
	if (price < 0 || !std::isfinite(quantity) || quantity < 0.0)
	{
		return std::nullopt;
	}
	// Quantities retain the development model's double arithmetic at this boundary.
	const double value = price.ToDouble() * quantity;
	const double whole = roundUp ? std::ceil(value) : std::round(value);
	// 2^63 is exactly representable; LLONG_MAX rounds up when converted to double.
	if (!std::isfinite(whole) || whole < 0 || whole >= std::ldexp(1.0, 63))
	{
		return std::nullopt;
	}
	return static_cast<long long>(whole);
}

bool CanRefundOrders(const EcsWorld &world, const std::vector<std::string> &orders, std::string &error)
{
	std::map<std::string, __int128> creditRefunds;
	for (const auto &orderId : orders)
	{
		const auto *info = world.Get<MarketOrderInfo>(orderId);
		const auto *owner = world.Get<OrderOwner>(orderId);
		if (info != nullptr && owner != nullptr)
		{
			creditRefunds[owner->civilisationId] += info->reservedCredits;
		}
	}
	for (const auto &[ownerId, refund] : creditRefunds)
	{
		if (const auto *treasury = world.Get<Treasury>(ownerId))
		{
			if (static_cast<__int128>(treasury->credits) + refund >
			    std::numeric_limits<long long>::max())
			{
				error = "Marketplace: reservation refunds exceed the credit range";
				return false;
			}
		}
	}
	return true;
}

double OpenQuantity(const MarketOrderInfo &request)
{
	return request.quantity - request.filled;
}

bool IsFulfilled(const MarketOrderInfo &request)
{
	return OpenQuantity(request) <= quantityEpsilon;
}

// The resting-first rule: of two orders that meet, the one that reached the book first
// is the maker and its limit price is the price of the fill.
bool RestsFirst(const MarketOrderInfo &left, const MarketOrderInfo &right)
{
	if (left.placedTick != right.placedTick)
	{
		return left.placedTick < right.placedTick;
	}

	return left.orderId < right.orderId;
}

// The units at the best price of one side of a book: the depth a quoter sees, which is
// the size a marketable order can take before the price moves.
// The units resting at the best price, and the number of the orders that quote it: the
// screen that shows a price shows the size of the pile at it in units and in orders, and
// the orders in the whole book are the pile below the best price, which is a number a
// screen has no use for.
unsigned CountAtLevel(
    const EcsWorld &ecsWorld, const std::vector<EcsWorld::Entity> &orders, Decimal price)
{
	unsigned count = 0;

	for (const EcsWorld::Entity entity : orders)
	{
		const auto *info = ecsWorld.Get<MarketOrderInfo>(entity);

		if (info != nullptr && info->limitPrice == price)
		{
			++count;
		}
	}

	return count;
}

double DepthAtLevel(
    const EcsWorld &ecsWorld, const std::vector<EcsWorld::Entity> &orders, Decimal price)
{
	double depth = 0.0;

	for (const EcsWorld::Entity entity : orders)
	{
		const auto *info = ecsWorld.Get<MarketOrderInfo>(entity);

		if (info != nullptr && info->limitPrice == price)
		{
			depth += OpenQuantity(*info);
		}
	}

	return depth;
}
} // namespace


MarketplaceService::MarketplaceService(MarketplaceOptions options) : m_options(options)
{
}


const MarketplaceOptions &MarketplaceService::Options() const
{
	return m_options;
}


void MarketplaceService::SetOptions(MarketplaceOptions options)
{
	m_options = options;
}


std::string MarketplaceService::VenueBlockReason(const EcsWorld &ecsWorld, const std::string &bodyId)
{
	if (bodyId.empty())
	{
		return "the body id is empty: a marketplace sits at a body, and a body is an entity of this "
		       "world";
	}

	if (!ecsWorld.Has(bodyId))
	{
		return "'" + bodyId +
		       "' is not an entity of this world: a venue is a body, and a body is an entity";
	}

	const auto *body = ecsWorld.Get<BodyInfo>(bodyId);

	if (body == nullptr)
	{
		return "'" + bodyId +
		       "' carries no BodyInfo: a market sits at a planet or at a satellite in orbit around one";
	}

	if (ecsWorld.HasComponent<MarketplaceInfo>(bodyId))
	{
		return "'" + bodyId + "' already hosts a marketplace";
	}

	if (body->civilisationId.empty())
	{
		return "'" + bodyId +
		       "' belongs to no civilisation: the venue's fee, its risk and its take all belong to a "
		       "world";
	}

	switch (body->kind)
	{
		case BodyKind::Planet: return std::string{};

		case BodyKind::Satellite:
			if (body->primaryBodyId.empty())
			{
				return "the satellite '" + bodyId +
				       "' orbits no planet: a station hosts a market because it sits above a world's "
				       "gravity well, and "
				       "this one sits above nothing";
			}

			if (body->primaryBodyId == body->bodyId)
			{
				return "the satellite '" + bodyId + "' orbits itself";
			}

			{
				const auto *primary = ecsWorld.Get<BodyInfo>(body->primaryBodyId);

				if (primary == nullptr)
				{
					return "the satellite '" + bodyId + "' orbits '" + body->primaryBodyId +
					       "', which is not a body of this world";
				}

				if (primary->kind != BodyKind::Planet)
				{
					return "the satellite '" + bodyId + "' orbits " + DescribeBodyKind(primary->kind) +
					       " '" + body->primaryBodyId +
					       "': the station trade is the trade of the ships that come up a planet's "
					       "gravity well, and a star has no market at the bottom of its";
				}
			}

			return std::string{};

		case BodyKind::Star: return "'" + bodyId + "' is a star: no people, so no market";
	}

	return "'" + bodyId + "' is no kind of body this core knows";
}


bool MarketplaceService::Open(EcsWorld &ecsWorld, const std::string &bodyId, std::string &errorMessage)
{
	const std::string reason = VenueBlockReason(ecsWorld, bodyId);

	if (!reason.empty())
	{
		errorMessage = "Marketplace: " + reason;

		return false;
	}

	const BodyInfo *body = ecsWorld.Get<BodyInfo>(bodyId);

	MarketplaceInfo venue;
	venue.marketId = bodyId;
	venue.venueName = body->displayName;
	venue.civilisationId = body->civilisationId;
	venue.atPlanet = body->kind == BodyKind::Planet;
	venue.atSatellite = body->kind == BodyKind::Satellite;
	venue.baseListingFeeBps = m_options.listingFeeBps;
	venue.orderLifeTicks = m_options.orderLifeTicks;
	venue.openedTick = 0;

	ecsWorld.Put<MarketplaceInfo>(bodyId, std::move(venue));

	// The venue's own record of itself: the quote table, the volume, the prices the
	// fills left behind, the log. A market that has never traded has a record and no
	// quotes, which is the reading a UI prints as "nothing traded here yet".
	ecsWorld.Put<MarketRecord>(bodyId, MarketRecord{});

	return true;
}


bool MarketplaceService::Close(EcsWorld &ecsWorld, const std::string &bodyId, std::string &errorMessage)
{
	auto *venue = ecsWorld.Get<MarketplaceInfo>(bodyId);

	if (venue == nullptr)
	{
		errorMessage = std::format("Marketplace: '{}' hosts no marketplace", bodyId);

		return false;
	}

	// Every order of the book goes back to the trader that posted it: a venue that
	// closes in the middle of a session does not keep the goods or the credits of the
	// orders it was holding.
	std::vector<std::string> orders;

	const auto view = ecsWorld.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const OrderAt *at = ecsWorld.Get<OrderAt>(entity);

		if (at != nullptr && at->marketId == bodyId)
		{
			const std::string name = ecsWorld.NameOf(entity);

			if (!name.empty())
			{
				orders.push_back(name);
			}
		}
	}

	std::sort(orders.begin(), orders.end());
	if (!CanRefundOrders(ecsWorld, orders, errorMessage))
	{
		return false;
	}

	for (const auto &orderId : orders)
	{
		if (!RetireOrder(ecsWorld, orderId, errorMessage))
		{
			return false;
		}
	}

	// The venue closes; the market effects of the civilisation stay bought. The tiers
	// were paid for, and a venue that reopens reads the same clearance and the same
	// risk the player bought, so the component is left where it is.
	ecsWorld.Remove<MarketRecord>(bodyId);
	ecsWorld.Remove<MarketplaceInfo>(bodyId);

	return true;
}


bool MarketplaceService::HasMarket(const EcsWorld &ecsWorld, const std::string &bodyId) const
{
	return ecsWorld.HasComponent<MarketplaceInfo>(bodyId);
}


std::vector<std::string> MarketplaceService::MarketIds(
    const EcsWorld &ecsWorld, const std::string &civilisationId) const
{
	std::vector<std::string> markets;

	const auto view = ecsWorld.View<MarketplaceInfo>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *venue = ecsWorld.Get<MarketplaceInfo>(entity);

		if (venue == nullptr || (!civilisationId.empty() && venue->civilisationId != civilisationId))
		{
			continue;
		}

		markets.push_back(venue->marketId);
	}

	std::sort(markets.begin(), markets.end());

	return markets;
}


std::size_t MarketplaceService::MarketCount(const EcsWorld &ecsWorld) const
{
	return ecsWorld.ComponentCount<MarketplaceInfo>();
}


const MarketplaceInfo *MarketplaceService::MarketAt(
    const EcsWorld &ecsWorld, const std::string &bodyId) const
{
	return ecsWorld.Get<MarketplaceInfo>(bodyId);
}


bool MarketplaceService::PostOrder(
    EcsWorld &ecsWorld, const OrderRequest &request, std::string &orderId, std::string &errorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(ecsWorld, request.marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	if (request.itemId.empty())
	{
		errorMessage = std::format("Marketplace '{}': the itemId is empty", request.marketId);

		return false;
	}

	if (request.limitPrice < 0)
	{
		errorMessage = std::format(
		    "Marketplace '{}': a limit price is never negative: a trader that would pay negative for a "
		    "good, or be paid negative to part with one, has a venue's problem and not this book's",
		    request.marketId);

		return false;
	}

	if (request.limitPrice == 0)
	{
		errorMessage = std::format(
		    "Marketplace '{}': a resting order has to state a limit price; the order that walks the "
		    "book is ExecuteOrder, and it names no price",
		    request.marketId);

		return false;
	}

	const std::string marketId = request.marketId;

	if (!CreateOrder(ecsWorld, request, marketId, orderId, errorMessage))
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
	(void)MatchInto(ecsWorld, marketId, fills, matchError);

	return RefreshQuotes(ecsWorld, marketId, errorMessage);
}


std::vector<TradeFill> MarketplaceService::ExecuteOrder(
    EcsWorld &ecsWorld, const OrderRequest &request, std::string &errorMessage)
{
	std::vector<TradeFill> fills;

	const MarketplaceInfo *venue = RequireVenue(ecsWorld, request.marketId, errorMessage);

	if (venue == nullptr)
	{
		return fills;
	}

	if (request.itemId.empty())
	{
		errorMessage = std::format("Marketplace '{}': the itemId is empty", request.marketId);

		return fills;
	}

	if (!std::isfinite(request.quantity) || request.quantity <= quantityEpsilon)
	{
		errorMessage =
		    std::format("Marketplace '{}': the quantity has to be above nothing", request.marketId);

		return fills;
	}

	if (request.limitPrice < 0)
	{
		errorMessage = "Marketplace: a limit price cannot be negative";
		return fills;
	}

	const std::string marketId = request.marketId;
	const OrderSide opposite = request.side == OrderSide::Buy ? OrderSide::Sell : OrderSide::Buy;

	// The order that walks the book is an order like any other, and it is posted like
	// any other, with one difference: it leaves nothing resting. Posting it reserves
	// what a walk needs - the goods of a sell, the credits of a buy - so a walk cannot
	// trade a unit the trader has not got, and the reservation that the walk leaves
	// behind goes back at the end.
	OrderRequest resting = request;

	if (resting.limitPrice <= 0)
	{
		// A marketable order names no price, and a resting order has to. The book's
		// worst quote on its own side is the limit: a buy that walks the asks cannot
		// pay more than the dearest ask, and the asks that are worse than the limit it
		// does name are the asks the limit is there to refuse.
		Decimal worst;

		if (request.side == OrderSide::Buy)
		{
			const std::vector<EcsWorld::Entity> asks =
			    ActiveOrders(ecsWorld, marketId, request.itemId, OrderSide::Sell);

			if (!asks.empty())
			{
				const MarketOrderInfo *ask = ecsWorld.Get<MarketOrderInfo>(asks.back());
				worst = ask != nullptr ? ask->limitPrice : Decimal{};
			}
		}
		else
		{
			const std::vector<EcsWorld::Entity> bids =
			    ActiveOrders(ecsWorld, marketId, request.itemId, OrderSide::Buy);

			if (!bids.empty())
			{
				const MarketOrderInfo *bid = ecsWorld.Get<MarketOrderInfo>(bids.back());
				worst = bid != nullptr ? bid->limitPrice : Decimal{};
			}
		}

		// A book with nothing on the other side has nothing to trade, and the limit of
		// a walk against an empty book is a price no fill can miss.
		resting.limitPrice =
		    worst > 0 ? worst : (request.side == OrderSide::Buy ? Decimal::Max() : Decimal{});

		if (resting.limitPrice <= 0)
		{
			errorMessage = std::format(
			    "Marketplace '{}': the book of '{}' has no bids for a sell order to walk, and a "
			    "marketable sell order has to name a price",
			    marketId, request.itemId);

			return fills;
		}
	}

	std::string takerId;

	if (!CreateOrder(ecsWorld, resting, marketId, takerId, errorMessage))
	{
		return fills;
	}

	EcsWorld::Entity taker = ecsWorld.Find(takerId);

	if (!ecsWorld.IsValid(taker))
	{
		errorMessage =
		    std::format("Marketplace '{}': the order '{}' is gone before it walked", marketId, takerId);

		return fills;
	}

	while (true)
	{
		auto *walking = ecsWorld.Get<MarketOrderInfo>(taker);

		if (walking == nullptr || IsFulfilled(*walking))
		{
			break;
		}

		const std::vector<EcsWorld::Entity> book =
		    ActiveOrders(ecsWorld, marketId, request.itemId, opposite);

		if (book.empty())
		{
			break;
		}

		const EcsWorld::Entity maker = book.front();
		const MarketOrderInfo *makerInfo = ecsWorld.Get<MarketOrderInfo>(maker);

		if (makerInfo == nullptr)
		{
			break;
		}

		// The walk's own limit: the asks above a buy limit, and the bids below a sell
		// limit, are refused, and the book is sorted best first, so the first refusal
		// is the end of the walk.
		if (request.side == OrderSide::Buy && makerInfo->limitPrice > walking->limitPrice)
		{
			break;
		}

		if (request.side == OrderSide::Sell && makerInfo->limitPrice < walking->limitPrice)
		{
			break;
		}

		if (!FillPair(ecsWorld, marketId, maker, taker, fills, errorMessage))
		{
			// The pair cannot trade a whole unit: the trader on the other side has run
			// out, so the walk has run out of book.
			break;
		}

		// The taker entity can be the one that filled, and a filled order leaves the
		// book, which destroys it: the walk is over with it.
		if (!ecsWorld.IsValid(taker))
		{
			break;
		}
	}

	if (ecsWorld.IsValid(taker))
	{
		if (!RetireOrder(ecsWorld, takerId, errorMessage))
		{
			return fills;
		}
	}

	RefreshQuotes(ecsWorld, marketId, errorMessage);

	return fills;
}


bool MarketplaceService::CancelOrder(
    EcsWorld &ecsWorld, const std::string &orderId, std::string &errorMessage)
{
	// An empty id names no order at all, which is a caller's error and not an order that
	// is gone: the venue answers it with a message, because a cancel posted with no id is
	// a call that has nothing to take off the book.
	if (orderId.empty())
	{
		errorMessage = "Marketplace: the orderId is empty; a cancel is answered by the id of the "
		               "order, and an id of nothing names no order";

		return false;
	}

	const MarketOrderInfo *info = RequireOrder(ecsWorld, orderId, errorMessage);

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
		for (const EcsWorld::Entity venueEntity : ecsWorld.View<MarketRecord>())
		{
			const MarketRecord *record = ecsWorld.Get<MarketRecord>(venueEntity);

			if (record == nullptr)
			{
				continue;
			}

			for (const TradeFill &fill : record->fills)
			{
				if (fill.makerOrderId == orderId || fill.takerOrderId == orderId)
				{
					errorMessage = std::format(
					    "Marketplace: the order '{}' is in the fill log of venue '{}': the order "
					    "traded, its promise is in the log, its reserved goods left the stockpile in "
					    "the fill, and a withdraw would pay the trader the cargo twice",
					    orderId, fill.marketId);

					return false;
				}
			}
		}

		// The target state of a cancel is that the order is gone, and an order that
		// expired, was cancelled, or was never posted is gone. A cancel that reports
		// failure for an order that is already gone leaves the caller with a book it
		// cannot clean, and the books of a tick are cleaned by cancelling what the venue
		// listed.
		errorMessage.clear();

		return true;
	}

	const OrderAt *at = ecsWorld.Get<OrderAt>(orderId);
	const std::string marketId = at != nullptr ? at->marketId : std::string{};

	if (!RetireOrder(ecsWorld, orderId, errorMessage))
	{
		return false;
	}

	// The cancel lifts a price off the book, and the orders behind it can now meet: the
	// bid that was third is the bid a resting ask can trade.
	std::vector<TradeFill> fills;
	std::string matchError;

	if (!marketId.empty() && HasMarket(ecsWorld, marketId))
	{
		(void)MatchInto(ecsWorld, marketId, fills, matchError);

		return RefreshQuotes(ecsWorld, marketId, errorMessage);
	}

	return true;
}


bool MarketplaceService::CreateOrder(
    EcsWorld &ecsWorld, const OrderRequest &request, const std::string &marketId, std::string &orderId,
    std::string &errorMessage)
{
	orderId.clear();

	const MarketplaceInfo *venue = RequireVenue(ecsWorld, marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	if (request.itemId.empty())
	{
		errorMessage = std::format(
		    "Marketplace '{}': the itemId is empty; a book is a pile of orders in one good", marketId);

		return false;
	}

	if (!std::isfinite(request.quantity) || request.quantity <= quantityEpsilon)
	{
		errorMessage = std::format("Marketplace '{}': the quantity has to be above nothing", marketId);

		return false;
	}

	if (request.limitPrice < 0)
	{
		errorMessage = std::format(
		    "Marketplace '{}': a limit price is never negative; a trader that would pay negative for a "
		    "good is a venue's problem, not this book's",
		    marketId);

		return false;
	}

	if (m_options.minQuantity > 0.0 && request.quantity + quantityEpsilon < m_options.minQuantity)
	{
		errorMessage = std::format(
		    "Marketplace '{}': the venue takes no order under {} units of a good", marketId,
		    FormatNumber(m_options.minQuantity));

		return false;
	}

	if (m_options.minPrice > 0 && request.limitPrice > 0 && request.limitPrice < m_options.minPrice)
	{
		errorMessage = std::format(
		    "Marketplace '{}': the venue takes no order under a price of {}", marketId,
		    m_options.minPrice.ToString());

		return false;
	}

	EcsWorld::Entity trader = EcsWorld::Null;

	if (!RequireTrader(ecsWorld, request.civilisationId, trader, errorMessage))
	{
		return false;
	}

	// The fee of the moment the order posts: a venue's fee moves with the risk of the
	// place and the clearance of the licence, and an order that posted at 40 bps is an
	// order that pays 40 bps, whatever the venue's charges read at the fill.
	const int feeBps = ListingFeeBps(ecsWorld, marketId);

	// The depth one side of one good's book may hold: a venue with a deck of a certain
	// size turns a quoter away when the deck is full, and the harbours and the tribunal
	// are what buy the size.
	const double cap = DepthUnits(ecsWorld, marketId);

	if (cap > 0.0)
	{
		double sideDepth = 0.0;

		for (const EcsWorld::Entity entity :
		     ActiveOrders(ecsWorld, marketId, request.itemId, request.side))
		{
			const MarketOrderInfo *resting = ecsWorld.Get<MarketOrderInfo>(entity);

			if (resting != nullptr)
			{
				sideDepth += OpenQuantity(*resting);
			}
		}

		if (sideDepth + request.quantity > cap + quantityEpsilon)
		{
			errorMessage = std::format(
			    "Marketplace '{}': the {} book of '{}' holds {} units and the deck of the venue holds "
			    "{}, the depth the harbours and the tribunal buy",
			    marketId, DescribeOrderSide(request.side), request.itemId, FormatNumber(sideDepth),
			    FormatNumber(cap));

			return false;
		}
	}

	MarketOrderInfo info;
	info.itemId = request.itemId;
	info.side = request.side;
	info.status = OrderStatus::Active;
	info.quantity = request.quantity;
	info.filled = 0.0;
	info.limitPrice = request.limitPrice;
	info.placedTick = request.tick;
	// The life of an order is the life the service posting it gives an order at the tick
	// it lands: the venue's own record keeps the life of the venue as it opened, and the
	// service that writes the order is the service whose charges the order is priced by,
	// the same service whose fee the reservation is built on. The life is added plain, and
	// a life of nothing leaves expiresTick at the tick the order was posted: an order that
	// rests without a life is an order that rests until it trades, is cancelled, or the
	// venue closes, which is the reading AdvanceTick keeps and the reason the expiry of an
	// order is read against the tick it was posted at and not against nothing.
	info.expiresTick = request.tick + m_options.orderLifeTicks;

	if (!WholeCost(request.limitPrice, request.quantity, true))
	{
		errorMessage = "Marketplace: order notional exceeds the credit range";
		return false;
	}

	// The reservation, the reason a fill never fails for want of the thing the fill
	// trades: the goods leave the seller's stockpile when the order posts and sit in the
	// order until it fills, leaves the book, or expires, and the credits leave the
	// buyer's treasury at the same time and sit at the worst price the limit allows.
	//
	// RequireTrader checked that the trader carries both components, so neither pointer
	// is null here, and the two pointers are read and written in the same pool for the
	// whole of the reservation.
	auto *treasury = ecsWorld.Get<Treasury>(trader);
	auto *stock = ecsWorld.Get<Stockpile>(trader);

	if (request.side == OrderSide::Sell)
	{
		const auto holding = stock->units.find(request.itemId);
		const double available = holding != stock->units.end() ? holding->second : 0.0;

		if (m_options.requireStock)
		{
			if (available + quantityEpsilon < request.quantity)
			{
				errorMessage = std::format(
				    "Marketplace '{}': the seller '{}' holds {} units of '{}' and posts {}; a sell "
				    "order has to be covered by the stockpile it sells from",
				    marketId, request.civilisationId, FormatNumber(available), request.itemId,
				    FormatNumber(request.quantity));

				return false;
			}

			stock->units[request.itemId] -= request.quantity;
			info.reservedGoods = request.quantity;
		}
		// A venue that allows a sell order for cargo no one holds leaves the goods in the
		// seller's stockpile and spends them at the fill, which is the promises venue and
		// is off by default.
	}
	else if (m_options.requireFunds)
	{
		const auto cost = WholeCost(request.limitPrice, request.quantity, true);
		const auto fee = cost ? CheckedFeeFor(*cost, feeBps) : std::nullopt;
		if (!cost || !fee ||
		    static_cast<__int128>(*cost) + *fee > std::numeric_limits<long long>::max())
		{
			errorMessage = "Marketplace: buy reservation exceeds the credit range";
			return false;
		}
		const long long need = *cost + *fee;

		if (treasury->credits < need)
		{
			errorMessage = std::format(
			    "Marketplace '{}': the buyer '{}' has {} credits and the order needs {}, the goods at "
			    "the limit price and the venue's fee on them",
			    marketId, request.civilisationId, std::to_string(treasury->credits),
			    std::to_string(need));

			return false;
		}

		treasury->credits -= need;
		info.reservedCredits = need;
	}

	orderId = ecsWorld.NextId("Order");
	info.orderId = orderId;

	EcsWorld::Entity order = EcsWorld::Null;

	if (!ecsWorld.Create(orderId, order, errorMessage))
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

	ecsWorld.Add<MarketOrderInfo>(order, std::move(info));
	ecsWorld.Add<OrderAt>(order, OrderAt{marketId});
	ecsWorld.Add<OrderOwner>(order, OrderOwner{request.civilisationId});

	return true;
}


bool MarketplaceService::MatchInto(
    EcsWorld &ecsWorld, const std::string &marketId, std::vector<TradeFill> &fillLog,
    std::string &errorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(ecsWorld, marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	// Every good with a live order on both sides of the book. The set is the order the
	// goods cross in, and it is the same order on every platform.
	std::set<std::string> items;

	const auto view = ecsWorld.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const OrderAt *at = ecsWorld.Get<OrderAt>(entity);
		const MarketOrderInfo *info = ecsWorld.Get<MarketOrderInfo>(entity);

		if (at == nullptr || info == nullptr || at->marketId != marketId ||
		    info->status != OrderStatus::Active)
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
			const std::vector<EcsWorld::Entity> asks =
			    ActiveOrders(ecsWorld, marketId, itemId, OrderSide::Sell);
			const std::vector<EcsWorld::Entity> bids =
			    ActiveOrders(ecsWorld, marketId, itemId, OrderSide::Buy);

			if (asks.empty() || bids.empty())
			{
				break;
			}

			const MarketOrderInfo *ask = ecsWorld.Get<MarketOrderInfo>(asks.front());
			const MarketOrderInfo *bid = ecsWorld.Get<MarketOrderInfo>(bids.front());

			if (ask == nullptr || bid == nullptr)
			{
				break;
			}

			if (bid->limitPrice < ask->limitPrice)
			{
				break;
			}

			// The resting-first rule: the order that reached the book first is the
			// maker, and its limit price is the price both sides trade at.
			const bool askRestsFirst = RestsFirst(*ask, *bid);
			const EcsWorld::Entity maker = askRestsFirst ? asks.front() : bids.front();
			const EcsWorld::Entity taker = askRestsFirst ? bids.front() : asks.front();

			if (!FillPair(ecsWorld, marketId, maker, taker, fillLog, errorMessage))
			{
				// The pair cannot trade a whole unit - the trader on one side has run out
				// of the thing the price is paid in - and the pair is the front of both
				// books, so nothing behind it can trade either.
				errorMessage.clear();

				break;
			}
		}
	}

	return true;
}


std::vector<TradeFill> MarketplaceService::MatchBook(
    EcsWorld &ecsWorld, const std::string &marketId, std::string &errorMessage)
{
	std::vector<TradeFill> fills;

	if (!MatchInto(ecsWorld, marketId, fills, errorMessage))
	{
		return {};
	}

	return fills;
}


std::vector<std::string> MarketplaceService::Book(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId,
    OrderSide side) const
{
	std::vector<std::string> orders;

	for (const EcsWorld::Entity entity : ActiveOrders(ecsWorld, marketId, itemId, side))
	{
		const std::string name = ecsWorld.NameOf(entity);

		if (!name.empty())
		{
			orders.push_back(name);
		}
	}

	return orders;
}


std::vector<MarketOrderInfo> MarketplaceService::OrdersOf(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &civilisationId) const
{
	std::vector<MarketOrderInfo> orders;

	const auto view = ecsWorld.View<MarketOrderInfo, OrderAt, OrderOwner>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *info = ecsWorld.Get<MarketOrderInfo>(entity);
		const auto *at = ecsWorld.Get<OrderAt>(entity);
		const auto *owner = ecsWorld.Get<OrderOwner>(entity);

		if (info == nullptr || at == nullptr || owner == nullptr)
		{
			continue;
		}

		if (!marketId.empty() && at->marketId != marketId)
		{
			continue;
		}

		if (!civilisationId.empty() && owner->civilisationId != civilisationId)
		{
			continue;
		}

		orders.push_back(*info);
	}

	std::sort(
	    orders.begin(), orders.end(), [](const MarketOrderInfo &left, const MarketOrderInfo &right)
	    { return left.orderId < right.orderId; });

	return orders;
}


std::size_t MarketplaceService::OpenOrderCount(
    const EcsWorld &ecsWorld, const std::string &marketId) const
{
	std::size_t open = 0;

	const auto view = ecsWorld.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *info = ecsWorld.Get<MarketOrderInfo>(entity);
		const auto *at = ecsWorld.Get<OrderAt>(entity);

		if (info != nullptr && at != nullptr && (marketId.empty() || at->marketId == marketId) &&
		    info->status == OrderStatus::Active && OpenQuantity(*info) > quantityEpsilon)
		{
			++open;
		}
	}

	return open;
}


bool MarketplaceService::RefreshQuotes(
    EcsWorld &ecsWorld, const std::string &marketId, std::string &errorMessage)
{
	const MarketplaceInfo *venue = RequireVenue(ecsWorld, marketId, errorMessage);

	if (venue == nullptr)
	{
		return false;
	}

	// The record keeps what the fills wrote - the last price, its tick, the volume ever
	// traded - and the quote table is rebuilt from the book: a quote is a fact about
	// the book, and a book that changed is a quote table that changed.
	MarketRecord record;

	const MarketRecord *existing = ecsWorld.Get<MarketRecord>(marketId);

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

	const auto view = ecsWorld.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const OrderAt *at = ecsWorld.Get<OrderAt>(entity);
		const MarketOrderInfo *info = ecsWorld.Get<MarketOrderInfo>(entity);

		if (at != nullptr && info != nullptr && at->marketId == marketId)
		{
			items.insert(info->itemId);
		}
	}

	for (const auto &itemId : items)
	{
		MarketQuote quote;
		quote.itemId = itemId;

		const std::vector<EcsWorld::Entity> asks =
		    ActiveOrders(ecsWorld, marketId, itemId, OrderSide::Sell);
		const std::vector<EcsWorld::Entity> bids =
		    ActiveOrders(ecsWorld, marketId, itemId, OrderSide::Buy);

		if (!asks.empty())
		{
			const MarketOrderInfo *best = ecsWorld.Get<MarketOrderInfo>(asks.front());

			quote.bestAsk = best != nullptr ? best->limitPrice : Decimal{};
			quote.askOrders = CountAtLevel(ecsWorld, asks, quote.bestAsk);
			quote.askDepth = DepthAtLevel(ecsWorld, asks, quote.bestAsk);
		}

		if (!bids.empty())
		{
			const MarketOrderInfo *best = ecsWorld.Get<MarketOrderInfo>(bids.front());

			quote.bestBid = best != nullptr ? best->limitPrice : Decimal{};
			quote.bidOrders = CountAtLevel(ecsWorld, bids, quote.bestBid);
			quote.bidDepth = DepthAtLevel(ecsWorld, bids, quote.bestBid);
		}

		if (quote.bestAsk > 0 && quote.bestBid > 0 && quote.bestBid <= quote.bestAsk)
		{
			quote.mid =
			    Decimal::FromRaw(quote.bestBid.Raw() + (quote.bestAsk.Raw() - quote.bestBid.Raw()) / 2);

			if (quote.mid > 0)
			{
				quote.spreadBps =
				    (quote.bestAsk - quote.bestBid).ToDouble() / quote.mid.ToDouble() * 10000.0;
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

	return ecsWorld.Put<MarketRecord>(marketId, std::move(record)) != nullptr;
}


std::vector<MarketQuote> MarketplaceService::Quotes(
    const EcsWorld &ecsWorld, const std::string &marketId) const
{
	std::vector<MarketQuote> quotes;

	const auto *record = ecsWorld.Get<MarketRecord>(marketId);

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


const MarketQuote *MarketplaceService::QuoteFor(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId) const
{
	const auto *record = ecsWorld.Get<MarketRecord>(marketId);

	if (record == nullptr)
	{
		return nullptr;
	}

	const auto entry = record->quotes.find(itemId);

	return entry != record->quotes.end() ? &entry->second : nullptr;
}


bool MarketplaceService::BestAsk(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId,
    Decimal &price) const
{
	const MarketQuote *quote = QuoteFor(ecsWorld, marketId, itemId);

	if (quote == nullptr || quote->bestAsk <= 0)
	{
		return false;
	}

	price = quote->bestAsk;

	return true;
}


bool MarketplaceService::BestBid(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId,
    Decimal &price) const
{
	const MarketQuote *quote = QuoteFor(ecsWorld, marketId, itemId);

	if (quote == nullptr || quote->bestBid <= 0)
	{
		return false;
	}

	price = quote->bestBid;

	return true;
}


bool MarketplaceService::MidPrice(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId,
    Decimal &price) const
{
	const MarketQuote *quote = QuoteFor(ecsWorld, marketId, itemId);

	if (quote == nullptr || quote->mid <= 0)
	{
		return false;
	}

	price = quote->mid;

	return true;
}


double MarketplaceService::SpreadBps(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId) const
{
	const MarketQuote *quote = QuoteFor(ecsWorld, marketId, itemId);

	return quote != nullptr ? quote->spreadBps : 0.0;
}


Decimal MarketplaceService::ReferencePrice(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId) const
{
	const MarketQuote *quote = QuoteFor(ecsWorld, marketId, itemId);

	if (quote == nullptr)
	{
		return {};
	}

	if (quote->mid > 0)
	{
		return quote->mid;
	}

	if (quote->bestAsk > 0)
	{
		return quote->bestAsk;
	}

	if (quote->bestBid > 0)
	{
		return quote->bestBid;
	}

	return quote->lastPrice;
}


int MarketplaceService::RiskPremiumBps(const EcsWorld &ecsWorld, const std::string &marketId) const
{
	const auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);

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

	if (const auto *effects = ecsWorld.Get<MarketEffects>(marketId))
	{
		risk += ReadEffectStat(effects->values, EffectStat::MarketRiskPremium);
	}

	const auto laneId = CivilisationEffectEngine::LaneStoreId(venue->civilisationId);

	if (const auto *lanes = ecsWorld.Get<RouteEffects>(laneId))
	{
		risk += ReadEffectStat(lanes->values, EffectStat::RouteRiskPremium);
	}

	return static_cast<int>(std::llround(risk));
}


int MarketplaceService::ListingFeeBps(const EcsWorld &ecsWorld, const std::string &marketId) const
{
	const auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		return m_options.listingFeeBps;
	}

	int fee = venue->baseListingFeeBps + RiskPremiumBps(ecsWorld, marketId);

	// The credit for the berth: one basis point off the fee for every two berths a day
	// the licence and the warrant clear, because a licence that clears the deck faster
	// is a deck that fills the same goods at a cheaper berth.
	if (const auto *effects = ecsWorld.Get<MarketEffects>(marketId))
	{
		const double clearance = ReadEffectStat(effects->values, EffectStat::MarketClearance);

		fee -= static_cast<int>(clearance / 2.0);
	}

	return fee > 0 ? fee : 0;
}


int MarketplaceService::InsuranceBps(const EcsWorld &ecsWorld, const std::string &marketId) const
{
	const auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);
	int bps = venue != nullptr ? m_options.insuranceBps : 0;

	if (const auto *effects = ecsWorld.Get<MarketEffects>(marketId))
	{
		bps += static_cast<int>(ReadEffectStat(effects->values, EffectStat::MarketInsurance));
	}

	return bps > 0 ? bps : 0;
}


double MarketplaceService::DepthUnits(const EcsWorld &ecsWorld, const std::string &marketId) const
{
	const auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr || m_options.depthUnits <= 0.0)
	{
		// No cap: a venue that has set no depth is a venue that quoters arrive at, not
		// a venue that turns them away, and the scaffold's default is that.
		return 0.0;
	}

	double depth = m_options.depthUnits;

	if (const auto *effects = ecsWorld.Get<MarketEffects>(marketId))
	{
		depth += ReadEffectStat(effects->values, EffectStat::MarketDepth);
	}

	return depth > 0.0 ? depth : 0.0;
}


bool MarketplaceService::ApplyProgression(
    EcsWorld &ecsWorld, const ProgressionManager &progression, const CivilisationEffectEngine &effects,
    const std::string &civilisationId, unsigned &applied, unsigned &skipped, std::string &errorMessage)
{
	if (!effects.ApplyProgression(
	        ecsWorld, progression, civilisationId, applied, skipped, errorMessage))
	{
		return false;
	}

	// The clearance of a venue moves its fee, and the fee is the number beside the
	// price in every quote the UI shows, so a tree that has been bought is a set of
	// venues whose charges have to be re-read.
	for (const auto &marketId : effects.MarketsOf(ecsWorld, civilisationId))
	{
		if (!RefreshQuotes(ecsWorld, marketId, errorMessage))
		{
			return false;
		}
	}

	return true;
}


unsigned MarketplaceService::AdvanceTick(EcsWorld &ecsWorld, long long tick, std::string &errorMessage)
{
	unsigned venues = 0;

	// Validate the complete expiry batch, including refunds shared across venues.
	std::vector<std::string> allExpired;
	for (const auto entity : ecsWorld.View<MarketOrderInfo, OrderAt>())
	{
		const auto *info = ecsWorld.Get<MarketOrderInfo>(entity);
		const auto *at = ecsWorld.Get<OrderAt>(entity);
		if (info != nullptr && at != nullptr && HasMarket(ecsWorld, at->marketId) &&
		    info->status == OrderStatus::Active && info->expiresTick > info->placedTick &&
		    info->expiresTick <= tick)
		{
			const auto name = ecsWorld.NameOf(entity);
			if (!name.empty())
			{
				allExpired.push_back(name);
			}
		}
	}
	std::sort(allExpired.begin(), allExpired.end());
	if (!CanRefundOrders(ecsWorld, allExpired, errorMessage))
	{
		return 0;
	}

	for (const auto &marketId : MarketIds(ecsWorld, {}))
	{
		std::vector<std::string> expired;

		const auto view = ecsWorld.View<MarketOrderInfo, OrderAt>();

		for (const EcsWorld::Entity entity : view)
		{
			const MarketOrderInfo *info = ecsWorld.Get<MarketOrderInfo>(entity);
			const OrderAt *at = ecsWorld.Get<OrderAt>(entity);

			if (info == nullptr || at == nullptr || at->marketId != marketId)
			{
				continue;
			}

			// An order leaves the book when the tick reaches the expiry its life gave it.
			// An expiry that does not run past the tick the order was posted at is an order
			// posted at a venue with no order life, and the sweep passes it: a venue that
			// sets no order life is a venue that keeps its book, and the order that rests
			// without a life rests until it trades, is cancelled, or the venue closes.
			if (info->status == OrderStatus::Active && info->expiresTick > info->placedTick &&
			    info->expiresTick <= tick)
			{
				const std::string name = ecsWorld.NameOf(entity);

				if (!name.empty())
				{
					expired.push_back(name);
				}
			}
		}

		// The orders that have outlived their order life leave, and their goods and
		// their credits go back: the berths they held are the berths this tick's
		// quoters need.
		std::sort(expired.begin(), expired.end());
		for (const auto &orderId : expired)
		{
			if (!RetireOrder(ecsWorld, orderId, errorMessage))
			{
				return venues;
			}
		}

		if (!RefreshQuotes(ecsWorld, marketId, errorMessage))
		{
			return venues;
		}

		++venues;
	}

	return venues;
}


std::vector<TradeFill> MarketplaceService::Fills(
    const EcsWorld &ecsWorld, const std::string &marketId) const
{
	std::vector<TradeFill> fills;

	const auto *record = ecsWorld.Get<MarketRecord>(marketId);

	if (record == nullptr)
	{
		return fills;
	}

	return record->fills;
}


std::string MarketplaceService::DescribeMarket(
    const EcsWorld &ecsWorld, const std::string &marketId) const
{
	const auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		return "Market '" + marketId + "': the world holds no such venue";
	}

	const auto *body = ecsWorld.Get<BodyInfo>(marketId);

	std::ostringstream out;

	out << "Market " << venue->marketId << " at "
	    << (venue->venueName.empty() ? venue->marketId : venue->venueName) << " ("
	    << DescribeBodyKind(body != nullptr ? body->kind : BodyKind::Planet) << " of "
	    << venue->civilisationId << ")";

	if (body != nullptr && body->kind == BodyKind::Satellite)
	{
		out << ", in orbit around " << body->primaryBodyId;
	}

	out << "\n  Charges " << ListingFeeBps(ecsWorld, venue->marketId) << " bps a side (base "
	    << venue->baseListingFeeBps << ", risk " << RiskPremiumBps(ecsWorld, venue->marketId)
	    << "), insurance " << InsuranceBps(ecsWorld, venue->marketId) << " bps, orders rest "
	    << venue->orderLifeTicks << " ticks, take " << venue->venueTake << " credits\n";

	out << "  " << OpenOrderCount(ecsWorld, venue->marketId) << " open orders, ";

	const std::vector<MarketQuote> quotes = Quotes(ecsWorld, venue->marketId);

	out << quotes.size() << " goods quoted";

	if (quotes.empty())
	{
		out << ": nothing traded here yet";

		return out.str();
	}

	// The most traded goods first, and inside a volume the item id: a venue quotes
	// dozens of goods and a screen shows a handful.
	std::vector<MarketQuote> ranked = quotes;

	std::sort(
	    ranked.begin(), ranked.end(),
	    [](const MarketQuote &left, const MarketQuote &right)
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

		out << "\n  " << quote.itemId << ": ask " << quote.bestAsk.ToString() << " bid "
		    << quote.bestBid.ToString() << ", mid " << quote.mid.ToString() << ", spread "
		    << FormatNumber(quote.spreadBps) << " bps, depth " << FormatNumber(quote.askDepth) << "/"
		    << FormatNumber(quote.bidDepth) << ", last " << quote.lastPrice.ToString() << " at tick "
		    << quote.lastTick << ", volume " << FormatNumber(quote.volume);
	}

	return out.str();
}


std::string MarketplaceService::DescribeOrder(
    const EcsWorld &ecsWorld, const std::string &orderId) const
{
	const auto *info = ecsWorld.Get<MarketOrderInfo>(orderId);

	if (info == nullptr)
	{
		return "Order '" + orderId +
		       "': no order in the world by that name - it filled, expired, was cancelled, or "
		       "was never posted";
	}

	const auto *at = ecsWorld.Get<OrderAt>(orderId);
	const auto *owner = ecsWorld.Get<OrderOwner>(orderId);

	std::ostringstream out;

	out << "Order " << info->orderId << " " << DescribeOrderSide(info->side) << " "
	    << FormatNumber(info->quantity) << " " << info->itemId << " @ " << info->limitPrice.ToString()
	    << " at " << (at != nullptr ? at->marketId : std::string("<no venue>")) << " for "
	    << (owner != nullptr ? owner->civilisationId : std::string("<no trader>")) << ", "
	    << DescribeOrderStatus(info->status) << ", " << FormatNumber(info->filled)
	    << " filled, posted at tick " << info->placedTick << ", leaves at tick " << info->expiresTick;

	return out.str();
}


bool MarketplaceService::LoadSimulation(
    EcsWorld &ecsWorld, const Simulation &sim, const MarketplaceOptions &options,
    std::size_t &venuesOpened, std::string &errorMessage)
{
	venuesOpened = 0;

	if (ecsWorld.EntityCount() != 0)
	{
		errorMessage = std::format(
		    "MarketplaceService: the world holds {} entities: the generated universe loads into a "
		    "fresh world, because the ids of a second load collide with the first",
		    std::to_string(ecsWorld.EntityCount()));

		return false;
	}

	if (!sim.IsGenerated())
	{
		errorMessage = "MarketplaceService: the simulation has not generated any worlds";

		return false;
	}

	MarketplaceService service(options);

	for (const Civilisation &world : sim.GetCivilisations())
	{
		if (!AddPlanet(
		        ecsWorld, world.id, world.name, world.systemName, world.id, world.ageId, errorMessage))
		{
			errorMessage = "MarketplaceService: " + errorMessage;

			return false;
		}

		// The economy model's own numbers, in the store's own components: the same
		// stockpile, the same treasury, the same decks, so a market settles against the
		// world the tick model produced and consumed.
		ecsWorld.Put<Population>(world.id, Population{world.population, world.population});
		ecsWorld.Put<Treasury>(world.id, Treasury{world.treasury});
		ecsWorld.Put<Stockpile>(world.id, Stockpile{world.stockpile});
		ecsWorld.Put<Production>(world.id, Production{world.production});
		ecsWorld.Put<Demand>(world.id, Demand{world.demand});

		if (!service.Open(ecsWorld, world.id, errorMessage))
		{
			errorMessage = std::format("MarketplaceService: world '{}': {}", world.id, errorMessage);

			return false;
		}

		++venuesOpened;
	}

	return true;
}


bool MarketplaceService::AddPlanet(
    EcsWorld &ecsWorld, const std::string &bodyId, const std::string &displayName,
    const std::string &systemName, const std::string &civilisationId, const std::string &ageId,
    std::string &errorMessage)
{
	if (bodyId.empty())
	{
		errorMessage = "MarketplaceService: the bodyId is empty";

		return false;
	}

	if (civilisationId.empty())
	{
		errorMessage = std::format(
		    "MarketplaceService: the planet '{}' belongs to no civilisation, and a venue's fee, risk "
		    "and take belong to a world",
		    bodyId);

		return false;
	}

	EcsWorld::Entity body = EcsWorld::Null;

	if (!ecsWorld.Create(bodyId, body, errorMessage))
	{
		return false;
	}

	BodyInfo info;
	info.bodyId = bodyId;
	info.kind = BodyKind::Planet;
	info.displayName = displayName;
	info.systemName = systemName;
	info.civilisationId = civilisationId;
	info.ageId = ageId;

	ecsWorld.Add<BodyInfo>(body, std::move(info));

	// A world created here settles: an empty treasury and an empty stockpile are the
	// facts a market needs to post against, and a body that cannot settle is a body a
	// trader cannot trade at.
	ecsWorld.Add<Treasury>(body, Treasury{0});
	ecsWorld.Add<Stockpile>(body, Stockpile{});
	ecsWorld.Add<Population>(body, Population{1.0, 1.0});

	return true;
}


bool MarketplaceService::AddSatellite(
    EcsWorld &ecsWorld, const std::string &satelliteId, const std::string &planetBodyId,
    const std::string &displayName, double radiusKm, double periodDays, std::string &errorMessage)
{
	if (satelliteId.empty())
	{
		errorMessage = "MarketplaceService: the satelliteId is empty";

		return false;
	}

	const BodyInfo *planet = ecsWorld.Get<BodyInfo>(planetBodyId);

	if (planet == nullptr)
	{
		errorMessage = std::format(
		    "MarketplaceService: the satellite '{}' orbits '{}', which is not a body of this world",
		    satelliteId, planetBodyId);

		return false;
	}

	if (planet->kind != BodyKind::Planet)
	{
		errorMessage = std::format(
		    "MarketplaceService: the satellite '{}' orbits {} '{}'; the station trade is the trade of "
		    "the ships that come up a planet's gravity well, and a star has no market at the bottom of "
		    "its",
		    satelliteId, DescribeBodyKind(planet->kind), planetBodyId);

		return false;
	}

	EcsWorld::Entity body = EcsWorld::Null;

	if (!ecsWorld.Create(satelliteId, body, errorMessage))
	{
		return false;
	}

	BodyInfo info;
	info.bodyId = satelliteId;
	info.kind = BodyKind::Satellite;
	info.displayName = displayName.empty() ? satelliteId : displayName;
	info.primaryBodyId = planet->bodyId;
	info.systemName = planet->systemName;

	// A station belongs to the world beneath it: the crews that trade there are the
	// crews of the ships that come up from that world's market, and the fee, the risk
	// and the take of the station's venue are that civilisation's.
	info.civilisationId = planet->civilisationId;

	ecsWorld.Add<BodyInfo>(body, std::move(info));
	ecsWorld.Add<Orbit>(body, Orbit{planet->bodyId, radiusKm, periodDays});
	ecsWorld.Add<Treasury>(body, Treasury{0});
	ecsWorld.Add<Stockpile>(body, Stockpile{});

	return true;
}


long long MarketplaceService::FeeFor(long long gross, int feeBps)
{
	const auto fee = CheckedFeeFor(gross, feeBps);
	JETTISON_VERIFY(fee.has_value());
	return *fee;
}


double MarketplaceService::AffordableQuantity(Decimal price, int feeBps, long long budget)
{
	if (price <= 0 || budget <= 0)
	{
		return 0.0;
	}

	// The whole the budget pays: the goods and the fee on them, at the fee rate, in
	// integer arithmetic, and the units that whole buys.
	const __int128 rate = static_cast<__int128>(10000) + std::max(feeBps, 0);
	const __int128 whole = static_cast<__int128>(budget) * 10000 / rate;

	if (whole <= 0)
	{
		return 0.0;
	}

	return static_cast<double>(whole) / price.ToDouble();
}


const MarketplaceInfo *MarketplaceService::RequireVenue(
    const EcsWorld &ecsWorld, const std::string &marketId, std::string &errorMessage) const
{
	if (marketId.empty())
	{
		errorMessage =
		    "Marketplace: the marketId is empty; a market is named by the body id of its venue";

		return nullptr;
	}

	const auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);

	if (venue == nullptr)
	{
		errorMessage =
		    std::format("Marketplace '{}': no marketplace at that body; Open it first", marketId);

		return nullptr;
	}

	return venue;
}


bool MarketplaceService::RequireTrader(
    const EcsWorld &ecsWorld, const std::string &civilisationId, EcsWorld::Entity &trader,
    std::string &errorMessage) const
{
	trader = ecsWorld.Find(civilisationId);

	if (!ecsWorld.IsValid(trader))
	{
		errorMessage = std::format(
		    "Marketplace: '{}' is not an entity of this world: a trader is a world, and a world is a "
		    "body",
		    civilisationId);

		return false;
	}

	if (ecsWorld.Get<Treasury>(trader) == nullptr || ecsWorld.Get<Stockpile>(trader) == nullptr)
	{
		errorMessage = std::format(
		    "Marketplace: '{}' carries no Treasury and Stockpile, and a trader without both cannot "
		    "settle a fill",
		    civilisationId);

		return false;
	}

	return true;
}


const MarketOrderInfo *MarketplaceService::RequireOrder(
    const EcsWorld &ecsWorld, const std::string &orderId, std::string &errorMessage) const
{
	if (orderId.empty())
	{
		errorMessage = "Marketplace: the orderId is empty";

		return nullptr;
	}

	const auto *info = ecsWorld.Get<MarketOrderInfo>(orderId);

	if (info == nullptr)
	{
		errorMessage = std::format(
		    "Marketplace: '{}' is no order of this world: it filled, expired, was cancelled, or was "
		    "never posted",
		    orderId);

		return nullptr;
	}

	return info;
}


std::vector<EcsWorld::Entity> MarketplaceService::ActiveOrders(
    const EcsWorld &ecsWorld, const std::string &marketId, const std::string &itemId,
    OrderSide side) const
{
	std::vector<EcsWorld::Entity> orders;

	const auto view = ecsWorld.View<MarketOrderInfo, OrderAt>();

	for (const EcsWorld::Entity entity : view)
	{
		const auto *info = ecsWorld.Get<MarketOrderInfo>(entity);
		const auto *at = ecsWorld.Get<OrderAt>(entity);

		if (info == nullptr || at == nullptr)
		{
			continue;
		}

		if (!marketId.empty() && at->marketId != marketId)
		{
			continue;
		}

		if (!itemId.empty() && info->itemId != itemId)
		{
			continue;
		}

		if (info->side != side || info->status != OrderStatus::Active)
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
	const EcsWorld *world = &ecsWorld;

	std::sort(
	    orders.begin(), orders.end(),
	    [world, side](EcsWorld::Entity left, EcsWorld::Entity right)
	    {
		    const auto *a = world->Get<MarketOrderInfo>(left);
		    const auto *b = world->Get<MarketOrderInfo>(right);

		    if (a == nullptr || b == nullptr)
		    {
			    return false;
		    }

		    // A sell book runs cheapest first and a buy book richest first, which is one
		    // ascending sort with the buy side's price turned over.
		    const Decimal keyA = side == OrderSide::Sell ? a->limitPrice : -a->limitPrice;
		    const Decimal keyB = side == OrderSide::Sell ? b->limitPrice : -b->limitPrice;

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


bool MarketplaceService::RetireOrder(
    EcsWorld &ecsWorld, const std::string &orderId, std::string &errorMessage)
{
	const MarketOrderInfo *info = ecsWorld.Get<MarketOrderInfo>(orderId);

	if (info == nullptr)
	{
		// The order is gone, and a thing that is gone has left nothing to refund.
		return true;
	}

	const OrderOwner *owner = ecsWorld.Get<OrderOwner>(orderId);

	// The reservation goes back: what the book held for an order that did not fill is
	// the trader's again, in full, and the ledger of the fill log is the only record of
	// what did trade.
	if (owner != nullptr)
	{
		EcsWorld::Entity trader = ecsWorld.Find(owner->civilisationId);

		if (ecsWorld.IsValid(trader))
		{
			const auto *refundTreasury = ecsWorld.Get<Treasury>(trader);
			if (refundTreasury != nullptr &&
			    static_cast<__int128>(refundTreasury->credits) + info->reservedCredits >
			        std::numeric_limits<long long>::max())
			{
				errorMessage = "Marketplace: reservation refund exceeds the credit range";
				return false;
			}
			if (info->reservedCredits > 0)
			{
				if (auto *treasury = ecsWorld.Get<Treasury>(trader))
				{
					treasury->credits += info->reservedCredits;
				}
			}

			if (info->reservedGoods > quantityEpsilon)
			{
				if (auto *stock = ecsWorld.Get<Stockpile>(trader))
				{
					stock->units[info->itemId] += info->reservedGoods;
				}
			}
		}
	}

	return ecsWorld.Destroy(orderId, errorMessage);
}


bool MarketplaceService::FillPair(
    EcsWorld &ecsWorld, const std::string &marketId, EcsWorld::Entity makerEntity,
    EcsWorld::Entity takerEntity, std::vector<TradeFill> &fillLog, std::string &errorMessage)
{
	auto *venue = ecsWorld.Get<MarketplaceInfo>(marketId);
	auto *maker = ecsWorld.Get<MarketOrderInfo>(makerEntity);
	auto *taker = ecsWorld.Get<MarketOrderInfo>(takerEntity);

	if (venue == nullptr || maker == nullptr || taker == nullptr)
	{
		errorMessage =
		    std::format("Marketplace '{}': the pair to fill is not a pair of live orders", marketId);

		return false;
	}

	if (maker->side == taker->side)
	{
		errorMessage = std::format(
		    "Marketplace '{}': two {} orders cannot trade with each other", marketId,
		    DescribeOrderSide(maker->side));

		return false;
	}

	if (maker->itemId != taker->itemId)
	{
		errorMessage = std::format(
		    "Marketplace '{}': '{}' cannot trade against '{}'", marketId, maker->itemId, taker->itemId);

		return false;
	}

	const OrderOwner *makerOwner = ecsWorld.Get<OrderOwner>(makerEntity);
	const OrderOwner *takerOwner = ecsWorld.Get<OrderOwner>(takerEntity);

	if (makerOwner == nullptr || takerOwner == nullptr)
	{
		errorMessage =
		    std::format("Marketplace '{}': an order of the pair carries no OrderOwner", marketId);

		return false;
	}

	const bool makerSells = maker->side == OrderSide::Sell;
	const std::string &sellerId = makerSells ? makerOwner->civilisationId : takerOwner->civilisationId;
	const std::string &buyerId = makerSells ? takerOwner->civilisationId : makerOwner->civilisationId;

	// The cargo and the money live on the trader, not on the order: a fill of a venue
	// that requires neither stock nor funds - the game of promises, off by default -
	// draws the stockpile of the seller and the treasury of the buyer, and it reads
	// them where they are, through the ids the orders name, as SettleParty and
	// RetireOrder read them. An order entity carries no Stockpile and no Treasury, so
	// a lookup on the order answers null for both, and both branches below then set
	// the quantity to nothing: the pair trades nothing and says nothing, at a venue
	// that requires funds and stock never noticed, because the reservations cover it.
	const EcsWorld::Entity sellerTrader = ecsWorld.Find(sellerId);
	const EcsWorld::Entity buyerTrader = ecsWorld.Find(buyerId);

	MarketOrderInfo *sellOrder = makerSells ? maker : taker;
	MarketOrderInfo *buyOrder = makerSells ? taker : maker;

	// The maker's limit price is the price: the resting trader set it, and the trader
	// that came looking took the book as it found it.
	const Decimal price = maker->limitPrice;

	if (price <= 0)
	{
		errorMessage =
		    std::format("Marketplace '{}': the maker '{}' names no price", marketId, maker->orderId);

		return false;
	}

	const std::string itemId = maker->itemId;
	const int feeBps = ListingFeeBps(ecsWorld, marketId);

	double quantity = std::min(OpenQuantity(*maker), OpenQuantity(*taker));

	// The goods: a sell order that posted under a venue that requires stock has the
	// units in the reservation, and the fill spends the reservation. A venue that
	// allows a sell order for cargo no one holds - the game of promises, off by default
	// - spends the seller's stockpile at the fill, and takes only what is there.
	if (sellOrder->reservedGoods > quantityEpsilon)
	{
		quantity = std::min(quantity, sellOrder->reservedGoods);
	}
	else if (const Stockpile *stock = ecsWorld.Get<Stockpile>(sellerTrader))
	{
		const auto holding = stock->units.find(itemId);
		const double available = holding != stock->units.end() ? holding->second : 0.0;

		quantity = std::min(quantity, available);
	}
	else
	{
		errorMessage = std::format(
		    "Marketplace '{}': the sell order '{}' names '{}', which carries no Stockpile to spend on "
		    "a fill",
		    marketId, sellOrder->orderId, sellerId);

		return false;
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
	else if (const Treasury *treasury = ecsWorld.Get<Treasury>(buyerTrader))
	{
		quantity = std::min(quantity, AffordableQuantity(price, feeBps, treasury->credits));
	}
	else
	{
		errorMessage = std::format(
		    "Marketplace '{}': the buy order '{}' names '{}', which carries no Treasury to spend on a "
		    "fill",
		    marketId, buyOrder->orderId, buyerId);

		return false;
	}

	if (quantity <= quantityEpsilon)
	{
		// Nothing of a whole unit can trade: the book has the price and the trader has
		// run out of the thing the price is paid in.
		return false;
	}

	const auto cost = WholeCost(price, quantity, false);
	if (!cost)
	{
		errorMessage = "Marketplace: fill gross exceeds the credit range";
		return false;
	}
	const long long gross = *cost;
	const auto fee = CheckedFeeFor(gross, feeBps);
	if (!fee)
	{
		errorMessage = "Marketplace: fill fee exceeds the credit range";
		return false;
	}
	const long long makerFee = *fee;
	const long long takerFee = *fee;

	// Validate both parties, the venue and the eventual refund before settlement writes.
	auto *buyerTreasury = ecsWorld.Get<Treasury>(buyerTrader);
	auto *sellerTreasury = ecsWorld.Get<Treasury>(sellerTrader);
	auto *buyerStock = ecsWorld.Get<Stockpile>(buyerTrader);
	auto *sellerStock = ecsWorld.Get<Stockpile>(sellerTrader);
	const __int128 bill = static_cast<__int128>(gross) + takerFee;
	const __int128 take = static_cast<__int128>(venue->venueTake) + makerFee + takerFee;
	const auto fits = [](__int128 value)
	{ return value >= 0 && value <= std::numeric_limits<long long>::max(); };
	if (buyerTreasury == nullptr || sellerTreasury == nullptr || buyerStock == nullptr ||
	    sellerStock == nullptr || !fits(bill) || !fits(take))
	{
		errorMessage = "Marketplace: settlement exceeds the credit range or lacks a ledger";
		return false;
	}
	const auto billCredits = static_cast<long long>(bill);
	const long long fromOrder = std::min(billCredits, buyOrder->reservedCredits);
	const long long fromTreasury = billCredits - fromOrder;
	const bool selfTrade = buyerTrader == sellerTrader;
	const __int128 nextBuyer = static_cast<__int128>(buyerTreasury->credits) - fromTreasury;
	const __int128 proceeds = static_cast<__int128>(gross) - makerFee;
	const bool completesBuy = OpenQuantity(*buyOrder) - quantity <= quantityEpsilon;
	const long long refund = completesBuy ? buyOrder->reservedCredits - fromOrder : 0;
	const __int128 finalBuyer = nextBuyer + refund + (selfTrade ? proceeds : 0);
	const __int128 finalSeller = static_cast<__int128>(sellerTreasury->credits) + proceeds;
	const double holding = buyerStock->units.contains(itemId) ? buyerStock->units.at(itemId) : 0.0;
	if (!fits(nextBuyer) || !fits(finalBuyer) || (!selfTrade && !fits(finalSeller)) ||
	    !std::isfinite(holding + quantity))
	{
		errorMessage = "Marketplace: settlement or refund exceeds the available ledger range";
		return false;
	}

	// Each side pays the fee of its own side of the trade: the resting trader pays the
	// maker's, the trader that came looking pays the taker's, and the venue is paid by
	// both, because the berth and the deck are the venue's.
	// Each side pays the fee of its own side of the trade: the resting trader pays the
	// maker's, the trader that came looking pays the taker's, and the venue is paid by
	// both, because the berth and the deck are the venue's. The buyer's money comes out
	// of the pocket its order holds, and the seller's proceeds go to its treasury.
	if (!SettleParty(
	        ecsWorld, buyerId, itemId, quantity, gross,
	        buyerId == makerOwner->civilisationId ? makerFee : takerFee, buyOrder->reservedCredits,
	        true, errorMessage))
	{
		return false;
	}

	if (!SettleParty(
	        ecsWorld, sellerId, itemId, quantity, gross,
	        sellerId == makerOwner->civilisationId ? makerFee : takerFee, sellOrder->reservedCredits,
	        false, errorMessage))
	{
		return false;
	}

	// A sell order with no reservation spends the seller's stockpile at the fill: the
	// promises venue, and the goods are taken from where they are.
	if (sellOrder->reservedGoods <= quantityEpsilon)
	{
		if (auto *stock = ecsWorld.Get<Stockpile>(sellerTrader))
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

	fillLog.push_back(fill);

	// The venue's record of the trade: the price, its tick, the volume, and the fill in
	// the log, oldest last and capped, so a long game does not grow the store for ever.
	MarketRecord record;

	if (const MarketRecord *existing = ecsWorld.Get<MarketRecord>(marketId); existing != nullptr)
	{
		record = *existing;
	}

	record.volume[itemId] += quantity;
	record.lastPrice[itemId] = price;
	record.lastTick[itemId] = fill.tick;
	record.fills.push_back(fill);

	if (record.fills.size() > marketFillLogSize)
	{
		record.fills.erase(
		    record.fills.begin(), record.fills.begin() + static_cast<std::ptrdiff_t>(
		                                                     record.fills.size() - marketFillLogSize));
	}

	ecsWorld.Put<MarketRecord>(marketId, std::move(record));

	// An order that has filled leaves the book: its entity goes, and the id stays in
	// the fill log as the number it was posted with.
	for (const EcsWorld::Entity settled : {makerEntity, takerEntity})
	{
		const MarketOrderInfo *done = ecsWorld.Get<MarketOrderInfo>(settled);

		if (done == nullptr || !IsFulfilled(*done))
		{
			continue;
		}

		const std::string name = ecsWorld.NameOf(settled);

		if (!name.empty())
		{
			MarketOrderInfo settledInfo = *done;
			settledInfo.status = OrderStatus::Filled;

			ecsWorld.Put<MarketOrderInfo>(settled, std::move(settledInfo));

			if (!RetireOrder(ecsWorld, name, errorMessage))
			{
				return false;
			}
		}
	}

	return true;
}


bool MarketplaceService::SettleParty(
    EcsWorld &ecsWorld, const std::string &civilisationId, const std::string &itemId, double quantity,
    long long gross, long long fee, long long &orderCredits, bool buys, std::string &errorMessage)
{
	const EcsWorld::Entity trader = ecsWorld.Find(civilisationId);

	if (!ecsWorld.IsValid(trader))
	{
		errorMessage =
		    std::format("Marketplace: '{}' is not an entity of this world at the fill", civilisationId);

		return false;
	}

	auto *treasury = ecsWorld.Get<Treasury>(trader);
	auto *stock = ecsWorld.Get<Stockpile>(trader);

	if (treasury == nullptr || stock == nullptr)
	{
		errorMessage = std::format(
		    "Marketplace: '{}' carries no Treasury and Stockpile, and a fill settles in credits and "
		    "goods",
		    civilisationId);

		return false;
	}

	if (buys)
	{
		const long long bill = gross + fee;

		// The money a buy pays is the money the venue took out of the treasury when the
		// order posted and put in the order, at the worst price the limit allows: the fill
		// spends that pocket, and the treasury pays only what the pocket does not cover,
		// which is nothing at a venue that requires funds. What is left of the pocket goes
		// back to the treasury when the order leaves the book, and a trader that paid at
		// the post is never charged a second time at the fill.
		const long long fromOrder = bill < orderCredits ? bill : orderCredits;

		orderCredits -= fromOrder;
		treasury->credits -= bill - fromOrder;
		stock->units[itemId] += quantity;
	}
	else
	{
		// The seller's cargo left its stockpile at the post and is handed over out of the
		// order it was posted in, and the proceeds of the sale arrive at its treasury,
		// less the fee of its own side of the trade.
		treasury->credits += gross - fee;
	}

	return true;
}
} // namespace Manifest::Core
