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
  for their freight; advances a tick of production, consumption and trade.

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
  development shell pins: v4.54.2 scopes YAML anchors to a single document, which the
  shared anchors between the item files do not survive, and the data build fails. CMake
  warns when the `yq` on `PATH` is not the tested version. A missing `yq` with the tests
  enabled is a fatal configure error, rather than a suite that silently runs nothing:
  build the core without the tests with `-DBUILD_TESTING=OFF`.
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
checks it with `clang-format --dry-run -Werror`. Format your work with:

```bash
find Source Tests -name '*.h' -o -name '*.cpp' | xargs clang-format -i
```

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
day, and the age sets the ceiling, so a SublimationAge world dwarfs a StoneAge one.

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

progression.TryUpgrade("Smelting", error);
```

Each loader replaces its output container, so loading the same file twice into the same
vector does not duplicate entries, and a failed load leaves the container empty.

## Processing the YAML

The YAML is edited as individual files with relative links and stored in **/data**. This
is to keep them manageable and the ideas separate from each other during development. I
am not aware of a way to make **yq** honour those links as separate files, so they are
all combined into monolithic files before being used in the simulation.

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

The `*-base` keys - `ages-base`, `categories-base` and `item-base` - exist only to
define the anchors used by the relative links, and `item-base` is repeated in every
item file. `build.sh` explodes the links and drops them, so every key in a generated
file appears exactly once, and each item carries the fields inherited from its category
and age.

`build.sh` concatenates the files into one stream and `yq ea` walks it document by
document, so an item file's `*categoryChemicals` and `*ironAge` resolve against the
anchors defined in the category and age files that precede them. That is yq behaviour
across documents, and it is version dependent: v4.54.1 resolves them, v4.54.2 enforces
the spec's per-document anchor scope and the merge fails. The development shell and CI
both pin v4.54.1, CMake warns on any other version, and `DataTests` asserts the scale
of the merged documents - ages, categories, items, skills - so a merge that quietly
truncates the game fails the suite instead of shipping a short one.

Because the merge flattens the anchors, an item's `categoryId` and `ageId` are written
out as ordinary fields, and the core reads them as optional metadata.

The full item list in `game-mode/full.yaml` is a selection of the anchors defined in the
`trade-items/*` files. After adding, removing or renaming an item, add its `- item:`
reference to `game-mode/full.yaml` (and to `game-mode/simple.yaml` if the item belongs
in the small game), then run `./build.sh` from the **/data** directory.

`universe/solar-system-names.yaml` holds the name pools for the generated worlds. It is
UTF-8, it is loaded by `Core/Universe.h`, and it is not part of the merge: `build.sh`
does not read it.

`skill-tree/skills.yaml` defines 17 skills, each with `maxTiers: 3` and
`resourceCostPerTier: 1`, so the full game offers 51 tiers to buy. The skills are all
root nodes with `requires: []` and `exclusiveGroupId: null`: the tree is a set of
parallel branches, not a hierarchy, and `ProgressionManager::Build` validates that
shape. `DataTests` asserts every skill is worth at least three tiers, so a merge that
dropped `maxTiers` - the loader default is one tier, which flattens a branch to a
single click - fails the suite.

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
the pinned yq v4.54.1. A second job configures with the tests enabled and `yq` absent,
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
