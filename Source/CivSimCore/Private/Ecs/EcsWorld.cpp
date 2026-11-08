#include "Ecs/EcsWorld.h"
#include <charconv>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace CivSimCore
{
namespace
{
// The prefix and the number of an id that spells "prefix-number": "Order-7" is the
// seventh number of the Order prefix, "Station-Civ-01" is the first number of the
// Station-Civ prefix. An id that spells no number - "Trader:ShieldFront", the source
// entity of one system on one hull - has no number to spend, and the answer is an
// empty prefix and no number.
std::pair<std::string, unsigned> NumericSuffix(const std::string &Id)
{
	const std::string::size_type dash = Id.rfind('-');

	if (dash == std::string::npos || dash + 1u >= Id.size() || dash == 0u)
	{
		return {};
	}

	const std::string tail = Id.substr(dash + 1u);

	// The number is every character of the tail, and short enough to count in an
	// unsigned: "Order-1", "Order-007", "Ship-12", and not "Order-1st", not
	// "Civ-01:Lanes-Civ-01:Convoy", whose tail carries the id of the store it lands on.
	if (tail.size() > 9u || tail.find_first_not_of("0123456789") != std::string::npos)
	{
		return {};
	}

	unsigned value = 0;
	const std::from_chars_result parsed = std::from_chars(tail.data(), tail.data() + tail.size(), value);

	if (parsed.ec != std::errc{} || parsed.ptr != tail.data() + tail.size())
	{
		return {};
	}

	return {Id.substr(0, dash), value};
}
} // namespace


void EcsWorld::SpendNumericSuffix(std::map<std::string, unsigned> &Spent, const std::string &Id)
{
	const auto [prefix, number] = NumericSuffix(Id);

	if (prefix.empty())
	{
		return;
	}

	const auto entry = Spent.find(prefix);

	if (entry == Spent.end() || number > entry->second)
	{
		Spent[prefix] = number;
	}
}
bool EcsWorld::Create(const std::string &Id, std::string &ErrorMessage)
{
	Entity created = Null;

	return Create(Id, created, ErrorMessage);
}


bool EcsWorld::Create(const std::string &Id, Entity &Created, std::string &ErrorMessage)
{
	if (Id.empty())
	{
		ErrorMessage = "EcsWorld: the id is empty; every entity carries the id the rest of the core names it by";

		return false;
	}

	if (entities.find(Id) != entities.end())
	{
		ErrorMessage = "EcsWorld: the id '" + Id + "' is already an entity in this world";

		return false;
	}

	Created = registry.create();
	registry.emplace<EntityId>(Created, EntityId{Id});
	entities[Id] = Created;
	SpendNumericSuffix(spent, Id);

	return true;
}


std::string EcsWorld::NextId(const std::string &Prefix)
{
	// "Order-7", "Ship-12": the count only ever goes up, so an id names the entity it
	// was handed out to for the life of the world, and a fill log can quote an order
	// number after the order has filled and left the book. Ids sort as strings, which
	// puts "Order-10" before "Order-7"; the order of a fill is the tick and the price,
	// never the id, so the reading is the same and the number stays unique.
	//
	// The count is the world's one clock, and it is raised to the numbers this prefix
	// has already worn, which are the numbers of the orders and the stations the world
	// was given by name: a world that holds "Order-3" hands out "Order-4", and the
	// "Civ-01" of a generated solar system spends no order number at all, because the
	// numbers of a prefix are its own.
	const auto entry = spent.find(Prefix);

	if (entry != spent.end() && entry->second > idCounter)
	{
		idCounter = entry->second;
	}

	std::ostringstream out;
	out << Prefix << '-' << ++idCounter;

	spent[Prefix] = idCounter;

	return out.str();
}


bool EcsWorld::Destroy(const std::string &Id, std::string &ErrorMessage)
{
	const auto entry = entities.find(Id);

	if (entry == entities.end())
	{
		ErrorMessage = "EcsWorld: the id '" + Id + "' is not an entity in this world";

		return false;
	}

	const Entity target = entry->second;
	entities.erase(entry);
	registry.destroy(target);

	return true;
}


bool EcsWorld::Destroy(Entity Target, std::string &ErrorMessage)
{
	if (!IsValid(Target))
	{
		ErrorMessage = "EcsWorld: the entity is not live in this world";

		return false;
	}

	const std::string name = NameOf(Target);

	if (!name.empty())
	{
		entities.erase(name);
	}

	registry.destroy(Target);

	return true;
}


void EcsWorld::Clear()
{
	// A copy, not the map itself: a component's on_destroy hook is free to destroy
	// other entities, and a hook that does that must not invalidate the iteration
	// that is carrying it.
	const std::vector<Entity> snapshot = [this]
	{
		std::vector<Entity> live;
		live.reserve(entities.size());

		for (const auto &entry : entities)
		{
			live.push_back(entry.second);
		}

		return live;
	}();

	for (const Entity entity : snapshot)
	{
		if (IsValid(entity))
		{
			registry.destroy(entity);
		}
	}

	entities.clear();
}


bool EcsWorld::Has(const std::string &Id) const
{
	return entities.find(Id) != entities.end();
}


bool EcsWorld::IsValid(Entity Candidate) const
{
	return Candidate != Null && registry.valid(Candidate);
}


EcsWorld::Entity EcsWorld::Find(const std::string &Id) const
{
	const auto entry = entities.find(Id);

	return entry != entities.end() ? entry->second : Null;
}


std::string EcsWorld::NameOf(Entity Named) const
{
	const EntityId *identity = Get<EntityId>(Named);

	return identity != nullptr ? identity->id : std::string{};
}


std::vector<std::string> EcsWorld::Ids() const
{
	std::vector<std::string> ids;
	ids.reserve(entities.size());

	for (const auto &entry : entities)
	{
		ids.push_back(entry.first);
	}

	return ids;
}


std::size_t EcsWorld::EntityCount() const
{
	return entities.size();
}


entt::registry &EcsWorld::Registry()
{
	return registry;
}


const entt::registry &EcsWorld::Registry() const
{
	return registry;
}
} // namespace CivSimCore
