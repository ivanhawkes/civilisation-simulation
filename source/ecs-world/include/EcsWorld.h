#pragma once

#include <cstddef>
#include <entt/entity/registry.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>


namespace ManifestCore
{
// The component an entity carries for its own string id, added by Create and never
// removed. It is what makes the registry self describing: a view over any component
// can say which world entity it is looking at, and a serialized entity keeps its name.
struct EntityId
{
	std::string id;
};

// The entity store the ship, body and marketplace subsystems share.
//
// EnTT identifies entities by a number that a destroyed entity hands back to the next
// entity created, which is what a game wants and what a simulation of a saved universe
// cannot debug: an order that filled last tick and a market opened this tick would
// share an identifier. Every entity here therefore carries the string id the rest of
// the core already uses - "Civ-01", "Ship-01", "Order-7" - because the configuration,
// the UI and the test suites all name things that way. The map is the index from that
// id to the registry's entity, and it is a std::map keyed by id, so iterating the
// world gives the same order on every platform, the way the loaders and
// ProgressionManager keep sorted order for the same reason.
//
// The registry is public: a subsystem reads and writes components through the helpers
// below, and a system that wants a view of several components at once takes
// `Registry()` and builds it, which is what EnTT is good at.
class EcsWorld
{
  public:
	using Entity = entt::entity;

	static constexpr Entity Null = entt::null;

	// Creates an entity under Id and gives it its EntityId component. Fails on an
	// empty id, and on an id the world already has: two entities of one name are an
	// economy the tests cannot read.
	bool Create(const std::string &Id, std::string &ErrorMessage);
	bool Create(const std::string &Id, Entity &Created, std::string &ErrorMessage);

	// A new entity under "Prefix-n", n counting up from 1 and never reused: an order
	// keeps the number it was posted with, and a fill names the order it filled.
	//
	// The count is the world's, not the prefix's: "Order-1", "Order-2" and then
	// "Ship-3" is one numbering series, because a fill log that names an entity by its
	// id reads one clock, not six. A number is spent for the world, and Create spends
	// the number an id it is given names: a world that has held an "Order-3" never
	// hands out "Order-3" again, and Clear, which empties the world, leaves the
	// numbers spent - the id of an order that filled and left the book is the id a
	// fill log quotes, and a number that came back would quote two orders.
	std::string NextId(const std::string &Prefix);

	// Destroys an entity and every component on it. Fails when the id is unknown,
	// because a typo in an id must not read as a clean destroy.
	bool Destroy(const std::string &Id, std::string &ErrorMessage);
	bool Destroy(Entity Target, std::string &ErrorMessage);

	// Every entity, in sorted id order: it takes a snapshot of the ids first, so a
	// component's on_destroy hook cannot invalidate the iteration it runs in.
	void Clear();

	bool Has(const std::string &Id) const;

	// True when the entity is live in this world's registry.
	bool IsValid(Entity Candidate) const;

	// Null for an id the world never had.
	Entity Find(const std::string &Id) const;

	// The id of a live entity, "" when it is not in this world.
	std::string NameOf(Entity Named) const;

	// Ids in sorted order.
	std::vector<std::string> Ids() const;

	std::size_t EntityCount() const;

	entt::registry &Registry();
	const entt::registry &Registry() const;

	// ─── Components ───
	// Every helper takes an id, the way the rest of the core addresses things, and an
	// overload that takes an entity for a system that is already iterating.
	//
	// They return a pointer, not a reference and not a thrown exception: a null
	// pointer says "this entity has no such component", which is the ordinary answer
	// in an ECS, and a library that writes nothing to the console reports it that way.

	// Adds a component built from Values. Null when the entity is unknown, and when it
	// already carries the component: a component is a fact about an entity, and the
	// second answer says the fact was already there.
	//
	// The pack of arguments is named Values, not Args: a function parameter pack is
	// declared by the name written after the type, and a parameter pack that spells
	// Args shadows the template pack of the same name, which leaves `std::forward<Args>`
	// naming a value and no longer a type.
	template <typename Type, typename... Args> Type *Add(const std::string &Id, Args &&...Values);
	template <typename Type, typename... Args> Type *Add(Entity Target, Args &&...Values);

	// Replaces a component, adding it when the entity does not carry it. Null when
	// the entity is unknown.
	template <typename Type> Type *Put(const std::string &Id, Type Value);
	template <typename Type> Type *Put(Entity Target, Type Value);

