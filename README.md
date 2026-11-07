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
