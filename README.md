# Manifest

A simulation of civilisations and their trade needs. Each civilisation is a planet or
satellite in a solar system that is generated at run time, produces some of the trade
goods, needs the rest, and trades against the others through prices that follow scarcity.

The design target is the trading element of games built on the
[Space Truckers](https://github.com/ivanhawkes/space-truckers) framework.

The engine is assembled primarily from open source libraries, each selected for fitness for
purpose. The criteria are:

- elegance
- efficiency
- modern C++ usage
- interoperability
- maintenance

All attempts will be made to observe best practice.

## Goal

A satisfying space trading element: worlds whose economies differ enough that a player
can find a profitable route, a skill tree that changes what a civilisation can produce,
and a data set big enough to explore. The scope today is the core model and its
configuration pipeline, the ECS store the universe is kept in, the two effects engines
that land the skill tree on hulls and on civilisations, the marketplace that trades the
goods of that universe, and a terminal front-end that drives the economy.

## Cloning

```
git clone git@github.com:ivanhawkes/manifest.git
```

## Building

The provided `devenv.nix` supplies the toolchain: Clang, CMake 3.28+, Ninja, the Vulkan SDK
pieces, `glslc` and the Wayland libraries.

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug

# Run from the root folder.
build/clang-debug/source/test/render_test
```

## Building

The provided `devenv.nix` supplies the toolchain: Clang, CMake 3.28+, Ninja, the Vulkan SDK
pieces, `glslc` and the Wayland libraries.

```bash
cmake --preset clang-debug
cmake --build --preset clang-debug

# Run from the root folder.
build/clang-debug/source/test/render_test

# The full gate: format, builds, tidy, headless render test on software Vulkan.
./scripts/ci.sh --llvmpipe --werror
```
## Documentation

Everything else is in [`docs/`](docs/README.md):

- [Scope](docs/scope.md): what is in and out, and why
- [Architecture](docs/architecture.md): layout, module dependencies, the simulation split,
  coordinate frames
- [Roadmap](docs/roadmap.md): the phases and a brief for every system
- [Coding standards](docs/coding-standards.md)
- [Build and gates](docs/build-and-gates.md)
- [History](docs/history.md)

## Architecture

| Target | What it is |
| --- | --- |
| `ManifestCore` | The plain C++20 core: configuration loading, universe names, the skill tree, the economy, the ECS store, the two effects engines and the marketplace. No game engine, no terminal library. |
| `ManifestCoreTui` | FTXUI terminal front-end used to drive the core during development. |
| `ConfigTests`, `UniverseTests`, `ProgressionTests`, `SimulationTests`, `EcsTests`, `EffectTests`, `MarketplaceTests` | The seven unit suites: 77 cases and 3486 checks. Each writes the documents and the worlds it needs, so they test the code and not the shipped data. |
| `DataTests` | Loads the generated documents and asserts they are a playable universe: 14 cases and 2673 checks over the merged ages, categories, goods, skill tree and name pools. |

Inside the core:

- `Core/SimulationConfig.h` - loads `ages.yaml`, `categories.yaml` and a game mode
  (items and skills). YAML nulls are "not set", never the string `"null"`; every field
  that is missing, null or mistyped is a load error naming the entry, for example
  `skills[2].requires[0]`. Two definitions that share an id are not a loader concern:
  `data/build.sh` stops the build on the pair, and `DataTests` fails on it in the
  merged document.
- `Core/Universe.h` - loads the solar system name pools (`latin`, `greek`, ...).
- `Core/ProgressionManager.h` - the skill tree: tiers capped by `maxTiers`,
  prerequisites, exclusive groups that lock out a rival branch, a resource pool to pay
  for a tier. `Build` validates the graph, including cycles and prerequisites that can
  never be reached.
- `Core/Simulation.h` - generates worlds from the ages, categories, items and name
  pools; gives each one an age, a population, specialties, production, demand and a
  stockpile; prices goods by scarcity, market-wide and per world; plans trades that pay
  for their freight; advances a tick of production, consumption and trade. A world's age
  bounds the goods it can make, and the goods of the essential categories - the food and
  fuels a population runs on, the medicines and the knowledge goods - are wanted at
  every age, so a world that cannot make medicine is a buyer of it.
- `Ecs/EcsWorld.h` - the entity store the body, ship and marketplace subsystems share:
  an EnTT registry whose entities carry the string ids the rest of the core uses
  (`Civ-01`, `Order-7`), indexed by a map keyed on the id, so an order that filled last
  tick does not share a number with a venue opened this tick, and a walk of the store
  gives the same order on every platform.
- `Ecs/Components.h` - what a body, a hull, a venue, an order and a store *are*, as
  data: plain structs, no constructor, no behaviour, so a system reads a fact without
  asking the entity's permission. `BodyInfo`, `Treasury`, `Stockpile`, `Production`,
  `Demand`, `ShipInfo`, `ShipSystems`, `ShipStats`, `MarketplaceInfo`, `MarketOrderInfo`,
  `MarketQuote`, `TradeFill`, and one effects component per domain.
- `Effects/EffectCatalogue.h` - what a tier of an effect is worth, keyed by the
  `targetEffectID` the tree names: 54 effects over six domains, 25 of the hull, 11 of the
  market, 6 of the body, 4 each of the yard, the lane and the survey. The tree names every
  one of them, and `Tests/EffectTests.cpp` asserts the join in both directions: the
  catalogue names no effect the tree does not buy, the tree names no effect the catalogue
  does not define, and every effect lands on the store its own domain names.
- `Effects/ShipEffectEngine.h` - the ship systems of the tree onto a hull. A `Skill`
  node is a system the crew fit, and the hull has to have a slot for it; a `Tech` node
  is knowledge the civilisation knows, costs no slot, and reaches every hull of its
  operator. Both write an `EffectSource` per effect and a `ShipStats` that is the hull's
  base plus every source.
- `Effects/CivilisationEffects.h` - the other five domains onto the stores a
  civilisation owns: its venues, its worlds and satellites, its one lane store, its one
  survey store and its one yard store. `DescribeCivilisation` prints the store lines.
- `Market/Marketplace.h` - the venue, the book, the fill and the quote table as systems
  over the store. `MarketplaceService` owns its options and nothing else, so two
  services over one world see one book, and that book is what a save game writes out.

The generation is deterministic for a seed, and the model uses only the fields the
merged documents carry, so it runs on the data as it is. `LoadSimulation` writes the
generated universe into the store - one body per world, with its identity, population,
treasury, stockpile, production deck and demand - and opens a venue at every world that
may host one.

## Requirements

I do most of the work on a NixOS Linux machine. The code is kept independent of any
game engine so that NixOS and the tools of my choice stay usable. You are free to use
whichever OS makes your heart flutter. Fellow NixOS users get a streamlined onboarding
process.

- CMake 3.24+ and Ninja, or any CMake generator you prefer.
- A C++20 compiler. The development shell pins LLVM 21 (`devenv.nix`).
- mikefarah/yq v4: `data/build.sh` merges the source YAML with it, and the test suite
  reads the generated documents. The merge is tested with **v4.54.1**, the version the
  development shell pins. The source files refer to each other by `itemId`, `skillId`,
  `categoryId` and `ageId`, so no anchor has to survive the trip from one file to the
  next and any yq v4 builds the data;
  CMake warns when the `yq` on `PATH` is not the tested version, which is the version the
  merge is known to do the job with. A missing `yq` with the tests enabled is a fatal
  configure error, rather than a suite that silently runs nothing: build the core without
  the tests with `-DBUILD_TESTING=OFF`.
- Git and network access at configure time: CMake fetches yaml-cpp 0.9.0, FTXUI 7.0.3
  and EnTT v4.0.0. EnTT's own `CMakeLists.txt` opens with `cmake_minimum_required(VERSION
  3.28)`, so the fetch needs CMake 3.28 even though this project configures on 3.24: CI
  ships 3.28, and the development shell 4.4.
- The ECS, effects and marketplace subsystems are in `ManifestCore` and are public
  headers: the core is one target, and `EcsTests`, `EffectTests` and `MarketplaceTests`
  link it the way the other suites do.

## 1. Activate the Nix development environment

From this directory:

```bash
direnv allow
```

The `.envrc` uses `devenv`. Ensure direnv is hooked into your interactive shell and
devenv is installed.

Example NixOS configuration for direnv:

```nix
{
  programs.direnv = {
    enable = true;
    nix-direnv.enable = true;
  };
}
```

Rebuild your NixOS configuration after changing it, then add the appropriate shell
hook:

```bash
# Bash
eval "$(direnv hook bash)"

# Zsh
eval "$(direnv hook zsh)"

# Fish
direnv hook fish | source
```

## 2. Build and test the standalone core

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build
ctest --test-dir build --output-on-failure
```

`cmake --build build` runs the `ManifestData` target, which is `data/build.sh`: it merges
the source YAML into `data/core-config`, so a clean clone builds and tests with no
manual data step. Eight suites then run: the seven unit suites on the documents and the
worlds they write themselves - 77 cases, 3486 checks - and `DataTests` against the
generated documents, 14 cases and 2673 checks.

Formatting is part of the build: `.clang-format` covers `Source` and `Tests`, and CI
checks it with `clang-format --dry-run -Werror` over every `.h`, `.hpp` and `.cpp` in
the two trees. Format your work with:

```bash
find Source Tests \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 | xargs -0 clang-format -i
```

The parentheses carry the weight: `find`'s `-print` binds to the last `-name`, so a
pipeline that ends `-name '*.h' -o -name '*.cpp' -print` lists the `.cpp` files only,
the gate runs on a file list that never contains a header, and header drift passes. It
is the command the CI step and the `wt` pre-merge hook run, so the gate and the habit
stay the same command.

## 3. Run the terminal front-end

```bash
./build/ManifestCoreTui
```

It loads `data/core-config` on startup, so the first command can be `generate 8 7`.

| Command | What it does |
| --- | --- |
| `status` | Configuration, universe and skill tree state. |
| `load [dir]` | Reload the ages, categories, full game mode and name pools. |
| `generate [n] [seed]` | Generate `n` worlds (default 8, seed 1). |
| `list` | Every world: name, pool, age, population, goods, treasury. |
| `show <Civ-01>` | One world in detail: specialties, production, shortages. |
| `prices [n]` | The `n` most over-priced goods, and the cheapest. |
| `plan [n]` | The `n` best trade routes, with freight and profit. |
| `tick [n]` | Advance `n` ticks: production, consumption, trades. |
| `skills` | The skill tree with tiers, costs and locked groups. |
| `upgrade <id>` | Buy one tier of a skill, paying from the resource pool. |
| `clear`, `help`, `version`, `quit` | Log and session housekeeping. |

`Tab` completes a command, `Up`/`Down` walk the command history, and `PageUp`/`PageDown`
read the log. Every line the front-end prints is appended to `build/tui-session.log`,
flushed as it is written, so `tail -f build/tui-session.log` in another pane follows a
session and a crash mid-frame keeps the transcript.

Population is in world units, not people: production and demand are units of goods per
tick, and the era sets the ceiling, so a world of `AgeSublimation` dwarfs a world of
`AgeStone`.

The front-end drives the economy model. The venues, the books and the fills are not in
its command set: the marketplace is driven from the core API and by
`Tests/MarketplaceTests.cpp`.

## 4. Core API

```cpp
#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Core/SimulationConfig.h"
#include "Core/Universe.h"

ManifestCore::Ages ages;
ManifestCore::Categories categories;
ManifestCore::GameModeNode gameMode;
ManifestCore::UniverseNamePools names;
std::string error;

if (!ManifestCore::LoadConfigAges("data/core-config/ages.yaml", ages, error) ||
    !ManifestCore::LoadConfigCategories("data/core-config/categories.yaml", categories, error) ||
    !ManifestCore::LoadConfigGameMode("data/core-config/full.yaml", gameMode, error) ||
    !ManifestCore::LoadUniverseNamePools("data/universe/solar-system-names.yaml", names, error))
{
    // The message names the file, the entry and the field.
}

ManifestCore::GenerateOptions options;
options.civilisationCount = 8;
options.seed = 7;

ManifestCore::Simulation simulation;
if (!simulation.Generate(ages, categories, gameMode.items, names, options, error))
{
    // Handle error.
}

// The trade the player is looking for: buy where the good is cheap, sell where it is
// scarce, and the route has to pay for its own freight.
for (const ManifestCore::TradeRoute &route : simulation.PlanTrade(10))
{
    // route.itemId, route.sellerId, route.buyerId, route.quantity, route.profit
}

simulation.AdvanceTick(error);

ManifestCore::ProgressionManager progression;
progression.SetResources(20);

if (!progression.Build(gameMode.skills, error))
{
    // A prerequisite is unknown, the graph has a cycle, a tier is out of reach, or a
    // skill requires a rival of its own exclusive group.
}

progression.TryUpgrade("SkillSmelting", error);
```

The same universe in the ECS store, with a venue at every world that may host one, an
order on the book and the quote table the quoters read:

```cpp
#include "Ecs/EcsWorld.h"
#include "Market/Marketplace.h"

ManifestCore::EcsWorld world;
std::size_t venuesOpened = 0;

// The generated worlds, their treasuries, stockpiles, production decks and demand,
// and a marketplace at every world that may host one.
if (!ManifestCore::MarketplaceService::LoadSimulation(
        world, simulation, ManifestCore::MarketplaceOptions{}, venuesOpened, error))
{
    // The message names the world and the field. Fails on a world that is not empty,
    // because the ids of a second load collide with the first.
}

ManifestCore::MarketplaceService market; // the default options: 40 bps, 120 bps, 24 ticks

ManifestCore::OrderRequest order;
order.marketId = "Civ-01"; // the venue's body id, which is the market's id
order.civilisationId = "Civ-01";
order.itemId = "ItemCopper";
order.side = ManifestCore::OrderSide::Sell;
order.quantity = 40.0;
order.limitPrice = 12.0;

std::string orderId;
if (!market.PostOrder(world, order, orderId, error))
{
    // No venue there, a trader of another world, a good the seller has no stock of, a
    // quantity under the venue's floor, or a book at its depth cap.
}

// A tick sweeps the orders that have outlived their order life, puts their goods and
// their credits back with the trader that posted them, and rebuilds the quote tables.
// It trades nothing: a post crosses the book it lands on, so the book is crossed down
// before the sweep.
market.AdvanceTick(world, simulation.GetTick(), error);
```

The marketplace, the effects engines and the store are one layer: `ApplyProgression`
walks the tiers the `ProgressionManager` has bought onto the venues, the worlds and the
lane of the civilisation that owns them, and the ship engine fits the ship systems of
the same walk onto its hulls. Every number a venue charges is the base of
`MarketplaceOptions` moved by the effects the tree put there, and
`Tests/EffectTests.cpp` and `Tests/MarketplaceTests.cpp` are the specification of that
layer. Each loader replaces its output container, so loading the same file twice into
the same vector does not duplicate entries, and a failed load leaves the container
empty.

## Processing the YAML

The YAML is edited as individual files that refer to each other by id - `itemId`,
`skillId`, `categoryId` and `ageId` - and are stored in **/data**: an item names the
category and the age it belongs to, and a game-mode file names the goods and the nodes
it plays with. The split keeps the files manageable and the ideas separate from each
other during development, and the merge resolves the references into monolithic
documents before the simulation reads them.

From the **/data** directory:

```bash
# Build all of the game-ready files using the source YAML.
./build.sh
```

`cmake --build build` runs the same script through its `ManifestData` target, so you only
need to run it by hand when you want the documents without a build.

This writes the game-ready documents to **/data/core-config**, which is generated and
not committed:

| File | Contents |
| --- | --- |
| `full.yaml` | Every item in the game, with the skills. |
| `simple.yaml` | The small subset of items used for quick matches and testing. |
| `ages.yaml` | The ages on their own. |
| `categories.yaml` | The categories on their own. |

Every input file is a complete YAML document that **yq** loads on its own: the item
definitions live under `item-base`, repeated in every item file, the nodes of the tree
under `skill-base`, repeated in every file of the skill tree, and the ages and the
categories under `ages` and `categories`. The merge reads the two base keys as a lookup
keyed on `itemId` and `skillId`, selects from it and drops them, so every key in a
generated file appears exactly once and the output is a plain document, with no anchors,
no aliases and no cross-file references left in it, ready for the loader.

The inputs are read in this order:

| Source | Contents |
| --- | --- |
| `base/ages.yaml` | The 14 ages, in age order. |
| `trade-items/categories.yaml` | The 43 categories. The goods of the item files are filed under 21 of them; 22 shelves hold no good at all, and 3 of those - `CategoryPharmaceuticals`, `CategoryPolymers` and `CategoryTextiles` - have a craft node of the same word. |
| `trade-items/chemicals.yaml` | The 11 goods of `CategoryChemicals`: sodium hydroxide, nitrogen, phosphorus, sulfur, sulfuric acid, antimony, chlorine, ammonia, ethylene, neodymium, graphene. |
| `trade-items/metals.yaml` | The 21 goods of `CategoryMetals`: copper, tin, brass, bronze, silver, nickel, iron, lead, cobalt, lithium, molybdenum, aluminium, palladium, platinum, potassium, rhodium, ruthenium, uranium, vanadium, zinc, gold. |
| `trade-items/minerals.yaml` | The 5 goods of `CategoryMinerals`: salt, lime, graphite, potash, natron. |
| `trade-items/misc.yaml` | The 42 goods of the 17 remaining categories: the consumables, the crafts, the ferments, the writing and the knowledge goods, the instruments of navigation, the sciences, farming, the home and the yard, charcoal, the luxury goods, the medicines, electronics, leather and furs, cement and screws, coins, and the one catch-all, slaves. |
| `trade-items/seeds.yaml` | The 10 goods of `CategorySeeds`: wheat, barley, oat, corn, rice, sorghum, poppy, cocoa, coca and canola. |
| `skill-tree/skills.yaml` | The 17 crafts of the economy. |
| `skill-tree/ages.yaml` | The 14 ages that gate the tree. |
| `skill-tree/spaceship.yaml` | The 25 ship systems, as a skill tree of their own. |
| `skill-tree/shipyards.yaml` | The 4 nodes of the yards that build and mend. |
| `skill-tree/lanes.yaml` | The 4 nodes of the lanes and the formations that run them. |
| `skill-tree/survey.yaml` | The 4 nodes of the survey and the claims it stakes. |
| `skill-tree/habitats.yaml` | The 4 nodes of the habitats and terraforming. |
| `skill-tree/piracy.yaml` | The 4 nodes of the pirate arts. |
| `skill-tree/law.yaml` | The 4 nodes of the warrant, the pardon, the bench and the standing. |
| `skill-tree/trade.yaml` | The 3 nodes of the docking licence, the cargo insurance and the fence. |
| `skill-tree/services.yaml` | The 2 nodes of public medicine and education. |
| `game-mode/full.yaml` | The full game mode: the item and node ids selected from the definitions above. |
| `game-mode/simple.yaml` | The small subset of items used for quick matches and testing. |

The order matters for the two lists the merge concatenates: `base/ages.yaml` fixes the
age order and `trade-items/categories.yaml` the category order. Each game-mode file
lists its own goods, so the sections of the file group the items of the document it
generates.

The `item-base` and `skill-base` keys hold the definitions, and they are the only keys
the merge reads as a lookup rather than as a list. Ages and categories have no base key:
nothing is inherited from them any more, so their `ageId`, `categoryId` and displayName
are written once, in the list the game reads.

`build.sh` concatenates the definition files into one stream and `yq ea` folds that
stream into the lookup: the ages, the categories, and the `itemId` and `skillId` to
definition maps for the items and the skills. Nothing crosses a file boundary as an
anchor, so a yq that scopes anchors to one document - v4.54.2 does exactly that - builds
the data as well as v4.54.1 does: the pin in the development shell, in CI and in the
CMake warning is a tested version, not a requirement. `DataTests` asserts the scale of
the merged documents - ages, categories, items, skills - so a merge that quietly
truncates the game fails the suite instead of shipping a short one.

Each item writes two references, `categoryId` and `ageId`, and both are plain text: the
`categoryId` of a category in `trade-items/categories.yaml` and the `ageId` of an age in
`base/ages.yaml`. They are the two dials the economy has: `ageId` bounds the goods a
world can produce, `categoryId` weights the goods a world wants. A good with no reference
is in every deck and wanted at the ordinary rate, so an item file that forgets the lines
costs the model a gate without failing a load. `DataTests` asserts that every item of the
full game carries both references and that the ids name a loaded age and a loaded
category.

Every id carries the prefix of the kind of thing it names: `Age` for an age, `Category`
for a category, `Item` for a good and `Skill` for a node of the tree. A reference
therefore says which table it points into, and the four namespaces cannot meet: the data
reuses one word across two tables - `SkillPharmaceuticals` is the bench that works the
drug and `CategoryPharmaceuticals` the shelf it lands on, `SkillPolymers` and
`SkillTextiles` the looms and `CategoryPolymers` and `CategoryTextiles` the bales,
`ItemElectronics` the good and `CategoryElectronics` its sort, `ItemSlaves` the cargo and
`CategoryUnknown` the shelf it is filed under, which is not the `CategorySlaves` the
category document names: the shelf is there and no good is on it - and the prefix is what
keeps the two ids apart.
`DataTests` asserts the prefixes, the one name shared by design - the gate of an age
carries the `ageId` of the age it opens - and that no other name is an age, a category, a
good and a node in turn. The ages drop the old `Age` postfix for the prefix, so no id
ends with the word it starts with.

The full item list in `game-mode/full.yaml` is a selection of the items defined in the
`trade-items/*` files, by id: `- itemId: ItemGold`. After adding, removing or renaming an
item, add its `- itemId:` reference to `game-mode/full.yaml` (and to
`game-mode/simple.yaml` if the item belongs in the small game), then run `./build.sh`
from the **/data** directory. The same is true of the `- skillId:` references of the skill
tree: the definitions live in the `skill-tree/*` files, the list lives in the two
`game-mode/*` files, and every node of the tree is in both game modes.

A reference with no definition behind it is the failure of a list typed by hand, and yq
answers it with a null that the loader reads as an entry with no fields. `build.sh`
compares the ids of a game-mode file with the ids of the definitions before it merges,
and stops with the names it cannot resolve; the same check catches two definitions that
share an `itemId` or a `skillId`, which the lookup would answer with the one it read
last.

`universe/solar-system-names.yaml` holds the name pools for the generated worlds, and
every top-level key of the file is a pool: the file is two pools today, `latin` with 2695
names and `greek` with 3571, 6266 entries of which 5640 are unique - 626 names sit in both
pools. Every name is one word, and a class word is never in the file: the class is the
generator's. `Core/Universe.h` keeps the pools in a map keyed by pool name, so
`PoolNames()` is sorted and a walk over the pools gives the same order on every platform;
the loader fails on a file with no pool, on a pool with no name, and on a null name. The
generation builds one deck of (pool, name) pairs over the pools in that order, shuffles it
with the seed of `GenerateOptions`, and takes the worlds off the front of it: a world's
`pool` is the pool its name came from, and its name is that name and a suffix from the
generator's list of ten - `Prime`, `Alpha`, `Beta`, `Gamma`, `Delta`, `Epsilon`, `Zeta`,
`Eta`, `Theta`, `Iota` - chosen by world index, so the first world of a run is
`<name> Prime`, the second `<name> Alpha`, the tenth `<name> Iota` and the eleventh
`<name> Prime` again. A run that asks for more
worlds than the pools hold names is answered with the two counts. The file is UTF-8, and it
is not part of the merge: `build.sh` does not read it. `DataTests` loads the real file and
asserts that the pools are many, that the names are more than a hundred, and that no pool
is empty; `UniverseTests` tests the loader on the documents it writes: two pools sorted,
loading twice replacing a pool, a null name rejected by pool name, and an unreadable file
naming the path.

The skill tree is a set of definitions like the items: the eleven files of `skill-tree/*`
define the nodes - the crafts, the ages, the ship, the yards, the lanes, the survey, the
habitats, the pirates, the law, the trade services and the public services - and the
`game-mode/*` files select them by id. The next section is about the tree.

### The skill tree

Every node is one of two kinds, and `nodeType` says which: a `Skill` is a craft that gets
better as it is bought a tier at a time, and a `Tech` is a gate that is open or shut. The
tree lives in eleven files:

| File | Nodes | Shape |
| --- | --- | --- |
| `skill-tree/skills.yaml` | 17 crafts, from `SkillMining` to `SkillFuelRefining` | `nodeType: Skill`, `maxTiers: 3`, `resourceCostPerTier: 1`, all roots, no exclusive groups: parallel branches. |
| `skill-tree/ages.yaml` | 14 age gates, one per age in `base/ages.yaml` | `nodeType: Tech`, `maxTiers: 1`, chained in age order, so each gate requires the gate of the age before it. The cost rises with the age, 1 to 5, so the ladder costs 31 units against the 1 a craft tier costs. |
| `skill-tree/spaceship.yaml` | 25 ship systems of the long-haul trader | `nodeType: Skill` for all 25, `maxTiers: 3`, no exclusive groups. Cost 3 for the drive, the two laser mounts, the two computers and the commercial array, 1 for the spares lockers, and 2 for the other 18: the three screens, the two gun decks, the jets, the attitude control, the long-range mast and its fire-control array, the holds, the bunkers, the salvage rig, the life support, the coating, the emissions control, the countermeasures, the crew, the repair party and the trader AI. |
| `skill-tree/shipyards.yaml` | 4 yard nodes: the drydock, the outfitting line, the modular hull, the refit dock | `maxTiers: 3`, cost 2 and 3. The hulls the ship systems are fitted to. The modular hull asks the Information Age gate as well as the drydock. |
| `skill-tree/lanes.yaml` | 4 lane nodes: convoy, escort wing, route ward, warden patrol | `maxTiers: 3`, cost 2 to 3. The formations a route is run with, and the warden's art is bought on top of the escort that escorts it. |
| `skill-tree/survey.yaml` | 4 survey nodes: the deep survey, the mining claim, the claim refinery, the exclusive claim | `maxTiers: 3`, cost 2 to 3. A claim asks the survey that found the ground *and* the Mining craft that works it; the refinery asks the Smelting craft. |
| `skill-tree/habitats.yaml` | 4 habitat nodes: the domes, the closed loop, the atmosphere, the biosphere | `maxTiers: 3`, cost 3 to 4. The domes ask the Space Colonisation Age gate; the biosphere asks the Agriculture craft. |
| `skill-tree/piracy.yaml` | 4 pirate nodes: raider hulls, boarding parties, ransom exchange, safe harbours | `maxTiers: 3`, cost 2. The technology of the raider: the hull is the root, and the parties, the ransom and the harbour that lands the cargo all hang on it. |
| `skill-tree/law.yaml` | 4 law nodes: the admiralty warrant, the pirate pardon, the tribunal, the standing among the crews | `maxTiers: 3`, cost 2 to 3. The warrant is a root, and the pardon and the tribunal hang on it; the standing is a root of its own. The warrant and the standing are in the exclusive group `CleanStanding`. |
| `skill-tree/trade.yaml` | 3 trade-service nodes: the docking licence, cargo insurance, the fencing network | `maxTiers: 3`, cost 1 to 2. Insurance asks the licence *and* the tribunal; the fence is the third member of `CleanStanding`. |
| `skill-tree/services.yaml` | 2 public nodes: the public medicine service, the education service | `maxTiers: 3`, cost 2. The demand side: medicine and education are demand, and these are the services a civilisation runs to meet it. The clinic asks the Pharmaceuticals craft and the school asks the Research craft, the crafts that train the people who staff them. |

An age gate carries the `ageId` of its age as its `skillId`: `AgeInformation` is the age
and the node that opens it, the two goods of that age are `ItemElectronics` and
`ItemGraphene`, and the modular hull of the yards asks the same gate. The gate and the
age are the same idea seen from the two ends, so a world generated in an age finds its
gate by a lookup on the age, and no second naming scheme has to be kept in step. The gate
writes no effect: `targetEffectID` is null on all 14, and both engines walk the whole
tree in one loop because a gate that asks for no effect is not a failure. The grid puts
the ladder on its own row at `gridY: -1.0`, in age order from left to right, with the ship
systems on the row at `gridY: 1.0`.

The ship systems are what a long-haul trader needs between worlds and against the pirates
who live off that trade: `SkillShipShieldsFront`, `SkillShipShieldsSide` and `SkillShipShieldsRear` by
arc of fire, `SkillShipGunsFront` and `SkillShipGunsRear`, `SkillShipLasersFront` and `SkillShipLasersRear`
for the range the guns do not reach, `SkillShipEngineThrust` for the drive, `SkillShipManoeuvringJets`
and `SkillShipAttitudeControl` for the working in space, `SkillShipSensorLongRange` with the
`SkillShipSensorFireControl` and `SkillShipSensorCommercial` arrays on its mast, and the two
computers that make the haul pay, `SkillShipNavigationComputer` for the jumps and
`SkillShipTraderAI` for the cargo. The rest of the ship is the reason it is long haul:
`SkillShipCargoHolds` and `SkillShipFuelBunkers` set the size of a run, `SkillShipSalvageRig` goes and
brings back the cargo of a hull that cannot, `SkillShipLifeSupport` sets the days a crew can
be out, and `SkillShipStealthCoating`, `SkillShipEmissionsControl` and `SkillShipCountermeasures`
decide whether anyone sees the ship doing it. Under all of it sits the crew:
`SkillShipCrewTraining` lets them work a bigger hull, `SkillShipDamageControl` keeps a ship flying
with a screen down, `SkillShipSparesLockers` lets the party do it twice.

Every system hangs off the one that has to be fitted first. Eight roots: the forward
screen, the forward gun deck, the navigation computer, the drive, the long-range mast,
the cargo holds, the stealth coating and the crew. The aft screen and the aft gun deck
hang on their forward twins, the two laser mounts hang on the forward gun deck, so a hull
that has bought the lasers has bought the guns and the deck that aims them; the jets hang
on the drive, the attitude control on the jets, and the fuel bunkers on the drive; the
fire-control array, the commercial array and the countermeasures hang on the long-range
mast; the trader AI hangs on the navigation computer; the salvage rig hangs on the holds;
the repair party hangs on the crew, the spares lockers on the repair party, and the life
support on the crew; and the emissions control hangs on the coating, because a plate that
hides a hull has to be run cold. No ship system is in an exclusive group, because a
trader who can afford a forward and an aft screen fits both.

The grid rows the lines in the order of a plan of the ship, not in the order of a fit: the
25 nodes run along `gridY: 1.0` from the forward screen at `gridX: 0.0` to the spares
lockers at `gridX: 31.0`, and `SkillShipFuelBunkers` at `gridX: 21.0` asks the drive four
columns to its left, so a row read left to right reads as a layout, not as a dependency
list. The head comment of the file says so, and every file of the tree but `skills.yaml`,
the first one written, carries a head comment that says what its nodes are, what the tier
of them buys, and what the grammar of `requires` means in that file.

All 25 ship systems are `nodeType: Skill`: a system the crew train and fit, in a slot of
the hull. The split the ship engine keeps is wider than the shipped tree uses: a `Tech`
node with a Ship effect is knowledge, not a system - it asks no slot, a hull never refuses
it, it never enters the `ShipSystems` list a yard refits, and it reaches every hull of the
operator that has bought it. The only `Tech` nodes in the shipped tree are the 14 gates,
which write no effect at all, so the knowledge half is exercised on nodes
`Tests/EffectTests.cpp` writes for itself. A hull that carries `ShipSlots` refuses a
system it has no slot for - the salvage rig and a hull with no rig frame are the test -
and a hull with no `ShipSlots` component has every slot, which is what a test hull is. The
yard tree is where the hulls with more slots come from.

The other eight trees are the 29 nodes a civilisation stands on, and they are where the
ship meets the ground. The yards at `gridY: 2.0` build and mend the hulls the systems are
fitted to: the drydock is the root, the outfitting line hangs on the drydock, the refit
docks hang on the outfitting line, and the modular hull hangs on the drydock *and* the
Information Age gate. The lanes beside them run the routes the ship sails: the convoy is
the root, the escort wing and the route ward hang on it, and the warden's patrol hangs on
the escort that escorts it. The survey and the claims it stakes ask a craft as well as a
sensor: the deep survey is a root, the mining claim needs the deep survey *and* the Mining
craft, the claim refinery needs the claim *and* the Smelting craft, and the exclusive
claim asks the second tier of the mining claim. The habitats at `gridY: 3.0` start at the
Space Colonisation Age gate, because a dome is the technology that age is named for: the
closed loop hangs on the domes, the atmosphere on the closed loop, and the biosphere
seeded on top of the weather asks the Agriculture craft. Below the age ladder, at
`gridY: -2.0`, run the pirate arts, the law, the trade services and the public services:
`SkillPirateRaiderHulls` is the root, and the boarding parties, the ransom exchange and
the safe harbours hang on the hull; `SkillLawAdmiraltyWarrant` is the root the pardon and
the tribunal hang on, and `SkillLawPirateStanding` is the standing the crews live by, a
root of its own; `SkillTradeDockingLicence` is a root, the cargo insurance asks the
licence *and* the tribunal, and the fencing network asks the safe harbour that lands the
cargo; and the two nodes of the demand side hang off the crafts that staff them, the
public medicine service asking the Pharmaceuticals craft and the education service asking
the Research craft. Prerequisites that cross the rows are the point of the shape, and
`DataTests` counts them, because a tree that stops linking is a set of lists.

`CleanStanding` is the one exclusive group in the shipped data, and it is the reason the
field exists: an admiralty warrant, a standing among the crews and a fence are one
reputation spent three ways, so buying the warrant shuts the crews' tongue and the fence
for good. `ProgressionManager::Validate` rejects a member that requires a rival of its
own group, which is why the warrant and the standing are roots; the fence may ask a
harbour, because a harbour is not a rival.

The full tree is 85 nodes and 227 tiers, and `game-mode/simple.yaml` selects the same
skills as `game-mode/full.yaml`: the items are what make the small game small, and a
short tree would leave the gates, the ship and the standing trees out of the quick
matches they are meant to test. `DataTests` asserts the scale of it: 14 gates one tier
deep and 71 `Skill` nodes three tiers deep, 25 ship systems and the 29 standing nodes, each
with the effect, the cost and the prerequisites it claims, the prerequisites that cross
the trees counted, every gate sitting on the gate of the age before in age order,
`CleanStanding` locking a chosen rival out and not the branch under it, and
`ProgressionManager::Build` accepting the whole graph. The three-tier floor is asserted on
the `Skill` nodes and not on the gates: a merge that dropped `maxTiers` lands on the
loader default of one tier, which flattens a craft to a single click, and that is what the
assertion is there to catch.

Not in the tree, and not to be built without a decision on each: crew wages and morale;
missions and contracts; fleets and task forces; price policy and market manipulation;
research that takes time; population growth and migration; plague, wrecks and the other
events that make a haul interesting; patrol and enforcement; government per world; the
jump gate as infrastructure; and the refining of fuel as a chain of supply. The item data
has a gap of the same shape: the item files run to the Information Age and stop, so
`AgePre-industrial`, `AgeSpace`, `AgeAI`, `AgeSpaceColonisation` and `AgeSublimation` are
gates that open no good. Nothing is opened and left empty: the ages above
`AgeInformation` are simply not written, and the production gate of a world generated in
one of them bounds nothing. The catalogue of effects and the tree name each other exactly:
54 effects, 54 `targetEffectID`s, and `Tests/EffectTests.cpp` asserts it both ways, so the
hole in the data is in the goods, not in the effects.

### The store, the effects and the marketplace

The economy model generates the worlds; the store keeps the universe. `EcsWorld` is an
EnTT registry whose entities carry the string ids the rest of the core uses - `Civ-01`,
`Ship-01`, `Order-7` - indexed by a `std::map` keyed on the id, so an order that filled
last tick and a venue opened this tick can never share a number, and a walk of the store
is a walk of a sorted map, the same order on every platform. `Create` spends the numeric
suffixes of the ids it is given by name, `NextId` raises the global counter to the
high-water mark of the prefix, and `Clear` empties the store and leaves the numbers spent:
a fill log can never quote two orders by one number. The registry is public, because a
system that wants a view of several components at once takes `Registry()` and builds it.

The components are the facts and nothing else: `BodyInfo` says what a body is, a `Planet`,
a `Satellite` in orbit around a planet, a `Star`; `Treasury`, `Stockpile`, `Production`,
`Demand` carry the economy's numbers on the entity; `ShipInfo`, `ShipSystems`, `ShipStats`,
`ShipSlots` carry a hull; `MarketplaceInfo`, `MarketOrderInfo`, `MarketQuote`, `TradeFill`
carry a venue; and there is one effects component per domain. No component has a
constructor, an invariant or a virtual function, so a new system is written over the same
store without touching one of them, and a component read out of the store and a
`Civilisation` read out of the economy model say the same thing in the same words.

The two engines land the tree on the store. A tier bought writes an `EffectSource` entity
- the skill id, the effect id, the origin, the tier - and an `EffectOn` naming the store it
lands on and the domain it lands there as, and the numbers of a store are the sum of the
tiers bought, in sorted effect-id order, on the base the store starts from: the
`ShipClassSpec` of a hull, the `MarketplaceOptions` of a venue, and nothing at all for a
lane, a survey and a yard store. The ship engine is `ShipEffectEngine`: a Ship effect is a
system, the hull must have a slot for it, the fitted systems sit in `ShipSystems`, and a
yard takes one off again - `UnfitSkill` sweeps the sources of one skill id off every hull
that carries them and rebuilds the hulls it touched, and `UnfitEffect` takes one system off
one hull, a system the hull never carried being a successful no-op. The civilisation engine
is `CivilisationEffectEngine`: its five domains write `MarketEffects` on the venues,
`BodyEffects` on worlds and satellites, and `RouteEffects`, `SurveyEffects` and
`YardEffects` on one store per civilisation, the lane store being `LaneStoreId(owner)`.
`ApplyProgression` walks the whole tree the `ProgressionManager` has bought, on both
engines, and both write the caller's two columns - the nodes applied and the nodes skipped,
gates, nodes with no tier bought, the systems of the other engine - as a *set*, not an
addition. A gate asks for no effect, buys nothing and returns true, and a Ship node returns
true with `Bought` false, so one tree walks in one loop and the ship systems go to the ship
engine.

A venue is a marketplace at a body, and the venue's id is the body's id. In the generated
universe that id is the world's own: `LoadSimulation` adds a planet whose body id, whose
owner and whose civilisation are `Civ-01`, and whose display name is the generated system
name, so the venue a trader posts at is `Civ-01` and the screen prints the world's
generated name.
`VenueBlockReason`
answers a body that may not host one, naming the thing it lacks: a planet hosts a market,
a satellite hosts one while it is in orbit around a planet, a star hosts none, and a
satellite of no planet - a rock in a star's orbit, a moon of a moon - has no well to sit
above and no people but the crews that pass. `LoadSimulation` writes the generated worlds
into the store and opens a venue at every one that may host one, and counts what opened, so
a satellite of a star leaves the count short; `AddPlanet`, `AddSatellite` and `Open` are the
way a test builds a venue by hand, and `Open` records on the venue the fee, the premium and
the order life the service that opened it carries.

The book is a price-time book: best price first, and inside a price the order that posted
first, and then the order id. A resting order states the worst price it will accept, and
posting reserves the thing the order trades: a sell order is covered by the seller's
stockpile and a buy order by the buyer's treasury, a fill spends the order's own pocket
first and the treasury pays only the remainder, so no buyer is charged twice and a fill
never fails for want of the thing it is trading. A post crosses the book it landed on, so
the venue keeps the invariant that the best bid sits under the best ask; a cancel moves
that side's best price further from the other side, so the crossing pass a cancel runs
trades nothing, and a cancel is the trader's goods and credits back and not a trade.
`MatchBook` runs the pass for a book a caller opened with orders in it, wrote by hand, or
opened at a venue whose floors have moved; `ExecuteOrder` walks the other side best price
first and leaves nothing resting.

A fill trades in whole units of the good and whole credits. The maker, the order that was
resting, sets the price of the fill; the fee is in basis points of the gross, charged to
both sides, rounded up, so the venue never pays a fraction of a credit out of its own
pocket. The goods move from the seller's `Stockpile` into the buyer's, the credits from the
buyer's `Treasury` into the seller's, the venue's share goes to its `venueTake`, and the
fill goes to the venue's log, which keeps the last 64 fills (`marketFillLogSize`), oldest
first. `CancelOrder` is idempotent for an order that is gone - expired, cancelled, never
posted - and refuses an order a venue's fill log names, because the cargo that order
reserved left the stockpile in the fill and a refund would pay the trader for it twice; an
empty id names no order, which is a caller's error and not an order that is gone.

The tree moves the charges of a venue, and every charge is a sum and a clamp read off the
market store of the venue and the lane store of its owner: `RiskPremiumBps` is the signed
sum of the venue's `MarketRiskPremium` and the `RouteRiskPremium` of `LaneStoreId(owner)`,
the law of the civilisation that owns the venue against the raids of the civilisation that
sails the lane; `ListingFeeBps` is the venue's base, 40 by default, plus that risk, less
one basis point for every two berths a day the licence clears (`MarketClearance`), and never
below nothing; `InsuranceBps` is the base premium, 120 by default, plus `MarketInsurance`,
and never below nothing; and `DepthUnits` is the cap on one side of the book, the
`depthUnits` of the service posting the order raised by the venue's `MarketDepth`, with 0
meaning no cap, which is the default: a venue that sets no depth is a venue quoters arrive
at, not a venue that turns them away. `MarketFenceShare` is a fraction of a cargo and
prices no charge of a venue at
all. `ApplyProgression` writes those effects and rebuilds every quote table of the
civilisation, because the fee is the number a screen shows beside the price.

A tick sweeps the orders that have outlived their order life - 24 ticks by default, the
berths a day needs, added plain, so a service that sets no life writes an expiry that never
runs past the tick it posted - puts their goods and their credits back with the trader that
posted them, and rebuilds the quote tables. A tick trades nothing: the book is crossed down
before the sweep, by the posts that crossed it. `RefreshQuotes` rebuilds a table from its
book and keeps what the fills left: the last trade, its tick, the volume ever traded. A
good enters the table when it is first quoted and never leaves it, so a row outlives the
orders
that made it, which is what a screen shows beside a good that just traded; the two order
counts of a row count the orders *at the best price level*, not the whole side. `BestAsk`,
`BestBid`, `MidPrice` and `SpreadBps` are the readings of a book; `ReferencePrice` is the
reading a ship, a UI and a test want: the mid when the book has both sides, the side it has
when it has one, the last trade when it has no orders left, 0.0 when the venue has never
seen the good.

`DescribeOrder` prints an order as one line - `Order Order-7 Buy 40 ItemCopper @ 12.5 at
Civ-01 for Civ-01, 12 filled` - and `DescribeMarket` prints a venue: what it is, what it
charges, the book of its most traded goods. `MarketIds` answers the venues of a
civilisation, `OrdersOf` its orders, `OrdersOf` with both ids narrows to one trader at one
venue, and `DescribeCivilisation` prints the stores the civilisation engine has written.
The service owns its options and nothing else, so two services over one world see the same
book, and that book is what a save game writes out. `Tests/MarketplaceTests.cpp` is the
specification of it: 12 cases, 1515 checks, the venue rule and the reservation, the crossing
and the walk, the quote table and the deck, the effect on the charge and the life on the
order, the tree read by the venue, the whole unit of a fill, the world the generator built
loaded into the store, and the same posts in the same order giving the same fills.

## Units, prices and storage

Every item carries five numeric fields. They describe the good as a physical object, so
that a future cargo model can price freight by volume and mass; the current economy
trades on `basePrice` alone and does not read the other four, and the marketplace settles
at the price a trader posts on its book, so it reads none of the five. Freight by mass and
volume is the work that gives these fields their reader. What freight exists today is one
number and a fraction of value: `Simulation::PlanTrade` charges `kFreightRate`, 0.08 of
the base price of a good for every unit the route moves, so the cost of a haul is a share
of what the good is worth, never a mass, a volume, a distance or a cube. The loader reads
the four fields as optional strings and keeps the text verbatim; `Simulation::BasePrice`
parses the fifth, and answers 0.0 for a price that is not a number. The storage cube is the
IBC cube, the one cubic metre intermediate bulk container, and the fields are defined
against it:

| Field | Meaning |
| --- | --- |
| `density` | Specific gravity of the material against water at sea level, so `1.0` is 1000 kg/m³. The solid value at 20 °C for a metal that is liquid at that temperature. |
| `unitType` | The unit `basePrice` and `unitsPerCubicMetre` count: `Kilogram`, `Gram`, `Litre`, `Kilolitre`, `Unit`, `Sheet`, `SquareMetre`. |
| `unitsPerCubicMetre` | Units of loose product filling one cubic metre of the material: see the table below. |
| `packFactor` | The fraction of a cube the product fills, counting packaging, dunnage and the voids between pieces. A hollow blown flask is mostly air, so `ItemGlassware` is `0.05`; stacked `ItemCopper` ingots are `0.85`. |
| `basePrice` | Market value per `unitType`, in credits at one credit to one US dollar, at 2025 spot and wholesale prices. Prices drive the model through the ratio of market price to base price, so the currency anchor only sets the relative level of the goods against each other. |

| `unitType` | `unitsPerCubicMetre` | |
| --- | --- | --- |
| `Kilolitre` | `1` | A kilolitre is exactly a cubic metre, so bulk volume goods count one to the cube. |
| `Litre` | `1000` | A cubic metre is a thousand litres. |
| `Kilogram` | `density x 1000` | The mass of solid material in a cubic metre: copper at 8.96 is 8960. |
| `Gram` | `density x 1000000` | The same mass in grams: `ItemGems` at 3.5 are 3500000. |
| `Unit` | tight-nested piece count | How many of the piece nest into a cubic metre: 2000 `ItemBooks`, 40000 pieces of `ItemJewellry`. |
| `Sheet` | tight-nested sheet count | Sheets stacked to a metre with their interleaves: 50000 sheets of `ItemPapyrus`, 20000 of `ItemParchment`. |
| `SquareMetre` | `1 / thickness` | Sheet goods sold by area: `ItemGraphite` is foil, and a millimetre of it is 1000 m² to the cube. |

For the goods counted by piece, sheet or square metre the count is a packed count, so
it fixes the mass of the piece, and the figures agree through a mass identity that is
the check to apply when a new item is added:

```
density x 1000 x packFactor  ==  kilograms in an IBC cube  ==  unitsPerCubicMetre x piece mass
```

A jeweller's piece of 15 grams of 12-specific-gravity metal takes 1.25 cm³ of metal in a
25 cm³ fitted tray, so 40000 to the cube, a packFactor of 0.05 and 600 kilograms of
metal in the cube on both sides of the identity. That is `ItemJewellry`, the id the data
spells with one `l`, and the spelling is the data's, not the loader's: `ItemJewellry`,
`ItemJewellery` and `ItemJewelry` are three different ids to the loader, and the first is
the one the game-mode lists name. Goods counted by mass or by volume
state the solid figure in `unitsPerCubicMetre`, and only `density x 1000 x packFactor`
holds for them: an IBC cube of stacked copper ingots is 8.96 x 1000 x 0.85 kilograms,
not 8960.

Wheat seeds, `ItemWheatSeeds`, show the split the fields make: the density of a wheat
kernel is 1.27, its test weight is 78 kg per hectolitre, so the packFactor is 0.61, the
mass of a full cube is 775 kg, and 350 credits buys that cubic metre of certified seed.
The seeds are `Kilolitre` goods, so the unit that costs 350 credits is the heap, not the
kilogram. The bulk minerals and chemicals sit in the same shape: a kilolitre is the heap,
and the air between the grains is most of the volume for a floured material like `ItemLime`
at 0.31, whose solid density is 3.34.

Prices are the current market figure for the traded form of the good, not for a purer
or rarer form of it: uranium is the 2025 spot price of `U3O8` concentrate, the
pharmaceuticals are bulk active-ingredient prices per kilogram, and the precious metals
are per kilogram rather than the per-troy-ounce quote the market uses, so `ItemGold` is
130000 a kilogram.

## Continuous integration

`.github/workflows/ci.yml` configures, builds, runs `ctest` and checks formatting, on
the pinned yq v4.54.1, the version the data merge is tested with. A second job
configures with the tests enabled and `yq` absent,
and fails if that configure succeeds: a suite that quietly disappears is the bug that
job guards.

`.config/wt.toml` mirrors the same gates locally. `pre-merge` builds, runs the tests and
checks formatting, so a merge needs all three.

## Compatibility notes

- The public core API does not expose yaml-cpp types.
- The store does expose EnTT: `EcsWorld::Entity` is `entt::entity`, and `Registry()`
  returns the registry, so anything that includes `Ecs/EcsWorld.h` needs the EnTT headers
  on the include path. The core is one target and the fetch is its own; `EcsTests`,
  `EffectTests` and `MarketplaceTests` link it the way the other suites do.
- yaml-cpp needs C++ exceptions, and the core is built with them.
- The sample uses C++20.
- The economy is a development harness: a small, deterministic supply, demand and price
  model that is meant to be reasoned about, not tuned. Population, treasury and freight
  are unitless model quantities. Population, production and demand count units per tick.
- The marketplace is deterministic in the same way the generation is: the book orders by
  price, then tick, then order id, every walk is a walk of a sorted map, and `Fills` is
  the last 64 of a venue, oldest first, so the same posts in the same order give the same
  fills. Money is whole credits and a fill trades whole units of a good: `TradeFill`,
  `MarketOrderInfo` and `MarketplaceInfo` keep integers, and a fee rounds up.
- A fill settles at the venue, goods and credits moving between the components of the two
  traders: freight to the venue and away from it is the economy model's job, the same job
  it does today by taking `kFreightRate`, 0.08 of the base price of a good, for every unit
  a route moves. A tick of the marketplace trades
  nothing; a tick of the economy model produces, consumes and trades.
- `data/core-config` is generated output: edit the source YAML under `data/` and rebuild.

## AI Disclosure

This project is being made with the assistance of several AI models:

- Qwen 3.8 125B - Swift IQ3 - KV 8,4 (local AI)
- Chat GPT (scaffold)
- Claude (core simulation, progression, tests, CI)

AI will be utilised in research, ideation, writing specifications, requirements gathering and code production.

I have 45 years of development experience from micro-controllers through to security and devops for highly available high traffic websites. 

At this point in time it would be utterly foolish to create software without support from AI models. In the hands of a skilled and experienced developer it is an incredible force multiplier.
