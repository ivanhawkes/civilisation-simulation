#include "Ecs/EcsWorld.h"
#include <sstream>
#include <string>
#include <utility>
#include <vector>


namespace CivSimCore
{
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

	return true;
}


std::string EcsWorld::NextId(const std::string &Prefix)
{
	// "Order-7", "Ship-12": the count only ever goes up, so an id names the entity it
	// was handed out to for the life of the world, and a fill log can quote an order
	// number after the order has filled and left the book. Ids sort as strings, which
	// puts "Order-10" before "Order-7"; the order of a fill is the tick and the price,
	// never the id, so the reading is the same and the number stays unique.
	std::ostringstream out;
	out << Prefix << '-' << ++idCounter;

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
