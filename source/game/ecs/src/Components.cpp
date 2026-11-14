#include <manifest/ecs/Components.h>


namespace Manifest::Core
{
const char *DescribeBodyKind(BodyKind kind)
{
	switch (kind)
	{
		case BodyKind::Planet: return "Planet";
		case BodyKind::Satellite: return "Satellite";
		case BodyKind::Star: return "Star";
	}

	return "Unknown";
}


const char *DescribeOrderSide(OrderSide side)
{
	switch (side)
	{
		case OrderSide::Buy: return "Buy";
		case OrderSide::Sell: return "Sell";
	}

	return "Unknown";
}


const char *DescribeOrderStatus(OrderStatus status)
{
	switch (status)
	{
		case OrderStatus::Active: return "Active";
		case OrderStatus::Filled: return "Filled";
		case OrderStatus::Cancelled: return "Cancelled";
		case OrderStatus::Expired: return "Expired";
	}

	return "Unknown";
}
} // namespace Manifest::Core