	// The component, null when the entity is unknown or does not carry it.
	template <typename Type> const Type *Get(const std::string &Id) const;
	template <typename Type> const Type *Get(Entity Target) const;
	template <typename Type> Type *Get(const std::string &Id);
	template <typename Type> Type *Get(Entity Target);

	// Removes a component. False when the entity is unknown or carries no such
	// component, which is the same answer Add gives for a component it already has.
	template <typename Type> bool Remove(const std::string &Id);
	template <typename Type> bool Remove(Entity Target);

	// True when the entity carries every component named.
	template <typename... Type> bool HasComponent(const std::string &Id) const;
	template <typename... Type> bool HasComponent(Entity Target) const;

	// A view over entities carrying Type..., in the order EnTT stores them: packed,
	// and the order of entities within it is not the id order. A system that reports
	// a result - a quote table, a ship read-out, a fill log - keys it by id, which is
	// the deterministic order the tests assert.
	template <typename... Type> auto View()
	{
		return registry.view<Type...>();
	}

	template <typename... Type> auto View() const
	{
		return registry.view<Type...>();
	}

	// The number of entities carrying Type: the size of a store, as EnTT counts it.
	// A store the world has never seen counts 0: on a const registry EnTT hands back a
	// pointer to the store, null when there is none, and it does not create one for the
	// act of asking.
	template <typename Type> std::size_t ComponentCount() const
	{
		const auto *store = registry.storage<Type>();

		return store ? static_cast<std::size_t>(store->size()) : 0;
	}

  private:
	// The high water mark of the numbers spent under one prefix, "Order" -> 3. The
	// count NextId hands out is one clock, and this is the book of numbers a prefix has
	// already worn, which is what makes an id that arrived through Create a spent number
	// as much as an id NextId handed out.
	static void SpendNumericSuffix(std::map<std::string, unsigned> &Spent, const std::string &Id);

	entt::registry registry;
	std::map<std::string, Entity> entities;
	std::map<std::string, unsigned> spent;
	unsigned idCounter{0};
};


template <typename Type, typename... Args> Type *EcsWorld::Add(const std::string &Id, Args &&...Values)
{
	return Add<Type>(Find(Id), std::forward<Args>(Values)...);
}


template <typename Type, typename... Args> Type *EcsWorld::Add(Entity Target, Args &&...Values)
{
	if (!IsValid(Target))
	{
		return nullptr;
	}

	if (registry.all_of<Type>(Target))
	{
		return nullptr;
	}

	return &registry.emplace<Type>(Target, std::forward<Args>(Values)...);
}


template <typename Type> Type *EcsWorld::Put(const std::string &Id, Type Value)
{
	return Put<Type>(Find(Id), std::move(Value));
}


template <typename Type> Type *EcsWorld::Put(Entity Target, Type Value)
{
	if (!IsValid(Target))
	{
		return nullptr;
	}

	return &registry.emplace_or_replace<Type>(Target, std::move(Value));
}


template <typename Type> const Type *EcsWorld::Get(const std::string &Id) const
{
	return Get<Type>(Find(Id));
}


template <typename Type> const Type *EcsWorld::Get(Entity Target) const
{
	return IsValid(Target) ? registry.try_get<Type>(Target) : nullptr;
}


template <typename Type> Type *EcsWorld::Get(const std::string &Id)
{
	return Get<Type>(Find(Id));
}


template <typename Type> Type *EcsWorld::Get(Entity Target)
{
	return IsValid(Target) ? registry.try_get<Type>(Target) : nullptr;
}


template <typename Type> bool EcsWorld::Remove(const std::string &Id)
{
	return Remove<Type>(Find(Id));
}


template <typename Type> bool EcsWorld::Remove(Entity Target)
{
	if (!IsValid(Target) || !registry.all_of<Type>(Target))
	{
		return false;
	}

	registry.remove<Type>(Target);

	return true;
}


template <typename... Type> bool EcsWorld::HasComponent(const std::string &Id) const
{
	return HasComponent<Type...>(Find(Id));
}


template <typename... Type> bool EcsWorld::HasComponent(Entity Target) const
{
	return IsValid(Target) && registry.all_of<Type...>(Target);
}
} // namespace ManifestCore
