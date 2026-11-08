#include "Ecs/Components.h"


namespace CivSimCore
{
const char *DescribeBodyKind(BodyKind Kind)
{
	switch (Kind)
	{
	case BodyKind::Planet:
		return "Planet";
	case BodyKind::Satellite:
		return "Satellite";
	case BodyKind::Star:
		return "Star";
	}

	return "Unknown";
}


const char *DescribeOrderSide(OrderSide Side)
{
	switch (Side)
	{
	case OrderSide::Buy:
		return "Buy";
	case OrderSide::Sell:
		return "Sell";
	}

	return "Unknown";
}


const char *DescribeOrderStatus(OrderStatus Status)
{
	switch (Status)
	{
	case OrderStatus::Active:
		return "Active";
	case OrderStatus::Filled:
		return "Filled";
	case OrderStatus::Cancelled:
		return "Cancelled";
	case OrderStatus::Expired:
		return "Expired";
	}

	return "Unknown";
}
} // namespace CivSimCore
