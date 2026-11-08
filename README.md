# CivSim

A simulation of civilisations and their trade needs. Each civilisation is a planet or
satellite in a solar system that is generated at run time, produces some of the trade
goods, needs the rest, and trades against the others through prices that follow scarcity.

The design target is the trading element of games built on the
[Space Truckers](https://github.com/ivanhawkes/space-truckers) framework.

## Goal

A satisfying space trading element: worlds whose economies differ enough that a player
can find a profitable route, a skill tree that changes what a civilisation can produce,
and a data set big enough to explore. The current scope is the core model, its
configuration pipeline and a terminal front-end to exercise them.

## Architecture

| Target | What it is |
| --- | --- |
| `CivSimCore` | The plain C++20 core: configuration loading, universe names, the skill tree, the economy. No game engine, no terminal library. |
| `CivSimCoreTui` | FTXUI terminal front-end used to drive the core during development. |
| `ConfigTests`, `UniverseTests`, `ProgressionTests`, `SimulationTests` | Unit suites that run on embedded YAML, so they test the code and not the data. |
| `DataTests` | Loads the generated documents and asserts they are a playable universe. |

Inside the core:

- `Core/SimulationConfig.h` - loads `ages.yaml`, `categories.yaml` and a game mode
  (items and skills). YAML nulls are "not set", never the string `"null"`; every field
  that is missing, null, mistyped or duplicated is a load error naming the entry, for
  example `skills[2].requires[0]`.
- `Core/Universe.h` - loads the solar system name pools (`latin`, `greek`, ...).
- `Core/ProgressionManager.h` - the skill tree: tiers capped by `maxTiers`,
  prerequisites, exclusive groups that lock out a rival branch, a resource pool to pay
  for a tier. `Build` validates the graph, including cycles and prerequisites that can
  never be reached.
- `Core/Simulation.h` - generates worlds from the ages, categories, items and name
  pools; gives each one an age, a population, specialities, production, demand and a
  stockpile; prices goods by scarcity, market-wide and per world; plans trades that pay
  for their freight; advances a tick of production, consumption and trade. A world's age
  bounds the goods it can make, and the goods of the essential categories - the water,
  food and fuel a population runs on, the medicines and the knowledge goods - are wanted
  at every age, so a world that cannot make medicine is a buyer of it.

The generation is deterministic for a seed, and the model uses only the fields the
merged documents carry, so it runs on the data as it is.

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
- Git and network access at configure time: CMake fetches yaml-cpp 0.9.0 and FTXUI
  7.0.3.

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

`cmake --build build` runs the `CivSimData` target, which is `data/build.sh`: it merges
the source YAML into `data/core-config`, so a clean clone builds and tests with no
manual data step. The five suites then run: the four unit suites on embedded YAML, and
`DataTests` against the generated documents.

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
./build/CivSimCoreTui
```

It loads `data/core-config` on startup, so the first command can be `generate 8 7`.

| Command | What it does |
| --- | --- |
| `status` | Configuration, universe and skill tree state. |
| `load [dir]` | Reload the ages, categories, full game mode and name pools. |
| `generate [n] [seed]` | Generate `n` worlds (default 8, seed 1). |
| `list` | Every world: name, pool, age, population, goods, treasury. |
| `show <Civ-01>` | One world in detail: specialities, production, shortages. |
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
day, and the age sets the ceiling, so a world of `AgeSublimation` dwarfs a world of
`AgeStone`.

## 4. Core API

```cpp
#include "Core/ProgressionManager.h"
#include "Core/Simulation.h"
#include "Core/SimulationConfig.h"
#include "Core/Universe.h"

CivSimCore::Ages ages;
CivSimCore::Categories categories;
CivSimCore::GameModeNode gameMode;
CivSimCore::UniverseNamePools names;
std::string error;

if (!CivSimCore::LoadConfigAges("data/core-config/ages.yaml", ages, error) ||
    !CivSimCore::LoadConfigCategories("data/core-config/categories.yaml", categories, error) ||
    !CivSimCore::LoadConfigGameMode("data/core-config/full.yaml", gameMode, error) ||
    !CivSimCore::LoadUniverseNamePools("data/universe/solar-system-names.yaml", names, error))
{
    // The message names the file, the entry and the field.
}

CivSimCore::GenerateOptions options;
options.civilisationCount = 8;
options.seed = 7;

CivSimCore::Simulation simulation;
if (!simulation.Generate(ages, categories, gameMode.items, names, options, error))
{
    // Handle error.
}

// The trade the player is looking for: buy where the good is cheap, sell where it is
// scarce, and the route has to pay for its own freight.
for (const CivSimCore::TradeRoute &route : simulation.PlanTrade(10))
{
    // route.itemId, route.sellerId, route.buyerId, route.quantity, route.profit
}

simulation.AdvanceTick(error);

CivSimCore::ProgressionManager progression;
progression.SetResources(20);

if (!progression.Build(gameMode.skills, error))
{
    // A prerequisite is unknown, the graph has a cycle, a tier is out of reach, or a
    // skill requires a rival of its own exclusive group.
}

progression.TryUpgrade("SkillSmelting", error);
```

Each loader replaces its output container, so loading the same file twice into the same
vector does not duplicate entries, and a failed load leaves the container empty.

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

`cmake --build build` runs the same script through its `CivSimData` target, so you only
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
| `base/ages.yaml` | The ages. |
| `trade-items/categories.yaml` | The categories. |
| `trade-items/chemicals.yaml` | Chemicals, fuels, medicines, pharmaceuticals, plastics, dyes, fertilisers, pesticides, drugs, explosives, paints, adhesives, solvents, lubricants, industrial gases, hybrid materials, nuclear materials. |
| `trade-items/metals.yaml` | Metals and metal goods: iron, copper, bronze, tin, lead, zinc, aluminium, titanium, nickel, chromium, tungsten, cobalt, silver, gold, platinum, machinery, tools, weapons, jewellery, structural materials. |
| `trade-items/minerals.yaml` | Coal, crude oil, sand, gravel, limestone, iron ore, copper ore, bauxite, gypsum, salt, phosphorus, potassium, aggregates. |
| `trade-items/misc.yaml` | The remaining categories: water, consumables, textiles, timber, precious stones, information, writing, medical equipment. |
| `trade-items/seeds.yaml` | Arable, pastoral, fish and tree seeds. |
| `skill-tree/skills.yaml` | The crafts of the economy. |
| `skill-tree/ages.yaml` | The ages that gate the tree. |
| `skill-tree/spaceship.yaml` | The ship systems, as a skill tree of their own. |
| `skill-tree/shipyards.yaml` | The yards that build and mend. |
| `skill-tree/lanes.yaml` | The lanes and the formations that run them. |
| `skill-tree/survey.yaml` | The survey and the claims it stakes. |
| `skill-tree/habitats.yaml` | Habitats and terraforming. |
| `skill-tree/piracy.yaml` | The pirate arts. |
| `skill-tree/law.yaml` | The warrant, the pardon, the bench, the standing. |
| `skill-tree/trade.yaml` | Docking licences, cargo insurance, the fence. |
| `skill-tree/services.yaml` | Public medicine and the education system. |
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
drug and `CategoryPharmaceuticals` the shelf it lands on, `SkillTextiles` the loom and
`CategoryTextiles` the bale, `ItemElectronics` the good and `CategoryElectronics` its
sort - and the prefix is what keeps the two ids apart. The ages drop the old `Age`
postfix for the prefix, so no id ends with the word it starts with. `DataTests` asserts
the prefixes, and that no name is an age, a category, a good and a node in turn.

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

`universe/solar-system-names.yaml` holds the name pools for the generated worlds. It is
UTF-8, it is loaded by `Core/Universe.h`, and it is not part of the merge: `build.sh`
does not read it.

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
| `skill-tree/spaceship.yaml` | 25 ship systems of the long-haul trader | `nodeType: Skill`, `maxTiers: 3`, cost 3 for the drive, the laser mounts, the computers, the life support, the coating, the countermeasures and the repair party, 2 for the shield banks, the gun batteries, the thrusters, the arrays, the holds, the bunkers, the salvage rig, the emissions and the spares, no exclusive groups. |
| `skill-tree/shipyards.yaml` | 4 yard nodes: the drydock, the outfitting line, the modular hull, the refit dock | `maxTiers: 3`, cost 2 to 4. The hulls the ship systems are fitted to. The modular hull asks the Information Age gate as well as the drydock. |
| `skill-tree/lanes.yaml` | 4 lane nodes: convoy, escort wing, route ward, warden patrol | `maxTiers: 3`, cost 2 to 3. The formations a route is run with, and the warden's art is bought on top of the escort that escorts it. |
| `skill-tree/survey.yaml` | 4 survey nodes: the deep survey, the mining claim, the claim refinery, the exclusive claim | `maxTiers: 3`, cost 2 to 3. A claim asks the survey that found the ground *and* the Mining craft that works it; the refinery asks the Smelting craft. |
| `skill-tree/habitats.yaml` | 4 habitat nodes: the domes, the closed loop, the atmosphere, the biosphere | `maxTiers: 3`, cost 3 to 4. The domes ask the Space Colonisation Age gate; the biosphere asks the Agriculture craft. |
| `skill-tree/piracy.yaml` | 4 pirate nodes: raider hulls, boarding parties, ransom exchange, safe harbours | `maxTiers: 3`, cost 2. The technology of the raider, from the hull that runs a trader down to the fence that lands the cargo. |
| `skill-tree/law.yaml` | 4 law nodes: the admiralty warrant, the pardon, the tribunal, the standing among the crews | `maxTiers: 3`, cost 2 to 3. The warrant and the crews' standing are in the exclusive group `CleanStanding`. |
| `skill-tree/trade.yaml` | 3 trade-service nodes: the docking licence, cargo insurance, the fencing network | `maxTiers: 3`, cost 1 to 2. Insurance asks the licence *and* the tribunal; the fence is the third member of `CleanStanding`. |
| `skill-tree/services.yaml` | 2 public nodes: the medicine service, the education system | `maxTiers: 3`, cost 2. The demand side: medicine and education are demand, and these are the services a civilisation runs to meet it. |

An age gate carries the `ageId` of its age as its `skillId`: `AgeInformation` is the age
and the skill node that opens computers, calculators, software and networks. The gate
and the age are the same idea seen from the two ends, so a world generated in an age
finds its gate by a lookup on the age, and no second naming scheme has to be kept in
step. The grid puts the ladder on its own row at `gridY: -1.0`, in age order from left
to right, with the ship systems on the row at `gridY: 1.0`.

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

Every system hangs off the one that has to be fitted first: each aft mount on its forward
twin, the jets on the drive, the attitude control on the jets, the fire control and
commercial arrays on the long range mast. The ship has one power feed and one dish, and
the tree says so. Attitude control is the pointing that decides which arcs are in the
fight, and fire control is the lock that fills them, so the propulsion line and the mast
run into the guns and the screens. The links repeat that grammar down the hull: emissions
control asks the coating, because a plate that hides a hull has to be run cold;
countermeasures ask the side screen, because a burst is bought to cover an arc; the
salvage rig asks the drive and the mast, because a tow is a propulsion job and a survey
one; the spares ask the repair party; and the life support asks the crew and the side
screen, because a breach is a damage-control problem in an arc no screen covers. No ship
system is in an exclusive group, because a trader who can afford a forward and an aft
screen fits both.

The other eight trees are the ones a civilisation stands on, and they are where the ship
meets the ground. The yards at `gridY: 2.0` build and mend the hulls the systems are
fitted to; the lanes beside them run the routes the ship sails; the survey and the claims
it stakes ask a craft as well as a sensor, so a mining claim needs the deep survey *and*
the Mining craft, and the refinery that pays for the claim needs the Smelting craft. The
habitats at `gridY: 3.0` start at the Space Colonisation Age gate, because a dome is the
technology that age is named for, and the biosphere seeded on top of the weather asks the
Agriculture craft. Below the age ladder, at `gridY: -2.0`, run the pirate arts, the law,
the
trade services and the public services: the raider hull, the boarding party, the ransom
and the harbour; the warrant, the pardon, the bench and the standing among the crews; the
docking licence, the cargo premium and the fence; the clinic and the school. Cargo
insurance asks the licence that registers a ship *and* the bench that prices a cargo, and
the fence asks the harbour that lands it: prerequisites that cross the rows are the point
of the shape, and `DataTests` counts them, because a tree that stops linking is a set of
lists.

`CleanStanding` is the one exclusive group in the shipped data, and it is the reason the
field exists: an admiralty warrant, a standing among the crews and a fence are one
reputation spent three ways, so buying the warrant shuts the crews' tongue and the fence
for good. `ProgressionManager::Validate` rejects a member that requires a rival of its
own group, which is why the warrant and the standing are roots; the fence may ask a
harbour, because a harbour is not a rival.

The full tree is 85 nodes and 227 tiers, and `game-mode/simple.yaml` selects the same
skills as `game-mode/full.yaml`: the items are what make the small game small, and a
short tree would leave the gates, the ship and the standing trees out of the quick
matches they are meant to test. `DataTests` asserts the scale of it - 71 crafts and ship
systems three tiers deep, 14 gates - that every gate is one tier and sits on the gate of
the age before, in age order, that the 25 ship systems and the 29 nodes of the standing
trees exist with the effects and the costs they claim, that `CleanStanding` locks a chosen
rival out, and that `ProgressionManager::Build` accepts the whole graph. The three-tier
floor is asserted on the `Skill` nodes and not on the gates: a merge that dropped
`maxTiers` lands on the loader default of one tier, which flattens a craft to a single
click, and that is what the assertion is there to catch.

Not in the tree, and not to be built without a decision on each: crew wages and morale;
missions and contracts; fleets and task forces; price policy and market manipulation;
research that takes time; population growth and migration; plague, wrecks and the other
events that make a haul interesting; patrol and enforcement; government per world; the
jump gate as infrastructure; and the refining of fuel as a chain of supply. The item data
has a gap of the same shape: `AgeSpace` and `AgeAI` ask for goods, and the merged full
game mode has no item carrying those ages, because the item files open those sections and
leave them empty. An age gate that opens no good is a gate with nothing behind it, and
the production gate makes the absence show.

## Units, prices and storage

Every item carries five numeric fields. They describe the good as a physical object, so
that a future cargo model can price freight by volume and mass; the current economy
trades on `basePrice` alone and does not read the other four. The storage cube is the
IBC cube, the one cubic metre intermediate bulk container, and the fields are defined
against it:

| Field | Meaning |
| --- | --- |
| `density` | Specific gravity of the material against water at sea level, so `1.0` is 1000 kg/m³. The solid value at 20 °C for a metal that is liquid at that temperature. |
| `unitType` | The unit `basePrice` and `unitsPerCubicMetre` count: `Kilogram`, `Gram`, `Litre`, `Kilolitre`, `Unit`, `Sheet`, `SquareMetre`. |
| `unitsPerCubicMetre` | Units of loose product filling one cubic metre of the material: see the table below. |
| `packFactor` | The fraction of a cube the product fills, counting packaging, dunnage and the voids between pieces. A hollow blown flask is mostly air, so it is `0.05`; stacked ingots are `0.85`. |
| `basePrice` | Market value per `unitType`, in credits at one credit to one US dollar, at 2025 spot and wholesale prices. Prices drive the model through the ratio of market price to base price, so the currency anchor only sets the relative level of the goods against each other. |

| `unitType` | `unitsPerCubicMetre` | |
| --- | --- | --- |
| `Kilolitre` | `1` | A kilolitre is exactly a cubic metre, so bulk volume goods count one to the cube. |
| `Litre` | `1000` | A cubic metre is a thousand litres. |
| `Kilogram` | `density x 1000` | The mass of solid material in a cubic metre: copper at 8.96 is 8960. |
| `Gram` | `density x 1000000` | The same mass in grams: gems at 3.5 are 3500000. |
| `Unit` | tight-nested piece count | How many of the piece nest into a cubic metre: 2000 books, 40000 pieces of jewellery. |
| `Sheet` | tight-nested sheet count | Sheets stacked to a metre with their interleaves. |
| `SquareMetre` | `1 / thickness` | Sheet goods sold by area: a millimetre of graphite foil is 1000 m² to the cube. |

For the goods counted by piece, sheet or square metre the count is a packed count, so
it fixes the mass of the piece, and the figures agree through a mass identity that is
the check to apply when a new item is added:

```
density x 1000 x packFactor  ==  kilograms in an IBC cube  ==  unitsPerCubicMetre x piece mass
```

A jeweller's piece of 15 grams of 12-specific-gravity metal takes 1.25 cm³ of metal in a
25 cm³ fitted tray, so 40000 to the cube, a packFactor of 0.05 and 600 kilograms of
metal in the cube on both sides of the identity. Goods counted by mass or by volume
state the solid figure in `unitsPerCubicMetre`, and only `density x 1000 x packFactor`
holds for them: an IBC cube of stacked copper ingots is 8.96 x 1000 x 0.85 kilograms,
not 8960.

Wheat seeds show the split the fields make: the density of a wheat kernel is 1.27, its
test weight is 78 kg per hectolitre, so the packFactor is 0.61, the mass of a full cube
is 775 kg, and 350 credits buys that cubic metre of certified seed. The bulk minerals
and chemicals sit in the same shape: a kilolitre is the heap, and the air between the
grains is most of the volume for a floured material like lime at 0.31.

Prices are the current market figure for the traded form of the good, not for a purer
or rarer form of it: uranium is the 2025 spot price of `U3O8` concentrate, the
pharmaceuticals are bulk active-ingredient prices per kilogram, and the precious metals
are per kilogram rather than the per-troy-ounce quote the market uses, so gold is
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
- yaml-cpp needs C++ exceptions, and the core is built with them.
- The sample uses C++20.
- The economy is a development harness: a small, deterministic supply, demand and price
  model that is meant to be reasoned about, not tuned. Population, treasury and freight
  are unitless model quantities.
- `data/core-config` is generated output: edit the source YAML under `data/` and rebuild.

## AI Disclosure

This project is being made with the assistance of several AI models:

- Qwen 3.8 125B - Swift IQ3 - KV 8,4 (local AI)
- Chat GPT (scaffold)
- Claude (core simulation, progression, tests, CI)

AI will be utilised in research, ideation, writing specifications, requirements gathering and code production.

I have 45 years of development experience from micro-controllers through to security and devops for highly available high traffic websites. 

At this point in time it would be utterly foolish to create software without support from AI models. In the hands of a skilled and experienced developer it is an incredible force multiplier.

## Marketplace recovery work in progress

Crash checkpoints `39a5a15` and `c52da5f` ("Crashed while adding new ECS parts",
deliberately broken builds) added EnTT, the ECS layer, the effects engines and the
marketplace with their tests. `35e3812` repaired the build: the build was green, 7 of the
8 suites passed. The recovery lands as commits on top of the two checkpoints, which are
left as they are; the checkpoint commits are never amended.

### State at the time of writing

| Test | Status |
| --- | --- |
| ConfigTests, UniverseTests, ProgressionTests, SimulationTests, EcsTests, EffectTests, DataTests | passing, as they have been since `35e3812`: 14 cases 97 checks, 8 and 26, 14 and 104, 12 and 728, 9 and 83, 8 and 933, and the data suite on the merged documents |
| MarketplaceTests | **green**: 12 cases, `1515 checks, 0 failures`, `exit=0`. No segfault and no failing check: the suite's last two failures, both of them in `the_marketplace_reads_the_tree_of_the_civilisation_that_owns_the_venue`, went in the venue-tree fix, committed at `9c6a50d` |
| `ctest` | **8 of 8 targets**, `100% tests passed` |
| `clang-format --dry-run -Werror` | **exit 0**: the 2266 drift lines the checkpoints and the recovery carried are formatted in the working tree, awaiting a commit of its own; the build and all 8 suites are green with the reformat in, on the same check counts |

The ECS, effects and marketplace code is committed on the branch at `35e3812`, which
repaired the broken build of the checkpoints `39a5a15` and `c52da5f`. The recovery lands
on top of it as commits, and the checkpoints are left as they are: `f24e0c3` ("Adding
python3 to the environment", python 3.12.15 in the devenv, 51 lines of
`Tests/MarketplaceTests.cpp`) carries the venue-charge case, `e109de5` the order-life case
with `Marketplace.h`, `Marketplace.cpp` and `Components.h`, and `3855390` the fill case
that ends the segfault. The venue-tree case - the last two checks of the suite - is
repaired at `9c6a50d` ("A civilisation's read-out names the effects it flies, and a control
venue is opened before it is read"), 121 lines inserted and 19 removed across
`Source/CivSimCore/Private/Effects/CivilisationEffects.cpp`,
`Source/CivSimCore/Public/Effects/CivilisationEffects.h` and `Tests/MarketplaceTests.cpp`.
Files of the
feature: `Source/CivSimCore/Private/{Ecs/EcsWorld,Effects/CivilisationEffects,Effects/ShipEffectEngine,Market/Marketplace}.cpp`,
the matching `Public/` headers, `Tests/EffectTests.cpp`, `Tests/MarketplaceTests.cpp`.

The line numbers the checklist quotes are as of `35e3812`, and the numbers a checked item
quotes - `1006`, `1160`, `1413`, `1476`, `1573`, `1745` - are the line of the check that
fired on the day it was measured, so they are the history of the case and not the address
of it today. The cases now start at 1173, 1329, 1451, 1611 and 1783, and
`Tests/MarketplaceTests.cpp` is 1905 lines: 21 lines from the venue-charge fix, 33 from the
order-life case, 34 from the fill fix (the `SetCredits` helper and the ledger comments the
moved lines need) and 38 from the venue-tree case (the control venue and the read-out
asserts). The venue-tree fix adds 54 lines to `CivilisationEffects.cpp` and 10 to its
header, the store read-out.

Commands used throughout:

```sh
# build + the marketplace suite in detail (stdbuf is needed: stdout is lost on a segfault)
cd build && cmake --build . 2>&1 | grep -E "warning:|error" | head
./MarketplaceTests >/dev/null 2>&1; echo "exit=$?"      # 139 = segfault
stdbuf -o0 -e0 ./MarketplaceTests 2>&1 | grep -E "FAIL|checks,|cases" | head -20

# the whole suite, then the pre-merge format gate from .config/wt.toml
ctest --test-dir build --output-on-failure
find Source Tests \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 \
  | xargs -0 -r clang-format --dry-run -Werror

# one case in a file of its own, printing the numbers a fill works with - how the causes of
# the failures in the list below were found, where a suite prints a check and dies
 clang++ -std=c++20 -g -O1 -w -DYAML_CPP_STATIC_DEFINE -ISource/CivSimCore/Public \
   -Ibuild/_deps/entt-src/src -Ibuild/_deps/yaml-cpp-src/include /tmp/probe.cpp \
   build/libCivSimCore.a build/_deps/yaml-cpp-build/libyaml-cppd.a -o /tmp/probe && /tmp/probe
# a probe that prints inside the library runs in every case of every suite, and a print in a
# loop that keeps the loop alive writes for ever: copy the file before instrumenting it,
# restore it after, and grep the tree for the marker before building the suites
```

Rules already implemented and validated in this session (do not re-litigate them, they
are the reason the tests read the way they do now):

- ECS: id numbers are spent facts. `Create` spends numeric suffixes of ids given by
  name; `NextId` raises the global counter to the per-prefix high-water mark. `Clear`
  leaves numbers spent, so a fill log can never quote two orders by one number.
- Progression walks *set* the caller's `Applied`/`Skipped`; a fleet walk is the sum of
  its hull walks. `tier <= 0` counts in neither column for the civilisation engine.
- Ship engine: Skill effects are fitted systems, Technology effects are sources only;
  `UnfitEffect` of a system the hull never carried is a successful no-op.
- Marketplace: seller goods are reserved into the order at post, buyer credits are
  reserved at post at the limit price plus fee; a fill spends the order's own pocket
  first and the treasury only pays the remainder, so no buyer is charged twice.
- A cancel cannot create a trade: the book's invariant is best bid under best ask, so
  taking the top bid away moves it further from the ask. A cancel is settled money.
- `CancelOrder` is idempotent for an order that is gone (expired, cancelled, never
  posted) and refuses an order named in a venue's fill log, because that cargo already
  left the stockpile. An empty id is a caller error.
- Quote rows persist: a good that has entered the quote table never leaves it.
  `askOrders`/`bidOrders` count orders *at the best price level* (`CountAtLevel`), not
  the whole side.
- Venue deck (`DepthUnits`) = `options.depthUnits` plus the market store's
  `MarketDepth` effect; 0 means no cap.
- Fees: base listing fee plus venue risk; fee per side, rounded up to whole credits.
  Test worlds with no effects have risk premium 0, so 40 bps on gross 100 is 1 credit.
- Venue charges are three sums and two clamps, all of them read off the market store of
  the venue and the lane store of its owner: `RiskPremiumBps` = the summed
  `MarketRiskPremium` of the venue + the `RouteRiskPremium` of `LaneStoreId(owner)`;
  `ListingFeeBps` = base + risk - `MarketClearance`/2, clamped at 0;
  `InsuranceBps` = base + `MarketInsurance`, clamped at 0. `MarketFenceShare` is a
  fraction of a cargo and prices no charge of the venue at all; the premium line clamps
  at nothing and the risk line does not.
- Order life is **24 ticks, the berths a day needs**, in both `MarketplaceOptions` and
  the `MarketplaceInfo` default. The life an order gets is the life the *service posting
  it* carries at the tick the order lands, not the life of the venue record: `Open`
  writes the venue's own record from the options of that moment, and a test that
  `SetOptions` between a post and a reopen gets the life it set. The life is added plain
  (`expiresTick = tick + orderLifeTicks`), so a service with no life writes
  `expiresTick == placedTick`, and `AdvanceTick` reads the expiry against `placedTick`:
  an expiry that does not run past the posting is the shape of "never". An order posted
  at a venue with no life rests until it trades, is cancelled, or the venue closes.
- `AdvanceTick` sweeps expiries and rebuilds quotes, and **it trades nothing**: a tick
  retires orders, refunds reservations in full, refreshes quotes, and returns the venues
  ticked. The book is kept crossed down by the posts that cross it, not by the tick.
- A resting buy order reserves the goods at the limit price **plus the fee of its own
  side** - the maker side - and never the taker's: the venue has not seen a trade, and a
  reservation that priced both sides would charge a resting trader for a fill it may
  never be in. 40 bps of 200 credits is 0.8 of a credit, rounded up to 1.
- A `Market`-domain effect can only land on a venue that is **open**: `ResolveStores`
  refuses a civilisation with no venue of its own. Buy the effect while the venue is
  open; `Close` leaves `MarketEffects` on the civilisation and removes `MarketRecord`
  and `MarketplaceInfo`, and the venue that reopens reads the tree of its world.
- `Close` returns what the book **holds**: the units a fill traded away are in the fill
  log and are not in an order to be given back.
- A venue's base charges are read **with the venue**: `RiskPremiumBps`, `InsuranceBps` and
  `DepthUnits` answer 0 for a body with no open venue - no venue record, so no risk, no
  insurance, no deck - and `ListingFeeBps` answers the service's base, because the fee a
  trader pays is the fee of the venue the order posts at. A test that reads a civilisation
  as the control of the charges has to **open its venue first**: Civ-02 in the venue-tree
  case is a venue with a record, a civilisation that buys no tier, and the base charges 40,
  120 and 0.
- `DescribeCivilisation` prints, for every store the civilisation owns, the effects the
  store flies - the name the catalogue gives them and the tier the tree bought, in effect-id
  order, which is the order the modifiers sum in - and the stats they sum to after them, in
  the stat-table order, in brackets: `Market Civ-01: Cargo Insurance x1, Docking Licence x1
  (Market Risk -15 bps, Docking Clearance 20 berths/day, Cargo Insurance -40 bps)`. The
  words of the catalogue, never an effect id, and never an enum spelling; a store that
  flies no effect prints the `no ... effects bought` line it has always printed, and a
  civilisation that owns no store prints nothing at all. `DescribeStore`, private to the
  engine, is the one helper of all five store lines, and it reads the store's sources with
  the `SourcesOn` that writes the store's cache, so the read-out and the cache are the same
  fact read the same way, and a panel that shows the tree twice shows the same lines.

### Remaining work

- [x] `a_market_effect_moves_the_charge_of_the_venue_it_lands_on`
  (`Tests/MarketplaceTests.cpp:1006`) passes. The diagnosis in this list was wrong, and
  the test moved, not the code:
  - `MarketFenceShare` never enters `RiskPremiumBps`: `MarketplaceService` prices no
    bps for the share, and never did. The 50 bps the case was missing is
    `CargoInsurance`, which carries `MarketInsurance -40` **and** `MarketRiskPremium
    -10` a tier: the case had bought 5 tiers, so -50 sat in the risk of the place, and
    the fence expectations (`-92+24`, `-68-24`, `-92`) were written before the policy
    block and never carried that -50 forward. The insurance read, the premium clamp and
    the ship/yard refusals were all passing and still do.
  - The rule the case now asserts: **the risk of the place is a plain sum of the signed
    market terms** - licence, bench, harbours, policy, fence - **plus the risk of the
    lane of the owner**. The premium line clamps at nothing and the risk line does not:
    a policy that covers the whole premium leaves the venue charging nothing for
    insurance and still the safer venue to trade at, so the fee keeps the policy's risk
    credit. It is the same sum `EffectTests` asserts (`3*-5 + 2*-10 + 4*-15`), the same
    rule `the_marketplace_reads_the_tree_of_...` reads, and it is why no term is
    special-cased in the code.
  - The case reads the effects in the order they are applied, so the fence (+3 * 8) and
    the convoy (-2 * 12) separate on their own lines, and the fence share is asserted
    as a fraction of the cargo (`3 * 0.15`), not as a charge.
  - Every `ReadStat` read in the case is now `CHECK_NEAR(..., 1e-9)`, as in
    `EffectTests`: `3 * 0.15` is not exactly 0.45 in a `double`, and the accumulation
    path is `double` end to end (`EffectModifier::perTier`, the stat map, the store), so
    there is no `float` to find.
- [x] `an_order_that_outlives_its_life_leaves_the_book` (`Tests/MarketplaceTests.cpp:1160`)
  passes. The code moved, and so did the arithmetic of the blocks of the case that had
  never run:
  - The order life is **24 ticks, the berths a day needs**: `MarketplaceOptions.orderLifeTicks{24}`
    and the `MarketplaceInfo` default in `Ecs/Components.h` with it, so the order posted
    at tick 10 leaves at tick 34. The only hard-coded 20 in the passing tests was
    `a_marketplace_sits_at_a_planet_and_at_a_station_...:148`, `venue->orderLifeTicks`,
    now 24; `the_book_...` and `the_deck_...` clear their books with `CancelOrder` and
    never read an expiry, so nothing else assumed 20.
  - The life an order gets is the life of **the service that posts it**, not the life of
    the venue record: the case calls `SetOptions(noLife)` after `Open`, `SetOptions`
    carries no world to write, and the order that lands next has to carry the life that
    was just set. `CreateOrder` adds the life plain,
    `expiresTick = Order.tick + options.orderLifeTicks`, so the order posted at tick 40
    under a service that sets no life carries `expiresTick == 40`, which is the number
    the case reads.
  - `AdvanceTick`'s expiry test is now `expiresTick > placedTick && expiresTick <= Tick`:
    an expiry that does not run past the posting is a life of nothing, and the sweep
    passes it. The case names that "a venue with no order life keeps its book", and it is
    a rule about the order, so an order posted with no life stays, at a venue that has
    since taken up a life.
  - 798 vs 799: **the code was right and the test moved.** 40 bps of 200 credits is 0.8
    of a credit, the venue rounds up to the whole credit it is owed, the pocket of the
    resting buy is 201, and Civ-02 sits at 799. The test's 2 credits is a maker fee and a
    taker fee on one order: a resting order is a maker and reserves its own side, the
    rule recorded above. The same shape in the immortal order of the case: 40 bps of 50 is
    0.2, the venue is owed 1, the pocket is 51, Civ-02 sits at 949.
  - The blocks that had never run were written against a 20-tick life, a book expected to
    hold an order past its expiry, and fills that never happened. They read from the rules
    now: `OpenOrderCount("Civ-01") == 1` at tick 1000, the 24-tick sell having left at
    tick 36 and the immortal being the one order the book keeps; `Credits("Civ-01") ==
    1000` at tick 100, because a tick trades nothing and the 81 the Civ-02 venue held for
    the buy of Civ-01 came back whole when it expired at tick 74; and a close block that
    reads a fill the crossing post makes, the buy at 60.0 meeting the ask resting at 40.0
    and trading 10 units at the maker's price at a venue the warrant has made free. The
    book the close returns is the book the fill left: Civ-01 ends with 90 units and Civ-02
    with 600 credits, and the case says so with the arithmetic in the comments.
  - The case buys the four tiers of the warrant (`MarketClearance 20`, `MarketRiskPremium
    -60`, fee 40 - 60 - 10 clamped to 0) **while the venue is open**, because a `Market`
    effect lands on a venue and `ResolveStores` refuses a civilisation with none, and it
    asserts that `Close` leaves `MarketEffects` where the tree put it, takes
    `MarketRecord` with it, and leaves the reopened venue reading its world's tree: base
    40 bps, charge 0.
- [x] Cases after line 1329 that the crashes left unreached: all four of them are green,
  and the one that failed was the first.
  - [x] `the_marketplace_reads_the_tree_of_the_civilisation_that_owns_the_venue` (1329)
    passes, and the two checks had two causes, not the one cause the list guessed.
    - **The insurance is a charge of an open venue, and the case's control had no venue.**
      `InsuranceBps` reads its base with the venue record - `venue != nullptr ?
      options.insuranceBps : 0` - and the passing cases already assert that rule at the
      end of it: `InsuranceBps(world, "Civ-99") == 0`, at 1016 and at 1167, a body with no
      venue having no venue insurance to read. Civ-02 is the control of the case, "the
      civilisation that owns a venue and bought nothing", and the case never opened its
      venue, so the control read 0 where the case wrote 120. The test moves, as the hint
      said: `service.Open(world, "Civ-02", ...)` goes in beside the open of Civ-01 and
      before the tree is applied, and a `Market` effect lands on the venues of the
      civilisation that owns them, so the tree of Civ-01 cannot reach Civ-02's venue and
      the control keeps the base charges the case reads: fee 40, insurance 120, risk 0,
      `MarketInsurance` 0.0. The insurance read, the fee, the risk, the deck and the quote
      of Civ-01 are the sums the venue-charge case reads, and they read as written.
    - **The read-out named the stats and not the effects, and the case was right.** The
      header of the engine says `DescribeCivilisation` prints "the effects a civilisation
      has bought ... one per store it owns", and `EffectTests` says the read-out "names the
      stores, the effects and the tiers, in words, in a fixed order, and prints no enum
      spelling"; the code printed the store id and the summed stats and named no effect, so
      `CargoInsurance` was not in the string. The code moves: `DescribeStore`, private and
      new, reads a store's sources with the `SourcesOn` the store's cache is written from
      and prints each effect by the name the catalogue gives it and the tier the tree
      bought, then the summed stats in brackets, and the five store lines of
      `DescribeCivilisation` call it:

      ```
      Market Civ-01: Cargo Insurance x1, Docking Licence x1 (Market Risk -15 bps, Docking Clearance 20 berths/day, Cargo Insurance -40 bps)
      Lanes Lanes-Civ-01: Convoy x1 (Route Risk -12 bps)
      Yards Yard-Civ-01: Drydock x1 (Drydock Throughput 1 hull/tick)
      Body Civ-01: no body effects bought
      ```

      The effects print in effect-id order, which is the order `WriteStore` sums the
      modifiers in, so the panel is the same panel twice; the words of the catalogue and not
      the id a `targetEffectID` carries, because the read-out is for a person; and a source
      of an effect the catalogue has forgotten prints the id, because the store flies it and
      the read-out prints what the store flies. A store with no source keeps the message it
      printed before, and `EffectTests` passes on the words it asserts, with no new assert
      needed: `Drydock` and `Docking Clearance` are in the lines as they were, and the
      read-out prints no enum.
    - The case now asserts the read-out as the read-out is: `Cargo Insurance x1` and
      `Docking Licence x1` on the market line, `Convoy x1` on the lane the goods come down
      and `Drydock x1` in the yard store, the two stat names, the three store ids, and the
      absence of the effect id `CargoInsurance`, of `EffectStat` and of `EffectDomain` - the
      shape `EffectTests` pins. The control is asserted too, because a venue that buys
      nothing flies no effect and says so, and `DescribeCivilisation(world, "Civ-99")` is
      the empty string it has always been.
    - **No line of `Marketplace.cpp` moved for this case.** The fill block reads Civ-01's
      venue and Civ-02's venue is a venue of a civilisation with no tree, so the fill keeps
      the case's arithmetic: gross 100, 1 credit a side, `venueTake` 2, Civ-01 at
      `1000 + 100 - 1` and Civ-02 at `1000 - 100 - 1`, `InsuranceBps(world, "Civ-01")` 80
      with one tier of insurance on the venue, and `OpenOrderCount` of the fill's pair
      unchanged.
  - [x] `a_fill_trades_in_whole_units_of_the_price_and_the_fee` (`Tests/MarketplaceTests.cpp:1413`)
    passes, and the segfault goes with it: the suite reports `12 cases`, `1499 checks,
    2 failures` and `exit=1`, the 2 failures being the venue-tree case above. The cause the
    list found in `FillPair` is the whole of the case, and it opens the promises block and
    the water block after it.
    - The code moved where the diagnosis put it. In `FillPair` the two lookups go through
      `World.Find(buyerId)` and `World.Find(sellerId)` - `buyerTrader` and `sellerTrader`
      now, with `sellerEntity` and `buyerEntity` gone and the comment that says why an
      order answers null for a `Treasury` and a `Stockpile` - in the goods branch, the
      credits branch and the promises tail, as `SettleParty` and `RetireOrder` already do.
      A promises venue therefore draws the money of the buyer and the cargo of the seller
      from the traders the orders name.
    - The two `quantity = 0.0` `else` branches of those reads become a refusal with a
      message - *the sell order 'Order-1' names 'Civ-03', which carries no Stockpile to
      spend on a fill* - and the silent `return false` that left `ErrorMessage` untouched,
      which is how the probe had to read it, is gone. A world whose trader carries no
      component a fill settles in is a broken world and is answered as one; a trader that
      has run out of the money the price is paid in is not that, `AffordableQuantity`
      answers 0 for it, and the quantity test below still returns false in the book's own
      reading.
    - The fill's own numbers passed as written, exactly as the list said: 5.9 units
      (`59 / 10.0`) at 10.0, gross 59, 1 credit a side, `venueTake` 2, the buyer's 60
      credits paid to nothing, Civ-02 at `1000 + 59 - 1`.
    - Three ledger lines of the block that had never run read a pile the case never gave
      anyone, and they move; the fill's lines do not, and neither does the read of
      `fills[0]`, which is the case's assertion of the fill and is not guarded:
      - Civ-01 is the buyer, and `AddWorld` gave it 100 units of iron_ore, so the 5.9 the
        fill trades read 105.9. The buyer of this case buys and holds nothing: its cargo is
        `{}`, and the fill's 5.9 units is the whole of its pile.
      - Civ-02 is at 90.0 and not 94.1: the sell reserved its 10 units into the order at
        the post, the pile is 90, the order that rests holds the 4.1 it has not traded, and
        94.1 is the good counted across the two, which is what the comment now says.
      - `OpenOrderCount("Civ-01")` is 2 and the **buy** book is 1 row, not the 1 order and
        0 rows the case guessed: the fill spent the buyer's whole 60 credits on 5.9 units,
        each order of the pair keeps the 4.1 of the 10 it has left to trade, neither is
        fulfilled, and a tick trades nothing, so both rest until a post crosses them again.
    - The promises block and the water block run for the first time and their numbers are
      the case's own, to the credit: 4 units of grain at 10.0, gross 40, 1 credit a side,
      Civ-03 at `1000 + 40 - 1`, Civ-01 at `60 - 40 - 1` = 19, the promises seller's pile
      taken to nothing and its order resting with 96 units to sell, and the water sell of a
      seller who holds no water trading nothing at all, with the ask the quoter still reads
      at 10.0. The block reads Civ-01 at 60 credits, and the iron_ore fill had left it at
      nothing, so the case sets the treasury its numbers read, with a `SetCredits` helper
      beside `Credits` and the comment that says why: a scenario that follows a fill starts
      from the pile it names, not from the dregs the fill left behind.
    - The two round-number `AffordableQuantity` guesses move to the division, as the list
      said they would: **10.3** units at a budget of 104, and **3984060** units at a
      million, each paying its budget exactly (103 goods + 1 fee; 996015 goods + 3985 fee),
      which is the invariant the loop under them asserts: 8 budgets, 6 fee rates and 5
      prices, 240 walks, and none of them needed a reading. The 9.9 that passed is the
      same arithmetic.
  - [x] `a_marketplace_loads_the_world_the_generator_built` (`Tests/MarketplaceTests.cpp:1573`)
    and `the_same_posts_in_the_same_order_give_the_same_fills` (`Tests/MarketplaceTests.cpp:1745`)
    pass. They were never reached after the fill case died, and they needed nothing: the
    fill fix opened them and they were green the first time they ran.
  - [x] The suite's **segfault is located: it is the test's out-of-range read at the fill
    case, and the library is clean.** Built with AddressSanitizer alone, `MarketplaceTests`
    prints `MarketplaceTests: 12 cases`, runs, fails `Tests/MarketplaceTests.cpp:1476:
    fills.size() == 1u (0 vs 1)` and dies on the lines after it: `SEGV on unknown address
    0x0000000000c0`, `The signal is caused by a READ memory access`, `Hint: address points to
    the zero page`, with `rdi = 0xc0` - a field of a `TradeFill` read through a null `data()`,
    which is `fills[0]` on an empty log, the historic pattern, measured. No heap-buffer-overflow,
    no use-after-free, no leak report anywhere in the run. The crash therefore joins the fill
    case above and goes with its fix, and there is no memory bug of the library to hunt. The
    **measured diagnosis was the whole of it: with the fill made to happen the suite exits 1,
    not 139, and no guard was written for a read of a log the case asserts is not empty.** The
    recipe that measures it is worth keeping:

    ```sh
    clang++ -std=c++20 -g -O1 -fsanitize=address -fno-omit-frame-pointer -w -ITests \
      -ISource/CivSimCore/Public -Ibuild/_deps/entt-src/src -Ibuild/_deps/yaml-cpp-src/include \
      -DYAML_CPP_STATIC_DEFINE -o /tmp/mkt-asanonly Tests/MarketplaceTests.cpp \
      build/libCivSimCore.a build/_deps/yaml-cpp-build/libyaml-cppd.a
    stdbuf -o0 -e0 /tmp/mkt-asanonly   # the case names arrive as the cases run
    ```

    **AddressSanitizer alone, not the README's `address,undefined,leak` with
    `-fno-sanitize-recover=all`:** UBSan with no-recover stops the process at the known break of
    `TestFramework.h` - `Test::Test()` calls `registry()[nullptr]`, which binds a reference to
    null at static initialisation - and the report is all that arrives: the suite's stdout, in
    the pipe's buffer, is lost, and the crash you came to locate never runs. That framework
    break is in the list below; until it is repaired, ASan-alone with `stdbuf` is the route.
- [x] Re-ran the suites that are believed green, after the order-life change to
  `MarketplaceOptions`, `MarketplaceInfo`, `Marketplace.cpp` and
  `Tests/MarketplaceTests.cpp`: `EcsTests` 9 cases 83 checks, `EffectTests` 8 cases 933
  checks, and `ctest` 7 of 8 targets - the failure is the segfault of
  `MarketplaceTests`, and the other six suites are clean. Re-run them when the suite is
  green.
- [x] Re-ran every suite after the venue-tree fix, the format gate not run (it fails on the
  code the checkpoints committed, and it is the task below): `ctest` **8 of 8 targets**,
  `100% tests passed`. `ConfigTests` 14 cases 97 checks, `UniverseTests` 8 and 26,
  `ProgressionTests` 14 and 104, `SimulationTests` 12 and 728, `EcsTests` 9 and 83, and
  `EffectTests` 8 and 933 - its describe case reads the richer lines of the read-out and
  passes on the words it already asserts - `MarketplaceTests` 12 and 1515, `DataTests` on
  the merged documents. No suite segfaults and every suite prints 0 failures.
- [x] Ran the format gate last, after the code and test churn:
  `clang-format --dry-run -Werror` over `*.h`, `*.hpp`, `*.cpp` in `Source` and `Tests`.
  **The gate failed on every commit of the recovery**, on the code the checkpoints committed
  and on the fixes on top of them, and the measure is the diff of `clang-format` against
  the file, tab-indented, ColumnLimit 104:

  ```sh
  cd /home/ivan/projects/civilisation-simulation
  find Source Tests \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' \) -print0 \
    | xargs -0 -r -I{} sh -c 'diff "{}" <(clang-format "{}") | grep -c "^[<>]"'
  ```

  The tree was 34 files and **2266 drift lines** at `9c6a50d`, 2248 of them at
  `3855390`: 18 lines were the venue-tree fix, 14 of `CivilisationEffects.cpp` (253 to 267)
  and 4 of its header (38 to 42), and the 38 lines the case adds to
  `Tests/MarketplaceTests.cpp` are format-clean, so the test file is at 493 before and
  after. The drift is in 16 files: `Tests/EffectTests.cpp` 664, `Tests/MarketplaceTests.cpp`
  493, `Market/Marketplace.cpp` 395, `Effects/CivilisationEffects.cpp` 267,
  `Effects/ShipEffectEngine.cpp` 118, `Market/Marketplace.h` 68, `Effects/EffectCatalogue.cpp`
  63, `Effects/EffectTypes.cpp` 62, `Effects/CivilisationEffects.h` 42,
  `Effects/ShipEffectEngine.h` 28, `Ecs/Components.cpp` 27, `Tests/EcsTests.cpp` 19,
  `Effects/EffectTypes.h` 9, `Ecs/EcsWorld.cpp` 6, `Tests/TestFramework.h` 3 and
  `Ecs/EcsWorld.h` 2 - every number the same at `3855390` as in the working tree, except the
  engine's two. Every other file in the tree is clean, the five `.cpp` of `Core` and their
  headers among them, and `Source/CivSimCore/Public/Ecs/Components.h` with them.

  `clang-format -i` over the two trees - LLVM 21.1.8, the version `.clang-format` needs and
  the version CI pins, because `BinPackParameters: BinPack` is a string a formatter older
  than 20 reads as an invalid boolean - and **every one of the 34 files is at 0 drift, and
  the CI gate exits 0**. It touched the 16 files of the drift list, 1456 lines in and 800
  out, and left the 18 clean files alone. A reformat of this size is exactly the reason it
  goes in a commit of its own: in a commit with code in it, the recovery drowns in
  whitespace.

  **The reformat moves no token, and that is measured.** Strip the whitespace from a file
  and from its `HEAD` version and compare -
  `diff <(git show HEAD:"$f" | tr -d '[:space:]') <(tr -d '[:space:]' < "$f")` - and 13 of
  the 16 files are identical. In the three that differ - `CivilisationEffects.cpp`,
  `Marketplace.cpp`, `EcsWorld.h` - the delta a token walk reports is `Foo(const` splitting
  into `Foo(` and `const`, and `"a""b"` splitting into `"a" "b"`: the boundaries a
  whitespace strip glues, not edits. The one real change is `SortIncludes` in
  `Source/CivSimCore/Public/Ecs/EcsWorld.h`, which moves `#include <cstddef>` above
  `#include <entt/entity/registry.hpp>`: the header is self-sufficient, the build is green
  with it moved, and an include order a formatter enforces is an order that cannot drift.

  With the reformat in, the build is clean - no warning, no error - `ctest` is **8 of 8
  targets, `100% tests passed`**, and the counts are the counts of the checked list above:
  `ConfigTests` 97, `UniverseTests` 26, `ProgressionTests` 104, `SimulationTests` 728,
  `EcsTests` 83, `EffectTests` 933, `MarketplaceTests` 1515, `DataTests` on the merged
  documents, 0 failures in every suite. A format that moves a check count is a format that
  changed the code, and this one changed nothing. CI runs build,
  `ctest --test-dir build --output-on-failure` and format on `ubuntu-24.04`
  (`.github/workflows/ci.yml`, `.config/wt.toml`); a merge needs all three, and all three
  pass now.
- [x] Committed the recovery as new commits on top of the two checkpoints, which are
  deliberately broken, are left as they are, and are never amended: `f24e0c3` the
  venue-charge case, `e109de5` the order-life case, `3855390` the fill case that ends the
  segfault, and `9c6a50d` ("A civilisation's read-out names the effects it flies, and a
  control venue is opened before it is read") the venue-tree case - 121 lines inserted and
  19 removed across `Source/CivSimCore/Private/Effects/CivilisationEffects.cpp`,
  `Source/CivSimCore/Public/Effects/CivilisationEffects.h` and `Tests/MarketplaceTests.cpp`,
  the whole suite green on them. The format above is the commit that follows.
- [ ] Optional cleanups noted in passing, not required for green:
  - [ ] `python3` arrived at `f24e0c3` (3.12.15 in the devenv), so the absences in the notes
    below are history. `perl -0pi -e 's{...}{...}' file` stays the tool for exact multi-line
    replacement of tab-indented C++ string literals (`cat -A` shows the tabs); the `edit` tool
    matches fine when the tabs are copied exactly, and `edits` must be a real JSON array of
    objects.
  - [ ] `find build -name 'effect-catalogue.gen.h'` found nothing; confirm the generated
    effect-catalogue header path in `CMakeLists.txt` is what the tests include.
  - [ ] Delete scratch probes when done: `/tmp/walk-probe.cpp`, `/tmp/walk-probe2.cpp`,
    `/tmp/probe.cpp`, `/tmp/dbg/`, `/tmp/scratch/`. They are outside the repo.
  - [ ] `Tests/MarketplaceTests.cpp`'s `AddWorld` helper is now
    `AddWorld(world, civ, planet, credits, cargo)` (24 call sites); the old
    `service` parameter is gone - keep that shape.
