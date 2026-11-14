#include <manifest/ecs/EcsWorld.h>

#include <format>

#include <charconv>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Manifest::Core
{
namespace
{
// The prefix and the number of an id that spells "prefix-number": "Order-7" is the
// seventh number of the Order prefix, "Station-Civ-01" is the first number of the
// Station-Civ prefix. An id that spells no number - "Trader:ShieldFront", the source
// entity of one system on one hull - has no number to spend, and the answer is an
// empty prefix and no number.
std::pair<std::string, unsigned> NumericSuffix(const std::string &id)
{
	const std::string::size_type dash = id.rfind('-');

	if (dash == std::string::npos || dash + 1u >= id.size() || dash == 0u)
	{
		return {};
	}

	const std::string tail = id.substr(dash + 1u);

	// The number is every character of the tail, and short enough to count in an
	// unsigned: "Order-1", "Order-007", "Ship-12", and not "Order-1st", not
	// "Civ-01:Lanes-Civ-01:Convoy", whose tail carries the id of the store it lands on.
	if (tail.size() > 9u || tail.find_first_not_of("0123456789") != std::string::npos)
	{
		return {};
	}

	unsigned value = 0;
	const std::from_chars_result parsed =
	    std::from_chars(tail.data(), tail.data() + tail.size(), value);

	if (parsed.ec != std::errc{} || parsed.ptr != tail.data() + tail.size())
	{
		return {};
	}

	return {id.substr(0, dash), value};
}
} // namespace


void EcsWorld::SpendNumericSuffix(std::map<std::string, unsigned> &spent, const std::string &id)
{
	const auto [prefix, number] = NumericSuffix(id);

	if (prefix.empty())
	{
		return;
	}

	const auto entry = spent.find(prefix);

	if (entry == spent.end() || number > entry->second)
	{
		spent[prefix] = number;
	}
}
bool EcsWorld::Create(const std::string &id, std::string &errorMessage)
{
	Entity created = Null;

	return Create(id, created, errorMessage);
}


bool EcsWorld::Create(const std::string &id, Entity &created, std::string &errorMessage)
{
	if (id.empty())
	{
		errorMessage =
		    "EcsWorld: the id is empty; every entity carries the id the rest of the core names it by";

		return false;
	}

	if (m_entities.find(id) != m_entities.end())
	{
		errorMessage = std::format("EcsWorld: the id '{}' is already an entity in this world", id);

		return false;
	}

	created = m_registry.create();
	m_registry.emplace<EntityId>(created, EntityId{id});
	m_entities[id] = created;
	SpendNumericSuffix(m_spent, id);

	return true;
}


std::string EcsWorld::NextId(const std::string &prefix)
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
	const auto entry = m_spent.find(prefix);

	if (entry != m_spent.end() && entry->second > m_idCounter)
	{
		m_idCounter = entry->second;
	}

	std::ostringstream out;
	out << prefix << '-' << ++m_idCounter;

	m_spent[prefix] = m_idCounter;

	return out.str();
}


bool EcsWorld::Destroy(const std::string &id, std::string &errorMessage)
{
	const auto entry = m_entities.find(id);

	if (entry == m_entities.end())
	{
		errorMessage = std::format("EcsWorld: the id '{}' is not an entity in this world", id);

		return false;
	}

	const Entity target = entry->second;
	m_entities.erase(entry);
	m_registry.destroy(target);

	return true;
}


bool EcsWorld::Destroy(Entity target, std::string &errorMessage)
{
	if (!IsValid(target))
	{
		errorMessage = "EcsWorld: the entity is not live in this world";

		return false;
	}

	const std::string name = NameOf(target);

	if (!name.empty())
	{
		m_entities.erase(name);
	}

	m_registry.destroy(target);

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
		live.reserve(m_entities.size());

		for (const auto &entry : m_entities)
		{
			live.push_back(entry.second);
		}

		return live;
	}();

	for (const Entity entity : snapshot)
	{
		if (IsValid(entity))
		{
			m_registry.destroy(entity);
		}
	}

	m_entities.clear();
}


bool EcsWorld::Has(const std::string &id) const
{
	return m_entities.find(id) != m_entities.end();
}


bool EcsWorld::IsValid(Entity candidate) const
{
	return candidate != Null && m_registry.valid(candidate);
}


EcsWorld::Entity EcsWorld::Find(const std::string &id) const
{
	const auto entry = m_entities.find(id);

	return entry != m_entities.end() ? entry->second : Null;
}


std::string EcsWorld::NameOf(Entity named) const
{
	const auto *identity = Get<EntityId>(named);

	return identity != nullptr ? identity->id : std::string{};
}


std::vector<std::string> EcsWorld::Ids() const
{
	std::vector<std::string> ids;
	ids.reserve(m_entities.size());

	for (const auto &entry : m_entities)
	{
		ids.push_back(entry.first);
	}

	return ids;
}


std::size_t EcsWorld::EntityCount() const
{
	return m_entities.size();
}


entt::registry &EcsWorld::Registry()
{
	return m_registry;
}


const entt::registry &EcsWorld::Registry() const
{
	return m_registry;
}
} // namespace Manifest::Core
