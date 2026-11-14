#include "TestHelpers.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <manifest/ecs/Components.h>
#include <manifest/ecs/EcsWorld.h>


namespace
{
using namespace Manifest::Core;

// A component with a value, so a check can say "this entity carries 12 of it".
struct Cargo
{
	std::string itemId;
	double units{0.0};
};

struct Screen
{
	double megawatts{0.0};
};

struct Station
{
	std::string name;
};
} // namespace


TEST_CASE("create_names_the_entity_and_the_index_is_sorted")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	CHECK(world.Create("Civ-02", errorMessage));
	CHECK(world.Create("Ship-01", errorMessage));
	CHECK(world.Create("Civ-01", errorMessage));

	CHECK_EQ(world.EntityCount(), 3u);
	CHECK_EQ(world.Ids().size(), 3u);
	CHECK_EQ(world.Ids()[0], std::string("Civ-01"));
	CHECK_EQ(world.Ids()[1], std::string("Civ-02"));
	CHECK_EQ(world.Ids()[2], std::string("Ship-01"));

	// The id is the address the rest of the core uses, and the entity carries it.
	CHECK(world.NameOf(world.Find("Ship-01")) == "Ship-01");
	CHECK(world.Has("Civ-01"));
	CHECK(!world.Has("Civ-03"));
	CHECK(world.IsValid(world.Find("Civ-01")));
	CHECK(world.Find("Civ-03") == Manifest::Core::EcsWorld::Null);
	CHECK(world.NameOf(Manifest::Core::EcsWorld::Null).empty());
}


TEST_CASE("create_refuses_an_empty_id_and_a_name_that_is_taken")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	CHECK(!world.Create("", errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "empty");

	CHECK(world.Create("Civ-01", errorMessage));
	CHECK(!world.Create("Civ-01", errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "already");

	// Two entities of one name is an economy the tests cannot read, so the second one
	// is a refusal and the first one is untouched.
	CHECK_EQ(world.EntityCount(), 1u);
}


TEST_CASE("next_id_counts_up_and_never_gives_a_number_twice")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	const std::string first = world.NextId("Order");
	const std::string second = world.NextId("Order");
	const std::string third = world.NextId("Ship");

	CHECK_EQ(first, std::string("Order-1"));
	CHECK_EQ(second, std::string("Order-2"));
	CHECK_EQ(third, std::string("Ship-3"));

	CHECK(world.Create(first, errorMessage));
	CHECK(!world.Create(first, errorMessage));

	// A destroy does not hand the number back: a fill log quotes an order number after
	// the order has filled and left the book.
	CHECK(world.Destroy(first, errorMessage));
	CHECK_EQ(world.NextId("Order"), std::string("Order-4"));
}


TEST_CASE("destroy_takes_the_id_and_the_components_with_it")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	CHECK(world.Create("Order-1", errorMessage));
	world.Add<Cargo>("Order-1", Cargo{"iron_ore", 40.0});

	CHECK_EQ(world.ComponentCount<Cargo>(), 1u);

	CHECK(world.Destroy("Order-1", errorMessage));
	CHECK(!world.Has("Order-1"));
	CHECK(!world.IsValid(world.Find("Order-1")));
	CHECK_EQ(world.ComponentCount<Cargo>(), 0u);
	CHECK_EQ(world.EntityCount(), 0u);

	CHECK(!world.Destroy("Order-1", errorMessage));
	MANIFEST_CHECK_CONTAINS(errorMessage, "not an entity");

	CHECK(!world.Destroy(Manifest::Core::EcsWorld::Null, errorMessage));
}


TEST_CASE("components_are_added_replaced_and_removed")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	CHECK(world.Create("Ship-01", errorMessage));

	auto *cargo = world.Add<Cargo>("Ship-01", Cargo{"iron_ore", 40.0});

	CHECK(cargo != nullptr);
	CHECK_EQ(cargo->itemId, std::string("iron_ore"));

	// A component is a fact, and the second answer says the fact was already there.
	CHECK(world.Add<Cargo>("Ship-01", Cargo{"water", 10.0}) == nullptr);

	const Cargo *read = world.Get<Cargo>("Ship-01");

	CHECK(read != nullptr);
	CHECK_EQ(read->units, 40.0);

	// Replacing is the answer for a fact that changes: a hull's stats are recomputed,
	// and the recomputed table replaces the old one.
	auto *moved = world.Put<Cargo>("Ship-01", Cargo{"iron_ore", 12.0});

	CHECK(moved != nullptr);
	CHECK_EQ(moved->units, 12.0);

	CHECK(world.Remove<Cargo>("Ship-01"));
	CHECK(world.Get<Cargo>("Ship-01") == nullptr);
	CHECK(!world.Remove<Cargo>("Ship-01"));

	// An id the world never had is a null pointer, not a crash and not a thrown
	// exception: this is library code.
	CHECK(world.Get<Cargo>("Ship-02") == nullptr);
	CHECK(world.Add<Cargo>("Ship-02", Cargo{"water", 1.0}) == nullptr);
	CHECK(world.Put<Cargo>("Ship-02", Cargo{"water", 1.0}) == nullptr);
	CHECK(!world.HasComponent<Cargo>("Ship-02"));
}


TEST_CASE("a_view_finds_every_entity_that_carries_the_component")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	for (const char *id : {"Ship-01", "Ship-02", "Civ-01"})
	{
		CHECK(world.Create(id, errorMessage));
	}

	world.Add<ShipInfo>(
	    "Ship-01", ShipInfo{"Ship-01", "HullTraderLongHaul", "Long-haul trader", {}, "Civ-01", {}});
	world.Add<ShipInfo>(
	    "Ship-02", ShipInfo{"Ship-02", "HullRaiderFast", "Fast raider", {}, "Civ-02", {}});
	world.Add<BodyInfo>(
	    "Civ-01", BodyInfo{
	                  "Civ-01",
	                  Manifest::Core::BodyKind::Planet,
	                  "Abigail Prime",
	                  {},
	                  "Abigail Prime",
	                  "Civ-01",
	                  {}});

	CHECK_EQ(world.ComponentCount<ShipInfo>(), 2u);
	CHECK_EQ(world.ComponentCount<BodyInfo>(), 1u);

	// A view over a component is a read of every hull in the store, and the id of each
	// one travels with it, because Create gave every entity its EntityId.
	std::vector<std::string> hulls;

	for (const Manifest::Core::EcsWorld::Entity entity : world.View<ShipInfo>())
	{
		const ShipInfo *info = world.Get<ShipInfo>(entity);

		CHECK(info != nullptr);
		hulls.push_back(world.NameOf(entity));
	}

	std::sort(hulls.begin(), hulls.end());

	CHECK_EQ(hulls.size(), 2u);
	CHECK_EQ(hulls[0], std::string("Ship-01"));
	CHECK_EQ(hulls[1], std::string("Ship-02"));

	// HasComponent takes more than one component, and the comma that separates them is
	// inside the angle brackets the preprocessor does not see: the condition goes in
	// parentheses of its own so the macro gets one argument.
	CHECK((world.HasComponent<ShipInfo, EntityId>("Ship-01")));
	CHECK(!(world.HasComponent<ShipInfo, BodyInfo>("Ship-01")));
	CHECK((world.HasComponent<BodyInfo, EntityId>("Civ-01")));
}


TEST_CASE("clear_empties_the_world_and_the_id_numbers_stay_spent")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	for (const char *id : {"Order-1", "Order-2", "Order-3"})
	{
		CHECK(world.Create(id, errorMessage));
		world.Add<Cargo>(id, Cargo{"water", 1.0});
	}

	CHECK_EQ(world.EntityCount(), 3u);

	world.Clear();

	CHECK_EQ(world.EntityCount(), 0u);
	CHECK_EQ(world.ComponentCount<Cargo>(), 0u);
	CHECK(world.Ids().empty());

	CHECK_EQ(world.NextId("Order"), std::string("Order-4"));
	CHECK(world.Create("Order-1", errorMessage));
}


TEST_CASE("the_registry_is_public_because_a_system_wants_a_view_of_several_components")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	CHECK(world.Create("Order-1", errorMessage));
	world.Add<MarketOrderInfo>("Order-1", MarketOrderInfo{});
	world.Add<OrderAt>("Order-1", OrderAt{"Market-Civ-01"});
	world.Add<OrderOwner>("Order-1", OrderOwner{"Civ-01"});

	// A system that reads a book wants the order, its venue and its owner at once, and
	// that is a view over three components, which EcsWorld does not wrap.
	std::size_t matched = 0;

	for (const Manifest::Core::EcsWorld::Entity entity :
	     world.Registry().view<MarketOrderInfo, OrderAt, OrderOwner>())
	{
		const MarketOrderInfo *order = world.Registry().try_get<MarketOrderInfo>(entity);
		const OrderAt *at = world.Registry().try_get<OrderAt>(entity);
		const OrderOwner *owner = world.Registry().try_get<OrderOwner>(entity);

		if (order != nullptr && at != nullptr && at->marketId == "Market-Civ-01" && owner != nullptr &&
		    owner->civilisationId == "Civ-01")
		{
			++matched;
		}
	}

	CHECK_EQ(matched, 1u);
}


TEST_CASE("the_component_helpers_work_on_an_entity_a_view_handed_over")
{
	Manifest::Core::EcsWorld world;
	std::string errorMessage;

	CHECK(world.Create("Ship-01", errorMessage));

	for (const Manifest::Core::EcsWorld::Entity entity : world.View<EntityId>())
	{
		world.Add<Screen>(entity, Screen{120.0});
		world.Add<Station>(entity, Station{"Abigail Prime"});

		auto *screen = world.Get<Screen>(entity);

		CHECK(screen != nullptr);
		CHECK_EQ(screen->megawatts, 120.0);

		screen->megawatts = 240.0;

		CHECK_EQ(world.Get<Screen>(world.NameOf(entity))->megawatts, 240.0);
		CHECK((world.HasComponent<Screen, Station, EntityId>(entity)));
		CHECK(!world.Remove<Screen>(Manifest::Core::EcsWorld::Null));
	}

	CHECK_EQ(world.ComponentCount<Screen>(), 1u);
}
